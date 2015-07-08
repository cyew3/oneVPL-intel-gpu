/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_H265_ENCODE_VAAPI__H
#define __MFX_H265_ENCODE_VAAPI__H

#include "mfx_common.h"

#if defined (MFX_VA_LINUX)

#include "umc_mutex.h"

#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_hevc.h>
#include "vaapi_ext_interface.h"

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_ddi.h"
#include "mfx_h265_encode_hw_ddi_trace.h"

#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if ( vaBufferId != VA_INVALID_ID)              \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

#define VAConfigAttribInputTiling  -1  // Inform the app what kind of tiling format supported by driver

namespace MfxHwH265Encode
{

mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl);

VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type);

mfxStatus SetHRD(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & hrdBuf_id);

mfxStatus SetPrivateParams(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & privateParams_id,
    mfxEncodeCtrl const * pCtrl = 0);

void FillConstPartOfPps(
    MfxHwH265Encode::MfxVideoParam const & par,
    VAEncPictureParameterBufferHEVC & pps);

mfxStatus SetFrameRate(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameRateBuf_id);

    typedef struct
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;
        mfxU32 size;
    } ExtVASurface;

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

        virtual
        mfxStatus Execute(
            Task            const & task,
            mfxHDL          surface);

        // recomendation from HW
        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT type,
            mfxFrameAllocRequest& request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS& caps);

        virtual
        mfxStatus QueryStatus(Task & task);

        virtual
        mfxStatus Destroy();

        virtual
        mfxStatus QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal);

    protected:
        VAAPIEncoder(const VAAPIEncoder&); // no implementation
        VAAPIEncoder& operator=(const VAAPIEncoder&); // no implementation

        void FillSps(MfxVideoParam const & par, VAEncSequenceParameterBufferHEVC & sps);

        VideoCORE*    m_core;
        MfxVideoParam m_videoParam;

        VADisplay    m_vaDisplay;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        // encode params (extended structures)
        VAEncSequenceParameterBufferHEVC m_sps;
        VAEncPictureParameterBufferHEVC  m_pps;
        std::vector<VAEncSliceParameterBufferHEVC> m_slice;

        // encode buffer to send vaRender()
        VABufferID m_spsBufferId;
        VABufferID m_hrdBufferId;
        VABufferID m_rateParamBufferId; // VAEncMiscParameterRateControl
        VABufferID m_frameRateId; // VAEncMiscParameterFrameRate
        VABufferID m_maxFrameSizeId; // VAEncMiscParameterFrameRate
        VABufferID m_ppsBufferId;
        VABufferID m_mbqpBufferId;
        VABufferID m_mbNoSkipBufferId;
        std::vector<VABufferID> m_sliceBufferId;

        VABufferID m_packedAudHeaderBufferId;
        VABufferID m_packedAudBufferId;
        VABufferID m_packedSpsHeaderBufferId;
        VABufferID m_packedSpsBufferId;
        VABufferID m_packedPpsHeaderBufferId;
        VABufferID m_packedPpsBufferId;
        VABufferID m_packedSeiHeaderBufferId;
        VABufferID m_packedSeiBufferId;
        VABufferID m_packedSkippedSliceHeaderBufferId;
        VABufferID m_packedSkippedSliceBufferId;
        std::vector<VABufferID> m_packeSliceHeaderBufferId;
        std::vector<VABufferID> m_packedSliceBufferId;

        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_bsQueue;
        std::vector<ExtVASurface> m_reconQueue;

        mfxU32 m_width;
        mfxU32 m_height;
        mfxU32 m_userMaxFrameSize;
        mfxU32 m_mbbrc;
        ENCODE_CAPS m_caps;

        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 26 + 5;

        UMC::Mutex m_guard;
        HeaderPacker m_headerPacker;

        VAEncMiscParameterRateControl  m_vaBrcPar;
        VAEncMiscParameterFrameRate    m_vaFrameRate;
    };
}

#endif // MFX_VA_LINUX
#endif // __MFX_H265_ENCODE_VAAPI__H
