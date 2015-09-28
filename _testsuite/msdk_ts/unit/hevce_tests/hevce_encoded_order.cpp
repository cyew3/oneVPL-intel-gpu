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

class EncodedOrderTest : public ::testing::Test {
protected:
    EncodedOrderTest() : ctor_status(MFX_ERR_UNKNOWN), core(), encoder(&core, &ctor_status), ctrl(), reordered(), bitstream(), entryPoint() {
        InitParamSetMandated(input);
        input.videoParam.mfx.FrameInfo.Width = 64;
        input.videoParam.mfx.FrameInfo.Height = 64;
        input.videoParam.mfx.BufferSizeInKB = 5;
        input.videoParam.mfx.TargetKbps = 20;
        input.videoParam.mfx.EncodedOrder = 1;
        input.extCodingOption2.AdaptiveI = OFF;
        input.extCodingOptionHevc.AnalyzeCmplx = 1; // off
        core.SetParamSet(input);
        bitstream.MaxLength = input.videoParam.mfx.BufferSizeInKB * 1000;
        bitstream.Data = bsBuf;
        Zero(surfaces);
        for (auto &surf: surfaces) {
            surf.Info = input.videoParam.mfx.FrameInfo;
            surf.Data.Pitch = surf.Info.Width;
            surf.Data.Y = lumaBuf;
            surf.Data.UV = chromaBuf;
        }
        runtimeLaStat = MakeExtBuffer<mfxExtLAFrameStatistics>();
        runtimeCodingOption = MakeExtBuffer<mfxExtCodingOption>();
        runtimeExtBufs[0] = &runtimeCodingOption.Header;
        runtimeExtBufs[1] = &runtimeLaStat.Header;
        ctrl.FrameType = MFX_FRAMETYPE_I;
        ctrl.NumExtParam = 2;
        ctrl.ExtParam = runtimeExtBufs;
    }

    ~EncodedOrderTest() {
    }

    mfxStatus ctor_status;
    MockMFXCoreInterface core;
    MFXVideoENCODEH265 encoder;
    ParamSet input;
    mfxEncodeCtrl ctrl;
    mfxFrameSurface1 surfaces[1];
    mfxFrameSurface1 *reordered;
    mfxBitstream bitstream;
    MFX_ENTRY_POINT entryPoint;
    mfxExtLAFrameStatistics runtimeLaStat;
    mfxExtCodingOption runtimeCodingOption;
    mfxExtBuffer *runtimeExtBufs[2];

    Ipp8u bsBuf[8 * 1024];
    Ipp8u lumaBuf[32 * 32];
    Ipp8u chromaBuf[16 * 16];
};

TEST_F(EncodedOrderTest, RunTimeParamValidation) {
    mfxEncodeCtrl goodCtrl = ctrl;
    ctrl.NumExtParam = 0;
    ctrl.ExtParam = nullptr;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));

    { SCOPED_TRACE("Test ctrl==nullptr");
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.EncodeFrameCheck(nullptr, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
    }
    { SCOPED_TRACE("Test invalid FrameType");
        ctrl.FrameType = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test valid case");
        EXPECT_CALL(core, IncreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
        EXPECT_EQ(MFX_ERR_NONE, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
    }
}

TEST_F(EncodedOrderTest, RunTimeParamValidationLaExt) {
    mfxEncodeCtrl goodCtrl = ctrl;
    input.videoParam.mfx.RateControlMethod = LA_EXT;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));

    { SCOPED_TRACE("Test ctrl==nullptr");
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.EncodeFrameCheck(nullptr, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
    }
    { SCOPED_TRACE("Test invalid FrameType");
        ctrl.FrameType = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test ExtParam==null");
        ctrl.ExtParam = nullptr;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test NumExtParam==0");
        ctrl.NumExtParam = 0;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test absence mfxExtLAFrameStatistics");
        ctrl.NumExtParam = 1;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
        ctrl = goodCtrl;
    }
    { SCOPED_TRACE("Test valid case");
        EXPECT_CALL(core, IncreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
        EXPECT_EQ(MFX_ERR_NONE, encoder.EncodeFrameCheck(&ctrl, surfaces, &bitstream, &reordered, nullptr, &entryPoint));
    }
}
