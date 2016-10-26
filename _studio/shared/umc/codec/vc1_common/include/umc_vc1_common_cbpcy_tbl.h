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

#ifndef __UMC_VC1_COMMON_CBPCY_TBL_H__
#define __UMC_VC1_COMMON_CBPCY_TBL_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 169: I-Picture CBPCY VLC Table
const extern Ipp32s VC1_CBPCY_Ipic[];

//VC-1 Table 170: P and B-Picture CBPCY VLC Table 0
const extern Ipp32s VC1_CBPCY_PBpic_tbl0[];

//VC-1 Table 171: P and B-Picture CBPCY VLC Table 1
const extern Ipp32s VC1_CBPCY_PBpic_tbl1[];

//VC-1 Table 172: P and B-Picture CBPCY VLC Table 2
const extern Ipp32s VC1_CBPCY_PBpic_tbl2[];

//VC-1 Table 173: P and B-Picture CBPCY VLC Table 3
const extern Ipp32s VC1_CBPCY_PBpic_tbl3[];

#endif //__umc_vc1_common_cbpcy_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
