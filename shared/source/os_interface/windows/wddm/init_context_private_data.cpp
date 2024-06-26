/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

CREATECONTEXT_PVTDATA initPrivateData(OsContextWin &osContext) {
    CREATECONTEXT_PVTDATA privateData = {};
    privateData.IsProtectedProcess = FALSE;
    privateData.IsDwm = FALSE;
    privateData.GpuVAContext = TRUE;
    privateData.IsMediaUsage = false;
    privateData.UmdContextType = UMD_OCL;

    return privateData;
}

} // namespace NEO
