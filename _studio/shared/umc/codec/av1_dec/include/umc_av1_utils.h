//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_UTILS_H_
#define __UMC_AV1_UTILS_H_

#include "umc_vp9_utils.h"

namespace UMC_AV1_DECODER
{
    using namespace UMC_VP9_DECODER;
    inline Ipp32u CeilLog2(Ipp32u x) { Ipp32u l = 0; while (x > (1U << l)) l++; return l; }
}

#endif //__UMC_AV1_UTILS_H_
#endif //UMC_ENABLE_AV1_VIDEO_DECODER
