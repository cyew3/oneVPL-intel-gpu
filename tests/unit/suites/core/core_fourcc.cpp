// Copyright (c) 2020 Intel Corporation
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

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#include "core_base.h"

namespace test
{


    struct coreFourcc
        : public coreBase
        , public testing::WithParamInterface<int>
    {
        void SetUp() override
        {
            coreBase::SetUp();
            int fourcc = GetParam();
            auto guid = DXVA_ModeHEVC_VLD_Main;
            vp = mocks::mfx::make_param(
                fourcc,
                mocks::mfx::make_param(guid, vp)
            );
            vp.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            if (fourcc == MFX_FOURCC_YUY2)
                vp.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
            auto constexpr type = mocks::mfx::HW_TGL_LP;
            component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);



            EndInitialization(fourcc, MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
        }
    };

    std::vector<int> FourCC =
    {
        MFX_FOURCC_NV12
      , MFX_FOURCC_YUY2
        //, MFX_FOURCC_YV12
        //, MFX_FOURCC_P8
    };

    INSTANTIATE_TEST_SUITE_P(
        FourCC,
        coreFourcc,
        testing::ValuesIn(FourCC)
    );

    TEST_P(coreFourcc, DoFastCopy)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopy(dst, src),
            MFX_ERR_NONE
        );
    }
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
