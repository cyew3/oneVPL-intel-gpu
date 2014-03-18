/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _GENX_H265_CMCODE_H_
#define _GENX_H265_CMCODE_H_

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm.h>
#include <genx_vme.h>

#define MBINTRADIST_SIZE    4 // mfxU16 intraDist;mfxU16 reserved;
#define MBDIST_SIZE     64  // 16*mfxU32
#define MVDATA_SIZE     4 // mfxI16Pair
#define DIST_SIZE       4
#define CURBEDATA_SIZE  160             //sizeof(H265EncCURBEData_t)

#define VME_CLEAR_UNIInput_IntraCornerSwap(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x7f)
#define VME_SET_CornerNeighborPixel0(p, v)    (VME_Input_S1(p, uint1, 1,  7)  = v)

#define SELECT_N_ROWS(m, from, nrows) m.select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS(m, from, ncols) m.select<m.ROWS, 1, ncols, 1>(0, from)
#define NROWS(m, from, nrows) SELECT_N_ROWS(m, from, nrows)
#define NCOLS(m, from, ncols) SELECT_N_COLS(m, from, ncols)
#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)

template <uint R, uint C>
inline _GENX_ matrix<float,R,C>
cm_atan2_fast2(matrix<int2,R,C> y, matrix<int2,R,C> x,
         const uint flags = 0)
{
    vector<float, R*C> a0;
    vector<float, R*C> a1;
    matrix<float,R,C> atan2;

    vector<unsigned short, R*C> mask;

    a0 = CM_CONST_PI * 0.75f;
    a1 = CM_CONST_PI * 0.25f;

    matrix<float,R,C> fx = x;
    matrix<float,R,C> fy = y;

    matrix<float,R,C> xy = fx * fy;
    matrix<float,R,C> x2 = fx * fx;
    matrix<float,R,C> y2 = fy * fy;

    a0 -= (xy / (y2 + 0.28f * x2));
    a1 += (xy / (x2 + 0.28f * y2));

    atan2.merge (a1, a0, cm_abs<int2>(y) <= cm_abs<int2>(x));
    atan2 = matrix<float,R,C>(atan2, (flags & SAT));
    return atan2;
}


extern "C" _GENX_MAIN_  void
AnalyzeGradient2(SurfaceIndex SURF_SRC,
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
    matrix<uint2, 4, 4> ang;
    matrix<uint2, 4, 4> mask_0;
    
    matrix<int2, 4, 4> absDx_copy;   
    matrix<int2, 4, 4> sloap, sloap_y;
    matrix<float, 4, 4> fdx, fdy, fdx_copy, ang_f;

    matrix<int4, 4, 4> tabIndex;
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

            ang_f = cm_atan2_fast2(dy, dx);
            ang = ang_f * 10.504226f; // div PI / 33
            ang += 2; 

            mask_0 = ((dx == 0) & (dy == 0));
            ang.merge(0, mask_0);

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

#endif