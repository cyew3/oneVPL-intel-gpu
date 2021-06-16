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
#include "test_dvp_bs.h"
#include <string>

#if defined (MFX_ENABLE_UNIT_TEST_DVP)

void createBitstream1(char const *fName) {
  FILE* fDest;
  MSDK_FOPEN(fDest, fName, "wb");
  fwrite(bs1, 1, bs1ln, fDest); fwrite(bs1, 1, bs1ln, fDest);
  fclose(fDest);
}

void createBitstream2(char const *fName) {
  FILE* fDest;
  MSDK_FOPEN(fDest, fName, "wb");
  fwrite(bs1, 1, bs1ln, fDest); fwrite(bs2, 1, bs2ln, fDest);
  fclose(fDest);
}

namespace dvp_status { namespace tests
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

    TEST(DVP, Status_check_2_compatible_SPS)
    {
        FILE* fSource;
        char const *fName = "test_stream_2sps.264";
        createBitstream1(fName);
        MSDK_FOPEN(fSource, fName, "rb");
        // Prepare Media SDK bit stream buffer
        mfxBitstream mfxBS = {};
        mfxBS.MaxLength = 1024 * 1024;
        std::unique_ptr<mfxU8[]> bsData(new mfxU8[mfxBS.MaxLength]);
        mfxBS.Data = bsData.get();

        // Read a chunk of data from stream file into bit stream buffer
        mfxStatus sts = MFX_ERR_NONE;
        sts = ReadBitStreamData(&mfxBS, fSource);

        // Create Media SDK decoder+vp
        Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL); //already checked in previous test
        MFXVideoDECODE_VPP mfxDecVP(session);

        // Set required video parameters for decode (1st output)
        mfxVideoParam mfxVideoParams = {};
        mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
        mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY; //or MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        mfxVideoParams.mfx.FrameInfo.ChannelId = 0; // 0 is reserved by MSDK for decode output

        sts = mfxDecVP.DecodeHeader(&mfxBS, &mfxVideoParams);

        // Initialize the Media SDK decoder without any VPPs
        mfxVideoChannelParam* mfxChannelParams[1]; //to bypass null ptr check create 1 VPP channel buffer
        sts = mfxDecVP.Init(&mfxVideoParams, mfxChannelParams, 0);

        // Main decoding+processing loop
        bool EndOfStream = false;
        for (;;)
        {
            // Decode and process frames asychronously (returns immediately)
            mfxSurfaceArray* pmfxOutSurfaceArr = nullptr;
            sts = mfxDecVP.DecodeFrameAsync(EndOfStream ? nullptr : &mfxBS, nullptr, 0, &pmfxOutSurfaceArr);

            if (MFX_WRN_DEVICE_BUSY == sts)
            {
                MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
                continue;
            }
            else if (EndOfStream && sts == MFX_ERR_MORE_DATA)
            {
               sts = MFX_ERR_NONE; //stream ended and all buffered frames have been retrieved, finish
               break;
            }
            else if (!EndOfStream && MFX_ERR_MORE_DATA == sts)
            {
                //need more data from bitstream
                sts = ReadBitStreamData(&mfxBS, fSource);
                if (sts == MFX_ERR_NONE) {
                    continue;
                }
                //bitstream ended, retrieve buffered frames from DVP
                EndOfStream = true;
                continue;
            }
            //check for error
            MSDK_BREAK_ON_ERROR(sts);

            for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++)
            {
                mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];

                sts = OutSurf->FrameInterface->Map(OutSurf, MFX_MAP_READ);
                MSDK_BREAK_ON_ERROR(sts);

                sts = OutSurf->FrameInterface->Unmap(OutSurf);
                MSDK_BREAK_ON_ERROR(sts);
            }

            //release output surfaces and surface array
            for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++)
            {
                mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];
                OutSurf->FrameInterface->Release(OutSurf);
            }
            pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr);
        }
        fclose(fSource);
        remove(fName);

        EXPECT_EQ(
            sts,
            MFX_ERR_NONE
        );
    }

    TEST(DVP, Status_check_2_incompatible_SPS)
    {
        FILE* fSource;
        char const *fName = "test_stream_2sps_diff.264";
        createBitstream2(fName);
        MSDK_FOPEN(fSource, fName, "rb");
        // Prepare Media SDK bit stream buffer
        mfxBitstream mfxBS = {};
        mfxBS.MaxLength = 1024 * 1024;
        std::unique_ptr<mfxU8[]> bsData(new mfxU8[mfxBS.MaxLength]);
        mfxBS.Data = bsData.get();

        // Read a chunk of data from stream file into bit stream buffer
        mfxStatus sts = MFX_ERR_NONE;
        sts = ReadBitStreamData(&mfxBS, fSource);

        // Create Media SDK decoder+vp
        Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL); //already checked in previous test
        MFXVideoDECODE_VPP mfxDecVP(session);

        // Set required video parameters for decode (1st output)
        mfxVideoParam mfxVideoParams = {};
        mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
        mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY; //or MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        mfxVideoParams.mfx.FrameInfo.ChannelId = 0; // 0 is reserved by MSDK for decode output

        sts = mfxDecVP.DecodeHeader(&mfxBS, &mfxVideoParams);

        // Initialize the Media SDK decoder without any VPPs
        mfxVideoChannelParam* mfxChannelParams[1]; //to bypass null ptr check create 1 VPP channel buffer
        sts = mfxDecVP.Init(&mfxVideoParams, mfxChannelParams, 0);

        // Main decoding+processing loop
        bool EndOfStream = false;
        for (;;)
        {
            // Decode and process frames asychronously (returns immediately)
            mfxSurfaceArray* pmfxOutSurfaceArr = nullptr;
            sts = mfxDecVP.DecodeFrameAsync(EndOfStream ? nullptr : &mfxBS, nullptr, 0, &pmfxOutSurfaceArr);

            if (MFX_WRN_DEVICE_BUSY == sts)
            {
                MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
                continue;
            }
            else if (EndOfStream && sts == MFX_ERR_MORE_DATA)
            {
               sts = MFX_ERR_NONE; //stream ended and all buffered frames have been retrieved, finish
               break;
            }
            else if (!EndOfStream && MFX_ERR_MORE_DATA == sts)
            {
                //need more data from bitstream
                sts = ReadBitStreamData(&mfxBS, fSource);
                if (sts == MFX_ERR_NONE) {
                    continue;
                }
                //bitstream ended, retrieve buffered frames from DVP
                EndOfStream = true;
                continue;
            }
            //check for error
            MSDK_BREAK_ON_ERROR(sts);

            for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++)
            {
                mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];

                sts = OutSurf->FrameInterface->Map(OutSurf, MFX_MAP_READ);
                MSDK_BREAK_ON_ERROR(sts);

                sts = OutSurf->FrameInterface->Unmap(OutSurf);
                MSDK_BREAK_ON_ERROR(sts);
            }

            //release output surfaces and surface array
            for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++)
            {
                mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];
                OutSurf->FrameInterface->Release(OutSurf);
            }
            pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr);
        }
        fclose(fSource);
        remove(fName);

        EXPECT_EQ(
            sts,
            MFX_ERR_INCOMPATIBLE_VIDEO_PARAM
        );
    }

} }

#endif //MFX_ENABLE_UNIT_TEST_DVP