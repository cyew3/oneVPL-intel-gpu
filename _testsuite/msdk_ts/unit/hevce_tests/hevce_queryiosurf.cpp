//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2016 Intel Corporation. All Rights Reserved.
//

#include "gtest/gtest.h"

#include "hevce_common_api.h"

using namespace ApiTestCommon;

#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_encode.h"

class QueryIoSurfTest : public ::testing::Test {
protected:
    QueryIoSurfTest() {
    }

    ~QueryIoSurfTest() {
    }

    ParamSet input;
};

TEST_F(QueryIoSurfTest, NullPtr) {
    // test if null ptr is handled properly
    mfxFrameAllocRequest request = {};
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::QueryIOSurf(nullptr, nullptr, nullptr));
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, nullptr));
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::QueryIOSurf(nullptr, nullptr, &request));
}

TEST_F(QueryIoSurfTest, ExtBuffers) {
    mfxFrameAllocRequest request = {};

    // test if duplicated ExtBuffers are handled properly
    auto duplicate = MakeExtBuffer<mfxExtCodingOptionHEVC>();
    input.videoParam.ExtParam[input.videoParam.NumExtParam++] = &duplicate.Header;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    input.videoParam.NumExtParam--;

    // test if unsupported ExtBuffer are handled properly
    mfxExtBuffer extUnknown = {};
    input.videoParam.ExtParam[input.videoParam.NumExtParam++] = &extUnknown;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    input.videoParam.NumExtParam--;

    // test if bad Header.BufferSz is handled properly
    input.extOpaqueSurfaceAlloc.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    input.extOpaqueSurfaceAlloc.Header.BufferSz--;
    input.extCodingOptionHevc.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    input.extCodingOptionHevc.Header.BufferSz--;
    input.extDumpFiles.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    input.extDumpFiles.Header.BufferSz--;
    input.extHevcTiles.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    input.extHevcTiles.Header.BufferSz--;
    input.extHevcRegion.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    input.extHevcRegion.Header.BufferSz--;
}

TEST_F(QueryIoSurfTest, MandatedParams) {
    mfxFrameAllocRequest request = {};
    InitParamSetMandated(input);
    mfxVideoParam tmp = input.videoParam;

    { SCOPED_TRACE("All mandated params are set");
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    }
    { SCOPED_TRACE("No IOPattern");
        input.videoParam.IOPattern = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
        input.videoParam.IOPattern = tmp.IOPattern;
    }
    { SCOPED_TRACE("No Width");
        input.videoParam.mfx.FrameInfo.Width = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
        input.videoParam.mfx.FrameInfo.Width = tmp.mfx.FrameInfo.Width;
    }
    { SCOPED_TRACE("No Height");
        input.videoParam.mfx.FrameInfo.Height = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
        input.videoParam.mfx.FrameInfo.Height = tmp.mfx.FrameInfo.Height;
    }
    { SCOPED_TRACE("No FourCC");
        input.videoParam.mfx.FrameInfo.FourCC = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
        input.videoParam.mfx.FrameInfo.FourCC = tmp.mfx.FrameInfo.FourCC;
    }
    { SCOPED_TRACE("No ChromaFormat");
        input.videoParam.mfx.FrameInfo.ChromaFormat = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
        input.videoParam.mfx.FrameInfo.ChromaFormat = tmp.mfx.FrameInfo.ChromaFormat;
    }
    { SCOPED_TRACE("No FrameRateExtN");
        input.videoParam.mfx.FrameInfo.FrameRateExtN = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
        input.videoParam.mfx.FrameInfo.FrameRateExtN = tmp.mfx.FrameInfo.FrameRateExtN;
    }
    { SCOPED_TRACE("No FrameRateExtD");
        input.videoParam.mfx.FrameInfo.FrameRateExtD = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
        input.videoParam.mfx.FrameInfo.FrameRateExtD = tmp.mfx.FrameInfo.FrameRateExtD;
    }
    { SCOPED_TRACE("No TargetKbps");
        Ipp16u rcMethods[] = {MFX_RATECONTROL_CBR, MFX_RATECONTROL_VBR, MFX_RATECONTROL_AVBR, MFX_RATECONTROL_LA_EXT};
        for (auto rcm: rcMethods) {
            input.videoParam.mfx.TargetKbps = 0;
            input.videoParam.mfx.RateControlMethod = rcm;
            EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
            input.videoParam.mfx.TargetKbps = tmp.mfx.TargetKbps;
            input.videoParam.mfx.RateControlMethod = tmp.mfx.RateControlMethod;
        }
    }
}

TEST_F(QueryIoSurfTest, Request) {
    InitParamSetMandated(input);
    mfxFrameAllocRequest request = {};

    input.videoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    input.videoParam.AsyncDepth = 3;
    input.videoParam.mfx.GopRefDist = 8;
    input.extCodingOptionHevc.FramesInParallel = 3;
    ASSERT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    EXPECT_EQ(mfxU16(MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY), request.Type);
    EXPECT_EQ(8+(3-1)+(3-1), request.NumFrameMin);
    EXPECT_LE(request.NumFrameMin, request.NumFrameSuggested);
    EXPECT_EQ(0, memcmp(&input.videoParam.mfx.FrameInfo, &request.Info, sizeof(mfxFrameInfo)));

    input.videoParam.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    input.videoParam.AsyncDepth = 1;
    input.videoParam.mfx.GopRefDist = 1;
    input.extCodingOptionHevc.FramesInParallel = 1;
    ASSERT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    EXPECT_EQ(mfxU16(MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET), request.Type);
    EXPECT_EQ(1+(1-1)+(1-1), request.NumFrameMin);
    EXPECT_LE(request.NumFrameMin, request.NumFrameSuggested);
    EXPECT_EQ(0, memcmp(&input.videoParam.mfx.FrameInfo, &request.Info, sizeof(mfxFrameInfo)));

    input.videoParam.IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY;
    input.videoParam.AsyncDepth = 1;
    input.videoParam.mfx.GopRefDist = 6;
    input.extCodingOptionHevc.FramesInParallel = 10;
    ASSERT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::QueryIOSurf(nullptr, &input.videoParam, &request));
    EXPECT_EQ(mfxU16(MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY), request.Type);
    EXPECT_EQ(6+(10-1)+(1-1), request.NumFrameMin);
    EXPECT_LE(request.NumFrameMin, request.NumFrameSuggested);
    EXPECT_EQ(0, memcmp(&input.videoParam.mfx.FrameInfo, &request.Info, sizeof(mfxFrameInfo)));
}
