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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include "va/va.h"

#if defined(_DEBUG)
    //#define HEVCEHW_VA_TRACE
    //#define HEVCEHW_VA_FAKE_CALL
#endif

namespace HEVCEHW
{
namespace Linux
{
#ifdef HEVCEHW_VA_TRACE
class VAWrapper
{
private:
    FILE* m_log;

public:
    VAWrapper();
    ~VAWrapper();

    int vaMaxNumProfiles(
        VADisplay dpy
    );

    int vaMaxNumEntrypoints(
        VADisplay dpy
    );

    VAStatus vaQueryConfigProfiles(
        VADisplay dpy,
        VAProfile *profile_list,	/* out */
        int *num_profiles		/* out */
    );

    VAStatus vaQueryConfigEntrypoints(
        VADisplay dpy,
        VAProfile profile,
        VAEntrypoint *entrypoint_list,	/* out */
        int *num_entrypoints		/* out */
    );

    VAStatus vaGetConfigAttributes(
        VADisplay dpy,
        VAProfile profile,
        VAEntrypoint entrypoint,
        VAConfigAttrib *attrib_list, /* in/out */
        int num_attribs
    );

    VAStatus vaCreateConfig(
        VADisplay dpy,
        VAProfile profile,
        VAEntrypoint entrypoint,
        VAConfigAttrib *attrib_list,
        int num_attribs,
        VAConfigID *config_id /* out */
    );

    VAStatus vaDestroyConfig(
        VADisplay dpy,
        VAConfigID config_id
    );

    VAStatus vaCreateContext(
        VADisplay dpy,
        VAConfigID config_id,
        int picture_width,
        int picture_height,
        int flag,
        VASurfaceID *render_targets,
        int num_render_targets,
        VAContextID *context		/* out */
    );

    VAStatus vaDestroyContext(
        VADisplay dpy,
        VAContextID context
    );

    VAStatus vaCreateBuffer(
        VADisplay dpy,
        VAContextID context,
        VABufferType type,	/* in */
        unsigned int size,	/* in */
        unsigned int num_elements, /* in */
        void *data,		/* in */
        VABufferID *buf_id	/* out */
    );

    VAStatus vaMapBuffer(
        VADisplay dpy,
        VABufferID buf_id,	/* in */
        void **pbuf 	/* out */
    );

    VAStatus vaUnmapBuffer(
        VADisplay dpy,
        VABufferID buf_id	/* in */
    );

    VAStatus vaDestroyBuffer(
        VADisplay dpy,
        VABufferID buffer_id
    );

    VAStatus vaBeginPicture(
        VADisplay dpy,
        VAContextID context,
        VASurfaceID render_target
    );

    VAStatus vaRenderPicture(
        VADisplay dpy,
        VAContextID context,
        VABufferID *buffers,
        int num_buffers
    );

    VAStatus vaEndPicture(
        VADisplay dpy,
        VAContextID context
    );

    VAStatus vaSyncSurface(
        VADisplay dpy,
        VASurfaceID render_target
    );
};

#else
class VAWrapper
{
public:
    VAWrapper() {};
    ~VAWrapper() {};
};
#endif

} //namespace Linux
} //namespace HEVCEHW
#endif
