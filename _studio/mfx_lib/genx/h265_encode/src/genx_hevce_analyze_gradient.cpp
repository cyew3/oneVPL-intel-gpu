//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>

#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)

static const uint1 MODE_INIT_TABLE[257] = {
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
    10, 10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
};

extern "C" _GENX_MAIN_  void
AnalyzeGradient(SurfaceIndex SURF_SRC,
                SurfaceIndex SURF_GRADIENT_4x4,
                SurfaceIndex SURF_GRADIENT_8x8,
                uint4        width)
{
    const uint W = 8;
    const uint H = 8;
    assert(W % 4 == 0 && H % 4 == 0);

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1, 6, 8> data;
    matrix<int2, 4, 4> dx, dy;
    matrix<uint2, 4, 4> absDx, absDy, amp;
    matrix<uint1, 4, 4> ang;
    matrix<int4, 4, 4> tabIndex;
    vector<uint1, 257> mode(MODE_INIT_TABLE);
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8 = 0;

#pragma unroll
    for (int yBlk = y * (H / 4); yBlk < y * (H / 4) + H / 4; yBlk++) {
#pragma unroll
        for (int xBlk = x * (W / 4); xBlk < x * (W / 4) + W / 4; xBlk++) {
            read(SURF_SRC, xBlk * 4 - 1, yBlk * 4 - 1, data);

            dx  = data.select<4, 1, 4, 1>(2, 0);     // x-1, y+1
            dx += data.select<4, 1, 4, 1>(2, 1) * 2; // x+0, y+1
            dx += data.select<4, 1, 4, 1>(2, 2);     // x+1, y+1
            dx -= data.select<4, 1, 4, 1>(0, 0);     // x-1, y-1
            dx -= data.select<4, 1, 4, 1>(0, 1) * 2; // x+0, y-1
            dx -= data.select<4, 1, 4, 1>(0, 2);     // x+1, y-1

            dy  = data.select<4, 1, 4, 1>(0, 0);
            dy += data.select<4, 1, 4, 1>(1, 0) * 2;
            dy += data.select<4, 1, 4, 1>(2, 0);
            dy -= data.select<4, 1, 4, 1>(0, 2);
            dy -= data.select<4, 1, 4, 1>(1, 2) * 2;
            dy -= data.select<4, 1, 4, 1>(2, 2);

            absDx = cm_abs<uint2>(dx);
            absDy = cm_abs<uint2>(dy);

            SIMD_IF_BEGIN(absDx >= absDy) {
                SIMD_IF_BEGIN(dx == 0) {
                    ang = 0;
                }
                SIMD_ELSE {
                    tabIndex = dy * 128;
                    tabIndex += dx / 2;
                    tabIndex /= dx;
                    tabIndex += 128;
                    ang = mode.iselect<uint4, 4 * 4>(tabIndex);
                }
                SIMD_IF_END;
            }
            SIMD_ELSE {
                SIMD_IF_BEGIN(dy == 0) {
                    ang = 0;
                }
                SIMD_ELSE {
                    tabIndex = dx * 128;
                    tabIndex += dy / 2;
                    tabIndex /= dy;
                    tabIndex += 128;
                    ang = 36 - mode.iselect<uint4, 4 * 4>(tabIndex);
                }
                SIMD_IF_END;
            }
            SIMD_IF_END;

            amp = absDx + absDy;

            histogram4x4 = 0;
#pragma unroll
            for (int i = 0; i < 4; i++)
#pragma unroll
                for (int j = 0; j < 4; j++)
                    histogram4x4( ang(i, j) ) += amp(i, j);

            histogram8x8 += histogram4x4;

            uint offset = ((width / 4) * yBlk + xBlk) * sizeof(uint2) * 40;
            write(SURF_GRADIENT_4x4, offset +  0, SLICE1(histogram4x4,  0, 32));
            write(SURF_GRADIENT_4x4, offset + 64, SLICE1(histogram4x4, 32,  8));
        }
    }

    uint offset = ((width / 8) * y + x) * sizeof(uint2) * 40;
    write(SURF_GRADIENT_8x8, offset +  0, SLICE1(histogram8x8,  0, 32));
    write(SURF_GRADIENT_8x8, offset + 64, SLICE1(histogram8x8, 32,  8));
}

