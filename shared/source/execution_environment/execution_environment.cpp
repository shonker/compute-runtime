/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/affinity_mask.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/wait_util.h"

namespace NEO {
ExecutionEnvironment::ExecutionEnvironment() {
    WaitUtils::init();
    this->configureNeoEnvironment();
}

void ExecutionEnvironment::releaseRootDeviceEnvironmentResources(RootDeviceEnvironment *rootDeviceEnvironment) {
    if (rootDeviceEnvironment == nullptr) {
        return;
    }
    SipKernel::freeSipKernels(rootDeviceEnvironment, memoryManager.get());
    if (rootDeviceEnvironment->builtins.get()) {
        rootDeviceEnvironment->builtins->freeSipKernels(memoryManager.get());
        rootDeviceEnvironment->builtins.reset();
    }
    rootDeviceEnvironment->releaseDummyAllocation();
}

ExecutionEnvironment::~ExecutionEnvironment() {
    if (directSubmissionController) {
        directSubmissionController->stopThread();
    }
    if (memoryManager) {
        memoryManager->commonCleanup();
        for (const auto &rootDeviceEnvironment : this->rootDeviceEnvironments) {
            releaseRootDeviceEnvironmentResources(rootDeviceEnvironment.get());
        }
    }
    rootDeviceEnvironments.clear();
    mapOfSubDeviceIndices.clear();
    this->restoreCcsMode();
}

bool ExecutionEnvironment::initializeMemoryManager() {
    if (this->memoryManager) {
        return memoryManager->isInitialized();
    }

    auto csrType = obtainCsrTypeFromIntegerValue(debugManager.flags.SetCommandStreamReceiver.get(), CommandStreamReceiverType::hardware);

    switch (csrType) {
    case CommandStreamReceiverType::tbx:
    case CommandStreamReceiverType::tbxWithAub:
    case CommandStreamReceiverType::aub:
    case CommandStreamReceiverType::nullAub:
        memoryManager = std::make_unique<OsAgnosticMemoryManager>(*this);
        break;
    case CommandStreamReceiverType::hardware:
    case CommandStreamReceiverType::hardwareWithAub:
    default: {
        auto driverModelType = DriverModelType::unknown;
        if (this->rootDeviceEnvironments[0]->osInterface && this->rootDeviceEnvironments[0]->osInterface->getDriverModel()) {
            driverModelType = this->rootDeviceEnvironments[0]->osInterface->getDriverModel()->getDriverModelType();
        }
        memoryManager = MemoryManager::createMemoryManager(*this, driverModelType);
    } break;
    }

    return memoryManager->isInitialized();
}

void ExecutionEnvironment::calculateMaxOsContextCount() {
    MemoryManager::maxOsContextCount = 0u;
    for (const auto &rootDeviceEnvironment : this->rootDeviceEnvironments) {
        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto &engineInstances = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment);
        auto osContextCount = static_cast<uint32_t>(engineInstances.size());
        auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);
        auto ccsCount = hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
        bool hasRootCsr = subDevicesCount > 1;

        uint32_t numRegularEngines = 0;
        for (const auto &engine : engineInstances) {
            if (engine.second == EngineUsage::regular) {
                numRegularEngines++;
            }
        }

        if (gfxCoreHelper.getContextGroupContextsCount() > 0) {
            MemoryManager::maxOsContextCount += numRegularEngines * gfxCoreHelper.getContextGroupContextsCount();
            MemoryManager::maxOsContextCount += static_cast<uint32_t>(hwInfo->featureTable.ftrBcsInfo.count()); // LP contexts
        }

        MemoryManager::maxOsContextCount += osContextCount * subDevicesCount + hasRootCsr;

        if (ccsCount > 1 && debugManager.flags.EngineInstancedSubDevices.get()) {
            MemoryManager::maxOsContextCount += ccsCount * subDevicesCount;
        }
    }
}

DirectSubmissionController *ExecutionEnvironment::initializeDirectSubmissionController() {
    std::lock_guard<std::mutex> lockForInit(initializeDirectSubmissionControllerMutex);
    auto initializeDirectSubmissionController = DirectSubmissionController::isSupported();

    if (debugManager.flags.SetCommandStreamReceiver.get() > 0) {
        initializeDirectSubmissionController = false;
    }

    if (debugManager.flags.EnableDirectSubmissionController.get() != -1) {
        initializeDirectSubmissionController = debugManager.flags.EnableDirectSubmissionController.get();
    }

    if (initializeDirectSubmissionController && this->directSubmissionController == nullptr) {
        this->directSubmissionController = std::make_unique<DirectSubmissionController>();
        this->directSubmissionController->startThread();
    }

    return directSubmissionController.get();
}

