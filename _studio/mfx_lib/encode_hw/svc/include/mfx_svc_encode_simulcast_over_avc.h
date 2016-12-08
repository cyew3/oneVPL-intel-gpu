//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_SVC_VIDEO_ENCODE_HW)

#include "mfx_h264_encode_interface.h"

namespace MfxHwH264Encode
{
    DriverEncoder * CreateMultipleAvcEncode(VideoCORE *);

    class MultipleAvcEncoder : public DriverEncoder
    {
    public:
        MultipleAvcEncoder();

        virtual ~MultipleAvcEncoder();

        virtual mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            GUID        guid,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false);

        virtual mfxStatus CreateAccelerationService(
            MfxVideoParam const & par);

        virtual mfxStatus Reset(
            MfxVideoParam const & par);

        virtual mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type);

        virtual mfxStatus Execute(
            mfxHDL                     surface,
            DdiTask const &            task,
            mfxU32                     fieldId,
            PreAllocatedVector const & sei);

        virtual mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request);

        virtual mfxStatus QueryEncodeCaps(
            ENCODE_CAPS & caps);

        virtual
        mfxStatus QueryMbPerSec(
            mfxVideoParam const & par,
            mfxU32              (&mbPerSec)[16]);

        virtual mfxStatus QueryStatus(
            DdiTask & task,
            mfxU32    fieldId);

        virtual mfxStatus Destroy();

        void ForceCodingFunction (mfxU16 codingFunction)
        {
            m_forcedCodingFunction = codingFunction;
        }

        virtual
        mfxStatus QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal)
        {
            core; guid; isTemporal;
            return MFX_ERR_UNSUPPORTED;
        }

    private:
        VideoCORE *                  m_core;
        std::auto_ptr<DriverEncoder> m_ddi[8];
        mfxU32                       m_reconRegCnt;
        mfxU32                       m_bitsrRegCnt;
        mfxU16                        m_forcedCodingFunction;
    };
};

#endif // #if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_SVC_VIDEO_ENCODE_HW)
