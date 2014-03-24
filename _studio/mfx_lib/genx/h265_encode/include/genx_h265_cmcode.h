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
#include "../include/utility_genx.h"

using namespace cmut;

_GENX_ vector<uint2, 36> g_Oneto36; //0, 1, 2, ..35

template <uint4 R, uint4 C>
inline void _GENX_ ReadGradient(SurfaceIndex SURF_SRC, uint4 x, uint4 y, matrix<int2, R, C> &outdx, matrix<int2, R, C> &outdy)
{
    ReadGradient(SURF_SRC, x, y, outdx.select_all(), outdy.select_all());
}

template <uint4 R, uint4 C>
inline void _GENX_ ReadGradient(SurfaceIndex SURF_SRC, uint4 x, uint4 y, matrix_ref<int2, R, C> outdx, matrix_ref<int2, R, C> outdy)
{
    matrix<uint1, R + 2, C * 2> src;
    matrix<int2, R + 2, C> temp;
    cmut::cmut_read(SURF_SRC, x - 1, y - 1, src);
    temp = cmut_cols<C>(src, 1) * 2;
    temp += cmut_cols<C>(src, 0);
    temp += cmut_cols<C>(src, 2);
    outdx = cm_add<int2>(cmut_rows<R>(temp, 2), -cmut_rows<R>(temp, 0));
    temp = cmut_cols<C>(src, 0) - cmut_cols<C>(src, 2);
    outdy = cmut_rows<R>(temp, 1) * 2;
    outdy += cmut_rows<R>(temp, 0);
    outdy += cmut_rows<R>(temp, 2);
}

template <uint R, uint C>
inline _GENX_ matrix<float, R, C> cm_atan2_fast2(matrix_ref<int2, R, C> y, matrix_ref<int2, R, C> x,
    const uint flags = 0)
{
#ifndef CM_CONST_PI
#define CM_CONST_PI 3.14159f
#define REMOVE_CM_CONSTPI_DEF
#endif
    static_assert((R*C % 16) == 0, "R*C must be multiple of 16");
    matrix<float, R, C> atan2;

    vector<unsigned short, R*C> mask;

#pragma unroll
    for (int i = 0; i < R*C; i+=16) {
        vector<float, 16> a0;
        vector<float, 16> a1;
        a0 = (float)(CM_CONST_PI * 0.75 + 2.0 * CM_CONST_PI / 33.0);
        a1 = (float)(CM_CONST_PI * 0.25 + 2.0 * CM_CONST_PI / 33.0);
        matrix<float, 1, 16> fx = x.select<16/C, 1, C, 1>(i/C, 0);
        matrix<float, 1, 16> fy = y.select<16 / C, 1, C, 1>(i / C, 0);

        matrix<float, 1, 16> xy = fx * fy;
        fx = fx * fx;
        fy = fy * fy;

        a0 -= (xy / (fy + 0.28f * fx));
        a1 += (xy / (fx + 0.28f * fy));

        atan2.select<16 / C, 1, C, 1>(i / C, 0).merge(a1, a0, cm_abs<int2>(y.select<16 / C, 1, C, 1>(i / C, 0)) <= cm_abs<int2>(x.select<16 / C, 1, C, 1>(i / C, 0)));
    }
    atan2 = matrix<float, R, C>(atan2, (flags & SAT));
    return atan2;
#ifdef REMOVE_CM_CONSTPI_DEF
#undef REMOVE_CM_CONSTPI_DEF
#undef CM_CONST_PI
#endif
}

template <typename Tgrad, typename Tang, typename Tamp, uint4 R, uint4 C>
inline _GENX_ void Gradiant2AngAmp_MaskAtan2(matrix<Tgrad, R, C> &dx, matrix<Tgrad, R, C> &dy, matrix<Tang, R, C> &ang, matrix<Tamp, R, C> &amp)
{
    Gradiant2AngAmp_MaskAtan2(dx.select_all(), dy.select_all(), ang.select_all(), amp.select_all());
}

