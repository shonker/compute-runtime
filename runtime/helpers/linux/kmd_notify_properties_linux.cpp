/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/kmd_notify_properties.h"

using namespace NEO;

void KmdNotifyHelper::updateAcLineStatus() {}

int64_t KmdNotifyHelper::getBaseTimeout(const int64_t &multiplier) const {
    return properties->delayKmdNotifyMicroseconds * multiplier;
}
