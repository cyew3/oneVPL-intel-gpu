/******************************************************************************\
Copyright (c) 2016-2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#if defined(LIBVA_SUPPORT)

#include "vaapi_buffer_allocator.h"

vaapiBufferAllocator::vaapiBufferAllocator(VADisplay dpy) :
    m_dpy(dpy),
    m_config(VA_INVALID_ID),
    m_context(VA_INVALID_ID)
{
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribStatisticsIntel;

    VAStatus vaSts = m_libva.vaGetConfigAttributes(m_dpy, VAProfileNone, VAEntrypointStatisticsIntel, &attrib, 1);
    if (VA_STATUS_SUCCESS != vaSts) {
        throw mfxError(MFX_ERR_DEVICE_FAILED, "Failed to get VA config attributes for buffer allocator");
    }

    vaSts = m_libva.vaCreateConfig(m_dpy, VAProfileNone, VAEntrypointStatisticsIntel, &attrib, 1, &m_config);
    if (VA_STATUS_SUCCESS != vaSts) {
        throw mfxError(MFX_ERR_DEVICE_FAILED, "Failed to create VA config for buffer allocator");
    }

    vaSts = m_libva.vaCreateContext(m_dpy, m_config, 1920, 1088, VA_PROGRESSIVE, NULL, 0, &m_context);
    if (VA_STATUS_SUCCESS != vaSts) {
        throw mfxError(MFX_ERR_DEVICE_FAILED, "Failed to create VA context for buffer allocator");
    }
}

vaapiBufferAllocator::~vaapiBufferAllocator()
{
    if (m_context != VA_INVALID_ID) {
        m_libva.vaDestroyContext(m_dpy, m_context);
    }
    if (m_config != VA_INVALID_ID) {
        m_libva.vaDestroyConfig(m_dpy, m_config);
    }
}

#endif // #if defined(LIBVA_SUPPORT)
