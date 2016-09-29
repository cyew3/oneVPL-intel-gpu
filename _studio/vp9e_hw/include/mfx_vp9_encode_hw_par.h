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

mfxStatus CheckExtBufferHeaders(mfxVideoParam const &par, bool isRuntime = false);

mfxStatus SetSupportedParameters(mfxVideoParam & par);

mfxStatus CheckParameters(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps);

mfxStatus CheckParametersAndSetDefaults(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps);

mfxStatus SetDefaults(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps);

mfxStatus CheckEncodeFrameParam(mfxVideoParam const & video,
                                mfxEncodeCtrl       * ctrl,
                                mfxFrameSurface1    * surface,
                                mfxBitstream        * bs,
                                bool                  isExternalFrameAllocator);

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
