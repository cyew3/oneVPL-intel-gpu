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

#define BORDER 4
#define MODE_SATD4 4
#define MODE_SATD8 8
#define SATD_BLOCKH 16
#define SATD_BLOCKW 16

_GENX_ vector<uint2, 36> g_Oneto36; //0, 1, 2, ..35
_GENX_ matrix<uint1, SATD_BLOCKH, SATD_BLOCKW> g_m_src;
_GENX_ matrix<uint1, SATD_BLOCKH, SATD_BLOCKW> g_m_avg;

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
    maxCombineOut = cm_reduced_max<uint4>(histCombined.select<33,1>(2)) & 255;
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
AnalyzeGradient3(SurfaceIndex SURF_SRC,
                 SurfaceIndex SURF_GRADIENT_4x4,
                 SurfaceIndex SURF_GRADIENT_8x8,
                 uint4        width)
{
    enum {
        W = 16,
        H = 16,
        HISTSIZE = sizeof(uint2)* 40,
        HISTBLOCKSIZE = 8 / 4 * HISTSIZE,
    };

    static_assert(W % 8 == 0 && H % 8 == 0, "Width and Height must be multiple of 8.");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<int2, 8, 8> dx, dy;
    matrix<uint2, 8, 8> amp;
    matrix<uint2, 8, 8> ang;
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8;
    const int yBase = y * H;
    const int xBase = x * W;

    const int histLineSize = (HISTSIZE / 4) * width;
    const int nextLine = histLineSize - HISTBLOCKSIZE;

#pragma unroll
    for (int yBlk8 = 0; yBlk8 < H; yBlk8 += 8) {
#pragma unroll
        for (int xBlk8 = 0; xBlk8 < W; xBlk8 += 8) {
            histogram8x8 = 0;
            ReadGradient(SURF_SRC, xBase + xBlk8, yBase + yBlk8, dx, dy);
            Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);

            uint offset = (yBase + yBlk8 >> 2) * histLineSize + (xBase + xBlk8) * (HISTSIZE / 4);
#pragma unroll
            for (int yBlk4 = 0; yBlk4 < 8; yBlk4 += 4) {
#pragma unroll
                for (int xBlk4 = 0; xBlk4 < 8; xBlk4 += 4) {

                    histogram4x4 = 0;
                    Histogram_iselect(ang.select<4, 1, 4, 1>(yBlk4, xBlk4), amp.select<4, 1, 4, 1>(yBlk4, xBlk4), histogram4x4.select<histogram4x4.SZ, 1>(0));

                    histogram8x8 += histogram4x4;

                    write(SURF_GRADIENT_4x4, offset + 0, cmut_slice<32>(histogram4x4, 0));
                    write(SURF_GRADIENT_4x4, offset + 64, cmut_slice<8>(histogram4x4, 32));
                    offset += HISTSIZE;
                }
                offset += nextLine;
            }

            offset = ((width / 8) * (yBase + yBlk8 >> 3) + (xBase + xBlk8 >> 3)) * HISTSIZE;
            write(SURF_GRADIENT_8x8, offset + 0, cmut_slice<32>(histogram8x8, 0));
            write(SURF_GRADIENT_8x8, offset + 64, cmut_slice<8>(histogram8x8, 32));
        }
    }
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

