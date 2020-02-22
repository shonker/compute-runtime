/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/bit_helpers.h"
#include "core/memory_manager/memory_manager.h"
#include "core/memory_manager/unified_memory_manager.h"

#include "CL/cl.h"
#include "extensions/public/cl_ext_private.h"
#include "helpers/mem_properties_parser_helper.h"
#include "mem_obj/mem_obj.h"
#include "memory_properties_flags.h"

namespace NEO {

class MemObjHelper {
  public:
    static const uint64_t extraFlags;
    static const uint64_t extraFlagsIntel;
    static const uint64_t commonFlags;
    static const uint64_t commonFlagsIntel;
    static const uint64_t validFlagsForBuffer;
    static const uint64_t validFlagsForBufferIntel;
    static const uint64_t validFlagsForImage;
    static const uint64_t validFlagsForImageIntel;

    static bool validateMemoryPropertiesForBuffer(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel);
    static bool validateMemoryPropertiesForImage(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem parent);
    static AllocationProperties getAllocationPropertiesWithImageInfo(uint32_t rootDeviceIndex, ImageInfo &imgInfo, bool allocateMemory, const MemoryPropertiesFlags &memoryProperties);
    static bool checkMemFlagsForSubBuffer(cl_mem_flags flags);
    static SVMAllocsManager::SvmAllocationProperties getSvmAllocationProperties(cl_mem_flags flags);
    static bool isSuitableForRenderCompression(bool renderCompressed, const MemoryPropertiesFlags &properties, Context &context, bool preferCompression);

  protected:
    static bool validateExtraMemoryProperties(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel);
};
} // namespace NEO
