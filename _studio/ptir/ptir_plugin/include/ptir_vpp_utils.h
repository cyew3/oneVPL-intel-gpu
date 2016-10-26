//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_PTIR_VPP_UTILS_PLUGIN_INCLUDED__)
#define __MFX_PTIR_VPP_UTILS_PLUGIN_INCLUDED__

#include "mfxvideo.h"

mfxStatus ColorSpaceConversionWCopy(mfxFrameSurface1* surface_in, mfxFrameSurface1* surface_out, mfxU32 dstFormat);
mfxStatus Ptir420toMfxNv12(mfxU8* buffer, mfxFrameSurface1* surface_out);
mfxStatus MfxNv12toPtir420(mfxFrameSurface1* surface_out, mfxU8* buffer);

#endif  // __MFX_PTIR_VPP_UTILS_PLUGIN_INCLUDED__