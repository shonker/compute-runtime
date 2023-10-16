/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/global_operations/windows/sysman_os_global_operations_imp.h"

namespace L0 {
namespace Sysman {

bool WddmGlobalOperationsImp::getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) {
    return false;
}

bool WddmGlobalOperationsImp::getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) {
    return false;
}

void WddmGlobalOperationsImp::getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getWedgedStatus(zes_device_state_t *pState) {
}
void WddmGlobalOperationsImp::getRepairStatus(zes_device_state_t *pState) {
}
bool WddmGlobalOperationsImp::getUuid(std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    return false;
}
bool WddmGlobalOperationsImp::generateUuidFromPciBusInfo(const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    return false;
}
ze_result_t WddmGlobalOperationsImp::reset(ze_bool_t force) {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;
    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::GlobalOperationsComponent;
    request.requestId = KmdSysman::Requests::GlobalOperation::TriggerDeviceLevelReset;
    request.dataSize = sizeof(uint32_t);
    value = static_cast<uint32_t>(force);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
    return pKmdSysManager->requestSingle(request, response);
}

ze_result_t WddmGlobalOperationsImp::resetExt(zes_reset_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::scanProcessesState(std::vector<zes_process_state_t> &pProcessList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::deviceGetState(zes_device_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmGlobalOperationsImp::WddmGlobalOperationsImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsGlobalOperations *OsGlobalOperations::create(OsSysman *pOsSysman) {
    WddmGlobalOperationsImp *pWddmGlobalOperationsImp = new WddmGlobalOperationsImp(pOsSysman);
    return static_cast<OsGlobalOperations *>(pWddmGlobalOperationsImp);
}

} // namespace Sysman
} // namespace L0
