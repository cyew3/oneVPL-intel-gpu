//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2017 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_VPX_DEC_COMMON__H__
#define __MFX_VPX_DEC_COMMON__H__

#include "mfx_config.h"
#include "mfx_common_int.h"

namespace MFX_VPX_Utility
{
    mfxStatus Query(VideoCORE*, mfxVideoParam const* in, mfxVideoParam* out, mfxU32 codecId, eMFXHWType type);

    bool CheckVideoParam(mfxVideoParam const*, mfxU32 codecId, eMFXPlatform platform = MFX_PLATFORM_HARDWARE);
    mfxStatus QueryIOSurfInternal(mfxVideoParam const*, mfxFrameAllocRequest*);

#ifndef OPEN_SOURCE
    // the function is used only by SW VP8/VP9 decoder plugins. Isn't required for open source
    mfxStatus Convert_YV12_to_NV12(mfxFrameData const* inData,  mfxFrameInfo const* inInfo, mfxFrameData* outData, mfxFrameInfo const* outInfo);
#endif
}

#endif //__MFX_VPX_DEC_COMMON__H__
