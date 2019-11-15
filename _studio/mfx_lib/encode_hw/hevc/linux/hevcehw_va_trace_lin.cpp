// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_va_trace_lin.h"

#ifdef HEVCEHW_VA_TRACE

#include "hevcehw_base.h"
#include <thread>
#include <sstream>

using namespace HEVCEHW::Linux;

VAWrapper::VAWrapper()
    : m_log(nullptr)
{
    m_log = fopen("MSDK_HEVCE_HW_VA_LOG.txt", "w");
}

VAWrapper::~VAWrapper()
{
    if (m_log)
        fclose(m_log);
}

static const VAProfile ProfilesSupported[] =
{
    VAProfileHEVCMain
    , VAProfileHEVCMain10
    , VAProfileHEVCMain
    , VAProfileHEVCMain10
    , VAProfileHEVCMain422_10
    , VAProfileHEVCMain422_10
    , VAProfileHEVCMain444
    , VAProfileHEVCMain444_10
    , VAProfileHEVCMain422_10
    , VAProfileHEVCMain422_10
    , VAProfileHEVCMain444
    , VAProfileHEVCMain444_10
};

static const VAEntrypoint EPSupported[] =
{
    VAEntrypointEncSlice
    , VAEntrypointEncSliceLP
};

int VAWrapper::vaMaxNumProfiles(
    VADisplay dpy
)
{
    std::stringstream ss;

    ss << "TH #"
        << std::this_thread::get_id()
        << ": vaMaxNumProfiles(dpy = "
        << dpy
        << ") = ";

#ifdef HEVCEHW_VA_FAKE_CALL
    int res = (int)Size(ProfilesSupported);
    dpy;
#else
    auto res = vaMaxNumProfiles(dpy);
#endif

    ss << res << "\n";

    fprintf(m_log, "%s\n", ss.str().c_str());
    fflush(m_log);

    return res;
}

int VAWrapper::vaMaxNumEntrypoints(
    VADisplay dpy
)
{
    std::stringstream ss;

    ss << "TH #"
        << std::this_thread::get_id()
        << ": vaMaxNumEntrypoints(dpy) = ";

#ifdef HEVCEHW_VA_FAKE_CALL
    int res = (int)Size(EPSupported);
    dpy;
#else
    auto res = vaMaxNumEntrypoints(dpy);
#endif

    ss << res << "\n";

    fprintf(m_log, "%s\n", ss.str().c_str());
    fflush(m_log);

    return res;
}

VAStatus VAWrapper::vaQueryConfigProfiles(
    VADisplay dpy,
    VAProfile * profile_list,	/* out */
    int * num_profiles		/* out */
)
{
    VAStatus sts = VA_STATUS_SUCCESS;
    std::stringstream ss;

    ss << "TH #"
        << std::this_thread::get_id()
        << ": vaQueryConfigProfiles(dpy, profile_list, num_profiles) = ";

#ifdef HEVCEHW_VA_FAKE_CALL
    std::copy(
        ProfilesSupported
        , ProfilesSupported + Size(ProfilesSupported)
        , profile_list);
    *num_profiles = (int)Size(ProfilesSupported);
    dpy, profile_list, num_profiles;
#else
    sts = vaQueryConfigProfiles(dpy, profile_list, num_profiles);
#endif

    ss << sts << "\n";

    if (sts == VA_STATUS_SUCCESS)
    {
        ss << "profile_list = {";

        std::for_each(
            profile_list
            , profile_list + *num_profiles
            , [&ss](VAProfile pf)
        {
            ss << pf << ", ";
        });

        ss << "}\n";
    }

    fprintf(m_log, "%s\n", ss.str().c_str());
    fflush(m_log);

    return sts;
}

VAStatus VAWrapper::vaQueryConfigEntrypoints(
    VADisplay dpy,
    VAProfile profile,
    VAEntrypoint * entrypoint_list,	/* out */
    int * num_entrypoints		/* out */
)
{
    VAStatus sts = VA_STATUS_SUCCESS;
    std::stringstream ss;

    ss << "TH #"
        << std::this_thread::get_id()
        << ": vaQueryConfigEntrypoints(dpy, profile = "
        << profile
        << ", entrypoint_list, num_entrypoints) = ";

#ifdef HEVCEHW_VA_FAKE_CALL
    std::copy(
        EPSupported
        , EPSupported + Size(EPSupported)
        , entrypoint_list);
    *num_entrypoints = (int)Size(EPSupported);
    dpy, profile, entrypoint_list, num_entrypoints;
#else
    sts = vaQueryConfigEntrypoints(dpy, profile, entrypoint_list, num_entrypoints);
#endif

    ss << sts << "\n";

    if (sts == VA_STATUS_SUCCESS)
    {
        ss << "entrypoint_list = {";

        std::for_each(
            entrypoint_list
            , entrypoint_list + *num_entrypoints
            , [&ss](VAEntrypoint ep)
        {
            ss << ep << ", ";
        });

        ss << "}\n";
    }

    fprintf(m_log, "%s\n", ss.str().c_str());
    fflush(m_log);

    return sts;
}

