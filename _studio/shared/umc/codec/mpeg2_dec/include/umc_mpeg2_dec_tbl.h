//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <ippdefs.h>

extern Ipp32s MBAdressing[];

extern Ipp32s IMBType[];

extern Ipp32s PMBType[];

extern Ipp32s BMBType[];

extern Ipp32s MBPattern[];

extern Ipp32s MotionVector[];

extern Ipp32s dct_dc_size_luma[];

extern Ipp32s dct_dc_size_chroma[];

extern Ipp32s dct_coeff_first_RL[];

extern Ipp32s dct_coeff_next_RL[];

extern Ipp32s Table15[];

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
