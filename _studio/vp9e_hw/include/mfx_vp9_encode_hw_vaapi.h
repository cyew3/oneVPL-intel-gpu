/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#if !defined (_WIN32) && !defined (_WIN64)

#include "mfx_vp9_encode_hw_ddi.h"

namespace MfxHwVP9Encode
{
#if defined (MFX_VA_LINUX)
#include <va/va_enc_vp9.h>

#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if ( vaBufferId != VA_INVALID_ID)              \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

#define D3DDDIFMT_INTELENCODE_BITSTREAMDATA     (D3DDDIFORMAT)164
#define D3DDDIFMT_INTELENCODE_MBDATA            (D3DDDIFORMAT)165
#define D3DDDIFMT_INTELENCODE_SEGMENTMAP        (D3DDDIFORMAT)178
#define D3DDDIFMT_INTELENCODE_COEFFPROB         (D3DDDIFORMAT)179
#define D3DDDIFMT_INTELENCODE_MBCOEFF           (D3DDDIFORMAT)184 // create new data format for VP9 hybrid coefficients

    enum {
        MFX_FOURCC_VP9_NV12    = MFX_MAKEFOURCC('V','P','8','N'),
        MFX_FOURCC_VP9_MBCOEFF = MFX_MAKEFOURCC('V','P','8','M'),
        MFX_FOURCC_VP9_SEGMAP  = MFX_MAKEFOURCC('V','P','8','S'),
    };

    /* Convert MediaSDK into DDI */

    void FillSpsBuffer(mfxVideoParam const & par,
        VAEncSequenceParameterBufferVP9 & sps);

    mfxStatus FillPpsBuffer(Task const &task, mfxVideoParam const & par,
        VAEncPictureParameterBufferVP9 & pps);

    typedef struct
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;

    } ExtVASurface;

    class VAAPIEncoder : public DriverEncoder
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            mfxCoreInterface* core,
            GUID       guid,
            mfxU32     width,
            mfxU32     height);

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            mfxVideoParam const & par);

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
            mfxHDL surface);

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
        mfxStatus QueryStatus(
            Task & task);

        virtual
            mfxU32 GetReconSurfFourCC();

        virtual
        mfxStatus Destroy();

    private:
        VAAPIEncoder(const VAAPIEncoder&); // no implementation
        VAAPIEncoder& operator=(const VAAPIEncoder&); // no implementation

        mfxCoreInterface * m_pmfxCore;
        VP9MfxParam    m_video;

        // encoder specific. can be encapsulated by auxDevice class
        VADisplay    m_vaDisplay;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        // encode params (extended structures)
        VAEncSequenceParameterBufferVP9        m_sps;
        VAEncPictureParameterBufferVP9         m_pps;
        VAEncMbCoeffDataBufferVP9              m_coeffBuf;
        VAEncMiscParameterVP9SegmentMapParams  m_segPar;
        VAEncMiscParameterVP9CpuPakFrameUpdate m_frmUpdate;
        VAEncMiscParameterFrameRate            m_frameRate;
        VAProbabilityDataBufferVP9             m_probUpdate;

        // encode buffer to send vaRender()
        VABufferID m_spsBufferId;
        VABufferID m_ppsBufferId;
        VABufferID m_coeffBufBufferId;
        VABufferID m_segMapBufferId;
        VABufferID m_segParBufferId;
        VABufferID m_frmUpdateBufferId;
        VABufferID m_frameRateBufferId;
        VABufferID m_rateCtrlBufferId;
        VABufferID m_hrdBufferId;
        VABufferID m_probUpdateBufferId;
        VABufferID m_qualityLevelBufferId;

        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_mbDataQueue;
        std::vector<ExtVASurface> m_mbCoeffQueue;
        std::vector<ExtVASurface> m_reconQueue;
        std::vector<ExtVASurface> m_segMapQueue;

        std::vector<mfxU8> m_seg_id;

        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 11; // sps, pps, coeff buffer, seg_map, per segment par, frame update data, frame rate, rate ctrl, hrd, probability update, quality level

        mfxU32 m_width;
        mfxU32 m_height;
        ENCODE_CAPS_VP9 m_caps;
        UMC::Mutex                      m_guard;
        VAEncMiscParameterRateControl m_vaBrcPar;
        VAEncMiscParameterFrameRate   m_vaFrameRate;
        bool                          m_isBrcResetRequired;
    };
#endif // (MFX_VA_LINUX)
} // MfxHwVP9Encode

#endif // !(_WIN32) && !(_WIN64)
