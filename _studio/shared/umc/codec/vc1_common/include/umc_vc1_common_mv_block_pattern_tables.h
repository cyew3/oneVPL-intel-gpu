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

#ifndef __UMC_VC1_COMMON_MV_BLOCK_PATTERN_TABLES_H__
#define __UMC_VC1_COMMON_MV_BLOCK_PATTERN_TABLES_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 117: 4MV block pattern table 0
const extern Ipp32s VC1_MV4BlockPatternTable0[];

//VC-1 Table 118: 4MV block pattern table 1
const extern Ipp32s VC1_MV4BlockPatternTable1[];

//VC-1 Table 119: 4MV block pattern table 2
const extern Ipp32s VC1_MV4BlockPatternTable2[];

//VC-1 Table 120: 4MV block pattern table 3
const extern Ipp32s VC1_MV4BlockPatternTable3[];

//VC-1 Table 121: 2MV block pattern table 0
const extern Ipp32s VC1_MV2BlockPatternTable0[];

//VC-1 Table 122: 2MV block pattern table 1
const extern Ipp32s VC1_MV2BlockPatternTable1[];

//VC-1 Table 123: 2MV block pattern table 2
const extern Ipp32s VC1_MV2BlockPatternTable2[];

//VC-1 Table 124: 2MV block pattern table 3
const extern Ipp32s VC1_MV2BlockPatternTable3[];

#endif //__umc_vc1_common_mv_block_pattern_tables_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
