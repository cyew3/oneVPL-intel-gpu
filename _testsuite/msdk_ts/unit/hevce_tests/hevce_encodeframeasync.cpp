//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2018 Intel Corporation. All Rights Reserved.
//

#include "stdexcept"

#include "gtest/gtest.h"

#include "hevce_common_api.h"
#include "hevce_mock_videocore.h"

#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_defs.h"
#include "mfx_h265_encode_api.h"
#include "mfx_task.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;

using namespace ApiTestCommon;
using namespace H265Enc::MfxEnumShortAliases;

class RuntimeTest : public ::testing::Test {
protected:
    RuntimeTest() : ctor_status(MFX_ERR_UNKNOWN), core(), encoder(&core, &ctor_status), ctrl(), reordered(), bitstream(), entryPoint() {
        InitParamSetMandated(input);
        input.videoParam.mfx.FrameInfo.Width = 64;
        input.videoParam.mfx.FrameInfo.Height = 64;
        input.videoParam.mfx.BufferSizeInKB = 5;
        input.videoParam.mfx.TargetKbps = 20;
        input.extCodingOption2.AdaptiveI = OFF;
        input.extCodingOptionHevc.AnalyzeCmplx = 1; // off
        input.extCodingOptionHevc.DeltaQpMode = 1; // off
        core.SetParamSet(input);
        if (encoder.Init(&input.videoParam) != MFX_ERR_NONE)
            throw std::exception();
        encoder.GetVideoParam(&input.videoParam);
        bitstream.MaxLength = input.videoParam.mfx.BufferSizeInKB * input.videoParam.mfx.BRCParamMultiplier * 1000;
        bitstream.Data = bsBuf;
        Zero(surfaces);
        for (auto &surf: surfaces) {
            surf.Info = input.videoParam.mfx.FrameInfo;
            surf.Data.Pitch = surf.Info.Width;
            surf.Data.Y = lumaBuf;
            surf.Data.UV = chromaBuf;
        }
    }

    ~RuntimeTest() {
    }

    mfxStatus ctor_status;
    MockMFXCoreInterface core;
    MFXVideoENCODEH265 encoder;
    ParamSet input;
    mfxEncodeCtrl ctrl;
    mfxFrameSurface1 surfaces[30];
    mfxFrameSurface1 *reordered;
    mfxBitstream bitstream;
    MFX_ENTRY_POINT entryPoint;

    Ipp8u bsBuf[8 * 1024];
    __ALIGN16 Ipp8u lumaBuf[32 * 32];
    __ALIGN16 Ipp8u chromaBuf[16 * 16];
};

