//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MFXSTRUCTURES_INT_H__
#define __MFXSTRUCTURES_INT_H__
#include "mfxstructures.h"
#include "mfx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

enum eMFXPlatform
{
    MFX_PLATFORM_SOFTWARE      = 0,
    MFX_PLATFORM_HARDWARE      = 1,
};

enum eMFXVAType
{
    MFX_HW_NO       = 0,
    MFX_HW_D3D9     = 1,
    MFX_HW_D3D11    = 2,
    MFX_HW_VAAPI    = 4,// Linux VA-API
#ifndef OPEN_SOURCE
    MFX_HW_VDAAPI   = 5,// OS X VDA-API
#endif
};

enum eMFXHWType
{
    MFX_HW_UNKNOWN   = 0,
    MFX_HW_SNB       = 0x300000,

    MFX_HW_IVB       = 0x400000,

    MFX_HW_HSW       = 0x500000,
    MFX_HW_HSW_ULT   = 0x500001,

    MFX_HW_VLV       = 0x600000,

    MFX_HW_BDW       = 0x700000,

    MFX_HW_CHT       = 0x800000,

    MFX_HW_SCL       = 0x900000,

    MFX_HW_APL       = 0x1000000,

    MFX_HW_KBL       = 0x1100000,
    MFX_HW_GLK       = MFX_HW_KBL + 1,
    MFX_HW_CFL       = MFX_HW_KBL + 2,

    MFX_HW_CNL       = 0x1200000,

#ifndef MFX_CLOSED_PLATFORMS_DISABLE

    MFX_HW_ICL       = 0x1400000,
    MFX_HW_ICL_LP    = MFX_HW_ICL + 1,
    MFX_HW_CNX_G     = MFX_HW_ICL + 2,

    MFX_HW_LKF       = 0x1500000,
    MFX_HW_JSL       = MFX_HW_LKF + 1,

    MFX_HW_TGL_LP    = 0x1600000,
    MFX_HW_TGL_HP    = MFX_HW_TGL_LP + 1,
#endif
};

// mfxU8 CodecProfile, CodecLevel
// They are the same numbers as used in specific codec standards.
enum {
    // AVC Profiles & Levels
    MFX_PROFILE_AVC_HIGH10      =110,
};

enum
{
    MFX_FOURCC_IMC3         = MFX_MAKEFOURCC('I','M','C','3'),
    MFX_FOURCC_YUV400       = MFX_MAKEFOURCC('4','0','0','P'),
    MFX_FOURCC_YUV411       = MFX_MAKEFOURCC('4','1','1','P'),
    MFX_FOURCC_YUV422H      = MFX_MAKEFOURCC('4','2','2','H'),
    MFX_FOURCC_YUV422V      = MFX_MAKEFOURCC('4','2','2','V'),
    MFX_FOURCC_YUV444       = MFX_MAKEFOURCC('4','4','4','P'),
    MFX_FOURCC_RGBP         = MFX_MAKEFOURCC('R','G','B','P')
};

#ifdef __cplusplus
};
#endif

#endif // __MFXSTRUCTURES_H
