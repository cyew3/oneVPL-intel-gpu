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

class MFXVideoENCODEH265Test : public MFXVideoENCODEH265 {
public:
    MFXVideoENCODEH265Test(MFXCoreInterface1 *core, mfxStatus *status) : MFXVideoENCODEH265(core, status) {}
    using MFXVideoENCODEH265::m_extBuffers;
};

class InitTest : public ::testing::Test {
protected:
    InitTest() : ctor_status(MFX_ERR_UNKNOWN), core(), encoder(&core, &ctor_status) {
        core.SetParamSet(input);
    }

    ~InitTest() {
    }

    mfxStatus ctor_status;
    MockMFXCoreInterface core;
    MFXVideoENCODEH265Test encoder;
    ParamSet input;
};

mfxStatus GetCoreParamImplDx9(mfxCoreParam *par)
{
    Zero(*par);
    par->Impl = MFX_IMPL_VIA_D3D9;
    return MFX_ERR_NONE;
}

mfxStatus CreateAccelerationDeviceImpl(mfxHandleType type, mfxHDL *handle)
{
    *handle = (mfxHDL *)1;
    return MFX_ERR_NONE;
}


TEST_F(InitTest, NullPtr) {
    EXPECT_EQ(MFX_ERR_NONE, ctor_status);
    EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.Init(nullptr));
}

TEST_F(InitTest, NotInitialized) {
    EXPECT_EQ(MFX_ERR_NOT_INITIALIZED, encoder.GetVideoParam(&input.videoParam));
    EXPECT_EQ(MFX_ERR_NOT_INITIALIZED, encoder.Reset(&input.videoParam));
    EXPECT_EQ(MFX_ERR_NOT_INITIALIZED, encoder.EncodeFrameCheck(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    EXPECT_EQ(MFX_ERR_NOT_INITIALIZED, encoder.GetEncodeStat(nullptr));
    EXPECT_EQ(MFX_ERR_NOT_INITIALIZED, encoder.Close());
}

TEST_F(InitTest, ConstnessOfInputParam) {
    ParamSet tmp;
    InitParamSetUnsupported(input, tmp); // temporarily use 'tmp' as 'expectedOutput'
    CopyParamSet(tmp, input);
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    ExpectEqual(tmp, input);

    InitParamSetCorrectable(input, tmp); // temporarily use 'tmp' as 'expectedOutput'
    CopyParamSet(tmp, input);
    EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, encoder.Init(&input.videoParam));
    ExpectEqual(tmp, input);
}

TEST_F(InitTest, DoubleInit) {
    // failed attempt due to invalid params
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    // after failed attempt with valid params init is ok
    InitParamSetValid(input);
    EXPECT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    // after successful attempt second init fails
    EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, encoder.Init(&input.videoParam));
}

TEST_F(InitTest, ExtBuffers_Duplicates) {
    // test if duplicated ExtBuffers are handled properly
    auto duplicate1 = MakeExtBuffer<mfxExtCodingOptionHEVC>();
    input.videoParam.ExtParam[input.videoParam.NumExtParam++] = &duplicate1.Header;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.videoParam.NumExtParam--;
}

TEST_F(InitTest, ExtBuffers_Unknown) {
    // test if unsupported ExtBuffers are handled properly
    mfxExtBuffer extUnknown = {};
    input.videoParam.ExtParam[input.videoParam.NumExtParam++] = &extUnknown;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.videoParam.NumExtParam--;
}

TEST_F(InitTest, ExtBuffers_BufferSz) {
    // test if bad Header.BufferSz is handled properly
    input.extOpaqueSurfaceAlloc.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extOpaqueSurfaceAlloc.Header.BufferSz--;
    input.extCodingOptionHevc.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extCodingOptionHevc.Header.BufferSz--;
    input.extDumpFiles.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extDumpFiles.Header.BufferSz--;
    input.extHevcTiles.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extHevcTiles.Header.BufferSz--;
    input.extHevcRegion.Header.BufferSz++;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extHevcRegion.Header.BufferSz--;
}

TEST_F(InitTest, ExtBuffers_Order) {
    InitParamSetValid(input);
    EXPECT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
    std::reverse(input.videoParam.ExtParam, input.videoParam.ExtParam + input.videoParam.NumExtParam);
    EXPECT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    while (input.videoParam.NumExtParam--) { // test that encoder is init-able with fewer ext buffers
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    }
}