void ExecutionEnvironment::prepareRootDeviceEnvironments(uint32_t numRootDevices) {
    if (rootDeviceEnvironments.size() < numRootDevices) {
        rootDeviceEnvironments.resize(numRootDevices);
    }
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        if (!rootDeviceEnvironments[rootDeviceIndex]) {
            rootDeviceEnvironments[rootDeviceIndex] = std::make_unique<RootDeviceEnvironment>(*this);
        }
    }
}

void ExecutionEnvironment::prepareForCleanup() const {
    for (auto &rootDeviceEnvironment : rootDeviceEnvironments) {
        if (rootDeviceEnvironment) {
            rootDeviceEnvironment->prepareForCleanup();
        }
    }
}

void ExecutionEnvironment::prepareRootDeviceEnvironment(const uint32_t rootDeviceIndexForReInit) {
    rootDeviceEnvironments[rootDeviceIndexForReInit] = std::make_unique<RootDeviceEnvironment>(*this);
}

bool ExecutionEnvironment::getSubDeviceHierarchy(uint32_t index, std::tuple<uint32_t, uint32_t, uint32_t> *subDeviceMap) {
    if (mapOfSubDeviceIndices.find(index) != mapOfSubDeviceIndices.end()) {
        *subDeviceMap = mapOfSubDeviceIndices.at(index);
        return true;
    } else {
        return false;
    }
}

