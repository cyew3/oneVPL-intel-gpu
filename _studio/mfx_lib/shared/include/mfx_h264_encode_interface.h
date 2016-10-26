//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_VA)

#ifndef __MFX_H264_ENCODE_INTERFACE__H
#define __MFX_H264_ENCODE_INTERFACE__H

#include <vector>
#include <assert.h>
#include "mfxdefs.h"
#include "mfxvideo++int.h"

#include "mfx_h264_enc_common_hw.h"

// AYA: temporal solution. structures will be abstracted (ENCODE_CAPS, D3DDDIFORMAT)
#if   defined(MFX_VA_WIN)
    #include "encoding_ddi.h"
#elif defined(MFX_VA_LINUX)
    #include "mfx_h264_encode_struct_vaapi.h"
#endif

namespace MfxHwH264Encode
{
    class PreAllocatedVector
    {
    public:
        PreAllocatedVector()
            : m_size(0)
        {
        }

        void Alloc(mfxU32 capacity)
        {
            m_storage.resize(capacity);
            m_size = 0;
        }

        void SetSize(mfxU32 size)
        {
            assert(size <= m_storage.size());
            m_size = size;
        }

        mfxU32 Size() const
        {
            return m_size;
        }

        mfxU32 Capacity() const
        {
            return (mfxU32)m_storage.size();
        }

        mfxU8 * Buffer()
        {
            assert(m_storage.size() > 0);
            return &m_storage[0];
        }

        mfxU8 const * Buffer() const
        {
            assert(m_storage.size() > 0);
            return &m_storage[0];
        }

    private:
        std::vector<mfxU8>  m_storage;
        mfxU32              m_size;
    };


    class DriverEncoder
    {
    public:

        virtual ~DriverEncoder(){}

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            GUID        guid,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false) = 0;

        virtual
        mfxStatus CreateAccelerationService(
            MfxVideoParam const & par) = 0;

        virtual
        mfxStatus Reset(
            MfxVideoParam const & par) = 0;

        virtual 
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type) = 0;

        virtual 
        mfxStatus Execute(
            mfxHDL                     surface,
            DdiTask const &            task,
            mfxU32                     fieldId,
            PreAllocatedVector const & sei) = 0;

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request) = 0;

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS & caps) = 0;

        virtual
        mfxStatus QueryMbPerSec(
            mfxVideoParam const & par,
            mfxU32              (&mbPerSec)[16]) = 0;

        virtual
        mfxStatus QueryInputTilingSupport(
            mfxVideoParam const & par,
            mfxU32               &inputTiling) { par; inputTiling = 0; return MFX_ERR_NONE; };

        virtual
        mfxStatus QueryStatus(
            DdiTask & task,
            mfxU32    fieldId) = 0;

        virtual
        mfxStatus Destroy() = 0;

        virtual
        void ForceCodingFunction (mfxU16 codingFunction) = 0;

        virtual
        mfxStatus QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal) = 0;

        virtual
        mfxStatus QueryEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS & caps) { caps; return MFX_ERR_UNSUPPORTED; };

        virtual
        mfxStatus GetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS & caps) { caps; return MFX_ERR_UNSUPPORTED; };

        virtual
        mfxStatus SetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS const & caps) { caps; return MFX_ERR_UNSUPPORTED; };
            
    };

    DriverEncoder* CreatePlatformH264Encoder( VideoCORE* core ); 
    DriverEncoder* CreatePlatformSvcEncoder( VideoCORE* core );

}; // namespace

#endif // __MFX_H264_ENCODE_INTERFACE__H
#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_VA_WIN)