TEST_F(InitTest, MandatedParams) {
    InitParamSetMandated(input);
    mfxVideoParam tmp = input.videoParam;
    { SCOPED_TRACE("All mandated params are set");
        EXPECT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
    }
    { SCOPED_TRACE("No IOPattern");
        input.videoParam.IOPattern = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
        input.videoParam.IOPattern = tmp.IOPattern;
    }
    { SCOPED_TRACE("No Width");
        input.videoParam.mfx.FrameInfo.Width = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
        input.videoParam.mfx.FrameInfo.Width = tmp.mfx.FrameInfo.Width;
    }
    { SCOPED_TRACE("No Height");
        input.videoParam.mfx.FrameInfo.Height = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
        input.videoParam.mfx.FrameInfo.Height = tmp.mfx.FrameInfo.Height;
    }
    { SCOPED_TRACE("No FourCC");
        input.videoParam.mfx.FrameInfo.FourCC = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
        input.videoParam.mfx.FrameInfo.FourCC = tmp.mfx.FrameInfo.FourCC;
    }
    { SCOPED_TRACE("No ChromaFormat");
        input.videoParam.mfx.FrameInfo.ChromaFormat = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
        input.videoParam.mfx.FrameInfo.ChromaFormat = tmp.mfx.FrameInfo.ChromaFormat;
    }
    { SCOPED_TRACE("No FrameRateExtN");
        input.videoParam.mfx.FrameInfo.FrameRateExtN = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
        input.videoParam.mfx.FrameInfo.FrameRateExtN = tmp.mfx.FrameInfo.FrameRateExtN;
    }
    { SCOPED_TRACE("No FrameRateExtD");
        input.videoParam.mfx.FrameInfo.FrameRateExtD = 0;
        EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
        input.videoParam.mfx.FrameInfo.FrameRateExtD = tmp.mfx.FrameInfo.FrameRateExtD;
    }
    { SCOPED_TRACE("No TargetKbps");
        Ipp16u rcMethods[] = {CBR, VBR, AVBR, LA_EXT};
        for (auto rcm: rcMethods) {
            input.videoParam.mfx.TargetKbps = 0;
            input.videoParam.mfx.RateControlMethod = rcm;
            EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
            input.videoParam.mfx.TargetKbps = tmp.mfx.TargetKbps;
            input.videoParam.mfx.RateControlMethod = tmp.mfx.RateControlMethod;
        }
    }
}

TEST_F(InitTest, OpaqueMemory) {
    InitParamSetMandated(input);
    input.videoParam.IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY;
    input.extOpaqueSurfaceAlloc.In.NumSurface = 1;
    input.extOpaqueSurfaceAlloc.In.Surfaces = (mfxFrameSurface1 **)0x88888888;

    // should with OPAQMEM
    // calling Alloc/Free for opaq and Alloc/Free for aux frame in case of video memory
    const Ipp32u types[] = {MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET, MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET, MFX_MEMTYPE_SYSTEM_MEMORY};
    for (auto type: types) {
        input.extOpaqueSurfaceAlloc.In.Type = type;
        EXPECT_CALL(core, MapOpaqueSurface(_,_,_)).WillOnce(Return(MFX_ERR_NONE));
        EXPECT_CALL(core, UnmapOpaqueSurface(_,_,_)).WillOnce(Return(MFX_ERR_NONE));
        if (type != MFX_MEMTYPE_SYSTEM_MEMORY) {
            EXPECT_CALL(core, GetCoreParam(_)).WillOnce(Invoke(&GetCoreParamImplDx9));
            EXPECT_CALL(core, CreateAccelerationDevice(_,_)).WillOnce(Invoke(&CreateAccelerationDeviceImpl));
        }
        EXPECT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
    }

    // should still fail with incorrect In.Type
    input.extOpaqueSurfaceAlloc.In.Type = MFX_MEMTYPE_FROM_DECODE;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extOpaqueSurfaceAlloc.In.Type = MFX_MEMTYPE_SYSTEM_MEMORY;

    // should still fail without zero In.Type
    input.extOpaqueSurfaceAlloc.In.Type = 0;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extOpaqueSurfaceAlloc.In.Type = MFX_MEMTYPE_SYSTEM_MEMORY;

    // should still fail with In.NumSurfaces == 0
    input.extOpaqueSurfaceAlloc.In.NumSurface = 0;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extOpaqueSurfaceAlloc.In.NumSurface = 1;

    // should still fail without In.Surfaces = nullptr
    input.extOpaqueSurfaceAlloc.In.Surfaces = nullptr;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.extOpaqueSurfaceAlloc.In.Surfaces = (mfxFrameSurface1 **)0x88888888;

    // should fail without mfxExtOpaqueSurfaceAlloc
    input.videoParam.ExtParam = nullptr;
    EXPECT_EQ(MFX_ERR_INVALID_VIDEO_PARAM, encoder.Init(&input.videoParam));
    input.videoParam.ExtParam = input.extBuffers;
}

