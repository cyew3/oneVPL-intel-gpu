/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __MFX_PLUGIN_DETECT_PEOPLE_API_H__
#define __MFX_PLUGIN_DETECT_PEOPLE_API_H__

#include "mfxdefs.h"

static const mfxPluginUID g_PeopleDetection_PluginGuid = {0x19,0x40,0xbc,0xe0,0xee,0x64,0x4d,0x09,0x8e,0xd6,0x2f,0x91,0xae,0x4b,0x6b,0x61};

enum {
    VA_EXTBUFF_VPP_PEOPLE_DETECTOR = MFX_MAKEFOURCC('P','P','L','D')
};

enum {
    VA_EXTBUFF_VPP_ROI_ARRAY = MFX_MAKEFOURCC('R','O','I','A')
};

#define MAX_ROI_NUM 256

typedef struct {
    mfxU32  Left;
    mfxU32  Top;
    mfxU32  Right;
    mfxU32  Bottom;
} vaROI;

typedef struct {
    mfxExtBuffer Header;
    mfxU16       NumROI;
    vaROI        ROI[MAX_ROI_NUM];
} vaROIArray;

enum {
    MFX_RENDER_ALWAYS     = 0,
    MFX_RENDER_NEVER      = 1,
    MFX_RENDER_IF_PRESENT = 2
};

typedef struct {
    mfxExtBuffer Header;
    mfxF32       Treshold;
    mfxU8        RenderFlag;
} vaExtVPPDetectPeople;

#endif // __MFX_PLUGIN_DETECT_PEOPLE_API_H__
