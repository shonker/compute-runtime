/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/string.h"
#include "unit_tests/mocks/mock_platform.h"

#include "api/dispatch.h"
#include "gtest/gtest.h"
#include "sharings/sharing_factory.h"

using namespace NEO;

class IcdRestore : public SharingFactory {
  public:
    IcdRestore() {
        icdSnapshot = icdGlobalDispatchTable;
        memcpy_s(savedState, sizeof(savedState), sharingContextBuilder, sizeof(sharingContextBuilder));
        for (auto &builder : sharingContextBuilder) {
            builder = nullptr;
        }
    }

    ~IcdRestore() {
        memcpy_s(sharingContextBuilder, sizeof(sharingContextBuilder), savedState, sizeof(savedState));
        icdGlobalDispatchTable = icdSnapshot;
    }

    template <typename F>
    void registerSharing(SharingType type) {
        auto object = std::make_unique<F>();
        sharingContextBuilder[type] = object.get();
        sharings.push_back(std::move(object));
    }

  protected:
    decltype(icdGlobalDispatchTable) icdSnapshot;
    decltype(SharingFactory::sharingContextBuilder) savedState;
    std::vector<std::unique_ptr<SharingBuilderFactory>> sharings;
};

void fakeGlF() {
}

class PlatformTestedSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override {
        return nullptr;
    }
    std::string getExtensions() override {
        return "--extension--";
    };
    void fillGlobalDispatchTable() override {
        icdGlobalDispatchTable.clCreateFromGLBuffer = (decltype(icdGlobalDispatchTable.clCreateFromGLBuffer)) & fakeGlF;
    };
    void *getExtensionFunctionAddress(const std::string &functionName) override {
        return nullptr;
    }
};

TEST(PlatformIcdTest, WhenPlatformSetupThenDispatchTableInitialization) {
    IcdRestore icdRestore;
    icdGlobalDispatchTable.clCreateFromGLBuffer = nullptr;
    EXPECT_EQ(nullptr, icdGlobalDispatchTable.clCreateFromGLBuffer);

    MockPlatform myPlatform;
    myPlatform.fillGlobalDispatchTable();
    EXPECT_EQ(nullptr, icdGlobalDispatchTable.clCreateFromGLBuffer);

    icdRestore.registerSharing<PlatformTestedSharingBuilderFactory>(SharingType::CLGL_SHARING);
    myPlatform.fillGlobalDispatchTable();
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateFromGLBuffer);
}