TEST_F(InitTest, GetVideoParam) {
    ParamSet output;
    InitParamSetValid(input);
    CopyParamSet(output, input);
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    EXPECT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    ExpectEqual(input, output);
}

TEST_F(InitTest, GetVideoParam_SpsPps) {
    input.videoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    input.videoParam.AsyncDepth = 1;
    input.videoParam.mfx.GopPicSize = 1;
    input.videoParam.mfx.FrameInfo.Width = 64;
    input.videoParam.mfx.FrameInfo.Height = 64;
    input.videoParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    input.videoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    input.videoParam.mfx.FrameInfo.FrameRateExtN = 30;
    input.videoParam.mfx.FrameInfo.FrameRateExtD = 1;
    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));

#if (defined(_WIN32) || defined(_WIN64))
    __declspec(align(32)) Ipp8u data[64*64*3/2] = {};
#else
    __attribute__ ((aligned(32))) Ipp8u data[64*64*3/2] = {};
#endif
    mfxFrameSurface1 surfaces[10];
    for (auto &surf: surfaces) {
        surf.Info = input.videoParam.mfx.FrameInfo;
        surf.Data.Y = data;
        surf.Data.UV = data + 64*64;
        surf.Data.Pitch = 64;
    }

    ParamSet output;
    mfxExtCodingOptionSPSPPS extSpsPps = MakeExtBuffer<mfxExtCodingOptionSPSPPS>();
    Ipp8u sps[1024], pps[1024];
    extSpsPps.SPSBuffer = sps;
    extSpsPps.PPSBuffer = pps;
    extSpsPps.SPSBufSize = sizeof(sps);
    extSpsPps.PPSBufSize = sizeof(pps);
    output.extBuffers[0] = &extSpsPps.Header;
    output.videoParam.NumExtParam = 1;

    { SCOPED_TRACE("Test extSpsPps.SPSBuffer==nullptr");
        extSpsPps.SPSBuffer = nullptr;
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.GetVideoParam(&output.videoParam));
        extSpsPps.SPSBuffer = sps;
    }
    { SCOPED_TRACE("Test extSpsPps.PPSBuffer==nullptr");
        extSpsPps.PPSBuffer = nullptr;
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.GetVideoParam(&output.videoParam));
        extSpsPps.PPSBuffer = pps;
    }
    { SCOPED_TRACE("Test small extSpsPps.SPSBufSize");
        extSpsPps.SPSBufSize = 4;
        EXPECT_EQ(MFX_ERR_NOT_ENOUGH_BUFFER, encoder.GetVideoParam(&output.videoParam));
        extSpsPps.SPSBufSize = sizeof(sps);
    }
    { SCOPED_TRACE("Test small extSpsPps.PPSBufSize");
        extSpsPps.PPSBufSize = 4;
        EXPECT_EQ(MFX_ERR_NOT_ENOUGH_BUFFER, encoder.GetVideoParam(&output.videoParam));
        extSpsPps.PPSBufSize = sizeof(pps);
    }

    EXPECT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    ASSERT_EQ((Ipp8u*)sps, extSpsPps.SPSBuffer); // check that app's pointer is not changed by GetVideoParam
    ASSERT_EQ((Ipp8u*)pps, extSpsPps.PPSBuffer); // check that app's pointer is not changed by GetVideoParam
    ASSERT_GE(sizeof(sps), extSpsPps.SPSBufSize); // check that app's buffer size is not changed by GetVideoParam
    ASSERT_GE(sizeof(pps), extSpsPps.PPSBufSize); // check that app's buffer size is not changed by GetVideoParam

    Ipp8u bsData[8192];
    mfxBitstream bs = {};
    bs.Data = bsData;
    bs.MaxLength = sizeof(bsData);

    mfxFrameSurface1 *reorder = nullptr;
    mfxFrameSurface1 *surf = surfaces;
    MFX_ENTRY_POINT entrypoint = {};
    EXPECT_CALL(core, IncreaseReference(_)).WillRepeatedly(Return(MFX_ERR_NONE));
    mfxStatus sts = (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
    while (sts == MFX_ERR_MORE_DATA_RUN_TASK && surf < surfaces + sizeof(surfaces) / sizeof(surfaces[0]))
        sts = encoder.EncodeFrameCheck(nullptr, surf++, &bs, &reorder, nullptr, &entrypoint);
    ASSERT_EQ(MFX_ERR_NONE, sts);

    EXPECT_CALL(core, DecreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
    ASSERT_EQ(MFX_ERR_NONE, entrypoint.pRoutine(entrypoint.pState, entrypoint.pParam, 0, 0));

    auto nals = SplitNals(bs.Data, bs.Data + bs.DataLength);
    for (auto &nal: nals) {
        if (nal.type == H265Enc::NAL_SPS) {
            SCOPED_TRACE("Testing SPS");
            EXPECT_EQ(Ipp32u(nal.end - nal.start), extSpsPps.SPSBufSize);
            EXPECT_TRUE(std::equal(nal.start, nal.end, extSpsPps.SPSBuffer));
        }
        if (nal.type == H265Enc::NAL_PPS) {
            SCOPED_TRACE("Testing PPS");
            EXPECT_EQ(Ipp32u(nal.end - nal.start), extSpsPps.PPSBufSize);
            EXPECT_TRUE(std::equal(nal.start, nal.end, extSpsPps.PPSBuffer));
        }
    }
}

TEST_F(InitTest, GetVideoParam_Vps) {
    input.videoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    input.videoParam.AsyncDepth = 1;
    input.videoParam.mfx.GopPicSize = 1;
    input.videoParam.mfx.FrameInfo.Width = 64;
    input.videoParam.mfx.FrameInfo.Height = 64;
    input.videoParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    input.videoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    input.videoParam.mfx.FrameInfo.FrameRateExtN = 30;
    input.videoParam.mfx.FrameInfo.FrameRateExtD = 1;
    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));

