/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "command_queue/command_queue.h"

using namespace NEO;

typedef api_tests clEnqueueMarkerWithWaitListTests;

TEST_F(clEnqueueMarkerWithWaitListTests, GivenNullCommandQueueWhenEnqueingMarkerWithWaitListThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueMarkerWithWaitList(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueMarkerWithWaitListTests, GivenValidCommandQueueWhenEnqueingMarkerWithWaitListThenSuccessIsReturned) {
    auto retVal = clEnqueueMarkerWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
