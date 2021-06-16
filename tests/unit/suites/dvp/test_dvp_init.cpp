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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "common_utils.h"
#include <string>

#if defined (MFX_ENABLE_UNIT_TEST_DVP)

void setInitTestVideoParametersDVP(mfxVideoParam &decPar, mfxVideoChannelParam &vppPar) {
    // Set video parameters for Decode component of DVP
    decPar.mfx.CodecId = MFX_CODEC_AVC;
    decPar.mfx.SkipOutput = true;
    decPar.mfx.FrameInfo.ChannelId = 0; // 0 is reserved by MSDK for decode output.
    decPar.mfx.FrameInfo.Width = 352;
    decPar.mfx.FrameInfo.Height = 288;
    decPar.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    decPar.mfx.FrameInfo.BitDepthLuma = 8;
    decPar.mfx.FrameInfo.BitDepthChroma = 8;
    decPar.mfx.FrameInfo.FrameRateExtN = 30;
    decPar.mfx.FrameInfo.FrameRateExtD = 1;
    // Set video parameters for VPP (resize) component of DVP
    vppPar.VPP = decPar.mfx.FrameInfo;
    vppPar.VPP.ChannelId = 1;
    vppPar.VPP.CropW = MSDK_ALIGN16(decPar.mfx.FrameInfo.Width / 2); // half original resolution
    vppPar.VPP.CropH = MSDK_ALIGN16(decPar.mfx.FrameInfo.Height / 2);
    vppPar.VPP.Width = vppPar.VPP.CropW;
    vppPar.VPP.Height = vppPar.VPP.CropH;
    vppPar.VPP.FrameRateExtN = decPar.mfx.FrameInfo.FrameRateExtN;
    vppPar.VPP.FrameRateExtD = decPar.mfx.FrameInfo.FrameRateExtD;
}

namespace dvp { namespace tests
{

    // Initialize Intel Media SDK session
    MainLoader loader;
    MainVideoSession session;
    //CORE20 doesn't support DX9 and auto chooses DX9, so choose DX11 explicitly
#if defined(_WIN32) || defined(_WIN64)
    mfxIMPL impl = MFX_IMPL_VIA_D3D11;
#else
    mfxIMPL impl = MFX_IMPL_VIA_VAAPI;
#endif
    mfxVersion minVer = { {0, 2} };
    mfxU16 adapterType = mfxMediaAdapterType::MFX_MEDIA_UNKNOWN;
    mfxU32 adapterNum = 0;

    // Try to initialize session
    TEST(DVP, Init)
    {
        ASSERT_EQ(
            Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL),
            MFX_ERR_NONE
        );
    }

    // Create video params for Decode and VPP componets of DVP
    mfxVideoParam decPar = {};
    mfxVideoChannelParam vppPar = {};
    mfxVideoChannelParam* channelsPar[1];

    // Correct call to DVP Init (resize with system memory)
    TEST(DVP, Init_resize_w_sys_mem)
    {
        setInitTestVideoParametersDVP(decPar, vppPar);
        decPar.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        vppPar.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        channelsPar[0] = &vppPar;
        Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL);
        MFXVideoDECODE_VPP mfxDecVP(session);
        EXPECT_EQ(
            mfxDecVP.Init(&decPar, &channelsPar[0], 1),
            MFX_ERR_NONE
        );
    }

    // Correct call to DVP Init (resize with video memory)
    TEST(DVP, Init_resize_w_vid_mem)
    {
        setInitTestVideoParametersDVP(decPar, vppPar);
        decPar.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        vppPar.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        channelsPar[0] = &vppPar;
        Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL);
        MFXVideoDECODE_VPP mfxDecVP(session);
        EXPECT_EQ(
            mfxDecVP.Init(&decPar, &channelsPar[0], 1),
            MFX_ERR_NONE
        );
    }

    // Incorrect/unsupported call to DVP Init (resize with different memory types)
    TEST(DVP, Init_resize_w_diff_sys_vid_mem)
    {
        setInitTestVideoParametersDVP(decPar, vppPar);
        decPar.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        vppPar.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        channelsPar[0] = &vppPar;
        Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL);
        MFXVideoDECODE_VPP mfxDecVP(session);
        EXPECT_EQ(
            mfxDecVP.Init(&decPar, &channelsPar[0], 1),
            MFX_ERR_UNSUPPORTED
        );
    }

    // Incorrect/unsupported call to DVP Init (resize with different memory types)
    TEST(DVP, Init_resize_w_diff_vid_sys_mem)
    {
        setInitTestVideoParametersDVP(decPar, vppPar);
        decPar.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        vppPar.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        channelsPar[0] = &vppPar;
        Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL);
        MFXVideoDECODE_VPP mfxDecVP(session);
        EXPECT_EQ(
            mfxDecVP.Init(&decPar, &channelsPar[0], 1),
            MFX_ERR_UNSUPPORTED
        );
    }

} }

#endif //MFX_ENABLE_UNIT_TEST_DVP
