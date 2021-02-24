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
    // TODO: Remove and use proper define
    enum
    {
        MFX_EXTBUFF_VPP_FIELD_WEAVING = MFX_MAKEFOURCC('F', 'I', 'W', 'F'),
        MFX_EXTBUFF_VPP_FIELD_SPLITTING = MFX_MAKEFOURCC('F', 'I', 'S', 'F'),
    };

    struct FormatDesc
    {
        std::set<mfxU32> OutFormats;
    };
    struct MemDescVp
    {
        mfxRange32U Width;
        mfxRange32U Height;
        // InFormat => FormatDesc
        std::map<mfxU32, FormatDesc> FormatDescs;
    };
    struct FilterDesc
    {
        mfxU16 MaxDelayInFrames;
        // ResourceType => MemDescVp
        std::map<mfxResourceType, MemDescVp> MemDescVps;
    };
    struct ReferenceVppImplsDescription
    {
        // FilterID => FilterDesc
        std::map<mfxU32, FilterDesc> Filters;
    };
    typedef ReferenceVppImplsDescription RefDesVpp;
#define START_VPP_DESCRIPTION(name) static RefDesVpp name = {{
#define END_VPP_DESCRIPTION         }};

#define START_FILTER(FilterID, MaxDelayInFrames)  { FilterID, { MaxDelayInFrames, {
#define END_FILTER                                }}},

#define START_MEMDESC_VP(ResourceType, widthMin, widthMax, widthStep, heightMin, heightMax, heightStep)  \
{ ResourceType, { {widthMin, widthMax, widthStep}, {heightMin, heightMax, heightStep}, {
#define END_MEMDESC_VP                 }}},

#define FORMAT_DESC(InFormat, ...)  {InFormat, {{__VA_ARGS__}}},

//TODO: update after double check width and height for the reference (Win : 16352,  Lin: 16384)
#if defined(_WIN32) || defined(_WIN64)   
#define DEFAULT_FILTER(filterId, MaxDelayInFrames)                                                                                    \
START_FILTER(filterId, MaxDelayInFrames)                                                                                              \
    START_MEMDESC_VP(MFX_RESOURCE_SYSTEM_SURFACE, 16, 16352, 16, 16, 16352, 16)                                                       \
        FORMAT_DESC(MFX_FOURCC_NV12,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_YUY2,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_RGB4,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_RGB565,                                                                                                \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_RGBP,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_P010,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_A2RGB10,                                                                                               \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_AYUV,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y210,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y410,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_P016,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y216,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y416,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_BGR4,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_ARGB16,                                                                                                \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_R16,                                                                                                   \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
    END_MEMDESC_VP                                                                                                                    \
    START_MEMDESC_VP(MFX_RESOURCE_DX11_TEXTURE, 16, 16352, 16, 16, 16352, 16)                                                         \
        FORMAT_DESC(MFX_FOURCC_NV12,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_YUY2,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_RGB4,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_RGB565,                                                                                                \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_RGBP,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_P010,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_A2RGB10,                                                                                               \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_AYUV,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y210,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y410,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_P016,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y216,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_Y416,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_BGR4,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_ARGB16,                                                                                                \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
        FORMAT_DESC(MFX_FOURCC_R16,                                                                                                   \
            MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, MFX_FOURCC_AYUV, \
            MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416, MFX_FOURCC_BGR4, MFX_FOURCC_ARGB16)  \
    END_MEMDESC_VP                                                                                                                    \
END_FILTER
#else
#define DEFAULT_FILTER(filterId, MaxDelayInFrames)                                                                                    \
START_FILTER(filterId, MaxDelayInFrames)                                                                                              \
    START_MEMDESC_VP(MFX_RESOURCE_SYSTEM_SURFACE, 16, 16384, 16, 16, 16384, 16)                                                       \
        FORMAT_DESC(MFX_FOURCC_NV12,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10,     \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                         \
        FORMAT_DESC(MFX_FOURCC_YV12,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_YUY2,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_RGB4,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_RGB565,                                                                                                \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_RGBP,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_P010,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_A2RGB10,                                                                                               \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_AYUV,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_UYVY,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y210,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y410,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_P016,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y216,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y416,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        END_MEMDESC_VP                                                                                                                \
        START_MEMDESC_VP(MFX_RESOURCE_VA_SURFACE, 16, 16384, 16, 16, 16384, 16)                                                       \
        FORMAT_DESC(MFX_FOURCC_NV12,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_YV12,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_YUY2,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_RGB4,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_RGB565,                                                                                                \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_RGBP,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_P010,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_A2RGB10,                                                                                               \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_AYUV,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_UYVY,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y210,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y410,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_P016,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y216,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
        FORMAT_DESC(MFX_FOURCC_Y416,                                                                                                  \
            MFX_FOURCC_NV12, MFX_FOURCC_YV12, MFX_FOURCC_YUY2, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_P010, MFX_FOURCC_A2RGB10, \
            MFX_FOURCC_AYUV, MFX_FOURCC_Y210, MFX_FOURCC_Y410, MFX_FOURCC_P016, MFX_FOURCC_Y216, MFX_FOURCC_Y416)                     \
    END_MEMDESC_VP                                                                                                                    \
END_FILTER
#endif

    class ReferenceVpp
    {
    public:
        RefDesVpp& GetReference()
        {
            return m_reference;
        }
    protected:
        RefDesVpp m_reference = {{
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_PROCAMP, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_MCTF, 1)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_DENOISE, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_DETAIL, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_ROTATION, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_MIRRORING, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_SCALING, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_COLOR_CONVERSION, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_FIELD_PROCESSING, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_FIELD_WEAVING, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_FIELD_SPLITTING, 0)
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_COMPOSITE, 0)
        }};
    };
    class ReferenceVppDefault : public ReferenceVpp
    {
    public:
        ReferenceVppDefault()
        {
            m_reference.Filters.clear();
        }
    };
    static ReferenceVppDefault ref_default_vpp;

    class ReferenceVppATS : public ReferenceVpp
    {};
    static ReferenceVppATS ref_ATS_vpp;

    class ReferenceVppDG2 : public ReferenceVpp
    {
    public:
        ReferenceVppDG2()
        {
            // No MCTF starting from DG2
            m_reference.Filters.erase(MFX_EXTBUFF_VPP_MCTF);
        }
    };
    static ReferenceVppDG2 ref_DG2_vpp;
}
