// Copyright (c) 2012-2020 Intel Corporation
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
            bool        isTemporal = false) override;

        virtual mfxStatus CreateAccelerationService(
            MfxVideoParam const & par) override;

        virtual mfxStatus Reset(
            MfxVideoParam const & par) override;

        virtual mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type) override;

        virtual mfxStatus Execute(
            mfxHDLPair                 pair,
            DdiTask const &            task,
            mfxU32                     fieldId,
            PreAllocatedVector const & sei) override;

        virtual mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request) override;

        virtual mfxStatus QueryEncodeCaps(
            MFX_ENCODE_CAPS & caps) override;

        virtual
        mfxStatus QueryMbPerSec(
            mfxVideoParam const & par,
            mfxU32              (&mbPerSec)[16]) override;

        virtual mfxStatus QueryStatus(
            DdiTask & task,
            mfxU32    fieldId,
            bool      useEvent = true) override;

        virtual mfxStatus Destroy() override;

        void ForceCodingFunction (mfxU16 codingFunction)
        {
            m_forcedCodingFunction = codingFunction;
        }

        virtual
        mfxStatus QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal)  override
        {
            core; guid; isTemporal;
            return MFX_ERR_UNSUPPORTED;
        }

    private:
        VideoCORE *                  m_core;
        std::unique_ptr<DriverEncoder> m_ddi[8];
        mfxU32                       m_reconRegCnt;
        mfxU32                       m_bitsrRegCnt;
        mfxU16                        m_forcedCodingFunction;
    };
};

#endif // #if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_SVC_VIDEO_ENCODE_HW)
