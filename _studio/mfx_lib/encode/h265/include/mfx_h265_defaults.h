//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2015 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_ext_buffers.h"

namespace H265Enc {
    extern const Ipp8u DEFAULT_ENABLE_CM;
    extern const Ipp8u tab_defaultNumRefFrameSw[8];
    extern const Ipp8u tab_defaultNumRefFrameGacc[8];
    extern const Ipp8u tab_defaultNumRefFrameLowDelaySw[8];
    extern const Ipp8u tab_defaultNumRefFrameLowDelayGacc[8];
    extern const mfxExtCodingOptionHEVC tab_defaultOptHevcSw[8];
    extern const mfxExtCodingOptionHEVC tab_defaultOptHevcGacc[8];
};


#endif // MFX_ENABLE_H265_VIDEO_ENCODE