template <typename Tgrad, typename Tang, typename Tamp, uint4 R, uint4 C>
inline _GENX_ void Gradiant2AngAmp_MaskAtan2(matrix_ref<Tgrad, R, C> dxIn, matrix_ref<Tgrad, R, C> dyIn, matrix_ref<Tang, R, C> ang, matrix_ref<Tamp, R, C> amp)
{
    ang = 10.504226f * cm_atan2_fast2(dyIn, dxIn);

    amp = cm_abs<Tgrad>(dxIn) +cm_abs<Tgrad>(dyIn);

    ang.merge(0, amp == 0);
}

template <typename Tang, typename Tamp, typename Thist, uint4 R, uint4 C, uint4 S>
inline _GENX_ void Histogram_iselect(matrix<Tang, R, C> &ang, matrix<Tamp, R, C> &amp, vector<Thist, S> &hist)
{
    Histogram_iselect(ang.select_all(), amp.select_all(), hist.template select<S, 1>(0));
}

template <typename Tang, typename Tamp, typename Thist, uint4 R, uint4 C, uint4 S>
inline _GENX_ void Histogram_iselect(matrix_ref<Tang, R, C> ang, matrix_ref<Tamp, R, C> amp, vector_ref<Thist, S> hist)
{
    // in our case, 4x4 selected from 8x8 is not contigous, so compiler will process SIMD4 instructions.
    // If we format them to vector first, then we will get SIMD8
    vector<Tang, R*C> vang = ang;
#pragma unroll
    for (int i = 0; i < R; i++)
#pragma unroll
        for (int j = 0; j < C; j++)
            hist(vang(i*R + j)) += amp(i, j);
}

// To use this method, need make sure g_Oneto36 is initialized
template <typename Tamp>
void _GENX_ inline
FindBestMod(vector_ref<Tamp, 36>hist, uint4 &maxCombineOut)
{
    vector <uint4, 36> histCombined = hist << 8;
    histCombined += g_Oneto36;
    maxCombineOut = cm_reduced_max<uint4>(histCombined) & 255;
}

void _GENX_ inline InitGlobalVariables()
{
    cm_vector_assign(g_Oneto36.select<g_Oneto36.SZ, 1>(0), 0, 1);
}

extern "C" _GENX_MAIN_  void
AnalyzeGradient2(SurfaceIndex SURF_SRC,
SurfaceIndex SURF_GRADIENT_4x4,
SurfaceIndex SURF_GRADIENT_8x8,
uint4        width)
{
    enum {
        W = 8,
        H = 8,
        HISTSIZE = sizeof(uint2)* 40,
        HISTBLOCKSIZE = W / 4 * HISTSIZE,
    };

    static_assert(W % 8 == 0 && H % 8 == 0, "Width and Height must be multiple of 8.");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1, 6, 8> data;
    matrix<int2, 8, 8> dx, dy;
    matrix<uint2, 8, 8> amp;
    matrix<uint2, 8, 8> ang;
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8 = 0;
    const int yBase = y * H;
    const int xBase = x * W;

    const int histLineSize = (HISTSIZE / 4) * width;
    const int nextLine = histLineSize - HISTBLOCKSIZE;
    uint offset = (yBase >> 2) * histLineSize + xBase * (HISTSIZE / 4);

    ReadGradient(SURF_SRC, xBase, yBase, dx, dy);
    Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);
#pragma unroll
    for (int yBlki = 0; yBlki < H; yBlki += 4) {
#pragma unroll
        for (int xBlki = 0; xBlki < W; xBlki += 4) {

            histogram4x4 = 0;
            Histogram_iselect(ang.select<4, 1, 4, 1>(yBlki, xBlki), amp.select<4, 1, 4, 1>(yBlki, xBlki), histogram4x4.select<histogram4x4.SZ, 1>(0));

            histogram8x8 += histogram4x4;

            write(SURF_GRADIENT_4x4, offset + 0, cmut_slice<32>(histogram4x4, 0));
            write(SURF_GRADIENT_4x4, offset + 64, cmut_slice<8>(histogram4x4, 32));
            offset += HISTSIZE;
        }
        offset += nextLine;
    }

    offset = ((width / 8) * y + x) * HISTSIZE;
    write(SURF_GRADIENT_8x8, offset + 0, cmut_slice<32>(histogram8x8, 0));
    write(SURF_GRADIENT_8x8, offset + 64, cmut_slice<8>(histogram8x8, 32));
}

