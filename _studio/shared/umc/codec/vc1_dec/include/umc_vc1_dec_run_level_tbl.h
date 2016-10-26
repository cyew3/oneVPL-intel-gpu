//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2009 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_DEC_RUN_LEVEL_TBL_H__
#define __UMC_VC1_DEC_RUN_LEVEL_TBL_H__


#include "umc_vc1_common_defs.h"

extern const Ipp32s VC1_DQScaleTbl[64];

const extern Ipp32s VC1_HighMotionIntraAC[];
const extern Ipp32s VC1_HighMotionInterAC[];
const extern Ipp32s VC1_LowMotionIntraAC[];
const extern Ipp32s VC1_LowMotionInterAC[];
const extern Ipp32s VC1_MidRateIntraAC[];
const extern Ipp32s VC1_MidRateInterAC[];
const extern Ipp32s VC1_HighRateIntraAC[];
const extern Ipp32s VC1_HighRateInterAC[];

#endif //__umc_vc1_dec_run_level_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
