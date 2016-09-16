/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
//          VP9 encoder common
//
*/

#pragma once

#include "mfx_common.h"
#include "mfx_ext_buffers.h"
#include "mfx_vp9_encode_hw_ddi.h"
#include "mfxvp9.h"

#if defined (AS_VP9E_PLUGIN)

namespace MfxHwVP9Encode
{

inline bool CheckPattern(mfxU32 inPattern)
{
    inPattern = inPattern & MFX_IOPATTERN_IN_MASK;
    return (inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY); // TODO: return correct check affter adding support of SYSTEM and OPAQUE memory at input
    /*return ( inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
                inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
                inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY);*/
}
inline bool CheckFrameSize(mfxU32 Width, mfxU32 Height)
{
    return ( ((Width & 0x0f) == 0) && ((Height& 0x0f) == 0) &&
                (Width > 0)  && (Width < 0x1FFF) &&
                (Height > 0) && (Height < 0x1FFF));
}

mfxStatus CheckEncodeFrameParam(mfxVideoParam const & video,
                                mfxEncodeCtrl       * ctrl,
                                mfxFrameSurface1    * surface,
                                mfxBitstream        * bs,
                                bool                  isExternalFrameAllocator);

mfxU16 GetDefaultAsyncDepth();

/*function for init/reset*/
mfxStatus CheckParametersAndSetDefault( mfxVideoParam*              pParamSrc,
                                        mfxVideoParam*              pParamDst,
                                        mfxExtCodingOptionVP9*      pExCodingVP8Dst,
                                        mfxExtOpaqueSurfaceAlloc*   pOpaqAllocDst,
                                        bool                        bExternalFrameAllocator,
                                        bool                        bReset = false);

/*functios for Query*/

mfxStatus SetSupportedParameters(mfxVideoParam* par);

mfxStatus CheckParameters(mfxVideoParam*   parSrc,
                            mfxVideoParam*   parDst);

mfxStatus CheckExtendedBuffers (mfxVideoParam* par);

mfxStatus CheckVideoParam(mfxVideoParam const & par, ENCODE_CAPS_VP9 const &caps);

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
