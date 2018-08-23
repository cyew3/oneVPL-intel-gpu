//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <ippdefs.h>

extern int32_t MBAdressing[];

extern int32_t IMBType[];

extern int32_t PMBType[];

extern int32_t BMBType[];

extern int32_t MBPattern[];

extern int32_t MotionVector[];

extern int32_t dct_dc_size_luma[];

extern int32_t dct_dc_size_chroma[];

extern int32_t dct_coeff_first_RL[];

extern int32_t dct_coeff_next_RL[];

extern int32_t Table15[];

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
