//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2018 Intel Corporation. All Rights Reserved.
//

#include <stdlib.h>
#include "mfxvideo.h"

int main(int, char**)
{
    mfxVersion minVersion = { 0, 1 };
    mfxSession session = NULL;
    mfxStatus stsInit = MFX_ERR_NONE,
              stsVersion = MFX_ERR_NONE;
    stsInit = MFXInit(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11, &minVersion, &session);
    if (stsInit == MFX_ERR_NONE)
    {
        MFXClose(session);
        session = NULL;
    }
    minVersion.Minor = MFX_VERSION_MINOR;
    stsVersion = MFXInit(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11, &minVersion, &session);
    if (stsVersion == MFX_ERR_NONE)
    {
        MFXClose(session);
        session = NULL;
    }
    if (stsInit == MFX_ERR_NONE         && stsVersion == MFX_ERR_NONE)        return 0; //Everything is ok, driver exists and MediaSDK have a right version
    if (stsInit == MFX_ERR_NONE         && stsVersion == MFX_ERR_UNSUPPORTED) return 1; //MediaSDK version is lower than expected
    if (stsInit == MFX_ERR_UNSUPPORTED  && stsVersion == MFX_ERR_UNSUPPORTED) return 2; //Can't initialize MediaSDK, no MediaSDK/Driver on system
    return 3; //Something wrong happend while initialization
}
