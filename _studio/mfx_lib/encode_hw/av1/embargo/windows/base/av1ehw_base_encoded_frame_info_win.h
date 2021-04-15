// Copyright (c) 2021 Intel Corporation
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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "av1ehw_base_encoded_frame_info.h"
#include "av1ehw_base_ddi_packer_win.h"

namespace AV1EHW
{
namespace Windows
{
namespace Base
{
    class EncodedFrameInfo
        : public AV1EHW::Base::EncodedFrameInfo
    {
    public:

        EncodedFrameInfo(mfxU32 FeatureId)
            : AV1EHW::Base::EncodedFrameInfo(FeatureId)
        {}

    protected:
        virtual mfxStatus GetDdiInfo(
            const void* pDdiFeedback
            , mfxExtAVCEncodedFrameInfo& info) override
        {
            MFX_CHECK(pDdiFeedback, MFX_ERR_UNDEFINED_BEHAVIOR);
            auto& fb = *(const ENCODE_QUERY_STATUS_PARAMS_AV1*)pDdiFeedback;

            info.QP = fb.QpY;

            return MFX_ERR_NONE;
        }
    };

} //Base
} //Windows
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)