TEST_F(RuntimeTest, ParamValidation) {
    mfxBitstream goodBitstream = bitstream;
    mfxFrameSurface1 goodSurface = surfaces[0];
    mfxEncodeCtrl goodCtrl = ctrl;
    mfxPayload payload = {};
    mfxPayload *payloadArr[1] = {};

    { SCOPED_TRACE("Test bitstream.Data==nullptr");
        bitstream.Data = nullptr;
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        bitstream = goodBitstream;
    }
    { SCOPED_TRACE("Test bitstream.Data==nullptr && bitstream.MaxLength==0");
        bitstream.Data = nullptr;
        bitstream.MaxLength = 0;
        EXPECT_EQ(MFX_ERR_NOT_ENOUGH_BUFFER, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        bitstream = goodBitstream;
    }
    { SCOPED_TRACE("Test bitstream==nullptr");
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.EncodeFrameCheck(&ctrl, surfaces, nullptr, &reordered, nullptr, &entryPoint));
    }
    { SCOPED_TRACE("Test entryPoint==nullptr");
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.EncodeFrameCheck(&ctrl, surfaces, nullptr, &reordered, nullptr, nullptr));
    }
    { SCOPED_TRACE("Test bitstream.MaxLength < bitstream.DataOffset + bitstream.DataLength");
        bitstream.DataOffset = bitstream.MaxLength / 2;
        bitstream.DataLength = bitstream.MaxLength - bitstream.DataOffset + 1;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        bitstream = goodBitstream;
    }
    { SCOPED_TRACE("Test bitstream.MaxLength - bitstream.DataOffset - bitstream.DataLength < BufferSizeInKB * 1000");
        bitstream.MaxLength = input.videoParam.mfx.BufferSizeInKB * 1000 - 1;
        EXPECT_EQ(MFX_ERR_NOT_ENOUGH_BUFFER, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        bitstream = goodBitstream;
    }
    { SCOPED_TRACE("Test surface.ChromaFormat != mfxVideoParam.ChromaFormat");
        surfaces[0].Info.ChromaFormat = YUV422;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        surfaces[0] = goodSurface;
    }
    { SCOPED_TRACE("Test surface.Width != mfxVideoParam.Width");
        surfaces[0].Info.Width = input.videoParam.mfx.FrameInfo.Width - 16;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        surfaces[0] = goodSurface;
    }
    { SCOPED_TRACE("Test surface.Height != mfxVideoParam.Height");
        surfaces[0].Info.Height = input.videoParam.mfx.FrameInfo.Height - 16;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        surfaces[0] = goodSurface;
    }
    { SCOPED_TRACE("Test surface.Y != nullptr and surface.UV == nullptr");
        surfaces[0].Data.UV = nullptr;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        surfaces[0] = goodSurface;
    }
    { SCOPED_TRACE("Test surface.Y != nullptr and surface.Pitch == 0");
        surfaces[0].Data.Pitch = 0;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        surfaces[0] = goodSurface;
    }
    { SCOPED_TRACE("Test surface.Y != nullptr and surface.Pitch >= 0x8000");
        surfaces[0].Data.Pitch = 0x8000;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        surfaces[0] = goodSurface;
    }
    { SCOPED_TRACE("Test surface.Y == nullptr and surface.UV != nullptr");
        surfaces[0].Data.Y = nullptr;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        surfaces[0] = goodSurface;
    }
    { SCOPED_TRACE("Test ctrl.FrameType != IDR");
        ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test ctrl.Payload[i] == nullptr");
        payloadArr[0] = nullptr;
        ctrl.Payload = payloadArr;
        ctrl.NumPayload = 1;
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test valid case of ctrl.Payload[i].Data == nullptr && ctrl.Payload[i].NumBit == 0");
        payloadArr[0] = &payload;
        ctrl.Payload = payloadArr;
        ctrl.NumPayload = 1;
        EXPECT_CALL(core, IncreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
        EXPECT_EQ(MFX_ERR_MORE_DATA_SUBMIT_TASK, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test case of ctrl.Payload[i].Data == nullptr && ctrl.Payload[i].NumBit > 0");
        payload.NumBit = 1;
        payloadArr[0] = &payload;
        ctrl.Payload = payloadArr;
        ctrl.NumPayload = 1;
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
        payload = mfxPayload();
    }
    { SCOPED_TRACE("Test case of ctrl.Payload[i].NumBit > 8 * ctrl.Payload[i].BufSize");
        Ipp8u data[11] = {};
        payload.Data = data;
        payload.NumBit = 81;
        payload.BufSize = 10;
        payloadArr[0] = &payload;
        ctrl.Payload = payloadArr;
        ctrl.NumPayload = 1;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
        payload = mfxPayload();
    }
    { SCOPED_TRACE("Test valid case");
        EXPECT_CALL(core, IncreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
        EXPECT_EQ(MFX_ERR_MORE_DATA_SUBMIT_TASK, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
    }
}

TEST_F(RuntimeTest, NullNativeSurface) {
    encoder.Close();

    EXPECT_CALL(core, MapOpaqueSurface(_,_,_))
        .WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(core, UnmapOpaqueSurface(_,_,_))
        .WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(core, GetRealSurface(_,_))
        .WillOnce(Return(MFX_ERR_NONE));

    mfxFrameSurface1 *opaqSurfaces = nullptr;
    input.videoParam.IOPattern = OPAQMEM;
    input.extOpaqueSurfaceAlloc.In.Type = MFX_MEMTYPE_SYSTEM_MEMORY;
    input.extOpaqueSurfaceAlloc.In.NumSurface = 1;
    input.extOpaqueSurfaceAlloc.In.Surfaces = &opaqSurfaces;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));

    // native surface == NULL while opaque surface != NULL
    EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
}

TEST_F(RuntimeTest, UserSeiMessages) {
    mfxFrameSurface1 *surf = surfaces;
    mfxStatus sts = (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK;

    Ipp8u seiData1[5] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    Ipp8u seiData2[510] = {};
    std::fill_n(seiData2, 510, 0xee);

    mfxPayload payloads[2] = {};
    payloads[0].BufSize = sizeof(seiData1);
    payloads[0].NumBit = 8 * sizeof(seiData1);
    payloads[0].Data = seiData1;
    payloads[0].Type = 135;
    payloads[1].BufSize = sizeof(seiData2);
    payloads[1].NumBit = 8 * sizeof(seiData2) - 5; // not byte-aligned
    payloads[1].Data = seiData2;
    payloads[1].Type = 136;
    mfxPayload *payloadPtrs[2] = { payloads + 0, payloads + 1 };
    ctrl.Payload = payloadPtrs;
    ctrl.NumPayload = sizeof(payloadPtrs) / sizeof(payloadPtrs[0]);

    while (sts == MFX_ERR_MORE_DATA_SUBMIT_TASK && surf < surfaces + sizeof(surfaces) / sizeof(surfaces[0])) {
        EXPECT_CALL(core, IncreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
        sts = encoder.EncodeFrameCheck(&ctrl, surf++, &bitstream, &reordered, nullptr, &entryPoint);
        if (sts == MFX_ERR_MORE_DATA_SUBMIT_TASK) {
            EXPECT_CALL(core, DecreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
            ASSERT_EQ(MFX_ERR_NONE, entryPoint.pRoutine(entryPoint.pState, entryPoint.pParam, 0, 0));
            ASSERT_EQ(MFX_ERR_NONE, entryPoint.pCompleteProc(entryPoint.pState, entryPoint.pParam, MFX_ERR_NONE));
        }
    }
    ASSERT_EQ(MFX_ERR_NONE, sts);

    EXPECT_CALL(core, DecreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
    ASSERT_EQ(MFX_ERR_NONE, entryPoint.pRoutine(entryPoint.pState, entryPoint.pParam, 0, 0));
    ASSERT_EQ(MFX_ERR_NONE, entryPoint.pCompleteProc(entryPoint.pState, entryPoint.pParam, MFX_ERR_NONE));

    bool foundSei1 = false;
    bool foundSei2 = false;
    auto nals = SplitNals(bitstream.Data, bitstream.Data + bitstream.DataLength);
    for (auto &nal: nals) {
        if (nal.type == H265Enc::NAL_UT_PREFIX_SEI) {
            auto seiRbsp = ExtractRbsp(nal.start, nal.end);
            const Ipp8u *sei = seiRbsp.data() + (seiRbsp[2] == 0 ? 6 : 5); // skip nal_unit_header

            Ipp32u seiType = 0;
            for (; *sei == 0xff; ++sei, seiType += 255);
            seiType += *sei++;

            Ipp32u seiSize = 0;
            for (; *sei == 0xff; ++sei, seiSize += 255);
            seiSize += *sei++;

            if (seiType == 135) {
                ASSERT_EQ(seiSize, seiRbsp.data() + seiRbsp.size() - sei - 1);
                ASSERT_EQ(sizeof(seiData1), seiSize);
                EXPECT_TRUE(std::equal(sei, sei + seiSize, seiData1));
                foundSei1 = true;
            } else if (seiType == 136) {
                ASSERT_EQ(seiSize, seiRbsp.data() + seiRbsp.size() - sei - 1);
                ASSERT_EQ(sizeof(seiData2), seiSize);
                EXPECT_TRUE(std::equal(sei, sei + seiSize - 1, &seiData2[0]));
                EXPECT_EQ(0xf0, sei[seiSize - 1]); // encoder is expected to put trailing bits in case of unaligned payload
                foundSei2 = true;
            }
        }
    }
    EXPECT_TRUE(foundSei1);
    EXPECT_TRUE(foundSei2);
}

TEST_F(RuntimeTest, EncodeStat) {
    mfxStatus sts = (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK;
    mfxEncodeStat stat = {};
    Ipp32u expectedNumFrame = 0;
    Ipp64u expectedNumBit = 0;
    Ipp32u expectedNumCachedFrame = 0;
    const Ipp32u NUM_SURF = sizeof(surfaces) / sizeof(surfaces[0]);
    const Ipp32u NUM_FRAMES_TO_ENCODE = 100;

    for (Ipp32u i = 0; i < NUM_FRAMES_TO_ENCODE; i++) {
        mfxFrameSurface1 *surf = surfaces + i % NUM_SURF;
        bitstream.DataLength = bitstream.DataOffset = 0;
        EXPECT_CALL(core, IncreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
        sts = encoder.EncodeFrameCheck(nullptr, surf, &bitstream, &reordered, nullptr, &entryPoint);
        expectedNumCachedFrame++;

        EXPECT_EQ(MFX_ERR_NONE, encoder.GetEncodeStat(&stat));
        EXPECT_EQ(expectedNumCachedFrame, stat.NumCachedFrame);
        EXPECT_EQ(expectedNumFrame, stat.NumFrame);
        EXPECT_EQ(expectedNumBit, stat.NumBit);

        if (sts == MFX_ERR_MORE_DATA_SUBMIT_TASK) {
            EXPECT_CALL(core, DecreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
            ASSERT_EQ(MFX_ERR_NONE, entryPoint.pRoutine(entryPoint.pState, entryPoint.pParam, 0, 0));
            ASSERT_EQ(MFX_ERR_NONE, entryPoint.pCompleteProc(entryPoint.pState, entryPoint.pParam, MFX_ERR_NONE));

            EXPECT_EQ(MFX_ERR_NONE, encoder.GetEncodeStat(&stat));
            EXPECT_EQ(expectedNumCachedFrame, stat.NumCachedFrame);
            EXPECT_EQ(expectedNumFrame, stat.NumFrame);
            EXPECT_EQ(expectedNumBit, stat.NumBit);
        } else if (sts == MFX_ERR_NONE) {
            EXPECT_CALL(core, DecreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
            ASSERT_EQ(MFX_ERR_NONE, entryPoint.pRoutine(entryPoint.pState, entryPoint.pParam, 0, 0));
            ASSERT_EQ(MFX_ERR_NONE, entryPoint.pCompleteProc(entryPoint.pState, entryPoint.pParam, MFX_ERR_NONE));

            expectedNumCachedFrame--;
            expectedNumFrame++;
            expectedNumBit += 8 * bitstream.DataLength;
            EXPECT_EQ(MFX_ERR_NONE, encoder.GetEncodeStat(&stat));
            EXPECT_EQ(expectedNumCachedFrame, stat.NumCachedFrame);
            EXPECT_EQ(expectedNumFrame, stat.NumFrame);
            EXPECT_EQ(expectedNumBit, stat.NumBit);
        }
    }

    while (true) {
        bitstream.DataLength = bitstream.DataOffset = 0;
        sts = encoder.EncodeFrameCheck(nullptr, nullptr, &bitstream, &reordered, nullptr, &entryPoint);
        EXPECT_TRUE(sts == MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA);
        if (sts != MFX_ERR_NONE)
            break;

        EXPECT_EQ(MFX_ERR_NONE, encoder.GetEncodeStat(&stat));
        EXPECT_EQ(expectedNumCachedFrame, stat.NumCachedFrame);
        EXPECT_EQ(expectedNumFrame, stat.NumFrame);
        EXPECT_EQ(expectedNumBit, stat.NumBit);

        ASSERT_EQ(MFX_ERR_NONE, entryPoint.pRoutine(entryPoint.pState, entryPoint.pParam, 0, 0));
        ASSERT_EQ(MFX_ERR_NONE, entryPoint.pCompleteProc(entryPoint.pState, entryPoint.pParam, MFX_ERR_NONE));

        expectedNumCachedFrame--;
        expectedNumFrame++;
        expectedNumBit += 8 * bitstream.DataLength;
        EXPECT_EQ(MFX_ERR_NONE, encoder.GetEncodeStat(&stat));
        EXPECT_EQ(expectedNumCachedFrame, stat.NumCachedFrame);
        EXPECT_EQ(expectedNumFrame, stat.NumFrame);
        EXPECT_EQ(expectedNumBit, stat.NumBit);
    }

    EXPECT_EQ(MFX_ERR_NONE, encoder.GetEncodeStat(&stat));
    EXPECT_EQ(0, stat.NumCachedFrame);
    EXPECT_EQ(NUM_FRAMES_TO_ENCODE, stat.NumFrame);
}
