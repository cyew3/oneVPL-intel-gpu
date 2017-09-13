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

#ifndef __VAAPI_BUFFER_ALLOCATOR_H__
#define __VAAPI_BUFFER_ALLOCATOR_H__

#include "mfxfeihevc.h"

#if defined(LIBVA_SUPPORT)

#include <va/va.h>
#include "vaapi_utils.h"

// class for buffers allocation via direct access to vaapi buffers
class vaapiBufferAllocator
{
public:
    vaapiBufferAllocator(VADisplay dpy);
    ~vaapiBufferAllocator();

    // T is external buffer structure
    template<typename T>
    void Alloc(T * buffer, mfxU32 num_elem)
    {
        if (!buffer) throw mfxError(MFX_ERR_NULL_PTR);
        if (buffer->Data != NULL) throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "Buffer is already allocated and using in app");

        mfxU32 size = sizeof(*(T::Data));

        VABufferType type = GetVABufferType(buffer->Header.BufferId);

        VAStatus sts = m_libva.vaCreateBuffer(m_dpy, m_context, type, size, num_elem, NULL, &buffer->VaBufferID);
        if (sts != VA_STATUS_SUCCESS) {
            buffer->VaBufferID = VA_INVALID_ID;
            throw mfxError(MFX_ERR_MEMORY_ALLOC, "vaCreateBuffer failed");
        }

        buffer->DataSize = size * num_elem;
        buffer->Data = NULL;
    };

    template<typename T>
    void Lock(T * buffer)
    {
        if (!buffer) throw mfxError(MFX_ERR_NULL_PTR);

        if (VA_INVALID_ID == buffer->VaBufferID)
          throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "Invalid VaBufferID. Lock with unallocated buffer");

        VAStatus sts = m_libva.vaMapBuffer(m_dpy, buffer->VaBufferID, (void**)&buffer->Data);
        if (sts != VA_STATUS_SUCCESS) {
            throw mfxError(MFX_ERR_DEVICE_FAILED, "vaMapBuffer failed");
        }
        if (!buffer->Data) throw mfxError(MFX_ERR_NULL_PTR, "No pointer to data");
    }

    template<typename T>
    void Unlock(T * buffer)
    {
        if (!buffer) throw mfxError(MFX_ERR_NULL_PTR);

        if (VA_INVALID_ID == buffer->VaBufferID)
          throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "Invalid VaBufferID. Unlock with unallocated buffer");

        VAStatus sts = m_libva.vaUnmapBuffer(m_dpy, buffer->VaBufferID);
        if (sts != VA_STATUS_SUCCESS) {
            throw mfxError(MFX_ERR_DEVICE_FAILED, "vaUnmapBuffer failed");
        }
        buffer->Data = NULL;
    };

    template<typename T>
    void Free(T * buffer)
    {
        if (!buffer) throw mfxError(MFX_ERR_NULL_PTR);
        if (VA_INVALID_ID == buffer->VaBufferID) return;
        if (buffer->Data != NULL) Unlock(buffer);

        VAStatus sts = m_libva.vaDestroyBuffer(m_dpy, buffer->VaBufferID);
        if (sts != VA_STATUS_SUCCESS) {
            throw mfxError(MFX_ERR_DEVICE_FAILED, "vaDestroyBuffer failed");
        }
        buffer->VaBufferID = VA_INVALID_ID;
        buffer->Data = NULL;
    };

protected:
    VABufferType GetVABufferType(mfxU32 bufferId)
    {
        switch (bufferId)
        {
        case MFX_EXTBUFF_HEVCFEI_ENC_QP:
            return VAEncQpBufferType;
        case MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED:
            return VAEncFEIMVPredictorBufferTypeIntel;
        default:
            throw mfxError(MFX_ERR_UNSUPPORTED, "Unsupported buffer type");
        }
    };

    MfxLoader::VA_Proxy m_libva;
    VADisplay m_dpy;
    VAConfigID m_config;
    VAContextID m_context;
};

#endif //#if defined(LIBVA_SUPPORT)

// T is external buffer structure
template<typename T>
class AutoBufferLocker
{
public:
    AutoBufferLocker(vaapiBufferAllocator & allocator, T & buffer)
        : m_allocator(allocator)
        , m_buffer(buffer)
    {
        m_allocator.Lock(&m_buffer);
    }

    ~AutoBufferLocker()
    {
        m_allocator.Unlock(&m_buffer);
    }

protected:
    vaapiBufferAllocator & m_allocator;
    T & m_buffer;

private:
    AutoBufferLocker(AutoBufferLocker const &);
    AutoBufferLocker & operator =(AutoBufferLocker const &);
};

#endif // __VAAPI_BUFFER_ALLOCATOR_H__