extern "C" _GENX_MAIN_  void
    AnalyzeGradient32x32Best(SurfaceIndex SURF_SRC,
                             SurfaceIndex SURF_GRADIENT_4x4,
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
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8;
    vector<uint4, 40> histogram16x16;
    vector<uint4, 40> histogram32x32=0;
    const int yBase = y * H;
    const int xBase = x * W;
    uint offset;

    matrix<uint4, 8, 8> output4x4 = 0;
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
            int xBlk8 = 0;
            int yBlk8 = 0;
            //#pragma unroll //remarked because after unroll compiler generates spilled code
            for (yBlk8 = 0; yBlk8 < 16; yBlk8 += 8) 
            {
#pragma unroll
                for (xBlk8 = 0; xBlk8 < 16; xBlk8 += 8) 
                {
                    ReadGradient(SURF_SRC, xBase + (xBlk16 + xBlk8), yBase + (yBlk16 + yBlk8), dx, dy);
                    Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);

                    histogram8x8 = 0;

#pragma unroll
                    for (int yBlk4 = 0; yBlk4 < 8; yBlk4 += 4) {
#pragma unroll
                        for (int xBlk4 = 0; xBlk4 < 8; xBlk4 += 4) {

                            histogram4x4 = 0;
                            Histogram_iselect(ang.select<4, 1, 4, 1>(yBlk4, xBlk4), amp.select<4, 1, 4, 1>(yBlk4, xBlk4), histogram4x4.select<histogram4x4.SZ, 1>(0));

                            histogram8x8 += histogram4x4;
                            FindBestMod(histogram4x4.select<36, 1>(0), output4x4((yBlk16 + yBlk8 + yBlk4) >> 2, (xBlk16 + xBlk8 + xBlk4) >> 2));
                        }
                    }

                    histogram16x16 += histogram8x8;
                    FindBestMod(histogram8x8.select<36, 1>(0), output8x8((yBlk16 + yBlk8) >> 3, (xBlk16 + xBlk8) >> 3));
                }
            }
            histogram32x32 += histogram16x16;
            FindBestMod(histogram16x16.select<36, 1>(0), output16x16(yBlk16 >> 4, xBlk16 >> 4));
        }
    }

    write(SURF_GRADIENT_4x4, x * (W / 4 * MODESIZE), y * (H / 4), output4x4);

    write(SURF_GRADIENT_8x8, x * (W / 8 * MODESIZE), y * (H / 8), output8x8);

    write(SURF_GRADIENT_16x16, x * (W / 16 * MODESIZE), y * (H / 16), output16x16);

    FindBestMod(histogram32x32.select<36, 1>(0), output32x32(0, 0));
    write(SURF_GRADIENT_32x32, x * (W / 32 * MODESIZE), y * (H / 32), output32x32);
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
}

template <uint W, uint H, uint B>
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
    matrix<uint1, H, W> outH; 
    matrix<uint1, H, W> outV; 
    matrix<uint1, H, W> outD; 

    read(SURF_FPEL, xBlk * W - B - 1, yBlk * H - B - 1, data);

    HoriDiagInterpolate(data, outH, outD);

    write(SURF_HPEL_HORZ, xBlk * W, yBlk * H, outH);
    write(SURF_HPEL_DIAG, xBlk * W, yBlk * H, outD);
}

template <uint W, uint H>
inline _GENX_
void VerticalInterpolate(matrix<uint1, H+3, W> &data, matrix<uint1, H, W> &outV)
{
    matrix<int2, H, W> interV; // x+0, y+0.5

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
}

template <uint W, uint H, uint B>
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
    matrix<uint1, H, W> outV; 

    read(SURF_FPEL, xBlk * W - B, yBlk * H - B - 1, data);

    VerticalInterpolate(data, outV);

    write(SURF_HPEL_VERT, xBlk * W, yBlk * H, outV);
}


template<uint R, uint C, uint WIDTH>
void _GENX_ inline sad_fixWidth(matrix_ref<uchar, R, C> a, matrix_ref<uchar, R, C> b, vector<ushort, WIDTH> &sum) {
    
    sum = 0;
    matrix_ref<uchar, R*C/WIDTH, WIDTH> a_ref = a.format<uchar, R*C/WIDTH, WIDTH>();
    matrix_ref<uchar, R*C/WIDTH, WIDTH> b_ref = b.format<uchar, R*C/WIDTH, WIDTH>();

#pragma unroll
    for (int i = 0; i<R*C/WIDTH; i++) {
        sum = cm_sada2<ushort>(a_ref.row(i), b_ref.row(i), sum);
    }
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSADFromIntPel(matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             matrix<uint1, BLOCKH, BLOCKW> m_src,
                             vector< uint,9 >& sad)
{
    enum
    {
        SAD_WIDTH = 16,
    };

    matrix<uint1, BLOCKH, BLOCKW> m_avg;
    vector<ushort, SAD_WIDTH> m_sad;

    m_avg = cm_avg<uint1>(m_ref_top, m_ref_left);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(0) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));    

    m_avg = cm_avg<uint1>(m_ref_top, m_ref);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(1) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref_top, m_ref_right);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(2) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(3) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    sad_fixWidth(m_ref.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(4) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0)); 

    m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(5) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   
 
    m_avg = cm_avg<uint1>(m_ref_bot, m_ref_left);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(6) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref_bot, m_ref);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(7) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));    

    m_avg = cm_avg<uint1>(m_ref_bot, m_ref_right);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(8) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   
}