void ExecutionEnvironment::parseAffinityMask() {

    // If the device hierarchy is Combined, then skip the affinity mask parsing until level zero device get.
    if (isCombinedDeviceHierarchy()) {
        return;
    }

    const auto &affinityMaskString = debugManager.flags.ZE_AFFINITY_MASK.get();

    if (affinityMaskString.compare("default") == 0 ||
        affinityMaskString.empty()) {
        return;
    }

    // If the user has requested FLAT device hierarchy models, then report all the sub devices as devices.
    bool exposeSubDevicesAsApiDevices = isExposingSubDevicesAsDevices();
    uint32_t numRootDevices = static_cast<uint32_t>(rootDeviceEnvironments.size());

    RootDeviceIndicesMap mapOfIndices;
    // Reserve at least for a size equal to rootDeviceEnvironments.size() times four,
    // which is enough for typical configurations
    size_t reservedSizeForIndices = numRootDevices * 4;
    mapOfIndices.reserve(reservedSizeForIndices);
    uint32_t hwSubDevicesCount = 0u;
    if (exposeSubDevicesAsApiDevices) {
        for (uint32_t currentRootDevice = 0u; currentRootDevice < static_cast<uint32_t>(rootDeviceEnvironments.size()); currentRootDevice++) {
            auto hwInfo = rootDeviceEnvironments[currentRootDevice]->getHardwareInfo();
            hwSubDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);
            uint32_t currentSubDevice = 0;
            mapOfIndices.push_back(std::make_tuple(currentRootDevice, currentSubDevice));
            for (currentSubDevice = 1; currentSubDevice < hwSubDevicesCount; currentSubDevice++) {
                mapOfIndices.push_back(std::make_tuple(currentRootDevice, currentSubDevice));
            }
        }

        numRootDevices = static_cast<uint32_t>(mapOfIndices.size());
    }

    std::vector<AffinityMaskHelper> affinityMaskHelper(numRootDevices);

    auto affinityMaskEntries = StringHelpers::split(affinityMaskString, ",");

    // Index of the Device to be returned to the user, not the physcial device index.
    uint32_t deviceIndex = 0;
    for (const auto &entry : affinityMaskEntries) {
        auto subEntries = StringHelpers::split(entry, ".");
        uint32_t rootDeviceIndex = StringHelpers::toUint32t(subEntries[0]);

        // tiles as devices
        if (exposeSubDevicesAsApiDevices) {
            if (rootDeviceIndex >= numRootDevices) {
                continue;
            }

            // FlatHierarchy not supported with AllowSingleTileEngineInstancedSubDevices
            // so ignore X.Y
            if (subEntries.size() > 1) {
                continue;
            }

            std::tuple<uint32_t, uint32_t> indexKey = mapOfIndices[rootDeviceIndex];
            auto hwDeviceIndex = std::get<0>(indexKey);
            auto tileIndex = std::get<1>(indexKey);
            affinityMaskHelper[hwDeviceIndex].enableGenericSubDevice(tileIndex);
            // Store the Physical Hierarchy for this SubDevice mapped to the Device Index passed to the user.
            mapOfSubDeviceIndices[deviceIndex++] = std::make_tuple(hwDeviceIndex, tileIndex, hwSubDevicesCount);
            continue;
        }

        // cards as devices
        if (rootDeviceIndex < numRootDevices) {
            auto hwInfo = rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
            auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);

            if (subEntries.size() > 1) {
                uint32_t subDeviceIndex = StringHelpers::toUint32t(subEntries[1]);

                bool enableSecondLevelEngineInstanced = ((subDevicesCount == 1) &&
                                                         (hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1) &&
                                                         debugManager.flags.AllowSingleTileEngineInstancedSubDevices.get());

                if (enableSecondLevelEngineInstanced) {
                    UNRECOVERABLE_IF(subEntries.size() != 2);

                    if (subDeviceIndex < hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled) {
                        // Store the Physical Hierarchy for this SubDevice mapped to the Device Index passed to the user.
                        mapOfSubDeviceIndices[rootDeviceIndex] = std::make_tuple(rootDeviceIndex, subDeviceIndex, subDevicesCount);
                        affinityMaskHelper[rootDeviceIndex].enableEngineInstancedSubDevice(0, subDeviceIndex); // Mask: X.Y
                    }
                } else if (subDeviceIndex < subDevicesCount) {
                    if (subEntries.size() == 2) {
                        // Store the Physical Hierarchy for this SubDevice mapped to the Device Index passed to the user.
                        mapOfSubDeviceIndices[rootDeviceIndex] = std::make_tuple(rootDeviceIndex, subDeviceIndex, subDevicesCount);
                        affinityMaskHelper[rootDeviceIndex].enableGenericSubDevice(subDeviceIndex); // Mask: X.Y
                    } else {
                        UNRECOVERABLE_IF(subEntries.size() != 3);
                        uint32_t ccsIndex = StringHelpers::toUint32t(subEntries[2]);

                        if (ccsIndex < hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled) {
                            affinityMaskHelper[rootDeviceIndex].enableEngineInstancedSubDevice(subDeviceIndex, ccsIndex); // Mask: X.Y.Z
                        }
                    }
                }
            } else {
                affinityMaskHelper[rootDeviceIndex].enableAllGenericSubDevices(subDevicesCount); // Mask: X
            }
        }
    }

    std::vector<std::unique_ptr<RootDeviceEnvironment>> filteredEnvironments;
    for (uint32_t i = 0u; i < numRootDevices; i++) {
        if (!affinityMaskHelper[i].isDeviceEnabled()) {
            continue;
        }

        rootDeviceEnvironments[i]->deviceAffinityMask = affinityMaskHelper[i];
        filteredEnvironments.emplace_back(rootDeviceEnvironments[i].release());
    }

    rootDeviceEnvironments.swap(filteredEnvironments);
}

void ExecutionEnvironment::sortNeoDevices() {
    std::sort(rootDeviceEnvironments.begin(), rootDeviceEnvironments.end(), comparePciIdBusNumber);
}

void ExecutionEnvironment::setDeviceHierarchy(const GfxCoreHelper &gfxCoreHelper) {
    NEO::EnvironmentVariableReader envReader;
    std::string hierarchyModel = envReader.getSetting("ZE_FLAT_DEVICE_HIERARCHY", std::string(gfxCoreHelper.getDefaultDeviceHierarchy()));
    if (strcmp(hierarchyModel.c_str(), "COMPOSITE") == 0) {
        setExposeSubDevicesAsDevices(false);
    }
    if (strcmp(hierarchyModel.c_str(), "FLAT") == 0) {
        setExposeSubDevicesAsDevices(true);
    }
    if (strcmp(hierarchyModel.c_str(), "COMBINED") == 0) {
        setCombinedDeviceHierarchy(true);
    }
}

