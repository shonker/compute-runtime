/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/helpers/hardware_commands_helper_bdw_and_later.inl"

#include <cstdint>

namespace NEO {
using FamilyType = Gen9Family;
} // namespace NEO

#include "opencl/source/helpers/enable_hardware_commands_helper_gpgpu.inl"
