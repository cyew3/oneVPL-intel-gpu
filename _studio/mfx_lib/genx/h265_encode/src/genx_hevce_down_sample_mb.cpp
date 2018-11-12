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

extern "C" _GENX_MAIN_  void
DownSampleMB(SurfaceIndex SurfIndex, SurfaceIndex Surf2XIndex)
{   // Luma only
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint ix = mbX << 4;
    uint iy = mbY << 4;
    uint ox2x = mbX << 3;
    uint oy2x = mbY << 3;

    matrix<uint1,16,16> inMb;
    matrix<uint1,8,8> outMb2x;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, inMb);

    matrix<uint2,8,16> sum8x16_16;
    matrix<uint2,8,8> sum8x8_16;

    // for 2X
    sum8x16_16.select<8,1,16,1>(0,0) = inMb.select<8,2,16,1>(0,0) + inMb.select<8,2,16,1>(1,0);
    sum8x8_16.select<8,1,8,1>(0,0) = sum8x16_16.select<8,1,8,2>(0,0) + sum8x16_16.select<8,1,8,2>(0,1);

    sum8x8_16.select<8,1,8,1>(0,0) += 2;
    outMb2x = sum8x8_16.select<8,1,8,1>(0,0) >> 2;

    write_plane(Surf2XIndex, GENX_SURFACE_Y_PLANE, ox2x, oy2x, outMb2x);
}
