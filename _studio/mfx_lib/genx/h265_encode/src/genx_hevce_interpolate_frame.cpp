/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/


#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>

#define BORDER 4

template <uint W, uint H>
inline _GENX_
void VerticalInterpolate(matrix<uint1, H+3, W> &data, matrix<uint1, H, W> &outV)
{
    matrix<int2, H, W> interV; // x+0, y+0.5

    matrix_ref<uint1, H, W>y_2  = data.template select<H, 1, W, 1>(0, 0); // y-1, x>=0
    matrix_ref<uint1, H, W>y0   = data.template select<H, 1, W, 1>(1, 0); // y+0
    matrix_ref<uint1, H, W>y2   = data.template select<H, 1, W, 1>(2, 0); // y+1
    matrix_ref<uint1, H, W>y4   = data.template select<H, 1, W, 1>(3, 0); // y+2

    matrix<int2, H, W> v_tmp = y0 + y2;
    interV = v_tmp * 5;
    interV -= y_2;
    interV -= y4;
    interV += 4;
    outV = cm_shr<uint1>(interV, 3, SAT);
}


template <uint W, uint H, uint B>
inline _GENX_
void InterpolateBlockV(
    SurfaceIndex SURF_FPEL,
    SurfaceIndex SURF_HPEL_HORZ,
    SurfaceIndex SURF_HPEL_VERT,
    SurfaceIndex SURF_HPEL_DIAG,
    uint xBlk,
    uint yBlk)
{
    enum {
        BLOCK_W = W,
        BLOCK_H = H+3,
    };

    matrix<uint1, BLOCK_H, BLOCK_W> data;
    matrix<uint1, H, W> outV; 

    read(SURF_FPEL, xBlk * W - B, yBlk * H - B - 1, data);

    VerticalInterpolate(data, outV);

    write(SURF_HPEL_VERT, xBlk * W, yBlk * H, outV);
}


template <uint W, uint H>
inline _GENX_
void HoriDiagInterpolate(matrix<uint1, H+3, W*2> &data, matrix<uint1, H, W> &outH, matrix<uint1, H, W> &outD)
{
    enum {
        BLOCK_W = W*2,
        BLOCK_H = H+3,
    };

    matrix<int2, BLOCK_H, W> interH; // x+0.5, y+0
    matrix<int2, H, W> interD; // x+0.5, y+0.5

    matrix_ref<uint1, BLOCK_H, W>x_2  = data.template select<BLOCK_H, 1, W, 1>(0, 0); // x-1, y>=-1
    matrix_ref<uint1, BLOCK_H, W>x0   = data.template select<BLOCK_H, 1, W, 1>(0, 1); // x+0
    matrix_ref<uint1, BLOCK_H, W>x2   = data.template select<BLOCK_H, 1, W, 1>(0, 2); // x+1
    matrix_ref<uint1, BLOCK_H, W>x4   = data.template select<BLOCK_H, 1, W, 1>(0, 3); // x+2

    matrix<int2, BLOCK_H, W> h_tmp = x0 + x2;
    interH = h_tmp * 5;
    interH -= x_2;
    interH -= x4;
    
    matrix_ref<int2, H, W>x1y_2  = interH.template select<H, 1, W, 1>(0, 0); // y-1, x>=0.5
    matrix_ref<int2, H, W>x1y0   = interH.template select<H, 1, W, 1>(1, 0); // y+0
    matrix_ref<int2, H, W>x1y2   = interH.template select<H, 1, W, 1>(2, 0); // y+1
    matrix_ref<int2, H, W>x1y4   = interH.template select<H, 1, W, 1>(3, 0); // y+2

    matrix<int2, H, W> d_tmp = x1y0 + x1y2;
    interD = d_tmp * 5;
    interD -= x1y_2;
    interD -= x1y4;
    interD += 32;
    outD = cm_shr<uint1>(interD, 6, SAT);
    
    interH += 4;
    outH = cm_shr<uint1>(interH.template select<H, 1, W, 1>(1, 0), 3, SAT);
}


template <uint W, uint H, uint B>
inline _GENX_
void InterpolateBlockHD(
    SurfaceIndex SURF_FPEL,
    SurfaceIndex SURF_HPEL_HORZ,
    SurfaceIndex SURF_HPEL_VERT,
    SurfaceIndex SURF_HPEL_DIAG,
    uint xBlk,
    uint yBlk)
{
    enum {
        BLOCK_W = W*2,
        BLOCK_H = H+3,
    };
    static_assert((BLOCK_W >= W + 3) && ((W & (W - 1)) == 0), "Width has to be power of 2 and greater than 3");

    matrix<uint1, BLOCK_H, BLOCK_W> data;
    matrix<uint1, H, W> outH; 
    matrix<uint1, H, W> outV; 
    matrix<uint1, H, W> outD; 

    read(SURF_FPEL, xBlk * W - B - 1, yBlk * H - B - 1, data);

    HoriDiagInterpolate(data, outH, outD);

    write(SURF_HPEL_HORZ, xBlk * W, yBlk * H, outH);
    write(SURF_HPEL_DIAG, xBlk * W, yBlk * H, outD);
}


extern "C" _GENX_MAIN_
void InterpolateFrameWithBorder(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                                SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG)
{
    const uint2 BlockW = 8;
    const uint2 BlockH = 8;

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    InterpolateBlockV<BlockW, BlockH, BORDER>(SURF_FPEL, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y);
    InterpolateBlockHD<BlockW, BlockH, BORDER>(SURF_FPEL, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y);

}
