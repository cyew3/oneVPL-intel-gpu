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

enum {
    MFX_EXTBUFF_VPP_PEOPLE_DETECTOR = MFX_MAKEFOURCC('P','P','L','D')
};

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
