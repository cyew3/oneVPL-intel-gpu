//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#include "umc_profile_level.h"

namespace UMC
{

///#define vm_trace_i(_x) printf(__FILE__ ": %d: " #_x " = %d = 0x%x", __LINE__, _x, _x)

typedef struct _ProfileSpec
{
    Ipp32s type;
    Ipp32s profile;
    Ipp32s max_level;
    bool   bReverse;
} ProfileSpec;

const ProfileSpec g_ProfileSpec[] =
{
    // Mpeg2
    { MPEG2_VIDEO, MPEG2_PROFILE_SIMPLE, MPEG2_LEVEL_MAIN, true },
    { MPEG2_VIDEO, MPEG2_PROFILE_MAIN,   MPEG2_LEVEL_HIGH, true },
    { MPEG2_VIDEO, MPEG2_PROFILE_HIGH,   MPEG2_LEVEL_HIGH, true },
    // Mpeg4
    { MPEG4_VIDEO, MPEG4_PROFILE_SIMPLE,          MPEG4_LEVEL_3, false },
    { MPEG4_VIDEO, MPEG4_PROFILE_ADVANCED_SIMPLE, MPEG4_LEVEL_5, false },
    // H264
    { H264_VIDEO, H264_PROFILE_BASELINE, H264_LEVEL_3,  false },
    { H264_VIDEO, H264_PROFILE_MAIN,     H264_LEVEL_41, false },
    { H264_VIDEO, H264_PROFILE_HIGH,     H264_LEVEL_41, false },
    // VC1 & WMV
    { VC1_VIDEO, VC1_PROFILE_ADVANCED, VC1_LEVEL_3,      false },
    { VC1_VIDEO, VC1_PROFILE_SIMPLE,   VC1_LEVEL_MEDIAN, false },
    { VC1_VIDEO, VC1_PROFILE_MAIN,     VC1_LEVEL_HIGH,   false },
    { WMV_VIDEO, VC1_PROFILE_ADVANCED, VC1_LEVEL_3,      false },
    { WMV_VIDEO, VC1_PROFILE_SIMPLE,   VC1_LEVEL_MEDIAN, false },
    { WMV_VIDEO, VC1_PROFILE_MAIN,     VC1_LEVEL_HIGH,   false },
    { 0, 0, 0, false }
};

bool CheckProfileLevelMenlow(UMC::VideoDecoderParams *pParams)
{
    bool bSupported = false;
    Ipp32s type = 0, stype = 0, profile = 0, level = 0, i = 0;

    if (NULL == pParams) bSupported = true;
    else
    {
        type    = pParams->info.stream_type;
        stype   = pParams->info.stream_subtype;
        profile = pParams->profile;
        level   = pParams->level;

        vm_trace_i(type);
        vm_trace_i(stype);
        vm_trace_i(profile);
        vm_trace_i(level);

        // if profile or level is not set we will try to play
        if (!profile && !level)
            bSupported = true;
        else
        {
            for (i = 0; g_ProfileSpec[i].type != 0; ++i)
            {
                if (g_ProfileSpec[i].type == type)
                {
                    if (g_ProfileSpec[i].profile == profile)
                    {
                        if (!g_ProfileSpec[i].bReverse && (level <= g_ProfileSpec[i].max_level))
                            bSupported = true;
                        if (g_ProfileSpec[i].bReverse && (level >= g_ProfileSpec[i].max_level))
                            bSupported = true;
                        break;
                    }
                }
            }
        }
    }
    return bSupported;
}

}; // namespace UMC
