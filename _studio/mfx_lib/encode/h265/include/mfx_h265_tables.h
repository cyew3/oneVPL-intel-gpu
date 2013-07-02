//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_TABLES_H__
#define __MFX_H265_TABLES_H__


extern const Ipp8s h265_log2m2[129];
extern const Ipp8u h265_log2table[];

extern const Ipp8u h265_type2idx[4];
extern const Ipp32u h265_min_in_group[10];
extern const Ipp32u h265_group_idx[32];

extern const Ipp8u *h265_scan_z2r[];
extern const Ipp8u *h265_scan_r2z[];
extern const Ipp16u *h265_sig_last_scan[3][5];
extern const Ipp16u h265_sig_scan_CG32x32[64];
extern const Ipp16u h265_sig_last_scan_8x8[ 4 ][ 4 ];
extern const Ipp8u h265_QPtoChromaQP[58];

#endif // __MFX_H265_TABLES_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
