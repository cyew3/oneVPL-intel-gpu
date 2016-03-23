/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.
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

namespace MfxHwH265Encode
{

mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl);

VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type);

mfxStatus SetHRD(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & hrdBuf_id);

mfxStatus SetQualityLevelParams(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & qualityParams_id);

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
    } ExtVASurface;

    class VAAPIEncoder : public DriverEncoder, DDIHeaderPacker
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            MFXCoreInterface * core,
            GUID       guid,
            mfxU32     width,
            mfxU32     height);

        virtual
        mfxStatus CreateAccelerationService(
            MfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            MfxVideoParam const & par);

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

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT type,
            mfxFrameAllocRequest& request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_HEVC& caps);

        virtual
        mfxStatus QueryStatus(Task & task);

        virtual
        mfxStatus Destroy();

    protected:
        VAAPIEncoder(const VAAPIEncoder&);
        VAAPIEncoder& operator=(const VAAPIEncoder&);

        void FillSps(MfxVideoParam const & par, VAEncSequenceParameterBufferHEVC & sps);

        MFXCoreInterface*    m_core;
        MfxVideoParam m_videoParam;

        VADisplay    m_vaDisplay;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        VAEncSequenceParameterBufferHEVC m_sps;
        VAEncPictureParameterBufferHEVC  m_pps;
        std::vector<VAEncSliceParameterBufferHEVC> m_slice;

        // encode buffer to send vaRender()
        VABufferID m_spsBufferId;
        VABufferID m_hrdBufferId;
        VABufferID m_rateParamBufferId;
        VABufferID m_BRCParallelParamBufferId; //VAEncMiscParameterParallelRateControl
        VABufferID m_frameRateId;
        VABufferID m_qualityLevelBufferId;
        VABufferID m_ppsBufferId;
        std::vector<VABufferID> m_sliceBufferId;

        VABufferID m_packedAudHeaderBufferId;
        VABufferID m_packedAudBufferId;
        VABufferID m_packedSpsHeaderBufferId;
        VABufferID m_packedSpsBufferId;
        VABufferID m_packedVpsHeaderBufferId;
        VABufferID m_packedVpsBufferId;
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
        ENCODE_CAPS_HEVC m_caps;

        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 26 + 5;

        UMC::Mutex m_guard;
        HeaderPacker m_headerPacker;

        VAEncMiscParameterRateControl  m_vaBrcPar;
        VAEncMiscParameterFrameRate    m_vaFrameRate;
    };
}

#endif // MFX_VA_LINUX
#endif // __MFX_H265_ENCODE_VAAPI__H