template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSADFromHalfPel( matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             matrix<uint1, BLOCKH, BLOCKW> m_src,
                             vector< uint,9 >& sad)
{
    enum
    {
        SAD_WIDTH = 16,
    };

    matrix<uint1, BLOCKH, BLOCKW> m_avg;
    vector<ushort, SAD_WIDTH> m_sad;
 
    m_avg = cm_avg<uint1>(m_ref, m_ref_topleft);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH,1,BLOCKW,1>(0,0), m_sad);
    sad(0) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_top);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(1) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_topright);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(2) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(3) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    sad_fixWidth(m_ref.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(4) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0)); 

    m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(5) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   
 
    m_avg = cm_avg<uint1>(m_ref, m_ref_botleft);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(6) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_bot);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(7) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_botright);
    sad_fixWidth(m_avg.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(8) = cm_sum<uint>(m_sad.select<SAD_WIDTH/2, 2>(0));   
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void read_block(SurfaceIndex SURF_IN, uint x, uint y, matrix<uint1, BLOCKH, BLOCKW>& m_in)
{
    enum
    {
        MAX_SIZE = 256
    };

    if (BLOCKH*BLOCKW <= MAX_SIZE)
    {
        read(SURF_IN, x, y, m_in);
    }
    else
    {
        const uint count = (BLOCKH*BLOCKW + MAX_SIZE - 1)/MAX_SIZE;
        const uint lines = MAX_SIZE/BLOCKW; 
        const uint done = lines*(count-1);
        const uint rest = BLOCKH - done;
        
        #pragma unroll
        for (int i=0; i<count-1; i++)
        {
            read(SURF_IN, x, y+lines*i, m_in.select<lines, 1, BLOCKW, 1>(lines*i, 0));
        }
        read(SURF_IN, x, y+done, m_in.select<rest, 1, BLOCKW, 1>(done, 0));
    }
}


template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtIntPel(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH, BLOCKW> m_ref;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_vert;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;
            
            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_REF, xref, yref, m_ref);
            read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            CalcQpelSADFromIntPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, m_src, sad_inter);
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtHalfPelDiag(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH, BLOCKW> m_ref;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_hori;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_vert;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_HPEL_DIAG, xref+BORDER, yref+BORDER, m_ref);
            read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_vert);
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            CalcQpelSADFromIntPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, m_src, sad_inter);
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtHalfPelHorz(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref;
    matrix<uint1, BLOCKH, BLOCKW> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW*2> m_ref_vert;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_diag;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_REF, xref, yref, m_ref);
            read_block(SURF_HPEL_DIAG, xref+BORDER, yref-1+BORDER, m_ref_diag);
            read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_diag.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_diag.select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_topright = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(1, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botright = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(1, 1);

            CalcQpelSADFromHalfPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_hori, m_src, sad_inter);
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtHalfPelVert(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref;
    matrix<uint1, BLOCKH+1, BLOCKW*2> m_ref_hori;
    matrix<uint1, BLOCKH, BLOCKW> m_ref_vert;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_diag;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_REF, xref, yref, m_ref);
            read_block(SURF_HPEL_DIAG, xref-1+BORDER, yref+BORDER, m_ref_diag);
            read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_vert);
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_diag.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_diag.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref.select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_topright = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(1, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botright = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(1, 1);

            CalcQpelSADFromHalfPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_vert, m_src, sad_inter);
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH>
_GENX_
vector< uint,9 > QpelRefinement(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                    SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                    uint x, uint y, int2 hpelMvx, int2 hpelMvy) 
{                
    static_assert(((BLOCKW & (BLOCKW - 1)) == 0), "Block width has to be power of 2");

    int2 xref = x + (hpelMvx >> 2);
    int2 yref = y + (hpelMvy >> 2);
    hpelMvx &= 3;
    hpelMvy &= 3;
    vector< uint,9 > sad;

    if (hpelMvx == 0 && hpelMvy == 0)
        InterpolateQpelAtIntPel< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    else if (hpelMvx == 2 && hpelMvy == 0)
        InterpolateQpelAtHalfPelHorz< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    else if (hpelMvx == 0 && hpelMvy == 2)
        InterpolateQpelAtHalfPelVert< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    else if (hpelMvx == 2 && hpelMvy == 2)
        InterpolateQpelAtHalfPelDiag< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    
    return sad;
}