extern "C" _GENX_MAIN_  void
AnalyzeGradient8x8(SurfaceIndex SURF_SRC,
SurfaceIndex SURF_GRADIENT_8x8,
uint4        width)
{
    enum {
        W = 8,
        H = 8,
        HISTSIZE = sizeof(uint2)* 40,
    };

    static_assert(W % 8 == 0 && H % 8 == 0, "Width and Height must be multiple of 8.");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1, 6, 8> data;
    matrix<int2, 8, 8> dx, dy;
    matrix<uint2, 8, 8> amp;
    matrix<uint2, 8, 8> ang;
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8 = 0;
    const int yBase = y * H;
    const int xBase = x * W;
    ReadGradient(SURF_SRC, xBase, yBase, dx, dy);
    Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);
    Histogram_iselect(ang.select_all(), amp.select_all(), histogram8x8.select<histogram8x8.SZ, 1>(0));

    uint offset = ((width / 8) * y + x) * HISTSIZE;
    write(SURF_GRADIENT_8x8, offset + 0, cmut_slice<32>(histogram8x8, 0));
    write(SURF_GRADIENT_8x8, offset + 64, cmut_slice<8>(histogram8x8, 32));
}

extern "C" _GENX_MAIN_  void
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

template <uint W, uint H>
inline _GENX_
void InterpolateBlockHDV(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                           SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG,
                           uint xBlk, uint yBlk)
{
    enum {
        BLOCK_W = W*2,
        BLOCK_H = H+3,
    };
    static_assert((BLOCK_W >= W + 3) && ((W & (W - 1)) == 0), "Width has to be power of 2 and greater than 3");

    matrix<uint1, BLOCK_H, BLOCK_W> data;
    matrix<int2, BLOCK_H, W> interH; // x+0.5, y+0
    matrix<int2, H, W> interV; // x+0, y+0.5
    matrix<int2, H, W> interD; // x+0.5, y+0.5

    matrix<uint1, BLOCK_H, W> outH; 
    matrix<uint1, H, W> outV; 
    matrix<uint1, H, W> outD; 

    read(SURF_FPEL, xBlk * W - 1, yBlk * H - 1, data);

    matrix_ref<uint1, BLOCK_H, W>x_2  = data.select<BLOCK_H, 1, W, 1>(0, 0); // x-1, y>=-1
    matrix_ref<uint1, BLOCK_H, W>x0   = data.select<BLOCK_H, 1, W, 1>(0, 1); // x+0
    matrix_ref<uint1, BLOCK_H, W>x2   = data.select<BLOCK_H, 1, W, 1>(0, 2); // x+1
    matrix_ref<uint1, BLOCK_H, W>x4   = data.select<BLOCK_H, 1, W, 1>(0, 3); // x+2

    matrix<int2, BLOCK_H, W> h_tmp = x0 + x2;
    interH = h_tmp * 5;
    interH -= x_2;
    interH -= x4;

    matrix_ref<int2, H, W>x1y_2  = interH.select<H, 1, W, 1>(0, 0); // y-1, x>=0.5
    matrix_ref<int2, H, W>x1y0   = interH.select<H, 1, W, 1>(1, 0); // y+0
    matrix_ref<int2, H, W>x1y2   = interH.select<H, 1, W, 1>(2, 0); // y+1
    matrix_ref<int2, H, W>x1y4   = interH.select<H, 1, W, 1>(3, 0); // y+2

    matrix<int2, H, W> d_tmp = x1y0 + x1y2;
    interD = d_tmp * 5;
    interD -= x1y_2;
    interD -= x1y4;
    interD += 32;
    outD = cm_shr<uint1>(interD, 6, SAT);

    interH += 4;
    outH = cm_shr<uint1>(interH, 3, SAT);

    matrix_ref<uint1, H, W>y_2  = data.select<H, 1, W, 1>(0, 1); // y-1, x>=0
    matrix_ref<uint1, H, W>y0   = data.select<H, 1, W, 1>(1, 1); // y+0
    matrix_ref<uint1, H, W>y2   = data.select<H, 1, W, 1>(2, 1); // y+1
    matrix_ref<uint1, H, W>y4   = data.select<H, 1, W, 1>(3, 1); // y+2

    matrix<int2, H, W> v_tmp = y0 + y2;
    interV = v_tmp * 5;
    interV -= y_2;
    interV -= y4;
    interV += 4;
    outV = cm_shr<uint1>(interV, 3, SAT);

    write(SURF_HPEL_HORZ, xBlk * W, yBlk * H, outH.select<H, 1, W, 1>(1, 0));
    write(SURF_HPEL_VERT, xBlk * W, yBlk * H, outV);
    write(SURF_HPEL_DIAG, xBlk * W, yBlk * H, outD);
}