#if (defined(_WIN32) || defined(_WIN64))
    __declspec(align(32)) Ipp8u data[64*64*3/2] = {};
#else
    __attribute__ ((aligned(32))) Ipp8u data[64*64*3/2] = {};
#endif
    mfxFrameSurface1 surfaces[10];
    for (auto &surf: surfaces) {
        surf.Info = input.videoParam.mfx.FrameInfo;
        surf.Data.Y = data;
        surf.Data.UV = data + 64*64;
        surf.Data.Pitch = 64;
    }

    ParamSet output;
    mfxExtCodingOptionVPS extVps = MakeExtBuffer<mfxExtCodingOptionVPS>();
    Ipp8u vps[1024];
    extVps.VPSBuffer = vps;
    extVps.VPSBufSize = sizeof(vps);
    output.extBuffers[0] = &extVps.Header;
    output.videoParam.NumExtParam = 1;

    { SCOPED_TRACE("Test extVps.VPSBuffer==nullptr");
        extVps.VPSBuffer = nullptr;
        EXPECT_EQ(MFX_ERR_NULL_PTR, encoder.GetVideoParam(&output.videoParam));
        extVps.VPSBuffer = vps;
    }
    { SCOPED_TRACE("Test small extVps.VPSBufSize");
        extVps.VPSBufSize = 4;
        EXPECT_EQ(MFX_ERR_NOT_ENOUGH_BUFFER, encoder.GetVideoParam(&output.videoParam));
        extVps.VPSBufSize = sizeof(vps);
    }

    EXPECT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    ASSERT_EQ((Ipp8u*)vps, extVps.VPSBuffer); // check that app's pointer is not changed by GetVideoParam
    ASSERT_GE(sizeof(vps), extVps.VPSBufSize); // check that app's buffer size is not changed by GetVideoParam

    Ipp8u bsData[8192];
    mfxBitstream bs = {};
    bs.Data = bsData;
    bs.MaxLength = sizeof(bsData);

    mfxFrameSurface1 *reorder = nullptr;
    mfxFrameSurface1 *surf = surfaces;
    MFX_ENTRY_POINT entrypoint = {};
    EXPECT_CALL(core, IncreaseReference(_)).WillRepeatedly(Return(MFX_ERR_NONE));
    mfxStatus sts = (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
    while (sts == MFX_ERR_MORE_DATA_RUN_TASK && surf < surfaces + sizeof(surfaces) / sizeof(surfaces[0]))
        sts = encoder.EncodeFrameCheck(nullptr, surf++, &bs, &reorder, nullptr, &entrypoint);
    ASSERT_EQ(MFX_ERR_NONE, sts);

    EXPECT_CALL(core, DecreaseReference(_)).WillOnce(Return(MFX_ERR_NONE));
    ASSERT_EQ(MFX_ERR_NONE, entrypoint.pRoutine(entrypoint.pState, entrypoint.pParam, 0, 0));

    auto nals = SplitNals(bs.Data, bs.Data + bs.DataLength);
    for (auto &nal: nals) {
        if (nal.type == H265Enc::NAL_VPS) {
            SCOPED_TRACE("Testing VPS");
            EXPECT_EQ(Ipp32u(nal.end - nal.start), extVps.VPSBufSize);
            EXPECT_TRUE(std::equal(nal.start, nal.end, extVps.VPSBuffer));
        }
    }
}

