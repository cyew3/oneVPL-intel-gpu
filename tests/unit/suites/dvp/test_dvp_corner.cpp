// Copyright (c) 2019-2020 Intel Corporation
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

#if defined(MFX_ONEVPL)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "common_utils.h"
#include "test_dvp_bs.h"
#include <string>

#if defined (MFX_ENABLE_UNIT_TEST_DVP)

namespace dvp_corner { namespace tests
{

    // Initialize Intel Media SDK session
    MFXVideoSession session;
    //CORE20 doesn't support DX9 and auto chooses DX9, so choose DX11 explicitly
#if defined(_WIN32) || defined(_WIN64)
    mfxIMPL impl = MFX_IMPL_VIA_D3D11;
#else
    mfxIMPL impl = MFX_IMPL_VIA_VAAPI;
#endif
    mfxVersion ver = { {0, 1} };

    class DVP_F : public ::testing::TestWithParam<mfxU32> {

        public:
        void SetUp() override {
            // Prepare Media SDK bit stream buffer
            mfxBS.DataLength = bs1ln;
            mfxBS.MaxLength = mfxBS.DataLength;
            mfxBS.Data = const_cast<mfxU8*>(bs1);

            // Set required video parameters for decode (1st output)
            decVideoParams.mfx.CodecId = MFX_CODEC_AVC;
            decVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY; //or MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            decVideoParams.mfx.FrameInfo.ChannelId = 0; // 0 is reserved by MSDK for decode output

            //Initialize error status
            sts = MFX_ERR_NONE;

            // Initialize session impl version
            sts = Initialize(impl, ver, &session, NULL);
            ASSERT_EQ(sts, MFX_ERR_NONE);

            // Create Media SDK decoder+vp
            mfxDecVP = new MFXVideoDECODE_VPP(session);

            // Decode stream header data
            sts = mfxDecVP->DecodeHeader(&mfxBS, &decVideoParams);
            ASSERT_EQ(sts, MFX_ERR_NONE);
        }

        void TearDown() override {
            delete mfxDecVP;
        }

        mfxBitstream mfxBS = {};
        mfxVideoParam decVideoParams = {};
        mfxStatus sts;
        MFXVideoDECODE_VPP *mfxDecVP;
    };

    TEST_P(DVP_F, Corner_check_using_VPPs) {

        // Set required video parameters for VPP
        mfxU32 numChannels = GetParam(); // num of VPP channels
        mfxVideoChannelParam vppChannelParams[numChannels]{};
        mfxVideoChannelParam* vppChannelParamsPtrs[numChannels]{};

        // Construct 128 VPP channels
        for (mfxU32 channelIdx = 0; channelIdx<numChannels; channelIdx++) {
            vppChannelParams[channelIdx].IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
            vppChannelParams[channelIdx].VPP = decVideoParams.mfx.FrameInfo;
            vppChannelParams[channelIdx].VPP.ChannelId = channelIdx + 1;
            vppChannelParamsPtrs[channelIdx] = &vppChannelParams[channelIdx];
        }

        // Initialize the Media SDK decoder+vp
        sts = mfxDecVP->Init(&decVideoParams, &vppChannelParamsPtrs[0], numChannels);

        if (sts == MFX_ERR_NONE) {

            // Main decoding+processing loop
            bool EndOfStream = true;
            for (;;) {
                // Decode and process frames asychronously (returns immediately)
                mfxSurfaceArray* pmfxOutSurfaceArr = nullptr;
                sts = mfxDecVP->DecodeFrameAsync(EndOfStream ? nullptr : &mfxBS, nullptr, 0, &pmfxOutSurfaceArr);

                if (MFX_WRN_DEVICE_BUSY == sts) {
                    MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
                    continue;
                }
                else if (EndOfStream && sts == MFX_ERR_MORE_DATA) {
                    sts = MFX_ERR_NONE; //stream ended and all buffered frames have been retrieved, finish
                    break;
                }
                //check for error
                MSDK_BREAK_ON_ERROR(sts);

                ASSERT_NE(pmfxOutSurfaceArr, nullptr);
                for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++) {
                    mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];

                    sts = OutSurf->FrameInterface->Map(OutSurf, MFX_MAP_READ);
                    MSDK_BREAK_ON_ERROR(sts);

                    sts = OutSurf->FrameInterface->Unmap(OutSurf);
                    MSDK_BREAK_ON_ERROR(sts);
                }

                //release output surfaces and surface array
                for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++) {
                    mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];
                    OutSurf->FrameInterface->Release(OutSurf);
                }
                pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr);
            }
        }

        EXPECT_EQ(
            sts,
            MFX_ERR_NONE
        );
    }

    INSTANTIATE_TEST_SUITE_P(
        Corner_check_VPPs,
        DVP_F,
        testing::Values(1, 128),
        testing::PrintToStringParamName()
    );

} }

#endif //MFX_ENABLE_UNIT_TEST_DVP
#endif