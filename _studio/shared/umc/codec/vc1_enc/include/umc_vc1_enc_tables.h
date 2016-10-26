//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_TABLES_H_
#define _ENCODER_VC1_TABLES_H_
#include "ippvc.h"
#include "umc_vc1_enc_def.h"

namespace UMC_VC1_ENCODER
{

extern const Ipp32u DCLumaLowMotionEncTableVLC   [120*2];
extern const Ipp32u DCLumaHighMotionEncTableVLC  [120*2];
extern const Ipp32u DCChromaLowMotionEncTableVLC [120*2];
extern const Ipp32u DCCromaHighMotionEncTableVLC [120*2];

extern const Ipp8u HighMotionIntraIndexLevelLast0[31];
extern const Ipp8u HighMotionIntraIndexLevelLast1[38];
extern const Ipp8u LowMotionIntraIndexLevelLast0 [21];
extern const Ipp8u LowMotionIntraIndexLevelLast1 [27];
extern const Ipp8u MidRateIntraIndexLevelLast0   [15];
extern const Ipp8u MidRateIntraIndexLevelLast1   [21];
extern const Ipp8u HighRateIntraIndexLevelLast0  [15];
extern const Ipp8u HighRateIntraIndexLevelLast1  [17];


extern const Ipp8u Mode3SizeConservativeVLC[12*2];
extern const Ipp8u Mode3SizeEfficientVLC   [12*2];

extern const Ipp8u  frameTypeCodesVLC[2][4*2];
extern const Ipp8u  MVRangeCodesVLC     [4*2];
extern const Ipp8u  ACTableCodesVLC     [3*2];

extern const Ipp8u  DCQuantValues       [32];
extern const Ipp8u  quantValue          [32];



extern const Ipp32u* DCTables[2][2];
extern const sACTablesSet ACTablesSet[8];

extern const Ipp8u MVModeP [4][4*2];


extern const Ipp8u QuantProfileTableVLC[12*2];

extern const Ipp16u*  MVDiffTablesVLC[4];

extern const Ipp8u    MVSizeOffset[VC1_ENC_MV_LIMIT*3];
extern const Ipp8u    longMVLength[4*2];
extern const Ipp16s   MVRange[4*2];
extern const Ipp16s   MVRangeFields[4*2];

extern const Ipp16u   VLCTableCBPCY_I[64*2];

extern const    eCodingSet  LumaCodingSetsIntra[2][3];
extern const    eCodingSet  ChromaCodingSetsIntra[2][3];
extern const    eCodingSet  CodingSetsInter[2][3];

extern const Ipp16u VLCTableCBPCY_PB_0[64*2];
extern const Ipp16u VLCTableCBPCY_PB_1[64*2];
extern const Ipp16u VLCTableCBPCY_PB_2[64*2];
extern const Ipp16u VLCTableCBPCY_PB_3[64*2];

extern const Ipp16u* VLCTableCBPCY_PB[4];

extern const Ipp8u    BMVTypeVLC[2][3*2];
extern const Ipp8u    BFractionVLC[9][8*2];
extern const Ipp8u    BFractionScaleFactor[9][8];

extern const  Ipp16s TTMBVLC_HighRate[2][4][3*2];            /*PQUANT<5 */
extern const  Ipp16s TTMBVLC_MediumRate[2][4][3*2];          /*5<=PQUANT<13 */
extern const  Ipp16s TTMBVLC_LowRate[2][4][3*2];             /* PQUANT>=13 */

extern const Ipp8u  TTBLKVLC_HighRate[4][3*2];
extern const Ipp8u  TTBLKVLC_MediumRate[4][3*2];
extern const Ipp8u  TTBLKVLC_LowRate[4][3*2];

extern const Ipp8u  SubPattern4x4VLC_HighRate[16*2];           /*PQUANT<5 */
extern const Ipp8u  SubPattern4x4VLC_MediumRate[16*2] ;        /*5<=PQUANT<13 */
extern const Ipp8u  SubPattern4x4VLC_LowRate[16*2] ;           /* PQUANT>=13 */

extern const Ipp8u SubPatternMask[4][2];
extern const Ipp8u SubPattern8x4_4x8VLC[3*2];

const extern Ipp8u* ZagTables_NoScan[4];
const extern Ipp8u* ZagTables_I[3];
const extern Ipp8u* ZagTables[4];
const extern Ipp8u* ZagTables_Adv[4];
const extern Ipp8u VC1_Inter_8x8_Scan[64];
const extern Ipp8u VC1_Inter_8x4_Scan[32];
const extern Ipp8u VC1_Inter_8x4_Scan_Adv[32];
const extern Ipp8u VC1_Inter_4x8_Scan[32];
const extern Ipp8u VC1_Inter_4x8_Scan_Adv[32];
const extern Ipp8u VC1_Inter_4x4_Scan[16];
const extern Ipp8u VC1_Inter_Interlace_8x4_Scan_Adv[32];
const extern Ipp8u VC1_Inter_Interlace_4x8_Scan_Adv[32];
const extern Ipp8u VC1_Inter_Interlace_4x4_Scan_Adv[16];

const extern eTransformType BlkTransformTypeTabl[4];

const extern Ipp8u*  MBTypeFieldMixedTable_VLC[8] ;
const extern Ipp8u*  MBTypeFieldTable_VLC[8] ;
const extern Ipp32u* MVModeField2RefTable_VLC[8];
const extern Ipp32u* MVModeField1RefTable_VLC[4];
const extern Ipp32u* CBPCYFieldTable_VLC[4];

const extern Ipp8u MVSizeOffsetFieldIndex [256];
const extern Ipp8u MVSizeOffsetFieldExIndex [512];
const extern Ipp16s MVSizeOffsetField [10];
const extern Ipp16s MVSizeOffsetFieldEx [10];

const extern Ipp8u VC1_Inter_InterlaceIntra_8x8_Scan_Adv[64];
const extern Ipp8u* ZagTables_Fields[4];
extern const Ipp8u MVModeBField [2][4*2];

const extern  Ipp8u VC1_No_Scan_8x8[64];
const extern  Ipp8u VC1_No_Scan_8x4[32];
const extern  Ipp8u VC1_No_Scan_4x8[32];
const extern  Ipp8u VC1_No_Scan_4x4[16];

const extern Ipp8u MV4BP_0 [16*2];
const extern Ipp8u MV4BP_1 [16*2];
const extern Ipp8u MV4BP_2 [16*2];
const extern Ipp8u MV4BP_3 [16*2];

const extern  Ipp8u* MV4BP[4] ;
const extern  Ipp32s intesityCompensationTbl[257];

}
#endif
#endif