TEST_F(InitTest, Default_NonHrdBufferSizeInKB) {
    ParamSet output;
    InitParamSetMandated(input);

    Ipp32u expectedBufferSizeInKB = 12 * input.videoParam.mfx.FrameInfo.Width * input.videoParam.mfx.FrameInfo.Height / 8000;

    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
    EXPECT_EQ(expectedBufferSizeInKB, output.videoParam.mfx.BufferSizeInKB);

    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_LA_EXT;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
    EXPECT_EQ(expectedBufferSizeInKB, output.videoParam.mfx.BufferSizeInKB);

    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    input.videoParam.mfx.QPI = 30;
    input.videoParam.mfx.QPP = 30;
    input.videoParam.mfx.QPB = 30;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    EXPECT_EQ(expectedBufferSizeInKB, output.videoParam.mfx.BufferSizeInKB);
}

TEST_F(InitTest, Default_Region) {
    InitParamSetMandated(input);
    input.videoParam.mfx.NumSlice = 10;
    input.extHevcRegion.RegionEncoding = MFX_HEVC_REGION_ENCODING_ON;
    input.extHevcRegion.RegionType = MFX_HEVC_REGION_SLICE;
    input.extHevcRegion.RegionId = 5;

    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    EXPECT_EQ(MFX_HEVC_REGION_ENCODING_ON, encoder.m_extBuffers.extRegion.RegionEncoding);
    EXPECT_EQ(MFX_HEVC_REGION_SLICE, encoder.m_extBuffers.extRegion.RegionType);
    EXPECT_EQ(5, encoder.m_extBuffers.extRegion.RegionId);
    EXPECT_EQ(MFX_ERR_NONE, encoder.Close());

    input.videoParam.NumExtParam = 0; // no mfxExtRegion
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    EXPECT_EQ(MFX_HEVC_REGION_ENCODING_OFF, encoder.m_extBuffers.extRegion.RegionEncoding);
}

TEST_F(InitTest, Default_Hrd) {
    ParamSet output;
    InitParamSetMandated(input);

    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    input.videoParam.mfx.TargetKbps = 10000;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    EXPECT_EQ(10000, output.videoParam.mfx.TargetKbps);                 // TargetKbps unchanged
    EXPECT_EQ(10000 * 2 / 8, output.videoParam.mfx.BufferSizeInKB);     // buffer - 2 seconds
    EXPECT_EQ(10000 * 3 / 16, output.videoParam.mfx.InitialDelayInKB);  // InitialDelayInKB - 75% of buffer

    EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    input.videoParam.mfx.TargetKbps = 10000;
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    EXPECT_EQ(10000, output.videoParam.mfx.TargetKbps);                 // TargetKbps unchanged
    EXPECT_EQ(10000 * 3 / 2, output.videoParam.mfx.MaxKbps);            // maxKbps - 150% of target
    EXPECT_EQ(10000 * 2 / 8, output.videoParam.mfx.BufferSizeInKB);     // buffer - 2 seconds
    EXPECT_EQ(10000 * 3 / 16, output.videoParam.mfx.InitialDelayInKB);  // InitialDelayInKB - 75% of buffer
}

