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
#include "../include/utility_genx.h"
#include "../include/genx_hevce_analyze_gradient_common.h"

using namespace cmut;

extern "C" _GENX_MAIN_ void
AnalyzeGradient32x32Modes(SurfaceIndex SURF_SRC,
                          SurfaceIndex SURF_GRADIENT_8x8,
                          SurfaceIndex SURF_GRADIENT_16x16,
                          SurfaceIndex SURF_GRADIENT_32x32,
                          uint4        width)
{
    InitGlobalVariables();
    enum {
        W = 32,
        H = 32,
        MODESIZE = sizeof(uint4),
    };

    static_assert(W % 32 == 0 && H % 32 == 0, "Width and Height must be multiple of 32");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1, 6, 8> data;
    matrix<int2, 8, 8> dx, dy;
    matrix<uint2, 8, 8> amp;
    matrix<uint2, 8, 8> ang;
    vector<uint2, 40> histogram8x8;
    vector<uint4, 40> histogram16x16;
    vector<uint4, 40> histogram32x32=0;
    const int yBase = y * H;
    const int xBase = x * W;
    uint offset;

    matrix<uint4, 4, 4> output8x8 = 0;
    matrix<uint4, 2, 2> output16x16 = 0;
    matrix<uint4, 1, 1> output32x32 = 0;
    int yBlk16 = 0;
    int xBlk16 = 0;
//#pragma unroll
    for (yBlk16 = 0; yBlk16 < H; yBlk16 += 16) 
    {
//#pragma unroll
        for (xBlk16 = 0; xBlk16 < W; xBlk16 += 16) 
        {
            histogram16x16 = 0;
            matrix<uint4, 2, 2> output = 0;
            int xBlki = 0;
            int yBlki = 0;
//#pragma unroll //remarked because after unroll compiler generates spilled code
            for (yBlki = 0; yBlki < 16; yBlki += 8) 
            {
#pragma unroll
                for (xBlki = 0; xBlki < 16; xBlki += 8) 
                {
                    ReadGradient(SURF_SRC, xBase + (xBlk16 + xBlki), yBase + (yBlk16 + yBlki), dx, dy);
                    Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);

                    histogram8x8 = 0;
                    Histogram_iselect(ang, amp, histogram8x8);

                    histogram16x16 += histogram8x8;
                    FindBestMod(histogram8x8.select<36, 1>(0), output8x8((yBlk16 + yBlki) >> 3, (xBlk16+xBlki) >> 3));
                }
            }
            histogram32x32 += histogram16x16;
            FindBestMod(histogram16x16.select<36, 1>(0), output16x16(yBlk16 >> 4, xBlk16 >> 4));
        }
    }
    
    write(SURF_GRADIENT_8x8, x * (W / 8 * MODESIZE), y * (H / 8), output8x8);

    write(SURF_GRADIENT_16x16, x * (W / 16 * MODESIZE), y * (H / 16), output16x16);

    FindBestMod(histogram32x32.select<36, 1>(0), output32x32(0, 0));
    write(SURF_GRADIENT_32x32, x * (W / 32 * MODESIZE), y * (H / 32), output32x32);
}
