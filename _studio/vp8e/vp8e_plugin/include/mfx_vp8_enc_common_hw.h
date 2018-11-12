// Copyright (c) 2012-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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