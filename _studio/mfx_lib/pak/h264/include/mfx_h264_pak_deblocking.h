//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2009 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H264_PAK_DEBLOCKING_H_
#define _MFX_H264_PAK_DEBLOCKING_H_

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_PAK

namespace H264Pak
{
    class MbDescriptor;

    void DeblockMb(
        const mfxFrameParamAVC& fp,
        const mfxSliceParamAVC& sp,
        const mfxExtAvcRefPicInfo& refInfo,
        const MbDescriptor& mbInfo,
        const mfxFrameSurface& fs);
}

#endif // MFX_ENABLE_H264_VIDEO_PAK
#endif // _MFX_H264_PAK_DEBLOCKING_H_
