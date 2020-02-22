/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/device_factory.h"

#include "command_stream/aub_command_stream_receiver.h"
#include "command_stream/command_stream_receiver_with_aub_dump.h"
#include "command_stream/tbx_command_stream_receiver.h"

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    auto funcCreate = commandStreamReceiverFactory[executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->platform.eRenderCoreFamily];
    if (funcCreate == nullptr) {
        return nullptr;
    }
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr < 0) {
        csr = CommandStreamReceiverType::CSR_HW;
    }
    switch (csr) {
    case CSR_HW:
        commandStreamReceiver = funcCreate(false, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_AUB:
        commandStreamReceiver = AUBCommandStreamReceiver::create("aubfile", true, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_TBX:
        commandStreamReceiver = TbxCommandStreamReceiver::create("", false, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_HW_WITH_AUB:
        commandStreamReceiver = funcCreate(true, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_TBX_WITH_AUB:
        commandStreamReceiver = TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, rootDeviceIndex);
        break;
    default:
        break;
    }
    return commandStreamReceiver;
}

bool getDevicesImpl(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    if (DeviceFactory::isHwModeSelected()) {
        return DeviceFactory::getDevices(numDevicesReturned, executionEnvironment);
    }
    return DeviceFactory::getDevicesForProductFamilyOverride(numDevicesReturned, executionEnvironment);
}

} // namespace NEO
