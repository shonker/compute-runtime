/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include <numeric>

extern PRODUCT_FAMILY productFamily;
extern GFXCORE_FAMILY renderCoreFamily;

TEST(IsDeviceBinaryFormatZebin, GivenValid32BitExecutableBinaryThenReturnTrue) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_32> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U)));
}

TEST(IsDeviceBinaryFormatZebin, GivenValidExecutableTypeBinaryThenReturnTrue) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U)));
}

TEST(IsDeviceBinaryFormatZebin, GivenValidRelocatableTypeBinaryThenReturnTrue) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_REL;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U)));
}

TEST(IsDeviceBinaryFormatZebin, GivenInvalidBinaryThenReturnFalse) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> someElf;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&someElf, 1U)));
}

TEST(UnpackSingleDeviceBinaryZebin, WhenInvalidElfThenUnpackingFails) {
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>({}, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Invalid or missing ELF header", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenUnhandledElfTypeThenUnpackingFails) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> someElf;
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&someElf, 1U), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Unhandled elf type\n", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenValidBinaryAndMatchedWithRequestedTargetDeviceThenReturnSelf) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_SKYLAKE);
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_GEN9_CORE);
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetDevice.productFamily = IGFX_UNKNOWN;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(zebin.machine);
    targetFlags.machineEntryUsesGfxCoreInsteadOfProductFamily = true;
    zebin.flags = targetFlags.packed;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenValidBinaryForDifferentDeviceThenUnpackingFails) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_SKYLAKE);
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine + 1);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8U;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device\n", unpackErrors.c_str());
    unpackErrors.clear();

    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_GEN9_CORE);
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetDevice.productFamily = IGFX_UNKNOWN;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(zebin.machine + 1);
    targetFlags.machineEntryUsesGfxCoreInsteadOfProductFamily = true;
    zebin.flags = targetFlags.packed;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device\n", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenValidBinaryWithUnsupportedPointerSizeThenUnpackingFails) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 4U;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device\n", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenNotRequestedThenDontValidateDeviceRevision) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8U;

    std::string unpackErrors;
    std::string unpackWarnings;
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetFlags.validateRevisionId = false;
    targetFlags.minHwRevisionId = 5;
    targetFlags.maxHwRevisionId = 7;
    zebin.flags = targetFlags.packed;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    targetDevice.stepping = 8U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenRequestedThenValidateRevision) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8U;

    std::string unpackErrors;
    std::string unpackWarnings;
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetFlags.validateRevisionId = true;
    targetFlags.minHwRevisionId = 5;
    targetFlags.maxHwRevisionId = 7;
    zebin.flags = targetFlags.packed;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device\n", unpackErrors.c_str());
    unpackErrors.clear();

    targetDevice.stepping = 8U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device\n", unpackErrors.c_str());
    unpackErrors.clear();

    targetDevice.stepping = 5U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    targetDevice.stepping = 6U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    targetDevice.stepping = 7U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenMachineIsIntelGTAndIntelGTNoteSectionIsValidThenReturnSelf) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.elfHeader->type = NEO::Elf::ET_REL;
    zebin.elfHeader->machine = NEO::Elf::ELF_MACHINE::EM_INTELGT;

    NEO::TargetDevice targetDevice;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.stepping = NEO::hardwareInfoTable[productFamily]->platform.usRevId;

    NEO::Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = (targetDevice.stepping == 0) ? targetDevice.stepping : targetDevice.stepping - 1;
    targetMetadata.maxHwRevisionId = targetDevice.stepping + 1;

    std::vector<NEO::Elf::ElfNoteSection> elfNoteSections;
    for (int i = 0; i < 3; i++) {
        auto &inserted = elfNoteSections.emplace_back();
        inserted.nameSize = 8;
        inserted.descSize = 4;
    }

    elfNoteSections.at(0).type = NEO::Elf::IntelGTSectionType::ProductFamily;
    elfNoteSections.at(1).type = NEO::Elf::IntelGTSectionType::GfxCore;
    elfNoteSections.at(2).type = NEO::Elf::IntelGTSectionType::TargetMetadata;

    std::vector<uint8_t *> descDatas;
    uint8_t platformData[4u];
    memcpy_s(platformData, 4u, &targetDevice.productFamily, 4u);
    descDatas.push_back(platformData);
    uint8_t coreData[4u];
    memcpy_s(coreData, 4u, &targetDevice.coreFamily, 4u);
    descDatas.push_back(coreData);
    uint8_t metadataPackedData[4u];
    memcpy_s(metadataPackedData, 4u, &targetMetadata.packed, 4u);
    descDatas.push_back(metadataPackedData);

    const auto sectionDataSize = std::accumulate(elfNoteSections.begin(), elfNoteSections.end(), size_t{0u},
                                                 [](auto totalSize, const auto &elfNoteSection) {
                                                     return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
                                                 });
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    auto sectionDataPointer = noteIntelGTSectionData.get();
    for (int i = 0; i < 3; i++) {
        memcpy_s(sectionDataPointer, sizeof(NEO::Elf::ElfNoteSection), &elfNoteSections.at(i), sizeof(NEO::Elf::ElfNoteSection));
        sectionDataPointer = ptrOffset(sectionDataPointer, sizeof(NEO::Elf::ElfNoteSection));
        strcpy_s(reinterpret_cast<char *>(sectionDataPointer), elfNoteSections.at(i).nameSize, NEO::Elf::IntelGtNoteOwnerName.str().c_str());
        sectionDataPointer = ptrOffset(sectionDataPointer, elfNoteSections.at(i).nameSize);
        memcpy_s(sectionDataPointer, elfNoteSections.at(i).descSize, descDatas.at(i), elfNoteSections.at(i).descSize);
        sectionDataPointer = ptrOffset(sectionDataPointer, elfNoteSections.at(i).descSize);
    }

    zebin.appendSection(NEO::Elf::SHT_NOTE, NEO::Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(zebin.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.productFamily, unpackResult.targetDevice.productFamily);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(zebin.storage.data(), unpackResult.deviceBinary.begin());
    EXPECT_EQ(zebin.storage.size(), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryZebin, GivenZebinWithSpirvAndBuildOptionsThenUnpackThemProperly) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t spirvData[30] = {0xd};
    zebin.appendSection(NEO::Elf::SHT_ZEBIN_SPIRV, NEO::Elf::SectionsNamesZebin::spv, spirvData);

    NEO::ConstStringRef buildOptions = "-cl-kernel-arg-info -cl-fast-relaxed-math";
    zebin.appendSection(NEO::Elf::SHT_ZEBIN_MISC, NEO::Elf::SectionsNamesZebin::buildOptions,
                        {reinterpret_cast<const uint8_t *>(buildOptions.data()), buildOptions.size()});

    auto elfHdrs = reinterpret_cast<NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> *>(
        ptrOffset(zebin.storage.data(), static_cast<size_t>(zebin.elfHeader->shOff)));
    auto spirvHdr = elfHdrs[3];
    auto buildOptionsHdr = elfHdrs[4];
    ASSERT_EQ(NEO::Elf::SHT_ZEBIN_SPIRV, spirvHdr.type);
    ASSERT_EQ(NEO::Elf::SHT_ZEBIN_MISC, buildOptionsHdr.type);

    zebin.elfHeader->type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.elfHeader->machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice = {};
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    targetDevice.maxPointerSizeInBytes = 8;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(zebin.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(zebin.storage.data(), unpackResult.deviceBinary.begin());

    EXPECT_FALSE(unpackResult.buildOptions.empty());
    auto buildOptionsPtr = reinterpret_cast<const char *>(ptrOffset(zebin.storage.data(), static_cast<size_t>(buildOptionsHdr.offset)));
    EXPECT_EQ(buildOptionsPtr, unpackResult.buildOptions.begin());

    EXPECT_FALSE(unpackResult.intermediateRepresentation.empty());
    auto spirvPtr = ptrOffset(zebin.storage.data(), static_cast<size_t>(spirvHdr.offset));
    EXPECT_EQ(spirvPtr, unpackResult.intermediateRepresentation.begin());
}

TEST(UnpackSingleDeviceBinaryZebin, GivenZebinForDifferentTargetDeviceWithIntermediateRepresentationThenDeviceBinaryIsEmptyIrIsSetAndWarningAboutRebuildIsReturned) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t spirvData[30] = {0xd};
    auto spirvHdr = zebin.appendSection(NEO::Elf::SHT_ZEBIN_SPIRV, NEO::Elf::SectionsNamesZebin::spv, spirvData);

    zebin.elfHeader->type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.elfHeader->machine = IGFX_UNKNOWN;

    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = IGFX_SKYLAKE;
    targetDevice.maxPointerSizeInBytes = 8;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(zebin.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_STREQ("Invalid target device. Rebuilding from intermediate representation.\n", unpackWarnings.c_str());

    EXPECT_TRUE(unpackResult.deviceBinary.empty());

    EXPECT_FALSE(unpackResult.intermediateRepresentation.empty());
    auto spirvPtr = ptrOffset(zebin.storage.data(), static_cast<size_t>(spirvHdr.offset));
    EXPECT_EQ(spirvPtr, unpackResult.intermediateRepresentation.begin());
}

TEST(UnpackSingleDeviceBinaryZebin, GivenMiscZebinSectionWithNameDifferentThanBuildOptionsThenItIsIgnored) {
    ZebinTestData::ValidEmptyProgram zebin;
    uint8_t secData;
    zebin.appendSection(NEO::Elf::SHT_ZEBIN_MISC, "not_build_options", ArrayRef<uint8_t>{&secData, 1U});

    zebin.elfHeader->type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.elfHeader->machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice = {};
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    targetDevice.maxPointerSizeInBytes = 8;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(zebin.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(zebin.storage.data(), unpackResult.deviceBinary.begin());
}