template<uint W, uint H>
void _GENX_ inline transpose4Lines(matrix<short, H, W>& a) {
   
    matrix_ref<int, H, W/2> a_int = a.format<int, H, W/2>();
    matrix<short, 2, W/2> b;
    matrix_ref<int, 2, W/4> b_int = b.format<int, 2, W/4>();

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        b_int = a_int.select<2, 1, W/4, 2>(i*4, 1);
        a_int.select<2, 1, W/4, 2>(i*4, 1) = a_int.select<2, 1, W/4, 2>(i*4+2, 0);
        a_int.select<2, 1, W/4, 2>(i*4+2, 0) = b_int;
    }

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        b = a.select<2, 2, W/2, 2>(i*4, 1);
        a.select<2, 2, W/2, 2>(i*4, 1) = a.select<2, 2, W/2, 2>(i*4+1, 0);
        a.select<2, 2, W/2, 2>(i*4+1, 0) = b;
    }
    
}

template<uint W, uint H>
void _GENX_ inline transpose8Lines(matrix<short, H, W>& a) {
   
    matrix<short, 4, 4> b;
#pragma unroll
    for (int i=0; i<H/8; i++)
    {
#pragma unroll
        for (int j=0; j<W/8; j++)
        {
            b = a.select<4, 1, 4, 1>(i*8, j*8+4);
            a.select<4, 1, 4, 1>(i*8, j*8+4) = a.select<4, 1, 4, 1>(i*8+4, j*8);
            a.select<4, 1, 4, 1>(i*8+4, j*8) = b;
        }
    }

    matrix<short, 2, W/2> c;
    matrix_ref<int, 2, W/4> c_int = c.format<int, 2, W/4>();
    matrix_ref<int, H, W/2> a_int = a.format<int, H, W/2>();

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        c_int = a_int.select<2, 1, W/4, 2>(i*4, 1);
        a_int.select<2, 1, W/4, 2>(i*4, 1) = a_int.select<2, 1, W/4, 2>(i*4+2, 0);
        a_int.select<2, 1, W/4, 2>(i*4+2, 0) = c_int;
    }

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        c = a.select<2, 2, W/2, 2>(i*4, 1);
        a.select<2, 2, W/2, 2>(i*4, 1) = a.select<2, 2, W/2, 2>(i*4+1, 0);
        a.select<2, 2, W/2, 2>(i*4+1, 0) = c;
    }
}

template<uint BLOCKH, uint BLOCKW>
_GENX_
uint CalcBlockSATD4x4Combine() {
    
    uint satd = 0;
    const uint WIDTH = 4;

    matrix<short, BLOCKH, BLOCKW> diff = cm_add<short>(g_m_avg, -g_m_src);;
    vector<short, BLOCKW> a01, a23, b01, b23;
    vector<uint, BLOCKW> abs = 0;

#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        a01 = diff.row(0 + i*WIDTH) + diff.row(1 + i*WIDTH);
        a23 = diff.row(2 + i*WIDTH) + diff.row(3 + i*WIDTH);
        b01 = diff.row(0 + i*WIDTH) - diff.row(1 + i*WIDTH);
        b23 = diff.row(2 + i*WIDTH) - diff.row(3 + i*WIDTH);

        diff.row(0 + i*WIDTH) = a01 + a23;
        diff.row(1 + i*WIDTH) = a01 - a23;
        diff.row(2 + i*WIDTH) = b01 - b23;
        diff.row(3 + i*WIDTH) = b01 + b23;
    }

    transpose4Lines(diff);
#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        a01 = diff.row(0 + i*WIDTH) + diff.row(1 + i*WIDTH);
        a23 = diff.row(2 + i*WIDTH) + diff.row(3 + i*WIDTH);
        b01 = diff.row(0 + i*WIDTH) - diff.row(1 + i*WIDTH);
        b23 = diff.row(2 + i*WIDTH) - diff.row(3 + i*WIDTH);

        abs += cm_abs<uint>(cm_add<int2>(a01, a23));
        abs += cm_abs<uint>(cm_add<int2>(a01, -a23));
        abs += cm_abs<uint>(cm_add<int2>(b01, b23));
        abs += cm_abs<uint>(cm_add<int2>(b01, -b23));
    }

    satd = cm_sum<uint>(abs);

    return satd;
}