VAStatus VAWrapper::vaGetConfigAttributes(
    VADisplay dpy,
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib * attrib_list, /* in/out */
    int num_attribs
)
{
    VAStatus sts = VA_STATUS_SUCCESS;
    std::stringstream ss;

    ss << "TH #"
        << std::this_thread::get_id()
        << ": vaGetConfigAttributes(dpy, profile = "
        << profile
        << ", entrypoint = "
        << entrypoint
        << "attrib_list, num_attribs = "
        << num_attribs
        <<  ") = ";

#ifdef HEVCEHW_VA_FAKE_CALL
    dpy;
    static const std::map<VAConfigAttribType, uint32_t> attr =
    {
          { VAConfigAttribRTFormat
            , VA_RT_FORMAT_YUV420
            | VA_RT_FORMAT_YUV422
            | VA_RT_FORMAT_YUV444
            | VA_RT_FORMAT_YUV411
            | VA_RT_FORMAT_YUV400
            | VA_RT_FORMAT_YUV420_10
            | VA_RT_FORMAT_YUV422_10
            | VA_RT_FORMAT_YUV444_10
            | VA_RT_FORMAT_YUV420_12
            | VA_RT_FORMAT_YUV422_12
            | VA_RT_FORMAT_YUV444_12
            | VA_RT_FORMAT_RGB16
            | VA_RT_FORMAT_RGB32
            | VA_RT_FORMAT_RGBP
            | VA_RT_FORMAT_RGB32_10 }
        , { VAConfigAttribRateControl
            , VA_RC_CQP
            |VA_RC_CBR
            |VA_RC_VBR
            |VA_RC_VCM
            |VA_RC_VBR_CONSTRAINED
            |VA_RC_ICQ
            |VA_RC_MB
            |VA_RC_PARALLEL
            |VA_RC_QVBR }
        , { VAConfigAttribEncQuantization
            , VA_ATTRIB_NOT_SUPPORTED}
        , { VAConfigAttribEncIntraRefresh
            , VA_ENC_INTRA_REFRESH_ROLLING_COLUMN
            | VA_ENC_INTRA_REFRESH_ROLLING_ROW
            | VA_ENC_INTRA_REFRESH_ADAPTIVE
            | VA_ENC_INTRA_REFRESH_CYCLIC
            | VA_ENC_INTRA_REFRESH_P_FRAME
            | VA_ENC_INTRA_REFRESH_B_FRAME
            | VA_ENC_INTRA_REFRESH_MULTI_REF }
        , { VAConfigAttribMaxPictureHeight      , 1088}
        , { VAConfigAttribMaxPictureWidth       , 1920}
        , { VAConfigAttribEncParallelRateControl, VA_ATTRIB_NOT_SUPPORTED}
        , { VAConfigAttribEncMaxRefFrames       , VA_ATTRIB_NOT_SUPPORTED}
        , { VAConfigAttribEncSliceStructure     , VA_ATTRIB_NOT_SUPPORTED}
        , { VAConfigAttribEncROI                , ~VA_ATTRIB_NOT_SUPPORTED}
    };

    std::for_each(attrib_list, attrib_list + num_attribs
        , [&](VAConfigAttrib& a)
    {
        a.value = attr.count(a.type) ? attr.at(a.type) : VA_ATTRIB_NOT_SUPPORTED;
    });

#else
    sts = vaGetConfigAttributes(dpy, profile, entrypoint, attrib_list, num_attribs);
#endif

    ss << sts << "\n";

    if (sts == VA_STATUS_SUCCESS)
    {
        ss << "entrypoint_list = {";

        std::for_each(
            attrib_list
            , attrib_list + num_attribs
            , [&ss](VAConfigAttrib atr)
        {
            ss << "{"
                << atr.type
                << ", "
                << atr.value
                << "}, ";
        });

        ss << "}\n";
    }

    fprintf(m_log, "%s\n", ss.str().c_str());
    fflush(m_log);

    return sts;
}

VAStatus VAWrapper::vaCreateConfig(
    VADisplay /*dpy*/,
    VAProfile /*profile*/,
    VAEntrypoint /*entrypoint*/,
    VAConfigAttrib * /*attrib_list*/,
    int /*num_attribs*/,
    VAConfigID * /*config_id*/ /* out */
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaDestroyConfig(
    VADisplay /*dpy*/,
    VAConfigID /*config_id*/
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaCreateContext(
    VADisplay /*dpy*/,
    VAConfigID /*config_id*/,
    int /*picture_width*/,
    int /*picture_height*/,
    int /*flag*/,
    VASurfaceID * /*render_targets*/,
    int /*num_render_targets*/,
    VAContextID * /*context*/		/* out */
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaDestroyContext(
    VADisplay /*dpy*/,
    VAContextID /*context*/
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaCreateBuffer(
    VADisplay /*dpy*/,
    VAContextID /*context*/,
    VABufferType /*type*/,	/* in */
    unsigned int /*size*/,	/* in */
    unsigned int /*num_elements*/, /* in */
    void * /*data*/,		/* in */
    VABufferID * /*buf_id*/	/* out */
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaMapBuffer(
    VADisplay /*dpy*/,
    VABufferID /*buf_id*/,	/* in */
    void ** /*pbuf*/ 	/* out */
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaUnmapBuffer(
    VADisplay /*dpy*/,
    VABufferID /*buf_id*/	/* in */
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaDestroyBuffer(
    VADisplay /*dpy*/,
    VABufferID /*buffer_id*/
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaBeginPicture(
    VADisplay /*dpy*/,
    VAContextID /*context*/,
    VASurfaceID /*render_target*/
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaRenderPicture(
    VADisplay /*dpy*/,
    VAContextID /*context*/,
    VABufferID * /*buffers*/,
    int /*num_buffers*/
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaEndPicture(
    VADisplay /*dpy*/,
    VAContextID /*context*/
)
{
    return VA_STATUS_SUCCESS; //TBD
}

VAStatus VAWrapper::vaSyncSurface(
    VADisplay /*dpy*/,
    VASurfaceID /*render_target*/
)
{
    return VA_STATUS_SUCCESS; //TBD
}

#endif //VA_TRACE

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)