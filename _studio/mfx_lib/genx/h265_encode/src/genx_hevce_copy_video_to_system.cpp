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

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>
#include "../include/utility_genx.h"

enum { W=32, H=8 };

extern "C" _GENX_MAIN_ void CopyVideoToSystem420(SurfaceIndex SRC, SurfaceIndex DST_LU, SurfaceIndex DST_CH, uint srcPaddingLuInBytes, uint srcPaddingChInBytes)
{
    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1,H,W> luma;
    read_plane(SRC, GENX_SURFACE_Y_PLANE, W*x, H*y, luma);
    write(DST_LU, srcPaddingLuInBytes+W*x, H*y, luma);
    matrix<uint1,H/2,W> chroma;
    read_plane(SRC, GENX_SURFACE_UV_PLANE, W*x, H/2*y, chroma);
    write(DST_CH, srcPaddingChInBytes+W*x, H/2*y, chroma);
}
