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

#ifndef _UMC_VC1_COMMON_MVDIFF_TBL_H__
#define _UMC_VC1_COMMON_MVDIFF_TBL_H__

//VC-1 Table 247: Motion Vector Differential VLC Table 0
const extern Ipp32s VC1_Progressive_MV_Diff_tbl0[];

//VC-1 Table 248: Motion Vector Differential VLC Table 1
const extern Ipp32s VC1_Progressive_MV_Diff_tbl1[];

//VC-1 Table 249: Motion Vector Differential VLC Table 2
const extern Ipp32s VC1_Progressive_MV_Diff_tbl2[];
const extern Ipp32s VC1_Progressive_MV_Diff_tbl3[];

//VC-1 Table 250: Motion Vector Differential VLC Table 3

//Table 74: k_x and k_y specified by MVRANGE
const extern VC1MVRange VC1_MVRangeTbl[4];


#endif //_umc_vc1_common_mvdiff_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