void ExecutionEnvironment::adjustCcsCountImpl(RootDeviceEnvironment *rootDeviceEnvironment) const {
    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();
    auto &productHelper = rootDeviceEnvironment->getHelper<ProductHelper>();
    productHelper.adjustNumberOfCcs(*hwInfo);
}

void ExecutionEnvironment::adjustCcsCount() {
    parseCcsCountLimitations();

    for (auto rootDeviceIndex = 0u; rootDeviceIndex < rootDeviceEnvironments.size(); rootDeviceIndex++) {
        auto &rootDeviceEnvironment = rootDeviceEnvironments[rootDeviceIndex];
        UNRECOVERABLE_IF(!rootDeviceEnvironment);
        if (!rootDeviceEnvironment->isNumberOfCcsLimited()) {
            adjustCcsCountImpl(rootDeviceEnvironment.get());
        }
    }
}

void ExecutionEnvironment::adjustCcsCount(const uint32_t rootDeviceIndex) const {
    auto &rootDeviceEnvironment = rootDeviceEnvironments[rootDeviceIndex];
    UNRECOVERABLE_IF(!rootDeviceEnvironment);
    if (rootDeviceNumCcsMap.find(rootDeviceIndex) != rootDeviceNumCcsMap.end()) {
        rootDeviceEnvironment->limitNumberOfCcs(rootDeviceNumCcsMap.at(rootDeviceIndex));
    } else {
        adjustCcsCountImpl(rootDeviceEnvironment.get());
    }
}

void ExecutionEnvironment::parseCcsCountLimitations() {
    const auto &numberOfCcsString = debugManager.flags.ZEX_NUMBER_OF_CCS.get();

    if (numberOfCcsString.compare("default") == 0 ||
        numberOfCcsString.empty()) {
        return;
    }

    const uint32_t numRootDevices = static_cast<uint32_t>(rootDeviceEnvironments.size());

    auto numberOfCcsEntries = StringHelpers::split(numberOfCcsString, ",");

    for (const auto &entry : numberOfCcsEntries) {
        auto subEntries = StringHelpers::split(entry, ":");
        uint32_t rootDeviceIndex = StringHelpers::toUint32t(subEntries[0]);

        if (rootDeviceIndex < numRootDevices) {
            if (subEntries.size() > 1) {
                uint32_t maxCcsCount = StringHelpers::toUint32t(subEntries[1]);
                rootDeviceNumCcsMap.insert({rootDeviceIndex, maxCcsCount});
                rootDeviceEnvironments[rootDeviceIndex]->limitNumberOfCcs(maxCcsCount);
            }
        }
    }
}

void ExecutionEnvironment::configureNeoEnvironment() {
    if (debugManager.flags.NEO_CAL_ENABLED.get()) {
        debugManager.flags.UseKmdMigration.setIfDefault(0);
        debugManager.flags.SplitBcsSize.setIfDefault(256);
    }
}

bool ExecutionEnvironment::comparePciIdBusNumber(std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment1, std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment2) {
    const auto pciOrderVar = debugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.get();
    if (!pciOrderVar) {
        auto isIntegrated1 = rootDeviceEnvironment1->getHardwareInfo()->capabilityTable.isIntegratedDevice;
        auto isIntegrated2 = rootDeviceEnvironment2->getHardwareInfo()->capabilityTable.isIntegratedDevice;
        if (isIntegrated1 != isIntegrated2) {
            return isIntegrated2;
        }
    }

    // BDF sample format is : 00:02.0
    auto pciBusInfo1 = rootDeviceEnvironment1->osInterface->getDriverModel()->getPciBusInfo();
    auto pciBusInfo2 = rootDeviceEnvironment2->osInterface->getDriverModel()->getPciBusInfo();

    if (pciBusInfo1.pciDomain != pciBusInfo2.pciDomain) {
        return (pciBusInfo1.pciDomain < pciBusInfo2.pciDomain);
    }

    if (pciBusInfo1.pciBus != pciBusInfo2.pciBus) {
        return (pciBusInfo1.pciBus < pciBusInfo2.pciBus);
    }

    if (pciBusInfo1.pciDevice != pciBusInfo2.pciDevice) {
        return (pciBusInfo1.pciDevice < pciBusInfo2.pciDevice);
    }

    return (pciBusInfo1.pciFunction < pciBusInfo2.pciFunction);
}

} // namespace NEO
