//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2010 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H264_PAK_PRED_H_
#define _MFX_H264_PAK_PRED_H_

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_PAK)

namespace H264Pak
{
    class MbDescriptor;
    union CoeffData;

    void PredictMb(
        mfxFrameParamAVC& fp,
        mfxExtAvcRefSliceInfo& sliceInfo,
        mfxFrameSurface& fs,
        MbDescriptor& neighbours,
        mfxI16 scaleMatrix8x8[2][6][64],
        mfxI16 scaleMatrix8x8Inv[2][6][64],
        CoeffData& coeffData);
};

#endif // MFX_ENABLE_H264_VIDEO_PAK
#endif // _MFX_H264_PAK_PRED_H_
