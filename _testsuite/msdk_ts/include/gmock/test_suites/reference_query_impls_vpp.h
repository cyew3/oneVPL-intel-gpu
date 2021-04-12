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

    struct MemDescVp
    {
        mfxRange32U Width;
        mfxRange32U Height;
        std::set<mfxU32> InFormats;
        std::set<mfxU32> OutFormats;
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
#define END_FILTER                                }}}

#define START_MEMDESC_VP(ResourceType, widthMin, widthMax, widthStep, heightMin, heightMax, heightStep)  \
{ ResourceType, { {widthMin, widthMax, widthStep}, {heightMin, heightMax, heightStep},
#define END_MEMDESC_VP                 }},

#define DEFAULT_IN_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216, MFX_FOURCC_AYUV,\
 MFX_FOURCC_Y410, MFX_FOURCC_Y416, MFX_FOURCC_RGB565, MFX_FOURCC_RGB4}
#define DEFAULT_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216, MFX_FOURCC_AYUV,\
 MFX_FOURCC_Y410, MFX_FOURCC_Y416, MFX_FOURCC_RGB565, MFX_FOURCC_RGB4, MFX_FOURCC_RGBP, MFX_FOURCC_A2RGB10}

#define PROCAMP_IN_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}
#define PROCAMP_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}

#define DETAIL_IN_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}
#define DETAIL_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}

#define DENOISE_IN_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}
#define DENOISE_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}

#define DEINTERLACE_IN_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016}
#define DEINTERLACE_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016}

#define FIELD_IN_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}
#define FIELD_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_P016, MFX_FOURCC_Y210, MFX_FOURCC_Y216,\
 MFX_FOURCC_AYUV,MFX_FOURCC_Y410, MFX_FOURCC_Y416}

#define FRC_IN_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_Y210, MFX_FOURCC_AYUV,MFX_FOURCC_Y410}
#define FRC_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12, MFX_FOURCC_YUY2, MFX_FOURCC_P010, MFX_FOURCC_Y210, MFX_FOURCC_AYUV,MFX_FOURCC_Y410}

#define MCTF_IN_FORMAT_LIST \
{MFX_FOURCC_NV12}
#define MCTF_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12}

#define FIELD_PROCESSING_IN_FORMAT_LIST \
{MFX_FOURCC_NV12}
#define FIELD_PROCESSING_OUT_FORMAT_LIST \
{MFX_FOURCC_NV12}

#if defined(_WIN32) || defined(_WIN64)
#define DEFAULT_FILTER(filterId, MaxDelayInFrames, inFormat, outFormat)                                                               \
START_FILTER(filterId, MaxDelayInFrames)                                                                                              \
    START_MEMDESC_VP(MFX_RESOURCE_SYSTEM_SURFACE, 16, 16384, 1, 16, 16384, 1)                                                         \
        inFormat,                                                                                                                     \
        outFormat                                                                                                                     \
    END_MEMDESC_VP                                                                                                                    \
    START_MEMDESC_VP(MFX_RESOURCE_DX11_TEXTURE, 16, 16384, 1, 16, 16384, 1)                                                           \
        inFormat,                                                                                                                     \
        outFormat                                                                                                                     \
    END_MEMDESC_VP                                                                                                                    \
END_FILTER
#else
#define DEFAULT_FILTER(filterId, MaxDelayInFrames, inFormat, outFormat)                                                               \
START_FILTER(filterId, MaxDelayInFrames)                                                                                              \
    START_MEMDESC_VP(MFX_RESOURCE_SYSTEM_SURFACE, 16, 16384, 1, 16, 16384, 1)                                                         \
        inFormat,                                                                                                                     \
        outFormat                                                                                                                     \
    END_MEMDESC_VP                                                                                                                    \
    START_MEMDESC_VP(MFX_RESOURCE_VA_SURFACE, 16, 16384, 1, 16, 16384, 1)                                                             \
        inFormat,                                                                                                                     \
        outFormat                                                                                                                     \
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
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_SCALING,                0, DEFAULT_IN_FORMAT_LIST,     DEFAULT_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_ROTATION,               0, DEFAULT_IN_FORMAT_LIST,     DEFAULT_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_MIRRORING,              0, DEFAULT_IN_FORMAT_LIST,     DEFAULT_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_COLORFILL,              0, DEFAULT_IN_FORMAT_LIST,     DEFAULT_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_COLOR_CONVERSION,       0, DEFAULT_IN_FORMAT_LIST,     DEFAULT_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_COMPOSITE,              0, DEFAULT_IN_FORMAT_LIST,     DEFAULT_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_PROCAMP,                0, PROCAMP_IN_FORMAT_LIST,     PROCAMP_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_DETAIL,                 0, DETAIL_IN_FORMAT_LIST,      DETAIL_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_DENOISE,                0, DENOISE_IN_FORMAT_LIST,     DENOISE_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_DEINTERLACING,          0, DEINTERLACE_IN_FORMAT_LIST, DEINTERLACE_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_FIELD_WEAVING,          0, FIELD_IN_FORMAT_LIST,       FIELD_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_FIELD_SPLITTING,        0, FIELD_IN_FORMAT_LIST,       FIELD_OUT_FORMAT_LIST),
            DEFAULT_FILTER(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION,  0, FRC_IN_FORMAT_LIST,         FRC_OUT_FORMAT_LIST)
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
    {
    public:
        ReferenceVppATS()
        {
            m_reference.Filters.insert(DEFAULT_FILTER(MFX_EXTBUFF_VPP_MCTF,             0, MCTF_IN_FORMAT_LIST,             MCTF_OUT_FORMAT_LIST));
            m_reference.Filters.insert(DEFAULT_FILTER(MFX_EXTBUFF_VPP_FIELD_PROCESSING, 0, FIELD_PROCESSING_IN_FORMAT_LIST, FIELD_PROCESSING_OUT_FORMAT_LIST));
        }
    };
    static ReferenceVppATS ref_ATS_vpp;

    class ReferenceVppDG2 : public ReferenceVpp
    {
    public:
        ReferenceVppDG2()
        {
        }
    };
    static ReferenceVppDG2 ref_DG2_vpp;
}