TEST_F(InitTest, Default_QPI_QPP_QPB) {
    ParamSet output;
    InitParamSetMandated(input);
    input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    Ipp32u configs[][6] = { {0,0,0, 30,31,31}, {20,0,0, 20,21,21}, {0,15,0, 14,15,15}, {0,0,25, 24,25,25}, {10,20,0, 10,20,20}, {35,0,40, 35,36,40}, {50,0,40, 50,51,40}, {33,31,22, 33,31,22}, {51,0,0, 51,51,51} };
    for (auto c: configs) {
        input.videoParam.mfx.QPI = c[0];
        input.videoParam.mfx.QPP = c[1];
        input.videoParam.mfx.QPB = c[2];
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(c[3], output.videoParam.mfx.QPI);
        EXPECT_EQ(c[4], output.videoParam.mfx.QPP);
        EXPECT_EQ(c[5], output.videoParam.mfx.QPB);
    }
}

TEST_F(InitTest, Default_EnableCm) {
    ParamSet output;
    InitParamSetMandated(input);
    ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
    ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
    EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
    EXPECT_EQ(OFF, output.extCodingOptionHevc.EnableCm);
}

TEST_F(InitTest, Default_GopPicSize) {
    ParamSet output;
    InitParamSetMandated(input);
    Ipp16u configs[] = {0, 1, 2, 7, 8, 29, 99, 0xff, 1000, 0xffff};
    for (auto gopPicSize: configs) {
        input.videoParam.mfx.GopPicSize = gopPicSize;
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(gopPicSize, output.videoParam.mfx.GopPicSize);
    }
}