template<uint BLOCKH, uint BLOCKW>
_GENX_ 
uint CalcBlockSATD8x8Combine() {
    
    uint satd = 0;
    const uint WIDTH = 8;

    matrix<short, BLOCKH, BLOCKW> diff = cm_add<short>(g_m_avg, -g_m_src);
    vector<short, BLOCKW> t0, t1, t2, t3, t4, t5, t6, t7;
    vector<short, BLOCKW> s0, s1, s2, s3, s4, s5, s6, s7;
    vector<uint, BLOCKW> abs = 0;

#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        t0 = diff.row(0 + i*WIDTH) + diff.row(4 + i*WIDTH);
        t4 = diff.row(0 + i*WIDTH) - diff.row(4 + i*WIDTH);
        t1 = diff.row(1 + i*WIDTH) + diff.row(5 + i*WIDTH);
        t5 = diff.row(1 + i*WIDTH) - diff.row(5 + i*WIDTH);
        t2 = diff.row(2 + i*WIDTH) + diff.row(6 + i*WIDTH);
        t6 = diff.row(2 + i*WIDTH) - diff.row(6 + i*WIDTH);
        t3 = diff.row(3 + i*WIDTH) + diff.row(7 + i*WIDTH);
        t7 = diff.row(3 + i*WIDTH) - diff.row(7 + i*WIDTH);

        s0 = t0 + t2;
        s2 = t0 - t2;
        s1 = t1 + t3;
        s3 = t1 - t3;
        s4 = t4 + t6;
        s6 = t4 - t6;
        s5 = t5 + t7;
        s7 = t5 - t7;

        diff.row(0+ i*WIDTH) = s0 + s1;
        diff.row(1+ i*WIDTH) = s0 - s1;
        diff.row(2+ i*WIDTH) = s2 + s3;
        diff.row(3+ i*WIDTH) = s2 - s3;
        diff.row(4+ i*WIDTH) = s4 + s5;
        diff.row(5+ i*WIDTH) = s4 - s5;
        diff.row(6+ i*WIDTH) = s6 + s7;
        diff.row(7+ i*WIDTH) = s6 - s7;
    }

    transpose8Lines(diff);
#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        t0 = diff.row(0 + i*WIDTH) + diff.row(4 + i*WIDTH);
        t4 = diff.row(0 + i*WIDTH) - diff.row(4 + i*WIDTH);
        t1 = diff.row(1 + i*WIDTH) + diff.row(5 + i*WIDTH);
        t5 = diff.row(1 + i*WIDTH) - diff.row(5 + i*WIDTH);
        t2 = diff.row(2 + i*WIDTH) + diff.row(6 + i*WIDTH);
        t6 = diff.row(2 + i*WIDTH) - diff.row(6 + i*WIDTH);
        t3 = diff.row(3 + i*WIDTH) + diff.row(7 + i*WIDTH);
        t7 = diff.row(3 + i*WIDTH) - diff.row(7 + i*WIDTH);

        s0 = t0 + t2;
        s2 = t0 - t2;
        s1 = t1 + t3;
        s3 = t1 - t3;
        s4 = t4 + t6;
        s6 = t4 - t6;
        s5 = t5 + t7;
        s7 = t5 - t7;

        abs += cm_abs<uint>(cm_add<int2>(s0, s1));
        abs += cm_abs<uint>(cm_add<int2>(s0, -s1));
        abs += cm_abs<uint>(cm_add<int2>(s2, s3));
        abs += cm_abs<uint>(cm_add<int2>(s2, -s3));
        abs += cm_abs<uint>(cm_add<int2>(s4, s5));
        abs += cm_abs<uint>(cm_add<int2>(s4, -s5));
        abs += cm_abs<uint>(cm_add<int2>(s6, s7));
        abs += cm_abs<uint>(cm_add<int2>(s6, -s7));
    }

    satd = cm_sum<uint>(abs);

    return satd;
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromIntDiagPel4x4(matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_left);
    sad(0) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref);
    sad(1) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_right);
    sad(2) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_left);
    sad(6) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref);
    sad(7) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_right);
    sad(8) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
   
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromIntDiagPel8x8(matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_left);
    sad(0) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref);
    sad(1) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_right);
    sad(2) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_left);
    sad(6) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref);
    sad(7) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_right);
    sad(8) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
   
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromHalfPel4x4( matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topleft);
    sad(0) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_top);
    sad(1) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topright);
    sad(2) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botleft);
    sad(6) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_bot);
    sad(7) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botright);
    sad(8) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
    
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromHalfPel8x8( matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topleft);
    sad(0) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_top);
    sad(1) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topright);
    sad(2) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botleft);
    sad(6) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_bot);
    sad(7) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botright);
    sad(8) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
    
}

