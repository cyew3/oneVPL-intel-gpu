/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//  This sample was distributed or derived from the Intel's Media Samples package.
//  The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
//  or https://software.intel.com/en-us/media-client-solutions-support.
//        Copyright (c) 2010 - 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __SAMPLE_VPP_CONFIG_H
#define __SAMPLE_VPP_CONFIG_H

#include "mfxvideo.h"
#include "mfxvideo++.h"

enum
{
    NOT_INIT_VALUE      =   0xFFF7
};

// number of video enhancement filters (denoise, procamp, detail, video_analysis, image stab)
#define ENH_FILTERS_COUNT                5

#define VPP_PROCAMP_BRIGHTNESS_DEFAULT    0.0
#define VPP_PROCAMP_CONTRAST_DEFAULT      1.0
#define VPP_PROCAMP_HUE_DEFAULT           0.0
#define VPP_PROCAMP_SATURATION_DEFAULT    1.0

#define VPP_DENOISE_FACTOR_DEFAULT      NOT_INIT_VALUE
#define VPP_FILTER_FACTOR_DEFAULT       NOT_INIT_VALUE

#define MAX_INPUT_STREAMS 64

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
    VAAPI_MEMORY  = 0x03
};

typedef enum
{
    VPP_FILTER_DISABLED           =0,
    VPP_FILTER_ENABLED_DEFAULT    =1,
    VPP_FILTER_ENABLED_CONFIGURED =7

} FilterConfig;

typedef struct
{
    FilterConfig mode;

} sVideoAnalysisParam;

typedef struct
{
    mfxF64   brightness;
    mfxF64   contrast;
    mfxF64   saturation;
    mfxF64   hue;

    FilterConfig mode;

} sProcAmpParam;

typedef struct
{
    mfxU16  factor;

    FilterConfig mode;

} sDenoiseParam;

typedef struct
{
    mfxU16  factor;

    FilterConfig mode;

} sDetailParam;

typedef struct
{
    mfxU8        istabMode;

    FilterConfig mode;

} sIStabParam;

typedef struct
{
    msdk_char             streamName[MSDK_MAX_FILENAME_LEN];
    mfxVPPCompInputStream compStream;
    mfxU32 streamFourcc;
} sCompositionStreamInfo;

typedef struct
{
    sCompositionStreamInfo streamInfo[MAX_INPUT_STREAMS];

    FilterConfig mode;
} sCompositionParam;

typedef struct
{
    mfxU16 algorithm;
    mfxU16 tc_pattern;
    mfxU16 tc_pos;
    FilterConfig mode;

} sDIParam;

typedef struct
{
    mfxU32       algorithm;
    FilterConfig mode;

} sFrameRateConversionParam;

#endif /* __SAMPLE_VPP_CONFIG_H */
/* EOF */
