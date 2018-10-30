//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#if !defined (_WIN32) && !defined (_WIN64)

#include "mfx_vp9_encode_hw_ddi.h"

namespace MfxHwVP9Encode
{
#if defined (MFX_VA_LINUX)
#include <va/va.h>
#include <va/va_enc_vp9.h>

#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if (vaBufferId != VA_INVALID_ID)               \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

    enum {
        MFX_FOURCC_VP9_NV12    = MFX_MAKEFOURCC('V','P','8','N'),
        MFX_FOURCC_VP9_SEGMAP  = MFX_MAKEFOURCC('V','P','8','S'),
    };

    typedef struct
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;

    } ExtVASurface;

    /* Convert MediaSDK into DDI */

    void FillSpsBuffer(mfxVideoParam const & par,
        VAEncSequenceParameterBufferVP9 & sps);

    mfxStatus FillPpsBuffer(Task const & task,
        mfxVideoParam const & par,
        VAEncPictureParameterBufferVP9 & pps,
        std::vector<ExtVASurface> const & reconQueue,
        BitOffsets const &offsets);

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
            VP9MfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            VP9MfxVideoParam const & par);

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
            Task const &task,
            mfxHDLPair pair);

        // recomendation from HW
        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT type,
            mfxFrameAllocRequest& request,
            mfxU32 frameWidth,
            mfxU32 frameHeight);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_VP9& caps);

        virtual
        mfxStatus QueryPlatform(
            eMFXHWType& platform);

        virtual
        mfxStatus QueryStatus(
            Task & task);

        virtual
            mfxU32 GetReconSurfFourCC();

        virtual
        mfxStatus Destroy();

    private:
        VAAPIEncoder(const VAAPIEncoder&); // no implementation
        VAAPIEncoder& operator=(const VAAPIEncoder&); // no implementation

        VideoCORE*  m_pmfxCore;
        VP9MfxVideoParam    m_video;

        // encoder specific. can be encapsulated by auxDevice class
        VADisplay    m_vaDisplay;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        // encode params (extended structures)
        VAEncSequenceParameterBufferVP9             m_sps;
        VAEncPictureParameterBufferVP9              m_pps;
        VAEncMiscParameterTypeVP9PerSegmantParam    m_segPar;
        VAEncMiscParameterRateControl               m_vaBrcPar;
        VAEncMiscParameterFrameRate                 m_vaFrameRate;

        VP9SeqLevelParam                            m_seqParam;

        // encode buffer to send vaRender()
        VABufferID m_spsBufferId;
        VABufferID m_ppsBufferId;
        VABufferID m_segMapBufferId;
        VABufferID m_segParBufferId;
        VABufferID m_frameRateBufferId;
        VABufferID m_rateCtrlBufferId;
        VABufferID m_hrdBufferId;
        VABufferID m_qualityLevelBufferId;
        VABufferID m_packedHeaderParameterBufferId;
        VABufferID m_packedHeaderDataBufferId;

        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_reconQueue;
        std::vector<ExtVASurface> m_segMapQueue;
        std::vector<ExtVASurface> m_bsQueue;

        std::vector<mfxU8> m_frameHeaderBuf;

        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 10; // sps, pps, bitstream, uncomp header, segment map, per-segment parameters, frame rate, rate ctrl, hrd, quality level

        mfxU32 m_width;
        mfxU32 m_height;
        bool m_isBrcResetRequired;

        ENCODE_CAPS_VP9 m_caps;
        eMFXHWType m_platform;

        UMC::Mutex                      m_guard;
    };
#endif // (MFX_VA_LINUX)
} // MfxHwVP9Encode

#endif // !(_WIN32) && !(_WIN64)
