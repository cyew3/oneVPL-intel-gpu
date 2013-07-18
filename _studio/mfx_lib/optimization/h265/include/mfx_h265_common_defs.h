//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#ifndef __MFX_H265_COMMON_DEFS_H__
#define __MFX_H265_COMMON_DEFS_H__

namespace MFX_HEVC_COMMON
{
    enum EnumInterpType
    {
        INTERP_HOR = 0,
        INTERP_VER
    };

    enum EnumAddAverageType
    {
        AVERAGE_NO = 0,
        AVERAGE_FROM_PIC,
        AVERAGE_FROM_BUF
    };
};

#endif // __MFX_H265_COMMON_DEFS_H__
#endif // defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
