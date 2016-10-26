//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2007 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_INTERLACE_MV_TABLES_H__
#define __UMC_VC1_COMMON_INTERLACE_MV_TABLES_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 133: 2-Field reference interlace MV table 0
const extern Ipp32s VC1_InterlacedMVDifTable0[];

//VC-1 Table 134: 2-Field reference interlace MV table 1
const extern Ipp32s VC1_InterlacedMVDifTable1[];

//VC-1 Table 135: 2-Field reference interlace MV table 2
const extern Ipp32s VC1_InterlacedMVDifTable2[];

//VC-1 Table 136: 2-Field reference interlace MV table 3
const extern Ipp32s VC1_InterlacedMVDifTable3[];

//VC-1 Table 137: 2-Field reference interlace MV table 4
const extern Ipp32s VC1_InterlacedMVDifTable4[];

//VC-1 Table 138: 2-Field reference interlace MV table 5
const extern Ipp32s VC1_InterlacedMVDifTable5[];

//VC-1 Table 139: 2-Field reference interlace MV table 6
const extern Ipp32s VC1_InterlacedMVDifTable6[];

//VC-1 Table 140: 2-Field reference interlace MV table 7
const extern Ipp32s VC1_InterlacedMVDifTable7[];

//VC-1 Table 141: 1-Field reference interlace MV table 0
const extern Ipp32s VC1_InterlacedMVDifTable8[];

//VC-1 Table 142: 1-Field reference interlace MV table 1
const extern Ipp32s VC1_InterlacedMVDifTable9[];

//VC-1 Table 143: 1-Field reference interlace MV table 2
const extern Ipp32s VC1_InterlacedMVDifTable10[];

//VC-1 Table 144: 1-Field reference interlace MV table 3
const extern Ipp32s VC1_InterlacedMVDifTable11[];


//for scaling mv predictors P picture
const extern VC1PredictScaleValuesPPic VC1_PredictScaleValuesPPicTbl1[16];
const extern VC1PredictScaleValuesPPic VC1_PredictScaleValuesPPicTbl2[16];

//for scaling mv predictors B picture
const extern VC1PredictScaleValuesBPic VC1_PredictScaleValuesBPicTbl1[16];

#endif //__umc_vc1_common_interlace_mv_tables_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
