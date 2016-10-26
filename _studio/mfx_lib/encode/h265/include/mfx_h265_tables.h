//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_TABLES_H__
#define __MFX_H265_TABLES_H__

namespace H265Enc {

extern const Ipp8s h265_log2m2[257];

extern const Ipp8u h265_type2idx[4];
extern const Ipp32u h265_min_in_group[10];
extern const Ipp32u h265_group_idx[32];

extern const Ipp8u h265_scan_z2r4[256];
extern const Ipp8u h265_scan_r2z4[256];
extern const Ipp16u *h265_sig_last_scan[3][5];
extern const Ipp16u h265_sig_scan_CG32x32[64];
extern const Ipp16u h265_sig_last_scan_8x8[ 4 ][ 4 ];
extern const Ipp32s h265_filteredModes[];
extern const Ipp32s h265_numPu[];

extern const Ipp8u h265_chroma422IntraAngleMappingTable[NUM_INTRA_MODE];

extern const Ipp8u h265_pgop_layers[PGOP_PIC_SIZE];

extern const CostType h265_lambda[100];

extern const float h265_reci_1to116[16];

const int PITCH_TU = 16;

} // namespace

#endif // __MFX_H265_TABLES_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