template <uint W, uint H>
inline _GENX_
void InterpolateBlockH(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                           SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG,
                           uint xBlk, uint yBlk)
{
    enum {
        BLOCK_W = W*2,
        BLOCK_H = H,
    };
    static_assert((BLOCK_W >= W + 3) && ((W & (W - 1)) == 0), "Width has to be power of 2 and greater than 3");

    matrix<uint1, BLOCK_H, BLOCK_W> data;
    matrix<int2, BLOCK_H, W> interH; // x+0.5, y+0
    matrix<int2, H, W> interD; // x+0.5, y+0.5
    matrix<uint1, BLOCK_H, W> outH; 
    matrix<uint1, H, W> outD; 

    read(SURF_FPEL, xBlk * W - 1, yBlk * H, data);

    matrix_ref<uint1, BLOCK_H, W>x_2  = data.select<BLOCK_H, 1, W, 1>(0, 0); // x-1, y>=0
    matrix_ref<uint1, BLOCK_H, W>x0   = data.select<BLOCK_H, 1, W, 1>(0, 1); // x+0
    matrix_ref<uint1, BLOCK_H, W>x2   = data.select<BLOCK_H, 1, W, 1>(0, 2); // x+1
    matrix_ref<uint1, BLOCK_H, W>x4   = data.select<BLOCK_H, 1, W, 1>(0, 3); // x+2

    matrix<int2, BLOCK_H, W> h_tmp = x0 + x2;
    interH = h_tmp * 5;
    interH -= x_2;
    interH -= x4;
    interH += 4;
    outH = cm_shr<uint1>(interH, 3, SAT);

    write(SURF_HPEL_HORZ, xBlk * W, yBlk * H, outH);
}

template <uint W, uint H>
inline _GENX_
void InterpolateBlockD(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                           SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG,
                           uint xBlk, uint yBlk)
{
    enum {
        BLOCK_W = W*2,
        BLOCK_H = H+3,
    };
    static_assert((BLOCK_W >= W + 3) && ((W & (W - 1)) == 0)), "Width has to be power of 2 and greater than 3");

    matrix<uint1, BLOCK_H, BLOCK_W> data;
    matrix<int2, BLOCK_H, W> interH; // x+0.5, y+0
    matrix<int2, H, W> interD; // x+0.5, y+0.5
    matrix<uint1, H, W> outD; 

    read(SURF_FPEL, xBlk * W - 1, yBlk * H - 1, data);

    matrix_ref<uint1, BLOCK_H, W>x_2  = data.select<BLOCK_H, 1, W, 1>(0, 0); // x-1, y>=-1
    matrix_ref<uint1, BLOCK_H, W>x0   = data.select<BLOCK_H, 1, W, 1>(0, 1); // x+0
    matrix_ref<uint1, BLOCK_H, W>x2   = data.select<BLOCK_H, 1, W, 1>(0, 2); // x+1
    matrix_ref<uint1, BLOCK_H, W>x4   = data.select<BLOCK_H, 1, W, 1>(0, 3); // x+2

    matrix<int2, BLOCK_H, W> h_tmp = x0 + x2;
    interH = h_tmp * 5;
    interH -= x_2;
    interH -= x4;

    matrix_ref<int2, H, W>x1y_2  = interH.select<H, 1, W, 1>(0, 0); // y-1, x>=0.5
    matrix_ref<int2, H, W>x1y0   = interH.select<H, 1, W, 1>(1, 0); // y+0
    matrix_ref<int2, H, W>x1y2   = interH.select<H, 1, W, 1>(2, 0); // y+1
    matrix_ref<int2, H, W>x1y4   = interH.select<H, 1, W, 1>(3, 0); // y+2

    matrix<int2, H, W> d_tmp = x1y0 + x1y2;
    interD = d_tmp * 5;
    interD -= x1y_2;
    interD -= x1y4;
    interD += 32;
    outD = cm_shr<uint1>(interD, 6, SAT);

    write(SURF_HPEL_DIAG, xBlk * W, yBlk * H, outD);
}

