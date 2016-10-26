//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>

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
inline _GENX_ matrix<float, R, C> cm_atan2_fast2(matrix_ref<int2, R, C> y, matrix_ref<int2, R, C> x, const uint flags = 0)
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
        matrix<float, 1, 16> fx = x.template select<16/C, 1, C, 1>(i/C, 0);
        matrix<float, 1, 16> fy = y.template select<16 / C, 1, C, 1>(i / C, 0);

        matrix<float, 1, 16> xy = fx * fy;
        fx = fx * fx;
        fy = fy * fy;

        a0 -= (xy / (fy + 0.28f * fx));
        a1 += (xy / (fx + 0.28f * fy));

        atan2.template select<16 / C, 1, C, 1>(i / C, 0).merge(a1, a0, cm_abs<int2>(y.template select<16 / C, 1, C, 1>(i / C, 0)) <= cm_abs<int2>(x.template select<16 / C, 1, C, 1>(i / C, 0)));
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
    cmtl::cm_vector_assign(g_Oneto36.select<g_Oneto36.SZ, 1>(0), 0, 1);
}

