/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen11/hw_cmds.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/helpers/hardware_commands_helper_bdw_and_later.inl"

namespace NEO {
using FamilyType = Gen11Family;
} // namespace NEO

#include "opencl/source/helpers/enable_hardware_commands_helper_gpgpu.inl"