template <uint W, uint H>
inline _GENX_
void InterpolateBlockHD(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                           SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG,
                           uint xBlk, uint yBlk)
{
    enum {
        BLOCK_W = W*2,
        BLOCK_H = H+3,
    };
    static_assert((BLOCK_W >= W + 3) && ((W & (W - 1)) == 0), "Width has to be power of 2 and greater than 3");

    matrix<uint1, BLOCK_H, BLOCK_W> data;
    matrix<int2, BLOCK_H, W> interH; // x+0.5, y+0
    matrix<int2, H, W> interD; // x+0.5, y+0.5
    matrix<uint1, H, W> outH; 
    matrix<uint1, H, W> outV; 
    matrix<uint1, H, W> outD; 

    read(SURF_FPEL, xBlk * W - 1, yBlk * H - 1, data);

    matrix_ref<uint1, BLOCK_H, W>x_2  = data.select<BLOCK_H, 1, W, 1>(0, 0); // x-1, y>=-1
    matrix_ref<uint1, BLOCK_H, W>x0   = data.select<BLOCK_H, 1, W, 1>(0, 1); // x+0
    matrix_ref<uint1, BLOCK_H, W>x2   = data.select<BLOCK_H, 1, W, 1>(0, 2); // x+1
    matrix_ref<uint1, BLOCK_H, W>x4   = data.select<BLOCK_H, 1, W, 1>(0, 3); // x+2

    matrix<int2, BLOCK_H, W> h_tmp = x0 + x2;
    interH = h_tmp * 5;
    interH -= x_2;
    interH -= x4;

    matrix_ref<int2, H, W>x1y_2  = interH.select<H, 1, W, 1>(0, 0); // y-1, x>=0.5
    matrix_ref<int2, H, W>x1y0   = interH.select<H, 1, W, 1>(1, 0); // y+0
    matrix_ref<int2, H, W>x1y2   = interH.select<H, 1, W, 1>(2, 0); // y+1
    matrix_ref<int2, H, W>x1y4   = interH.select<H, 1, W, 1>(3, 0); // y+2

    matrix<int2, H, W> d_tmp = x1y0 + x1y2;
    interD = d_tmp * 5;
    interD -= x1y_2;
    interD -= x1y4;
    interD += 32;
    outD = cm_shr<uint1>(interD, 6, SAT);

    interH += 4;
    outH = cm_shr<uint1>(interH.select<H, 1, W, 1>(1, 0), 3, SAT);

    write(SURF_HPEL_HORZ, xBlk * W, yBlk * H, outH);
    write(SURF_HPEL_DIAG, xBlk * W, yBlk * H, outD);
}

template <uint W, uint H>
inline _GENX_
void InterpolateBlockV(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                           SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG,
                           uint xBlk, uint yBlk)
{
    enum {
        BLOCK_W = W,
        BLOCK_H = H+3,
    };

    matrix<uint1, BLOCK_H, BLOCK_W> data;
    matrix<int2, H, W> interV; // x+0, y+0.5
    matrix<uint1, H, W> outV; 

    read(SURF_FPEL, xBlk * W, yBlk * H - 1, data);

    matrix_ref<uint1, H, W>y_2  = data.select<H, 1, W, 1>(0, 0); // y-1, x>=0
    matrix_ref<uint1, H, W>y0   = data.select<H, 1, W, 1>(1, 0); // y+0
    matrix_ref<uint1, H, W>y2   = data.select<H, 1, W, 1>(2, 0); // y+1
    matrix_ref<uint1, H, W>y4   = data.select<H, 1, W, 1>(3, 0); // y+2

    matrix<int2, H, W> v_tmp = y0 + y2;
    interV = v_tmp * 5;
    interV -= y_2;
    interV -= y4;
    interV += 4;
    outV = cm_shr<uint1>(interV, 3, SAT);

    write(SURF_HPEL_VERT, xBlk * W, yBlk * H, outV);
}

#endif