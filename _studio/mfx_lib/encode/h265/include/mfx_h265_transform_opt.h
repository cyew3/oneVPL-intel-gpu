//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_TRANSFORM_OPT_H__
#define __MFX_H265_TRANSFORM_OPT_H__

/* Forward transforms */
#include "mfx_h265_defs.h"

void h265_dst_fwd4x4(CoeffsType *srcdst,
                   Ipp32s bit_depth);

void h265_dct_fwd4x4(CoeffsType *srcdst,
                     Ipp32s bit_depth);

void h265_dct_fwd8x8(CoeffsType *srcdst,
                     Ipp32s bit_depth);

void h265_dct_fwd16x16(CoeffsType *srcdst,
                       Ipp32s bit_depth);


void h265_dct_fwd32x32(CoeffsType *srcdst,
                       Ipp32s bit_depth);

/* Inverse transforms */

void h265_dst_inv4x4(CoeffsType *srcdst,
                   Ipp32s bit_depth);

void h265_dct_inv4x4(CoeffsType *srcdst,
                     Ipp32s bit_depth);

void h265_dct_inv8x8(CoeffsType *srcdst,
                     Ipp32s bit_depth);

void h265_dct_inv16x16(CoeffsType *srcdst,
                       Ipp32s bit_depth);

void h265_dct_inv32x32(CoeffsType *srcdst,
                       Ipp32s bit_depth);

namespace MFX_HEVC_ENCODER
{
void inv_4x4_dct_sse2(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
void inv_4x4_dst_sse2(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
void inv_8x8_dct_sse2(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
void inv_16x16_dct_sse2(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
void DCTInverse32x32_sse(const short* __restrict src, void *destPtr, int destStride, int destSize);
};

#endif // __MFX_H265_TRANSFORM_OPT_H__
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
