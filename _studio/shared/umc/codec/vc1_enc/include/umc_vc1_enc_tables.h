// Copyright (c) 2008-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_TABLES_H_
#define _ENCODER_VC1_TABLES_H_
#include "ippvc.h"
#include "umc_vc1_enc_def.h"

namespace UMC_VC1_ENCODER
{

extern const uint32_t DCLumaLowMotionEncTableVLC   [120*2];
extern const uint32_t DCLumaHighMotionEncTableVLC  [120*2];
extern const uint32_t DCChromaLowMotionEncTableVLC [120*2];
extern const uint32_t DCCromaHighMotionEncTableVLC [120*2];

extern const uint8_t HighMotionIntraIndexLevelLast0[31];
extern const uint8_t HighMotionIntraIndexLevelLast1[38];
extern const uint8_t LowMotionIntraIndexLevelLast0 [21];
extern const uint8_t LowMotionIntraIndexLevelLast1 [27];
extern const uint8_t MidRateIntraIndexLevelLast0   [15];
extern const uint8_t MidRateIntraIndexLevelLast1   [21];
extern const uint8_t HighRateIntraIndexLevelLast0  [15];
extern const uint8_t HighRateIntraIndexLevelLast1  [17];


extern const uint8_t Mode3SizeConservativeVLC[12*2];
extern const uint8_t Mode3SizeEfficientVLC   [12*2];

extern const uint8_t  frameTypeCodesVLC[2][4*2];
extern const uint8_t  MVRangeCodesVLC     [4*2];
extern const uint8_t  ACTableCodesVLC     [3*2];

extern const uint8_t  DCQuantValues       [32];
extern const uint8_t  quantValue          [32];



extern const uint32_t* DCTables[2][2];
extern const sACTablesSet ACTablesSet[8];

extern const uint8_t MVModeP [4][4*2];


extern const uint8_t QuantProfileTableVLC[12*2];

extern const uint16_t*  MVDiffTablesVLC[4];

extern const uint8_t    MVSizeOffset[VC1_ENC_MV_LIMIT*3];
extern const uint8_t    longMVLength[4*2];
extern const int16_t   MVRange[4*2];
extern const int16_t   MVRangeFields[4*2];

extern const uint16_t   VLCTableCBPCY_I[64*2];

extern const    eCodingSet  LumaCodingSetsIntra[2][3];
extern const    eCodingSet  ChromaCodingSetsIntra[2][3];
extern const    eCodingSet  CodingSetsInter[2][3];

extern const uint16_t VLCTableCBPCY_PB_0[64*2];
extern const uint16_t VLCTableCBPCY_PB_1[64*2];
extern const uint16_t VLCTableCBPCY_PB_2[64*2];
extern const uint16_t VLCTableCBPCY_PB_3[64*2];

extern const uint16_t* VLCTableCBPCY_PB[4];

extern const uint8_t    BMVTypeVLC[2][3*2];
extern const uint8_t    BFractionVLC[9][8*2];
extern const uint8_t    BFractionScaleFactor[9][8];

extern const  int16_t TTMBVLC_HighRate[2][4][3*2];            /*PQUANT<5 */
extern const  int16_t TTMBVLC_MediumRate[2][4][3*2];          /*5<=PQUANT<13 */
extern const  int16_t TTMBVLC_LowRate[2][4][3*2];             /* PQUANT>=13 */

extern const uint8_t  TTBLKVLC_HighRate[4][3*2];
extern const uint8_t  TTBLKVLC_MediumRate[4][3*2];
extern const uint8_t  TTBLKVLC_LowRate[4][3*2];

extern const uint8_t  SubPattern4x4VLC_HighRate[16*2];           /*PQUANT<5 */
extern const uint8_t  SubPattern4x4VLC_MediumRate[16*2] ;        /*5<=PQUANT<13 */
extern const uint8_t  SubPattern4x4VLC_LowRate[16*2] ;           /* PQUANT>=13 */

extern const uint8_t SubPatternMask[4][2];
extern const uint8_t SubPattern8x4_4x8VLC[3*2];

const extern uint8_t* ZagTables_NoScan[4];
const extern uint8_t* ZagTables_I[3];
const extern uint8_t* ZagTables[4];
const extern uint8_t* ZagTables_Adv[4];
const extern uint8_t VC1_Inter_8x8_Scan[64];
const extern uint8_t VC1_Inter_8x4_Scan[32];
const extern uint8_t VC1_Inter_8x4_Scan_Adv[32];
const extern uint8_t VC1_Inter_4x8_Scan[32];
const extern uint8_t VC1_Inter_4x8_Scan_Adv[32];
const extern uint8_t VC1_Inter_4x4_Scan[16];
const extern uint8_t VC1_Inter_Interlace_8x4_Scan_Adv[32];
const extern uint8_t VC1_Inter_Interlace_4x8_Scan_Adv[32];
const extern uint8_t VC1_Inter_Interlace_4x4_Scan_Adv[16];

const extern eTransformType BlkTransformTypeTabl[4];

const extern uint8_t*  MBTypeFieldMixedTable_VLC[8] ;
const extern uint8_t*  MBTypeFieldTable_VLC[8] ;
const extern uint32_t* MVModeField2RefTable_VLC[8];
const extern uint32_t* MVModeField1RefTable_VLC[4];
const extern uint32_t* CBPCYFieldTable_VLC[4];

const extern uint8_t MVSizeOffsetFieldIndex [256];
const extern uint8_t MVSizeOffsetFieldExIndex [512];
const extern int16_t MVSizeOffsetField [10];
const extern int16_t MVSizeOffsetFieldEx [10];

const extern uint8_t VC1_Inter_InterlaceIntra_8x8_Scan_Adv[64];
const extern uint8_t* ZagTables_Fields[4];
extern const uint8_t MVModeBField [2][4*2];

const extern  uint8_t VC1_No_Scan_8x8[64];
const extern  uint8_t VC1_No_Scan_8x4[32];
const extern  uint8_t VC1_No_Scan_4x8[32];
const extern  uint8_t VC1_No_Scan_4x4[16];

const extern uint8_t MV4BP_0 [16*2];
const extern uint8_t MV4BP_1 [16*2];
const extern uint8_t MV4BP_2 [16*2];
const extern uint8_t MV4BP_3 [16*2];

const extern  uint8_t* MV4BP[4] ;
const extern  int32_t intesityCompensationTbl[257];

}
#endif
#endif
