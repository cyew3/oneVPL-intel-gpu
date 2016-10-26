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

#ifndef __UMC_VC1_COMMON_H__
#define __UMC_VC1_COMMON_H__

#include "ippdefs.h"

namespace UMC
{
    namespace VC1Common
    {
        void SwapData                                        (Ipp8u *src, Ipp32u dataSize);
    }
}

#endif //__UMC_VC1_COMMON_H__

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
