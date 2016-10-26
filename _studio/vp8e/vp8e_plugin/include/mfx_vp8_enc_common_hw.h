//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE_HW)
 
#ifndef __MFX_VP8_ENC_COMMON_HW__H
#define __MFX_VP8_ENC_COMMON_HW__H
 
#include "mfxvp8.h"

namespace MFX_VP8ENC
{
    mfxU16 GetDefaultAsyncDepth();

    /*function for init/reset*/
    mfxStatus CheckParametersAndSetDefault( mfxVideoParam*              pParamSrc,
                                            mfxVideoParam*              pParamDst,
                                            mfxExtVP8CodingOption*      pExtVP8OptDst,
                                            mfxExtOpaqueSurfaceAlloc*   pOpaqAllocDst,
                                            bool                        bExternalFrameAllocator,
                                            bool                        bReset = false);

    /*functios for Query*/

    mfxStatus SetSupportedParameters(mfxVideoParam* par);

    mfxStatus CheckParameters(mfxVideoParam*   parSrc,
                              mfxVideoParam*   parDst);

    mfxStatus CheckExtendedBuffers (mfxVideoParam* par);

} // namespace MFX_VP8ENC
#endif 
#endif 