TEST_F(InitTest, Default_Profile) {
    ParamSet output;
    InitParamSetMandated(input);
    Ipp32u configs[][3] = { {NV12, YUV420, MAIN}, {NV16, YUV422, REXT}, {P010, YUV420, MAIN10}, {P210, YUV422, REXT} };
    for (auto c: configs) {
        input.videoParam.mfx.FrameInfo.FourCC = c[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = c[1];
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(c[2], output.videoParam.mfx.CodecProfile);
    }
}

TEST_F(InitTest, Default_DiableVUI) {
    ParamSet output;
    InitParamSetMandated(input);
    Ipp32u rcMethods[] = {CBR, VBR, AVBR, LA_EXT, CQP};
    Ipp32u triStates[] = {0, ON, OFF};
    for (auto rcm: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcm;
        if (rcm == CQP)
            input.videoParam.mfx.TargetKbps = 0;
        for (auto state: triStates) {
            input.extCodingOption2.DisableVUI = state;
            ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
            ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
            EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
            if (state == 0)
                EXPECT_EQ(OFF, output.extCodingOption2.DisableVUI);
            else
                EXPECT_EQ(state, output.extCodingOption2.DisableVUI);
        }
    }
}

TEST_F(InitTest, Default_AUDelimiter) {
    ParamSet output;
    InitParamSetMandated(input);
    input.extCodingOption.AUDelimiter = 0;
    Ipp32u rcMethods[] = {CBR, VBR, AVBR, LA_EXT, CQP};
    for (auto rcm: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcm;
        if (rcm == CQP)
            input.videoParam.mfx.TargetKbps = 0;
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(ON, output.extCodingOption.AUDelimiter);
    }
}

TEST_F(InitTest, Default_AdaptiveI) {
    ParamSet output;
    InitParamSetMandated(input);
    Ipp32u rcMethods[] = {CBR, VBR, AVBR, LA_EXT, CQP};
    Ipp32u gopOptFlags[] = {0, MFX_GOP_STRICT};
    for (auto rcm: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcm;
        for (auto gof: gopOptFlags) {
            input.videoParam.mfx.GopOptFlag = gof;
            ASSERT_LE(MFX_ERR_NONE, encoder.Init(&input.videoParam));
            ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
            EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
            if (gof == MFX_GOP_STRICT)
                EXPECT_EQ(OFF, output.extCodingOption2.AdaptiveI);
            else if (rcm == CBR || rcm == VBR || rcm == AVBR)
                EXPECT_EQ(ON, output.extCodingOption2.AdaptiveI);
            else
                EXPECT_EQ(OFF, output.extCodingOption2.AdaptiveI);
        }
    }
}

TEST_F(InitTest, Default_AnalyzeCmplx) {
    ParamSet output;
    InitParamSetMandated(input);
    Ipp32u rcMethods[] = {CBR, VBR, AVBR, LA_EXT, CQP};
    for (auto rcm: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcm;
        ASSERT_LE(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        if (rcm == CBR || rcm == VBR || rcm == AVBR)
            EXPECT_EQ(2, output.extCodingOptionHevc.AnalyzeCmplx);
        else
            EXPECT_EQ(1, output.extCodingOptionHevc.AnalyzeCmplx);
        EXPECT_EQ(3, output.extCodingOptionHevc.LowresFactor);
    }
}

TEST_F(InitTest, Default_RateControlDepth_LowresFactor) {
    ParamSet output;
    InitParamSetMandated(input);
    Ipp32u configs[][3] = {{1,8,0}, {2,1,2}, {2,8,9}};
    for (auto p: configs) {
        input.extCodingOptionHevc.AnalyzeCmplx = p[0];
        input.videoParam.mfx.GopRefDist = p[1];
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(p[2], output.extCodingOptionHevc.RateControlDepth);
        EXPECT_EQ(3, output.extCodingOptionHevc.LowresFactor);
    }
}

TEST_F(InitTest, Default_FramesInParallel) {
    ParamSet output;
    InitParamSetMandated(input);
    Ipp32u configs[][4] = {{2,0,0,0}, {0,2,1,0}, {0,1,2,0}, {0,0,0,1}};
    input.videoParam.mfx.FrameInfo.Width = 512;
    input.videoParam.mfx.FrameInfo.Height = 128;
    for (auto p: configs) {
        input.videoParam.mfx.NumSlice = p[0];
        input.extHevcTiles.NumTileRows = p[1];
        input.extHevcTiles.NumTileColumns = p[2];
        input.videoParam.AsyncDepth = p[3];
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        EXPECT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(1, output.extCodingOptionHevc.FramesInParallel);
    }
}

TEST_F(InitTest, Default_DeltaQpMode) {
    ParamSet output;
    InitParamSetMandated(input);
    input.videoParam.mfx.FrameInfo.Width = 64;
    input.videoParam.mfx.FrameInfo.Height = 64;
    input.videoParam.mfx.RateControlMethod = CQP;
    input.videoParam.mfx.TargetUsage = 1;
    input.videoParam.mfx.QPI = 30;
    input.videoParam.mfx.QPP = 31;
    input.videoParam.mfx.QPB = 31;

    { SCOPED_TRACE("DeltaQP is limited to CAQ for P010");
        input.videoParam.mfx.FrameInfo.FourCC = P010;
        input.videoParam.mfx.FrameInfo.ChromaFormat = YUV420;
        input.videoParam.mfx.GopRefDist = 8;
        input.extCodingOptionHevc.BPyramid = ON;
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(2, output.extCodingOptionHevc.DeltaQpMode);
    }

    { SCOPED_TRACE("DeltaQP is limited to CAQ for P210");
        input.videoParam.mfx.FrameInfo.FourCC = P210;
        input.videoParam.mfx.FrameInfo.ChromaFormat = YUV422;
        input.videoParam.mfx.GopRefDist = 8;
        input.extCodingOptionHevc.BPyramid = ON;
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(2, output.extCodingOptionHevc.DeltaQpMode);
        input.videoParam.mfx.FrameInfo.FourCC = NV12;
    }

    { SCOPED_TRACE("DeltaQP should be off if no B frames");
        input.videoParam.mfx.FrameInfo.FourCC = NV12;
        input.videoParam.mfx.FrameInfo.ChromaFormat = YUV420;
        input.videoParam.mfx.GopRefDist = 1;
        input.extCodingOptionHevc.BPyramid = ON;
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(1, output.extCodingOptionHevc.DeltaQpMode);
    }

    { SCOPED_TRACE("DeltaQP should be off if no BPyramid");
        input.videoParam.mfx.FrameInfo.FourCC = NV12;
        input.videoParam.mfx.FrameInfo.ChromaFormat = YUV420;
        input.videoParam.mfx.GopRefDist = 8;
        input.extCodingOptionHevc.BPyramid = OFF;
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(1, output.extCodingOptionHevc.DeltaQpMode);

        input.videoParam.mfx.FrameInfo.FourCC = P010; // check combination, just in case
        ASSERT_EQ(MFX_ERR_NONE, encoder.Init(&input.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.GetVideoParam(&output.videoParam));
        ASSERT_EQ(MFX_ERR_NONE, encoder.Close());
        EXPECT_EQ(1, output.extCodingOptionHevc.DeltaQpMode);
    }
}
