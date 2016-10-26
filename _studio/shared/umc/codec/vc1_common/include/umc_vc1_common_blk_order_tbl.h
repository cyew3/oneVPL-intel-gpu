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

#ifndef __UMC_VC1_COMMON_BLK_ORDER_TBL_H__
#define __UMC_VC1_COMMON_BLK_ORDER_TBL_H__

#include "ippdefs.h"

const extern Ipp32u VC1_pixel_table[6];
//const extern Ipp16u VC1_BlkOrderTbl[6][64];

const extern Ipp32u VC1_PredDCIndex[3][6];
const extern Ipp32u VC1_QuantIndex [2][6];

const extern Ipp32u VC1_BlockTable[50];

const extern Ipp32u VC1_BlkStart[];

#endif //__umc_vc1_common_blk_order_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
