/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "reference_query_impls_defs.h"

namespace query_impls_description
{
    struct MemDesc
    {
        mfxRange32U Width;
        mfxRange32U Height;
        std::set<mfxU32> ColorFormats;
    };
    struct ProfileDesc
    {
        std::map<mfxResourceType, MemDesc> memDescs;
    };
    struct CodecDesc
    {
        mfxU16 MaxcodecLevel;
        mfxU16 BiDirectionalPrediction;
        // Profile => ResourceType => MemDesc
        std::map<mfxU32, ProfileDesc> Profiles;
    };
    struct ReferenceCodecImplsDescription
    {
        // CodecID => CodecDesc
        std::map<mfxU32, CodecDesc> Codecs;
    };
    typedef ReferenceCodecImplsDescription RefDesCodec;

#define START_CODEC(codecId, maxLevel, BiDirectionalPrediction)  { codecId, { maxLevel, BiDirectionalPrediction, {
#define END_CODEC                                                }}},

#define START_PROFILE(profile) { profile, {{
#define END_PROFILE            }}},

#define MEMDESC(resType, widthMin, widthMax, widthStep, heightMin, heightMax, heightStep, ...)  \
    {resType, {{widthMin, widthMax, widthStep}, {heightMin, heightMax, heightStep}, {__VA_ARGS__}}},
}
