// Copyright (c) 2014-2019 Intel Corporation
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

typedef char  int1;
typedef short int2;
typedef int   int4;
typedef unsigned char  uint1;
typedef unsigned short uint2;
typedef unsigned int   uint4;

enum { X, Y };

template<uint R, uint C> inline _GENX_ matrix<uint1,R/2,C/2> Average2x2(matrix_ref<uint1,R,C> in)
{
    const int STEP = R/2<2?1:2; // to silence warning
    matrix<uint2,R,C/2> tmp = in.template select<R,1,C/2,2>(0,0) + in.template select<R,1,C/2,2>(0,1);
    matrix<uint2,R/2,C/2> tmp2 = tmp.template select<R/2,STEP,C/2,1>(0,0) + tmp.template select<R/2,STEP,C/2,1>(1,0);
    tmp2 += 2;
    return tmp2 >> 2;
}

extern "C" _GENX_MAIN_ void PrepareSrcNv12(SurfaceIndex SYS_LU, SurfaceIndex SYS_CH, SurfaceIndex VID_1X, SurfaceIndex VID_2X,
                                           SurfaceIndex VID_4X, SurfaceIndex VID_8X, SurfaceIndex VID_16X,
                                           uint sysPaddingLu, uint sysPaddingCh, uint copyChroma)
{
    vector<int,2> loc;
    loc(X) = get_thread_origin_x() * 64;
    loc(Y) = get_thread_origin_y() * 16;

    if (copyChroma) {
        #pragma unroll
        for (uint x = 0; x < 64; x += 32) {
            matrix<uint1,8,32> block;
            read(SYS_CH, sysPaddingCh + loc(X) + x, loc(Y) >> 1, block);
            write_plane(VID_1X, GENX_SURFACE_UV_PLANE, loc(X) + x, loc(Y) >> 1, block);
        }
    }

    matrix<uint1,16,64> block1x;

    #pragma unroll
    for (uint y = 0; y < 16; y += 8) {
        #pragma unroll
        for (uint x = 0; x < 64; x += 32) {
            matrix<uint1,8,32> block;
            read(SYS_LU, sysPaddingLu + loc(X) + x, loc(Y) + y, block);
            write(VID_1X, loc(X) + x, loc(Y) + y, block);
            block1x.select<8,1,32,1>(y, x) = block;
        }
    }

    matrix<uint1,8,32> block2x = Average2x2(block1x.select_all());
    matrix<uint1,4,16> block4x = Average2x2(block2x.select_all());
    matrix<uint1,2,8>  block8x = Average2x2(block4x.select_all());
    matrix<uint1,1,4> block16x = Average2x2(block8x.select_all());

    loc >>= 1; write(VID_2X,  loc(X), loc(Y), block2x);
    loc >>= 1; write(VID_4X,  loc(X), loc(Y), block4x);
    loc >>= 1; write(VID_8X,  loc(X), loc(Y), block8x);
    loc >>= 1; write(VID_16X, loc(X), loc(Y), block16x);
}
