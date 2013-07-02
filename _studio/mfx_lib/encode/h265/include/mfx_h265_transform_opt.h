//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H265_TRANSFORM_OPT_H__
#define __MFX_H265_TRANSFORM_OPT_H__

/* Forward transforms */

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

#endif // __MFX_H265_TRANSFORM_OPT_H__
