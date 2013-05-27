/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_H264_ENCODE_VAAPI__H
#define __MFX_H264_ENCODE_VAAPI__H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_VA_LINUX)

#include "umc_mutex.h"

#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_h264.h>

#include "mfx_h264_encode_interface.h"

namespace MfxHwH264Encode
{
    class VAAPIEncoder : public DriverEncoder
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE* core,
            GUID       guid,
            mfxU32     width,
            mfxU32     height);

        virtual
        mfxStatus CreateAccelerationService(
            MfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            MfxVideoParam const & par);

        // empty  for Lin
        virtual
        mfxStatus Register(
            mfxMemId memId,
            D3DDDIFORMAT type);

        // 2 -> 1
        virtual
        mfxStatus Register(
            mfxFrameAllocResponse& response,
            D3DDDIFORMAT type);

        // (mfxExecuteBuffers& data)
        virtual
        mfxStatus Execute(
            mfxHDL          surface,
            DdiTask const & task,
            mfxU32          fieldId,
            PreAllocatedVector const & sei);

        // recomendation from HW
        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT type,
            mfxFrameAllocRequest& request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS& caps);

        virtual
        mfxStatus QueryStatus(
            DdiTask & task,
            mfxU32    fieldId);

        virtual
        mfxStatus Destroy();

    private:
        VAAPIEncoder(const VAAPIEncoder&); // no implementation
        VAAPIEncoder& operator=(const VAAPIEncoder&); // no implementation

        VideoCORE*    m_core;
        MfxVideoParam m_videoParam;

        // encoder specific. can be encapsulated by auxDevice class
        VADisplay    m_vaDisplay;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        // encode params (extended structures)
#if defined(MFX_VA_ANDROID) && (MFX_ANDROID_VERSION == MFX_HONEYCOMB_VPG)
        VAEncSequenceParameterBufferH264Ext m_sps;
        VAEncPictureParameterBufferH264Ext  m_pps;
        std::vector<VAEncSliceParameterBufferH264Ext> m_slice;
#else
        VAEncSequenceParameterBufferH264 m_sps;
        VAEncPictureParameterBufferH264  m_pps;
        std::vector<VAEncSliceParameterBufferH264> m_slice;
#endif

        // encode buffer to send vaRender()
        VABufferID m_spsBufferId;
        VABufferID m_hrdBufferId;
        VABufferID m_rateParamBufferId; // VAEncMiscParameterRateControl
        VABufferID m_frameRateId; // VAEncMiscParameterFrameRate
        VABufferID m_ppsBufferId;
        std::vector<VABufferID> m_sliceBufferId;


        VABufferID m_packedSpsHeaderBufferId;
        VABufferID m_packedSpsBufferId;
        VABufferID m_packedPpsHeaderBufferId;
        VABufferID m_packedPpsBufferId;
        VABufferID m_packedSeiHeaderBufferId;
        VABufferID m_packedSeiBufferId;

        // map feedbackNumber <-> VASurface
        typedef struct
        {
            VASurfaceID surface;
            mfxU32 number;
            mfxU32 idxBs;

        } ExtVASurface;


        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_bsQueue;
        std::vector<ExtVASurface> m_reconQueue;

        // workarround
        mfxU32 m_width;
        mfxU32 m_height;

        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 10;

        static const mfxU32 MAX_PACKED_SPSPPS_SIZE = 1024;
        mfxU8 m_packedSps[MAX_PACKED_SPSPPS_SIZE];
        mfxU8 m_packedPps[MAX_PACKED_SPSPPS_SIZE];
        
        UMC::Mutex m_guard;

    };

}; // namespace

#endif // MFX_ENABLE_H264_VIDEO_ENCODE && (MFX_VA_LINUX)
#endif // __MFX_H264_ENCODE_VAAPI__H
/* EOF */
