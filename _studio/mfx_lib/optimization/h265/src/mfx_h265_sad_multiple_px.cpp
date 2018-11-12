// Copyright (c) 2013-2018 Intel Corporation
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

/* General HEVC SAD functions
 * calculates SAD's of 3 or 4 reference blocks (ref0-3) against a single source block (src)
 * no alignment assumed for input buffers
 */

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

namespace MFX_HEVC_PP
{

static Ipp32s h265_SAD_reference_8u32s_C1R(const Ipp8u* pSrcCur, Ipp32s srcCurStep, const Ipp8u* pSrcRef, Ipp32s srcRefStep, Ipp32s width, Ipp32s height)
{
    Ipp32s var, sad;

    sad = 0;
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            var = pSrcCur[i + j*srcCurStep] - pSrcRef[i + j*srcRefStep];
            sad += abs(var);
        }
    }

    return sad;
}

void H265_FASTCALL MAKE_NAME(h265_SAD_MxN_x3_8u)(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[3])
{
    sads[0] = h265_SAD_reference_8u32s_C1R(src, src_stride, ref0, ref_stride, width, height);
    sads[1] = h265_SAD_reference_8u32s_C1R(src, src_stride, ref1, ref_stride, width, height);
    sads[2] = h265_SAD_reference_8u32s_C1R(src, src_stride, ref2, ref_stride, width, height);
}

void H265_FASTCALL MAKE_NAME(h265_SAD_MxN_x4_8u)(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[4])
{
    sads[0] = h265_SAD_reference_8u32s_C1R(src, src_stride, ref0, ref_stride, width, height);
    sads[1] = h265_SAD_reference_8u32s_C1R(src, src_stride, ref1, ref_stride, width, height);
    sads[2] = h265_SAD_reference_8u32s_C1R(src, src_stride, ref2, ref_stride, width, height);
    sads[3] = h265_SAD_reference_8u32s_C1R(src, src_stride, ref3, ref_stride, width, height);
}

} // end namespace MFX_HEVC_PP

#endif
#endif

