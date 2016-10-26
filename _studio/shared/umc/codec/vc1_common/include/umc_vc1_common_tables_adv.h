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

#ifndef __UMC_VC1_COMMON_TABLES_ADV_H__
#define __UMC_VC1_COMMON_TABLES_ADV_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 107: Refdist table
const extern Ipp32s VC1_FieldRefdistTable[];

#endif //__umc_vc1_common_tables_ADV_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