template <uint W, uint H, uint BLOCKW, uint BLOCKH, uint SADMODE>
inline _GENX_
void InterpolateQpelAtIntDiagPelSATD(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, int2 hpelMvx, int2 hpelMvy, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_ref;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_vert;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;

    for (int i=0; i<H/BLOCKH; i++)
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;
            
            read_block(SURF_SRC, x, y, g_m_src);

            if (hpelMvx == 0 && hpelMvy == 0){
                read_block(SURF_REF, xref, yref, m_ref);
                read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_hori);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            } 
            else {
                read_block(SURF_HPEL_DIAG, xref+BORDER, yref+BORDER, m_ref);
                read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_vert);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_hori);
            }
            
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_hori.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            if (MODE_SATD4 == SADMODE)
            {
                CalcQpelSATDFromIntDiagPel4x4<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, sad_inter);
            }
            else
            {
                CalcQpelSATDFromIntDiagPel8x8<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, sad_inter);
            }

            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH, uint SADMODE>
inline _GENX_
void InterpolateQpelAtHoriVertPelSATD(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, int2 hpelMvx, int2 hpelMvy, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref;
    matrix<uint1, BLOCKH, BLOCKW> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW*2> m_ref_vert;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_diag;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;

    for (int i=0; i<H/BLOCKH; i++)
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, g_m_src);

            if (hpelMvx == 2 && hpelMvy == 0)
            {
                read_block(SURF_REF, xref, yref, m_ref);
                read_block(SURF_HPEL_DIAG, xref+BORDER, yref-1+BORDER, m_ref_diag);
                read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_hori);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            }
            else
            {
                read_block(SURF_REF, xref, yref, m_ref_diag);
                read_block(SURF_HPEL_DIAG, xref-1+BORDER, yref+BORDER, m_ref);
                read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_vert);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_hori);        
            }

            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_diag.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_diag.select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_topright = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(1, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botright = m_ref_vert.select<BLOCKH, 1, BLOCKW, 1>(1, 1);

            if (MODE_SATD4 == SADMODE) 
            {
                 CalcQpelSATDFromHalfPel4x4<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                    m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_hori, sad_inter);
            }
            else
            {
                 CalcQpelSATDFromHalfPel8x8<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                    m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_hori, sad_inter);
           
            }
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH, uint SADMODE>
_GENX_
vector< uint,9 > QpelRefinementSATD(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                    SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                    uint x, uint y, int2 hpelMvx, int2 hpelMvy) 
{                
    static_assert(((BLOCKW & (BLOCKW - 1)) == 0), "Block width has to be power of 2");

    int2 xref = x + (hpelMvx >> 2);
    int2 yref = y + (hpelMvy >> 2);
    hpelMvx &= 3;
    hpelMvy &= 3;
    vector< uint,9 > sad;

    if (hpelMvx == 0 && hpelMvy == 0 || hpelMvx == 2 && hpelMvy == 2)
        InterpolateQpelAtIntDiagPelSATD< W, H, BLOCKW, BLOCKH, SADMODE >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, hpelMvx, hpelMvy, sad );
    else if (hpelMvx == 2 && hpelMvy == 0 || hpelMvx == 0 && hpelMvy == 2)
        InterpolateQpelAtHoriVertPelSATD< W, H, BLOCKW, BLOCKH, SADMODE >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, hpelMvx, hpelMvy, sad );
    return sad;
}

#endif