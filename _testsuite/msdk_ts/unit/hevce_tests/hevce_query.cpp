#include "gtest/gtest.h"
#include <algorithm>

#include "hevce_common_api.h"

using namespace ApiTestCommon;

#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_defs.h"
#include "mfx_h265_encode_api.h"

using namespace H265Enc::MfxEnumShortAliases;

namespace {
    template <class T> void FillExtBufferFF(T &buffer) {
        memset((char *)&buffer + sizeof(mfxExtBuffer), 0xff, sizeof(T) - sizeof(mfxExtBuffer));
    }

    template <class T> void CheckExtBufHeader(const T &buffer) {
        EXPECT_EQ(ExtBufInfo<T>::id, buffer.Header.BufferId);
        EXPECT_EQ(ExtBufInfo<T>::sz, buffer.Header.BufferSz);
    }

    template <class T> bool IsZero(const T& obj) {
        const char *p = reinterpret_cast<const char *>(&obj);
        for (size_t i = 0; i < sizeof(T); i++, p++)
            if (*p)
                return false;
        return true;
    }

    Ipp32u Fourcc2ChromaFormat(Ipp32u fourcc) {
        return (fourcc == NV12 || fourcc == P010) ? YUV420 : (fourcc == NV16 || fourcc == P210) ? YUV422 : 0;
    }

};


class QueryTest : public ::testing::Test {
protected:
    QueryTest() {
    }

    ~QueryTest() {
    }

    ParamSet input, output, originalInput, expectedOutput;

    template <class T, size_t N>
    void TestOneFieldOk(T &inField, T &outField, const T (& values)[N])
    {
        for (auto value: values) {
            inField = value;
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(value, inField);
            EXPECT_EQ(value, outField);
        }
        inField = 0;
    }
    template <class T, class U, size_t N>
    void TestOneFieldErr(T &inField, T &outField, U correctedValue, mfxStatus expectedErr, const T (& values)[N])
    {
        for (auto value: values) {
            inField = value;
            EXPECT_EQ(expectedErr, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(value, inField);
            EXPECT_EQ(correctedValue, outField);
        }
        inField = 0;
    }
};

TEST_F(QueryTest, Mode1_NullPtr) {
    // test if null ptr is handled properly
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::Query(nullptr, nullptr, nullptr));

    // test case when ExtParam==NULL && NumExtParam>0
    mfxExtBuffer **extParamSave = output.videoParam.ExtParam;
    output.videoParam.ExtParam = nullptr;
    EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, nullptr, &output.videoParam));
    output.videoParam.ExtParam = extParamSave;

    // test case when ExtParam[i]==NULL
    output.videoParam.ExtParam[output.videoParam.NumExtParam++] = nullptr;
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::Query(nullptr, nullptr, &output.videoParam));
    output.videoParam.NumExtParam--;
}

TEST_F(QueryTest, Mode1_ExtBuffers) {
    // test if duplicated ExtBuffers are handled properly
    auto duplicate = MakeExtBuffer<mfxExtCodingOptionHEVC>();
    output.videoParam.ExtParam[output.videoParam.NumExtParam++] = &duplicate.Header;
    EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, nullptr, &output.videoParam));
    output.videoParam.NumExtParam--;

    // test if unsupported ExtBuffer are handled properly
    mfxExtBuffer extUnknown = {};
    output.videoParam.ExtParam[output.videoParam.NumExtParam++] = &extUnknown;
    EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, nullptr, &output.videoParam));
    output.videoParam.NumExtParam--;

    // test if bad Header.BufferSz is handled properly
    for (Ipp32u i = 0; i < output.videoParam.NumExtParam; i++) {
        output.videoParam.ExtParam[i]->BufferSz++;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, nullptr, &output.videoParam));
        output.videoParam.ExtParam[i]->BufferSz--;
    }

    // test that Query doesn't fail with fewer ExtBuffers
    while (output.videoParam.NumExtParam) {
        output.videoParam.NumExtParam--;
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, nullptr, &output.videoParam));
    }
}

TEST_F(QueryTest, Mode1_Main) {
    // init VideoParam and ExtBuffers
    output.videoParam.AsyncDepth = 0xffff;
    output.videoParam.Protected = 0xffff;
    output.videoParam.IOPattern = 0xffff;
    FillExtBufferFF(output.extOpaqueSurfaceAlloc);
    FillExtBufferFF(output.extCodingOptionHevc);
    FillExtBufferFF(output.extDumpFiles);
    FillExtBufferFF(output.extHevcTiles);
    FillExtBufferFF(output.extHevcRegion);
    FillExtBufferFF(output.extHevcParam);
    FillExtBufferFF(output.extCodingOption);
    FillExtBufferFF(output.extCodingOption2);

    // save original ExtOpaqueSurfaceAlloc
    expectedOutput.extOpaqueSurfaceAlloc = output.extOpaqueSurfaceAlloc;

    // test normal case
    mfxStatus sts = MFXVideoENCODEH265::Query(nullptr, nullptr, &output.videoParam);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    // check if VideoParam fields are set correctly
    EXPECT_EQ(1, output.videoParam.AsyncDepth);
    EXPECT_EQ(0, output.videoParam.Protected);
    EXPECT_EQ(1, output.videoParam.IOPattern);
    EXPECT_EQ(0, output.videoParam.mfx.LowPower);
    EXPECT_EQ(1, output.videoParam.mfx.BRCParamMultiplier);
    EXPECT_EQ(MFX_CODEC_HEVC, output.videoParam.mfx.CodecId);
    EXPECT_EQ(1, output.videoParam.mfx.CodecProfile);
    EXPECT_EQ(1, output.videoParam.mfx.CodecLevel);
    EXPECT_EQ(1, output.videoParam.mfx.NumThread);
    EXPECT_EQ(1, output.videoParam.mfx.TargetUsage);
    EXPECT_EQ(1, output.videoParam.mfx.GopPicSize);
    EXPECT_EQ(1, output.videoParam.mfx.GopRefDist);
    EXPECT_EQ(1, output.videoParam.mfx.GopOptFlag);
    EXPECT_EQ(1, output.videoParam.mfx.IdrInterval);
    EXPECT_EQ(1, output.videoParam.mfx.RateControlMethod);
    EXPECT_EQ(1, output.videoParam.mfx.InitialDelayInKB);
    EXPECT_EQ(1, output.videoParam.mfx.BufferSizeInKB);
    EXPECT_EQ(1, output.videoParam.mfx.TargetKbps);
    EXPECT_EQ(1, output.videoParam.mfx.MaxKbps);
    EXPECT_EQ(1, output.videoParam.mfx.NumSlice);
    EXPECT_EQ(1, output.videoParam.mfx.NumRefFrame);
    EXPECT_EQ(1, output.videoParam.mfx.EncodedOrder);
    EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.BitDepthLuma);
    EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.BitDepthChroma);
    EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.Shift);
    EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameId.TemporalId);
    EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameId.PriorityId);
    EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameId.DependencyId);
    EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameId.QualityId);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.FourCC);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.Width);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.Height);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.CropX);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.CropY);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.CropW);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.CropH);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.FrameRateExtN);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.FrameRateExtD);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.AspectRatioW);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.AspectRatioH);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.PicStruct);
    EXPECT_EQ(1, output.videoParam.mfx.FrameInfo.ChromaFormat);

    // check if ExtOpaqueSurfaceAlloc is untouched
    EXPECT_EQ(0, MemCompare(expectedOutput.extOpaqueSurfaceAlloc, output.extOpaqueSurfaceAlloc));
    // check if other ExtBuffers headers are untouched
    CheckExtBufHeader(output.extCodingOptionHevc);
    CheckExtBufHeader(output.extDumpFiles);
    CheckExtBufHeader(output.extHevcTiles);
    CheckExtBufHeader(output.extHevcRegion);
    CheckExtBufHeader(output.extHevcParam);
    CheckExtBufHeader(output.extCodingOption);
    CheckExtBufHeader(output.extCodingOption2);
    
    // check if ExtCodingOptionHevc fields are set correctly
    EXPECT_EQ(1, output.extCodingOptionHevc.Log2MaxCUSize);
    EXPECT_EQ(1, output.extCodingOptionHevc.MaxCUDepth);
    EXPECT_EQ(1, output.extCodingOptionHevc.QuadtreeTULog2MaxSize);
    EXPECT_EQ(1, output.extCodingOptionHevc.QuadtreeTULog2MinSize);
    EXPECT_EQ(1, output.extCodingOptionHevc.QuadtreeTUMaxDepthIntra);
    EXPECT_EQ(1, output.extCodingOptionHevc.QuadtreeTUMaxDepthInter);
    EXPECT_EQ(1, output.extCodingOptionHevc.QuadtreeTUMaxDepthInterRD);
    EXPECT_EQ(1, output.extCodingOptionHevc.AnalyzeChroma);
    EXPECT_EQ(1, output.extCodingOptionHevc.SignBitHiding);
    EXPECT_EQ(1, output.extCodingOptionHevc.RDOQuant);
    EXPECT_EQ(1, output.extCodingOptionHevc.SAO);
    EXPECT_EQ(1, output.extCodingOptionHevc.SplitThresholdStrengthCUIntra);
    EXPECT_EQ(1, output.extCodingOptionHevc.SplitThresholdStrengthTUIntra);
    EXPECT_EQ(1, output.extCodingOptionHevc.SplitThresholdStrengthCUInter);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand1_2);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand1_3);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand1_4);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand1_5);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand1_6);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand2_2);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand2_3);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand2_4);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand2_5);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand2_6);
    EXPECT_EQ(1, output.extCodingOptionHevc.WPP);
    EXPECT_EQ(1, output.extCodingOptionHevc.GPB);
    EXPECT_EQ(1, output.extCodingOptionHevc.PartModes);
    EXPECT_EQ(0, output.extCodingOptionHevc.CmIntraThreshold);
    EXPECT_EQ(1, output.extCodingOptionHevc.TUSplitIntra);
    EXPECT_EQ(1, output.extCodingOptionHevc.CUSplit);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraAngModes);
    EXPECT_EQ(0, output.extCodingOptionHevc.EnableCm);
    EXPECT_EQ(1, output.extCodingOptionHevc.BPyramid);
    EXPECT_EQ(1, output.extCodingOptionHevc.FastPUDecision);
    EXPECT_EQ(1, output.extCodingOptionHevc.HadamardMe);
    EXPECT_EQ(1, output.extCodingOptionHevc.TMVP);
    EXPECT_EQ(1, output.extCodingOptionHevc.Deblocking);
    EXPECT_EQ(1, output.extCodingOptionHevc.RDOQuantChroma);
    EXPECT_EQ(1, output.extCodingOptionHevc.RDOQuantCGZ);
    EXPECT_EQ(1, output.extCodingOptionHevc.SaoOpt);
    EXPECT_EQ(1, output.extCodingOptionHevc.SaoSubOpt);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand0_2);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand0_3);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand0_4);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand0_5);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraNumCand0_6);
    EXPECT_EQ(1, output.extCodingOptionHevc.CostChroma);
    EXPECT_EQ(1, output.extCodingOptionHevc.PatternIntPel);
    EXPECT_EQ(1, output.extCodingOptionHevc.FastSkip);
    EXPECT_EQ(1, output.extCodingOptionHevc.PatternSubPel);
    EXPECT_EQ(1, output.extCodingOptionHevc.ForceNumThread);
    EXPECT_EQ(1, output.extCodingOptionHevc.FastCbfMode);
    EXPECT_EQ(1, output.extCodingOptionHevc.PuDecisionSatd);
    EXPECT_EQ(1, output.extCodingOptionHevc.MinCUDepthAdapt);
    EXPECT_EQ(1, output.extCodingOptionHevc.MaxCUDepthAdapt);
    EXPECT_EQ(1, output.extCodingOptionHevc.NumBiRefineIter);
    EXPECT_EQ(1, output.extCodingOptionHevc.CUSplitThreshold);
    EXPECT_EQ(1, output.extCodingOptionHevc.DeltaQpMode);
    EXPECT_EQ(1, output.extCodingOptionHevc.Enable10bit);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraAngModesP);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraAngModesBRef);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraAngModesBnonRef);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraChromaRDO);
    EXPECT_EQ(1, output.extCodingOptionHevc.FastInterp);
    EXPECT_EQ(1, output.extCodingOptionHevc.SplitThresholdTabIndex);
    EXPECT_EQ(1, output.extCodingOptionHevc.CpuFeature);
    EXPECT_EQ(1, output.extCodingOptionHevc.TryIntra);
    EXPECT_EQ(1, output.extCodingOptionHevc.FastAMPSkipME);
    EXPECT_EQ(1, output.extCodingOptionHevc.FastAMPRD);
    EXPECT_EQ(1, output.extCodingOptionHevc.SkipMotionPartition);
    EXPECT_EQ(1, output.extCodingOptionHevc.SkipCandRD);
    EXPECT_EQ(1, output.extCodingOptionHevc.FramesInParallel);
    EXPECT_EQ(1, output.extCodingOptionHevc.AdaptiveRefs);
    EXPECT_EQ(1, output.extCodingOptionHevc.FastCoeffCost);
    EXPECT_EQ(1, output.extCodingOptionHevc.NumRefFrameB);
    EXPECT_EQ(1, output.extCodingOptionHevc.IntraMinDepthSC);
    EXPECT_EQ(1, output.extCodingOptionHevc.InterMinDepthSTC);
    EXPECT_EQ(1, output.extCodingOptionHevc.MotionPartitionDepth);
    EXPECT_EQ(1, output.extCodingOptionHevc.AnalyzeCmplx);
    EXPECT_EQ(1, output.extCodingOptionHevc.RateControlDepth);
    EXPECT_EQ(1, output.extCodingOptionHevc.LowresFactor);
    EXPECT_EQ(1, output.extCodingOptionHevc.DeblockBorders);
    EXPECT_EQ(1, output.extCodingOptionHevc.SAOChroma);
    EXPECT_EQ(1, output.extCodingOptionHevc.RepackProb);
    EXPECT_EQ(1, output.extCodingOptionHevc.NumRefLayers);
    EXPECT_EQ(1, output.extCodingOptionHevc.ConstQpOffset);
    // check if the rest of ExtCodingOptionHevc is zeroed
    EXPECT_EQ(true, IsZero(output.extCodingOptionHevc.reserved));
    EXPECT_EQ(38 * sizeof(mfxU16), sizeof(output.extCodingOptionHevc.reserved)); // if fails here then new fields were added, need to add tests for these fields

    // check if ExtHevcTiles fields are set correctly
    EXPECT_EQ(1, output.extHevcTiles.NumTileRows);
    EXPECT_EQ(1, output.extHevcTiles.NumTileColumns);
    EXPECT_EQ(true, IsZero(output.extHevcTiles.reserved));

    // check if ExtHevcParam fields are set correctly
    EXPECT_EQ(1, output.extHevcParam.PicWidthInLumaSamples);
    EXPECT_EQ(1, output.extHevcParam.PicHeightInLumaSamples);
    EXPECT_EQ(1, output.extHevcParam.GeneralConstraintFlags);
    EXPECT_EQ(true, IsZero(output.extHevcTiles.reserved));

    // check if ExtHevcRegion fields are set correctly
    EXPECT_EQ(1, output.extHevcRegion.RegionId);
    EXPECT_EQ(1, output.extHevcRegion.RegionType);
    EXPECT_EQ(1, output.extHevcRegion.RegionEncoding);
    EXPECT_EQ(true, IsZero(output.extHevcRegion.reserved));

    // check if ExtCodingOption2 fields are set correctly
    EXPECT_EQ(0, output.extCodingOption.RateDistortionOpt);
    EXPECT_EQ(0, output.extCodingOption.MECostType);
    EXPECT_EQ(0, output.extCodingOption.MESearchType);
    EXPECT_EQ(0, output.extCodingOption.MVSearchWindow.x);
    EXPECT_EQ(0, output.extCodingOption.MVSearchWindow.y);
    EXPECT_EQ(0, output.extCodingOption.EndOfSequence);
    EXPECT_EQ(0, output.extCodingOption.FramePicture);
    EXPECT_EQ(0, output.extCodingOption.CAVLC);
    EXPECT_EQ(0, output.extCodingOption.RecoveryPointSEI);
    EXPECT_EQ(0, output.extCodingOption.ViewOutput);
    EXPECT_EQ(0, output.extCodingOption.NalHrdConformance);
    EXPECT_EQ(0, output.extCodingOption.SingleSeiNalUnit);
    EXPECT_EQ(0, output.extCodingOption.VuiVclHrdParameters);
    EXPECT_EQ(0, output.extCodingOption.RefPicListReordering);
    EXPECT_EQ(0, output.extCodingOption.ResetRefList);
    EXPECT_EQ(0, output.extCodingOption.RefPicMarkRep);
    EXPECT_EQ(0, output.extCodingOption.FieldOutput);
    EXPECT_EQ(0, output.extCodingOption.IntraPredBlockSize);
    EXPECT_EQ(0, output.extCodingOption.InterPredBlockSize);
    EXPECT_EQ(0, output.extCodingOption.MVPrecision);
    EXPECT_EQ(0, output.extCodingOption.MaxDecFrameBuffering);
    EXPECT_EQ(1, output.extCodingOption.AUDelimiter);
    EXPECT_EQ(0, output.extCodingOption.EndOfStream);

    // check if ExtCodingOption2 fields are set correctly
    EXPECT_EQ(0, output.extCodingOption2.IntRefType);
    EXPECT_EQ(0, output.extCodingOption2.IntRefCycleSize);
    EXPECT_EQ(0, output.extCodingOption2.IntRefQPDelta);
    EXPECT_EQ(0, output.extCodingOption2.MaxFrameSize);
    EXPECT_EQ(0, output.extCodingOption2.MaxSliceSize);
    EXPECT_EQ(0, output.extCodingOption2.BitrateLimit);
    EXPECT_EQ(0, output.extCodingOption2.MBBRC);
    EXPECT_EQ(0, output.extCodingOption2.ExtBRC);
    EXPECT_EQ(0, output.extCodingOption2.LookAheadDepth);
    EXPECT_EQ(0, output.extCodingOption2.Trellis);
    EXPECT_EQ(0, output.extCodingOption2.RepeatPPS);
    EXPECT_EQ(0, output.extCodingOption2.BRefType);
    EXPECT_EQ(1, output.extCodingOption2.AdaptiveI);
    EXPECT_EQ(0, output.extCodingOption2.AdaptiveB);
    EXPECT_EQ(0, output.extCodingOption2.LookAheadDS);
    EXPECT_EQ(0, output.extCodingOption2.NumMbPerSlice);
    EXPECT_EQ(0, output.extCodingOption2.SkipFrame);
    EXPECT_EQ(0, output.extCodingOption2.MinQPI);
    EXPECT_EQ(0, output.extCodingOption2.MaxQPI);
    EXPECT_EQ(0, output.extCodingOption2.MinQPP);
    EXPECT_EQ(0, output.extCodingOption2.MaxQPP);
    EXPECT_EQ(0, output.extCodingOption2.MinQPB);
    EXPECT_EQ(0, output.extCodingOption2.MaxQPB);
    EXPECT_EQ(0, output.extCodingOption2.FixedFrameRate);
    EXPECT_EQ(0, output.extCodingOption2.DisableDeblockingIdc);
    EXPECT_EQ(1, output.extCodingOption2.DisableVUI);
    EXPECT_EQ(0, output.extCodingOption2.BufferingPeriodSEI);
    EXPECT_EQ(0, output.extCodingOption2.EnableMAD);
    EXPECT_EQ(0, output.extCodingOption2.UseRawRef);
}

TEST_F(QueryTest, Mode2_NullPtr) {
    // test if null ptr is handled properly
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, nullptr));

    // test case when ExtParam==NULL && NumExtParam>0
    mfxExtBuffer **inputExtParamSave = input.videoParam.ExtParam;
    mfxExtBuffer **outputExtParamSave = output.videoParam.ExtParam;
    input.videoParam.ExtParam = nullptr;
    output.videoParam.ExtParam = nullptr;
    EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    input.videoParam.ExtParam = inputExtParamSave;
    output.videoParam.ExtParam = outputExtParamSave;

    // test case when ExtParam[i]==NULL
    input.videoParam.ExtParam[input.videoParam.NumExtParam++] = nullptr;
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    input.videoParam.NumExtParam--;
    output.videoParam.ExtParam[output.videoParam.NumExtParam++] = nullptr;
    EXPECT_EQ(MFX_ERR_NULL_PTR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    output.videoParam.NumExtParam--;

}

TEST_F(QueryTest, Mode2_ExtBuffers) {
    // reverse ExtBuffer pointers to test that Query doesn't rely on the same order of ExtBuffers
    std::reverse(output.videoParam.ExtParam, output.videoParam.ExtParam + output.videoParam.NumExtParam);

    // test if duplicated ExtBuffers are handled properly
    auto duplicate1 = MakeExtBuffer<mfxExtCodingOptionHEVC>();
    input.videoParam.ExtParam[input.videoParam.NumExtParam++] = &duplicate1.Header;
    EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    input.videoParam.NumExtParam--;
    auto duplicate2 = MakeExtBuffer<mfxExtHEVCTiles>();
    output.videoParam.ExtParam[output.videoParam.NumExtParam++] = &duplicate2.Header;
    EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    output.videoParam.NumExtParam--;

    // test if unsupported ExtBuffers are handled properly
    mfxExtBuffer extUnknown = {};
    input.videoParam.ExtParam[input.videoParam.NumExtParam++] = &extUnknown;
    EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    input.videoParam.NumExtParam--;
    output.videoParam.ExtParam[output.videoParam.NumExtParam++] = &extUnknown;
    EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    output.videoParam.NumExtParam--;

    // test if mismatch between input/output ExtBuffers are handled properly
    input.videoParam.NumExtParam--;
    EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    input.videoParam.NumExtParam++;
    output.videoParam.NumExtParam -= 2;
    EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    output.videoParam.NumExtParam += 2;

    // test if it is OK when ExtOpaqueSurfaceAlloc is not in output param
    output.videoParam.NumExtParam--;
    EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    output.videoParam.NumExtParam++;

    // test if bad Header.BufferSz is handled properly
    for (Ipp32u i = 0; i < input.videoParam.NumExtParam; i++) {
        input.videoParam.ExtParam[i]->BufferSz++;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        input.videoParam.ExtParam[i]->BufferSz--;
    }
    for (Ipp32u i = 0; i < output.videoParam.NumExtParam; i++) {
        output.videoParam.ExtParam[i]->BufferSz++;
        EXPECT_EQ(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        output.videoParam.ExtParam[i]->BufferSz--;
    }

    // restore original order of ExtBuffer pointers
    std::reverse(output.videoParam.ExtParam, output.videoParam.ExtParam + output.videoParam.NumExtParam);

    // test that Query doesn't fail with fewer ExtBuffers
    while (input.videoParam.NumExtParam) {
        input.videoParam.NumExtParam--;
        output.videoParam.NumExtParam--;
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    }
}

TEST_F(QueryTest, Mode2_Thoroughness) {
    { SCOPED_TRACE("Test that Query goes thru all valid fields");
        InitParamSetValid(input);
        CopyParamSet(originalInput, input);
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        ExpectEqual(originalInput, input);
        ExpectEqual(input, output);
    }
    { SCOPED_TRACE("Test that Query goes thru all correctable fields before exit");
        InitParamSetCorrectable(input, expectedOutput);
        CopyParamSet(originalInput, input);
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        ExpectEqual(originalInput, input);
        ExpectEqual(expectedOutput, output);
    }
    { SCOPED_TRACE("Test that Query goes thru all unsupported fields before exit");
        InitParamSetUnsupported(input, expectedOutput);
        CopyParamSet(originalInput, input);
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        ExpectEqual(originalInput, input);
        ExpectEqual(expectedOutput, output);
    }
}

TEST_F(QueryTest, Mode2_Single) {
    // test fields one by one
    Zero(input.videoParam.mfx);
    output.videoParam.NumExtParam = 0;
    input.videoParam.NumExtParam = 0;
    input.videoParam.AsyncDepth = 0;
    input.videoParam.IOPattern = 0;
    input.videoParam.Protected = 0;

    { SCOPED_TRACE("Test RateControlMethods");
        const Ipp16u supported[] = {MFX_RATECONTROL_CBR, MFX_RATECONTROL_VBR, MFX_RATECONTROL_AVBR, MFX_RATECONTROL_CQP, MFX_RATECONTROL_LA_EXT};
        TestOneFieldOk(input.videoParam.mfx.RateControlMethod, output.videoParam.mfx.RateControlMethod, supported);
        const Ipp16u unsupported[] = {MFX_RATECONTROL_RESERVED1, MFX_RATECONTROL_RESERVED2, MFX_RATECONTROL_RESERVED3, MFX_RATECONTROL_RESERVED4, MFX_RATECONTROL_LA, MFX_RATECONTROL_ICQ, MFX_RATECONTROL_VCM, MFX_RATECONTROL_LA_ICQ, MFX_RATECONTROL_LA_HRD, MFX_RATECONTROL_QVBR, MFX_RATECONTROL_VME};
        TestOneFieldErr(input.videoParam.mfx.RateControlMethod, output.videoParam.mfx.RateControlMethod, 0, MFX_ERR_UNSUPPORTED, unsupported);
    }
    { SCOPED_TRACE("Test TargetUsage");
        const Ipp16u supported[] = {1, 2, 3, 4, 5, 6, 7};
        TestOneFieldOk(input.videoParam.mfx.TargetUsage, output.videoParam.mfx.TargetUsage, supported);
        const Ipp16u unsupported[] = {8, 100, 0xffff};
        TestOneFieldErr(input.videoParam.mfx.TargetUsage, output.videoParam.mfx.TargetUsage, 7, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test GopRefDist");
        const Ipp16u supported[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        TestOneFieldOk(input.videoParam.mfx.GopRefDist, output.videoParam.mfx.GopRefDist, supported);
        const Ipp16u unsupported[] = {17, 100, 0xffff};
        TestOneFieldErr(input.videoParam.mfx.GopRefDist, output.videoParam.mfx.GopRefDist, 16, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test NumRefFrame");
        const Ipp16u supported[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        TestOneFieldOk(input.videoParam.mfx.NumRefFrame, output.videoParam.mfx.NumRefFrame, supported);
        const Ipp16u unsupported[] = {17, 100, 0xffff};
        TestOneFieldErr(input.videoParam.mfx.NumRefFrame, output.videoParam.mfx.NumRefFrame, 16, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test CodecProfile");
        const Ipp16u supported[] = {MFX_PROFILE_HEVC_MAIN, MFX_PROFILE_HEVC_MAIN10, MFX_PROFILE_HEVC_REXT};
        TestOneFieldOk(input.videoParam.mfx.CodecProfile, output.videoParam.mfx.CodecProfile, supported);
        const Ipp16u unsupported[] = {MFX_PROFILE_HEVC_MAINSP, 5, 6, 100, 0xffff};
        TestOneFieldErr(input.videoParam.mfx.CodecProfile, output.videoParam.mfx.CodecProfile, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test CodecLevel");
        const Ipp16u supported[] = {M10, M20, M21, M30, M31, M40, M41, M50, M51, M52, M60, M61, M62, H40, H41, H50, H51, H52, H60, H61, H62};
        TestOneFieldOk(input.videoParam.mfx.CodecLevel, output.videoParam.mfx.CodecLevel, supported);
        const Ipp16u unsupported[] = {256|10, 256|20, 256|21, 256|30, 256|31, 100, 256, 0xffff};
        TestOneFieldErr(input.videoParam.mfx.CodecLevel, output.videoParam.mfx.CodecLevel, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test GopOptFlag");
        const Ipp16u supported[] = {0, MFX_GOP_CLOSED, MFX_GOP_STRICT, MFX_GOP_CLOSED | MFX_GOP_STRICT};
        TestOneFieldOk(input.videoParam.mfx.GopOptFlag, output.videoParam.mfx.GopOptFlag, supported);
        const Ipp16u unsupported[] = {4, 7, 8, 100, 0xffff};
        TestOneFieldErr(input.videoParam.mfx.GopOptFlag, output.videoParam.mfx.GopOptFlag, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test QPI, QPP, QPB");
        input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        const Ipp16u supported[] = {0, 1, 2, 3, 5, 8, 13, 21, 34, 51};
        TestOneFieldOk(input.videoParam.mfx.QPI, output.videoParam.mfx.QPI, supported);
        TestOneFieldOk(input.videoParam.mfx.QPP, output.videoParam.mfx.QPP, supported);
        TestOneFieldOk(input.videoParam.mfx.QPB, output.videoParam.mfx.QPB, supported);
        const Ipp16u unsupported[] = {52, 100, 0xff, 0xffff};
        TestOneFieldErr(input.videoParam.mfx.QPI, output.videoParam.mfx.QPI, 51, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.videoParam.mfx.QPP, output.videoParam.mfx.QPP, 51, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.videoParam.mfx.QPB, output.videoParam.mfx.QPB, 51, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        input.videoParam.mfx.RateControlMethod = 0;
    }
    { SCOPED_TRACE("Test Accuracy");
        input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
        const Ipp16u supported[] = {1, 100, 0xff, 0xfff, 0xffff};
        TestOneFieldOk(input.videoParam.mfx.Accuracy, output.videoParam.mfx.Accuracy, supported);
        input.videoParam.mfx.RateControlMethod = 0;
    }
    { SCOPED_TRACE("Test Convergence");
        input.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
        const Ipp16u supported[] = {1, 100, 0xff, 0xfff, 0xffff};
        TestOneFieldOk(input.videoParam.mfx.Convergence, output.videoParam.mfx.Convergence, supported);
        input.videoParam.mfx.RateControlMethod = 0;
    }
    { SCOPED_TRACE("Test Width");
        const Ipp16u supported[] = {16, 32, 352, 720, 1280, 1920, 3840, 4096, 8192};
        TestOneFieldOk(input.videoParam.mfx.FrameInfo.Width, output.videoParam.mfx.FrameInfo.Width, supported);
        const Ipp16u unsupported[] = {1, 17, 1900, 4001, 4095, 8191, 8208, 0xfff0};
        TestOneFieldErr(input.videoParam.mfx.FrameInfo.Width, output.videoParam.mfx.FrameInfo.Width, 0, MFX_ERR_UNSUPPORTED, unsupported);
    }
    { SCOPED_TRACE("Test Height");
        const Ipp16u supported[] = {16, 32, 288, 480, 1088, 2160, 2048, 4320};
        TestOneFieldOk(input.videoParam.mfx.FrameInfo.Height, output.videoParam.mfx.FrameInfo.Height, supported);
        const Ipp16u unsupported[] = {1, 17, 1080, 1089, 4319, 4336, 0xfff0};
        TestOneFieldErr(input.videoParam.mfx.FrameInfo.Height, output.videoParam.mfx.FrameInfo.Height, 0, MFX_ERR_UNSUPPORTED, unsupported);
    }
    { SCOPED_TRACE("Test IOPattern");
        enum {SYS=MFX_IOPATTERN_IN_SYSTEM_MEMORY, VID=MFX_IOPATTERN_IN_VIDEO_MEMORY, OPQ=MFX_IOPATTERN_IN_OPAQUE_MEMORY};
        const Ipp16u supported[] = {SYS, VID, OPQ};
        TestOneFieldOk(input.videoParam.IOPattern, output.videoParam.IOPattern, supported);
        const Ipp16u correctable2Sys[] = {SYS|VID, SYS|OPQ, SYS|VID|OPQ, SYS|8, SYS|16, SYS|32, SYS|64, SYS|128};
        TestOneFieldErr(input.videoParam.IOPattern, output.videoParam.IOPattern, SYS, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, correctable2Sys);
        const Ipp16u correctable2Vid[] = {VID|OPQ, VID|8, VID|16, VID|32, VID|64, VID|128};
        TestOneFieldErr(input.videoParam.IOPattern, output.videoParam.IOPattern, VID, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, correctable2Vid);
        const Ipp16u correctable2Opq[] = {OPQ|8, OPQ|16, OPQ|32, OPQ|64, OPQ|128};
        TestOneFieldErr(input.videoParam.IOPattern, output.videoParam.IOPattern, OPQ, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, correctable2Opq);
        const Ipp16u unsupported[] = {8, 16, 32, 64, 128, 0xf0, 0xfff0};
        TestOneFieldErr(input.videoParam.IOPattern, output.videoParam.IOPattern, 0, MFX_ERR_UNSUPPORTED, unsupported);
    }

    // test ExtHevcTiles field by field
    input.extHevcTiles = MakeExtBuffer<mfxExtHEVCTiles>();
    input.videoParam.NumExtParam = 1;
    input.videoParam.ExtParam[0] = &input.extHevcTiles.Header;
    output.videoParam.NumExtParam = 1;
    output.videoParam.ExtParam[0] = &output.extHevcTiles.Header;
    { SCOPED_TRACE("Test NumTileRows");
        const Ipp16u supported[] = {1, 2, 3, 5, 8, 13, 21, 22};
        TestOneFieldOk(input.extHevcTiles.NumTileRows, output.extHevcTiles.NumTileRows, supported);
        const Ipp16u unsupported[] = {23, 24, 0xff, 0xffff};
        TestOneFieldErr(input.extHevcTiles.NumTileRows, output.extHevcTiles.NumTileRows, 22, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test NumTileColumns");
        const Ipp16u supported[] = {1, 2, 3, 5, 8, 13, 19, 20};
        TestOneFieldOk(input.extHevcTiles.NumTileColumns, output.extHevcTiles.NumTileColumns, supported);
        const Ipp16u unsupported[] = {21, 22, 0xff, 0xffff};
        TestOneFieldErr(input.extHevcTiles.NumTileColumns, output.extHevcTiles.NumTileColumns, 20, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }

    // test ExtHevcParam field by field
    input.extHevcParam = MakeExtBuffer<mfxExtHEVCParam>();
    input.videoParam.NumExtParam = 1;
    input.videoParam.ExtParam[0] = &input.extHevcParam.Header;
    output.videoParam.NumExtParam = 1;
    output.videoParam.ExtParam[0] = &output.extHevcParam.Header;

    { SCOPED_TRACE("Test PicWidthInLumaSamples");
        const Ipp16u supported[] = {8, 16, 24, 32, 1920, 3840, 4096, 8192};
        TestOneFieldOk(input.extHevcParam.PicWidthInLumaSamples, output.extHevcParam.PicWidthInLumaSamples, supported);
        const Ipp16u unsupported[] = {1, 4, 12, 20, 28, 4095, 8191, 8208, 0xfff0};
        TestOneFieldErr(input.extHevcParam.PicWidthInLumaSamples, output.extHevcParam.PicWidthInLumaSamples, 0, MFX_ERR_UNSUPPORTED, unsupported);
    }
    { SCOPED_TRACE("Test PicHeightInLumaSamples");
        const Ipp16u supported[] = {16, 32, 288, 480, 1080, 2160, 2048, 4320};
        TestOneFieldOk(input.extHevcParam.PicHeightInLumaSamples, output.extHevcParam.PicHeightInLumaSamples, supported);
        const Ipp16u unsupported[] = {1, 4, 12, 20, 28, 1081, 4319, 4336, 0xfff0};
        TestOneFieldErr(input.extHevcParam.PicHeightInLumaSamples, output.extHevcParam.PicHeightInLumaSamples, 0, MFX_ERR_UNSUPPORTED, unsupported);
    }
    { SCOPED_TRACE("Test GeneralConstraintFlags");
        const Ipp64u supported[] = {0, MAIN_422_10};
        TestOneFieldOk(input.extHevcParam.GeneralConstraintFlags, output.extHevcParam.GeneralConstraintFlags, supported);
        for (Ipp64u i = 1; i < (1 << 9) - 1; i++) {
            if (i != MAIN_422_10) {
                const Ipp64u unsupported[] = { i, i<<9, i<<18, i<<27 };
                TestOneFieldErr(input.extHevcParam.GeneralConstraintFlags, output.extHevcParam.GeneralConstraintFlags, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
            }
        }
    }

    // test ExtCodingOptionHevc field by field
    input.extCodingOptionHevc = MakeExtBuffer<mfxExtCodingOptionHEVC>();
    input.videoParam.NumExtParam = 1;
    input.videoParam.ExtParam[0] = &input.extCodingOptionHevc.Header;
    output.videoParam.NumExtParam = 1;
    output.videoParam.ExtParam[0] = &output.extCodingOptionHevc.Header;

    { SCOPED_TRACE("Test Log2MaxCUSize");
        const Ipp16u supported[] = {5, 6};
        TestOneFieldOk(input.extCodingOptionHevc.Log2MaxCUSize, output.extCodingOptionHevc.Log2MaxCUSize, supported);
        const Ipp16u unsupported[] = {1, 2, 3, 4, 7, 8, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.Log2MaxCUSize, output.extCodingOptionHevc.Log2MaxCUSize, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test MaxCUDepth");
        const Ipp16u supported[] = {1, 2, 3, 4};
        TestOneFieldOk(input.extCodingOptionHevc.MaxCUDepth, output.extCodingOptionHevc.MaxCUDepth, supported);
        const Ipp16u unsupported[] = {5, 6, 7, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.MaxCUDepth, output.extCodingOptionHevc.MaxCUDepth, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test QuadtreeTULog2MaxSize, QuadtreeTULog2MinSize");
        const Ipp16u supported[] = {2, 3, 4, 5};
        TestOneFieldOk(input.extCodingOptionHevc.QuadtreeTULog2MaxSize, output.extCodingOptionHevc.QuadtreeTULog2MaxSize, supported);
        TestOneFieldOk(input.extCodingOptionHevc.QuadtreeTULog2MinSize, output.extCodingOptionHevc.QuadtreeTULog2MinSize, supported);
        const Ipp16u unsupported[] = {1, 6, 7, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.QuadtreeTULog2MaxSize, output.extCodingOptionHevc.QuadtreeTULog2MaxSize, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.QuadtreeTULog2MinSize, output.extCodingOptionHevc.QuadtreeTULog2MinSize, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test QuadtreeTUMaxDepthIntra, QuadtreeTUMaxDepthInter, QuadtreeTUMaxDepthInterRD");
        const Ipp16u supported[] = {1, 2, 3, 4};
        TestOneFieldOk(input.extCodingOptionHevc.QuadtreeTUMaxDepthIntra, output.extCodingOptionHevc.QuadtreeTUMaxDepthIntra, supported);
        TestOneFieldOk(input.extCodingOptionHevc.QuadtreeTUMaxDepthInter, output.extCodingOptionHevc.QuadtreeTUMaxDepthInter, supported);
        TestOneFieldOk(input.extCodingOptionHevc.QuadtreeTUMaxDepthInterRD, output.extCodingOptionHevc.QuadtreeTUMaxDepthInterRD, supported);
        const Ipp16u unsupported[] = {5, 6, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.QuadtreeTUMaxDepthIntra, output.extCodingOptionHevc.QuadtreeTUMaxDepthIntra, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.QuadtreeTUMaxDepthInter, output.extCodingOptionHevc.QuadtreeTUMaxDepthInter, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.QuadtreeTUMaxDepthInterRD, output.extCodingOptionHevc.QuadtreeTUMaxDepthInterRD, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test EnableCm");
        const Ipp16u supported[] = {MFX_CODINGOPTION_OFF};
        TestOneFieldOk(input.extCodingOptionHevc.EnableCm, output.extCodingOptionHevc.EnableCm, supported);
        const Ipp16u unsupported[] = {1, 2, MFX_CODINGOPTION_ON, MFX_CODINGOPTION_ON|MFX_CODINGOPTION_OFF, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.EnableCm, output.extCodingOptionHevc.EnableCm, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test all tri-states");
        const Ipp16u supported[] = {MFX_CODINGOPTION_ON, MFX_CODINGOPTION_OFF};
        TestOneFieldOk(input.extCodingOptionHevc.AnalyzeChroma, output.extCodingOptionHevc.AnalyzeChroma, supported);
        TestOneFieldOk(input.extCodingOptionHevc.SignBitHiding, output.extCodingOptionHevc.SignBitHiding, supported);
        TestOneFieldOk(input.extCodingOptionHevc.RDOQuant, output.extCodingOptionHevc.RDOQuant, supported);
        TestOneFieldOk(input.extCodingOptionHevc.SAO, output.extCodingOptionHevc.SAO, supported);
        TestOneFieldOk(input.extCodingOptionHevc.WPP, output.extCodingOptionHevc.WPP, supported);
        TestOneFieldOk(input.extCodingOptionHevc.GPB, output.extCodingOptionHevc.GPB, supported);
        TestOneFieldOk(input.extCodingOptionHevc.BPyramid, output.extCodingOptionHevc.BPyramid, supported);
        TestOneFieldOk(input.extCodingOptionHevc.FastPUDecision, output.extCodingOptionHevc.FastPUDecision, supported);
        TestOneFieldOk(input.extCodingOptionHevc.TMVP, output.extCodingOptionHevc.TMVP, supported);
        TestOneFieldOk(input.extCodingOptionHevc.Deblocking, output.extCodingOptionHevc.Deblocking, supported);
        TestOneFieldOk(input.extCodingOptionHevc.RDOQuantChroma, output.extCodingOptionHevc.RDOQuantChroma, supported);
        TestOneFieldOk(input.extCodingOptionHevc.RDOQuantCGZ, output.extCodingOptionHevc.RDOQuantCGZ, supported);
        TestOneFieldOk(input.extCodingOptionHevc.CostChroma, output.extCodingOptionHevc.CostChroma, supported);
        TestOneFieldOk(input.extCodingOptionHevc.FastSkip, output.extCodingOptionHevc.FastSkip, supported);
        TestOneFieldOk(input.extCodingOptionHevc.FastCbfMode, output.extCodingOptionHevc.FastCbfMode, supported);
        TestOneFieldOk(input.extCodingOptionHevc.PuDecisionSatd, output.extCodingOptionHevc.PuDecisionSatd, supported);
        TestOneFieldOk(input.extCodingOptionHevc.MinCUDepthAdapt, output.extCodingOptionHevc.MinCUDepthAdapt, supported);
        TestOneFieldOk(input.extCodingOptionHevc.MaxCUDepthAdapt, output.extCodingOptionHevc.MaxCUDepthAdapt, supported);
        TestOneFieldOk(input.extCodingOptionHevc.Enable10bit, output.extCodingOptionHevc.Enable10bit, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraChromaRDO, output.extCodingOptionHevc.IntraChromaRDO, supported);
        TestOneFieldOk(input.extCodingOptionHevc.FastInterp, output.extCodingOptionHevc.FastInterp, supported);
        TestOneFieldOk(input.extCodingOptionHevc.SkipCandRD, output.extCodingOptionHevc.SkipCandRD, supported);
        TestOneFieldOk(input.extCodingOptionHevc.AdaptiveRefs, output.extCodingOptionHevc.AdaptiveRefs, supported);
        TestOneFieldOk(input.extCodingOptionHevc.FastCoeffCost, output.extCodingOptionHevc.FastCoeffCost, supported);
        TestOneFieldOk(input.extCodingOptionHevc.DeblockBorders, output.extCodingOptionHevc.DeblockBorders, supported);
        TestOneFieldOk(input.extCodingOptionHevc.SAOChroma, output.extCodingOptionHevc.SAOChroma, supported);
        const Ipp16u unsupported[] = {1, 2, MFX_CODINGOPTION_ON|MFX_CODINGOPTION_OFF, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.AnalyzeChroma, output.extCodingOptionHevc.AnalyzeChroma, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.SignBitHiding, output.extCodingOptionHevc.SignBitHiding, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.RDOQuant, output.extCodingOptionHevc.RDOQuant, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.SAO, output.extCodingOptionHevc.SAO, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.WPP, output.extCodingOptionHevc.WPP, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.GPB, output.extCodingOptionHevc.GPB, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.BPyramid, output.extCodingOptionHevc.BPyramid, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.FastPUDecision, output.extCodingOptionHevc.FastPUDecision, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.TMVP, output.extCodingOptionHevc.TMVP, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.Deblocking, output.extCodingOptionHevc.Deblocking, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.RDOQuantChroma, output.extCodingOptionHevc.RDOQuantChroma, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.RDOQuantCGZ, output.extCodingOptionHevc.RDOQuantCGZ, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.CostChroma, output.extCodingOptionHevc.CostChroma, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.FastSkip, output.extCodingOptionHevc.FastSkip, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.FastCbfMode, output.extCodingOptionHevc.FastCbfMode, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.PuDecisionSatd, output.extCodingOptionHevc.PuDecisionSatd, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.MinCUDepthAdapt, output.extCodingOptionHevc.MinCUDepthAdapt, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.MaxCUDepthAdapt, output.extCodingOptionHevc.MaxCUDepthAdapt, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.Enable10bit, output.extCodingOptionHevc.Enable10bit, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraChromaRDO, output.extCodingOptionHevc.IntraChromaRDO, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.FastInterp, output.extCodingOptionHevc.FastInterp, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.SkipCandRD, output.extCodingOptionHevc.SkipCandRD, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.AdaptiveRefs, output.extCodingOptionHevc.AdaptiveRefs, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.FastCoeffCost, output.extCodingOptionHevc.FastCoeffCost, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.DeblockBorders, output.extCodingOptionHevc.DeblockBorders, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.SAOChroma, output.extCodingOptionHevc.SAOChroma, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test SplitThresholdStrengthCUIntra, SplitThresholdStrengthTUIntra, SplitThresholdStrengthCUInter");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.SplitThresholdStrengthCUIntra, output.extCodingOptionHevc.SplitThresholdStrengthCUIntra, supported);
        TestOneFieldOk(input.extCodingOptionHevc.SplitThresholdStrengthTUIntra, output.extCodingOptionHevc.SplitThresholdStrengthTUIntra, supported);
        TestOneFieldOk(input.extCodingOptionHevc.SplitThresholdStrengthCUInter, output.extCodingOptionHevc.SplitThresholdStrengthCUInter, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.SplitThresholdStrengthCUIntra, output.extCodingOptionHevc.QuadtreeTUMaxDepthInterRD, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.SplitThresholdStrengthCUInter, output.extCodingOptionHevc.SplitThresholdStrengthCUInter, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.SplitThresholdStrengthTUIntra, output.extCodingOptionHevc.SplitThresholdStrengthTUIntra, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test all IntraNumCandX_Y");
        const Ipp16u supported[] = {1, 2, 3, 5, 8, 13, 21, 35};
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand0_2, output.extCodingOptionHevc.IntraNumCand0_2, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand0_3, output.extCodingOptionHevc.IntraNumCand0_3, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand0_4, output.extCodingOptionHevc.IntraNumCand0_4, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand0_5, output.extCodingOptionHevc.IntraNumCand0_5, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand0_6, output.extCodingOptionHevc.IntraNumCand0_6, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand1_2, output.extCodingOptionHevc.IntraNumCand1_2, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand1_3, output.extCodingOptionHevc.IntraNumCand1_3, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand1_4, output.extCodingOptionHevc.IntraNumCand1_4, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand1_5, output.extCodingOptionHevc.IntraNumCand1_5, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand1_6, output.extCodingOptionHevc.IntraNumCand1_6, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand2_2, output.extCodingOptionHevc.IntraNumCand2_2, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand2_3, output.extCodingOptionHevc.IntraNumCand2_3, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand2_4, output.extCodingOptionHevc.IntraNumCand2_4, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand2_5, output.extCodingOptionHevc.IntraNumCand2_5, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraNumCand2_6, output.extCodingOptionHevc.IntraNumCand2_6, supported);
        const Ipp16u unsupported[] = {36, 37, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand0_2, output.extCodingOptionHevc.IntraNumCand0_2, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand0_3, output.extCodingOptionHevc.IntraNumCand0_3, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand0_4, output.extCodingOptionHevc.IntraNumCand0_4, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand0_5, output.extCodingOptionHevc.IntraNumCand0_5, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand0_6, output.extCodingOptionHevc.IntraNumCand0_6, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand1_2, output.extCodingOptionHevc.IntraNumCand1_2, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand1_3, output.extCodingOptionHevc.IntraNumCand1_3, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand1_4, output.extCodingOptionHevc.IntraNumCand1_4, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand1_5, output.extCodingOptionHevc.IntraNumCand1_5, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand1_6, output.extCodingOptionHevc.IntraNumCand1_6, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand2_2, output.extCodingOptionHevc.IntraNumCand2_2, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand2_3, output.extCodingOptionHevc.IntraNumCand2_3, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand2_4, output.extCodingOptionHevc.IntraNumCand2_4, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand2_5, output.extCodingOptionHevc.IntraNumCand2_5, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraNumCand2_6, output.extCodingOptionHevc.IntraNumCand2_6, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }

    { SCOPED_TRACE("Test PartModes");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.PartModes, output.extCodingOptionHevc.PartModes, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.PartModes, output.extCodingOptionHevc.PartModes, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test TUSplitIntra");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.TUSplitIntra, output.extCodingOptionHevc.TUSplitIntra, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.TUSplitIntra, output.extCodingOptionHevc.TUSplitIntra, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test CUSplit");
        const Ipp16u supported[] = {1, 2};
        TestOneFieldOk(input.extCodingOptionHevc.CUSplit, output.extCodingOptionHevc.CUSplit, supported);
        const Ipp16u unsupported[] = {3, 4, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.CUSplit, output.extCodingOptionHevc.CUSplit, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test IntraAngModes");
        const Ipp16u supported[] = {1, 2, 3, 99};
        TestOneFieldOk(input.extCodingOptionHevc.IntraAngModes, output.extCodingOptionHevc.IntraAngModes, supported);
        const Ipp16u unsupported[] = {4, 5, 98, 100, 101, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.IntraAngModes, output.extCodingOptionHevc.IntraAngModes, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test IntraAngModesP, IntraAngModesBRef, IntraAngModesBnonRef");
        const Ipp16u supported[] = {1, 2, 3, 99, 100};
        TestOneFieldOk(input.extCodingOptionHevc.IntraAngModesP, output.extCodingOptionHevc.IntraAngModesP, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraAngModesBRef, output.extCodingOptionHevc.IntraAngModesBRef, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraAngModesBnonRef, output.extCodingOptionHevc.IntraAngModesBnonRef, supported);
        const Ipp16u unsupported[] = {4, 5, 98, 101, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.IntraAngModesP, output.extCodingOptionHevc.IntraAngModesP, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraAngModesBRef, output.extCodingOptionHevc.IntraAngModesBRef, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
        TestOneFieldErr(input.extCodingOptionHevc.IntraAngModesBnonRef, output.extCodingOptionHevc.IntraAngModesBnonRef, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test HadamardMe");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.HadamardMe, output.extCodingOptionHevc.HadamardMe, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.HadamardMe, output.extCodingOptionHevc.HadamardMe, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test SaoOpt");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.SaoOpt, output.extCodingOptionHevc.SaoOpt, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.SaoOpt, output.extCodingOptionHevc.SaoOpt, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test SaoSubOpt");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.SaoSubOpt, output.extCodingOptionHevc.SaoSubOpt, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.SaoSubOpt, output.extCodingOptionHevc.SaoSubOpt, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test PatternIntPel");
        const Ipp16u supported[] = {1, 100};
        TestOneFieldOk(input.extCodingOptionHevc.PatternIntPel, output.extCodingOptionHevc.PatternIntPel, supported);
        const Ipp16u unsupported[] = {2, 3, 99, 101, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.PatternIntPel, output.extCodingOptionHevc.PatternIntPel, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test PatternSubPel");
        const Ipp16u supported[] = {1, 2, 3, 4, 5, 6};
        TestOneFieldOk(input.extCodingOptionHevc.PatternSubPel, output.extCodingOptionHevc.PatternSubPel, supported);
        const Ipp16u unsupported[] = {7, 8, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.PatternSubPel, output.extCodingOptionHevc.PatternSubPel, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test DeltaQpMode");
        const Ipp16u supported[] = {1, 2, 3, 4, 5, 6, 7, 8};
        TestOneFieldOk(input.extCodingOptionHevc.DeltaQpMode, output.extCodingOptionHevc.DeltaQpMode, supported);
        const Ipp16u unsupported[] = {9, 10, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.DeltaQpMode, output.extCodingOptionHevc.DeltaQpMode, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test SplitThresholdTabIndex");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.SplitThresholdTabIndex, output.extCodingOptionHevc.SplitThresholdTabIndex, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.SplitThresholdTabIndex, output.extCodingOptionHevc.SplitThresholdTabIndex, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test CpuFeature");
        const Ipp16u supported[] = {1, 2, 3, 4, 5};
        TestOneFieldOk(input.extCodingOptionHevc.CpuFeature, output.extCodingOptionHevc.CpuFeature, supported);
        const Ipp16u unsupported[] = {6, 7, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.CpuFeature, output.extCodingOptionHevc.CpuFeature, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test TryIntra");
        const Ipp16u supported[] = {1, 2};
        TestOneFieldOk(input.extCodingOptionHevc.TryIntra, output.extCodingOptionHevc.TryIntra, supported);
        const Ipp16u unsupported[] = {3, 4, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.TryIntra, output.extCodingOptionHevc.TryIntra, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test FastAMPSkipME");
        const Ipp16u supported[] = {1, 2};
        TestOneFieldOk(input.extCodingOptionHevc.FastAMPSkipME, output.extCodingOptionHevc.FastAMPSkipME, supported);
        const Ipp16u unsupported[] = {3, 4, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.FastAMPSkipME, output.extCodingOptionHevc.FastAMPSkipME, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test FastAMPRD");
        const Ipp16u supported[] = {1, 2};
        TestOneFieldOk(input.extCodingOptionHevc.FastAMPRD, output.extCodingOptionHevc.FastAMPRD, supported);
        const Ipp16u unsupported[] = {3, 4, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.FastAMPRD, output.extCodingOptionHevc.FastAMPRD, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test SkipMotionPartition");
        const Ipp16u supported[] = {1, 2};
        TestOneFieldOk(input.extCodingOptionHevc.SkipMotionPartition, output.extCodingOptionHevc.SkipMotionPartition, supported);
        const Ipp16u unsupported[] = {3, 4, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.SkipMotionPartition, output.extCodingOptionHevc.SkipMotionPartition, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test NumRefFrameB");
        const Ipp16u supported[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        TestOneFieldOk(input.extCodingOptionHevc.NumRefFrameB, output.extCodingOptionHevc.NumRefFrameB, supported);
        const Ipp16u unsupported[] = {17, 100, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.NumRefFrameB, output.extCodingOptionHevc.NumRefFrameB, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test AnalyzeCmplx");
        const Ipp16u supported[] = {1, 2};
        TestOneFieldOk(input.extCodingOptionHevc.AnalyzeCmplx, output.extCodingOptionHevc.AnalyzeCmplx, supported);
        const Ipp16u unsupported[] = {3, 4, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.AnalyzeCmplx, output.extCodingOptionHevc.AnalyzeCmplx, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test LowresFactor");
        const Ipp16u supported[] = {1, 2, 3};
        TestOneFieldOk(input.extCodingOptionHevc.LowresFactor, output.extCodingOptionHevc.LowresFactor, supported);
        const Ipp16u unsupported[] = {4, 5, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.LowresFactor, output.extCodingOptionHevc.LowresFactor, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test NumRefLayers");
        const Ipp16u supported[] = {1, 2, 3, 4};
        TestOneFieldOk(input.extCodingOptionHevc.NumRefLayers, output.extCodingOptionHevc.NumRefLayers, supported);
        const Ipp16u unsupported[] = {5, 6, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOptionHevc.NumRefLayers, output.extCodingOptionHevc.NumRefLayers, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }
    { SCOPED_TRACE("Test all fields w/o limits");
        const Ipp16u supported[] = {0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368, 0xffff};
		TestOneFieldOk(input.extCodingOptionHevc.CmIntraThreshold, output.extCodingOptionHevc.CmIntraThreshold, supported);
		TestOneFieldOk(input.extCodingOptionHevc.ForceNumThread, output.extCodingOptionHevc.ForceNumThread, supported);
		TestOneFieldOk(input.extCodingOptionHevc.NumBiRefineIter, output.extCodingOptionHevc.NumBiRefineIter, supported);
		TestOneFieldOk(input.extCodingOptionHevc.CUSplitThreshold, output.extCodingOptionHevc.CUSplitThreshold, supported);
		TestOneFieldOk(input.extCodingOptionHevc.FramesInParallel, output.extCodingOptionHevc.FramesInParallel, supported);
		TestOneFieldOk(input.extCodingOptionHevc.RateControlDepth, output.extCodingOptionHevc.RateControlDepth, supported);
		TestOneFieldOk(input.extCodingOptionHevc.IntraMinDepthSC, output.extCodingOptionHevc.IntraMinDepthSC, supported);
        TestOneFieldOk(input.extCodingOptionHevc.MotionPartitionDepth, output.extCodingOptionHevc.MotionPartitionDepth, supported);
        TestOneFieldOk(input.extCodingOptionHevc.InterMinDepthSTC, output.extCodingOptionHevc.InterMinDepthSTC, supported);
        TestOneFieldOk(input.extCodingOptionHevc.IntraMinDepthSC, output.extCodingOptionHevc.IntraMinDepthSC, supported);
		TestOneFieldOk(input.extCodingOptionHevc.RepackProb, output.extCodingOptionHevc.RepackProb, supported);
    }

    // test ExtCodingOption field by field
    input.extCodingOption = MakeExtBuffer<mfxExtCodingOption>();
    input.videoParam.NumExtParam = 1;
    input.videoParam.ExtParam[0] = &input.extCodingOption.Header;
    output.videoParam.NumExtParam = 1;
    output.videoParam.ExtParam[0] = &output.extCodingOption.Header;

    { SCOPED_TRACE("Test AUDelimiter");
        const Ipp16u supported[] = {MFX_CODINGOPTION_ON, MFX_CODINGOPTION_OFF};
        TestOneFieldOk(input.extCodingOption.AUDelimiter, output.extCodingOption.AUDelimiter, supported);
        const Ipp16u unsupported[] = {1, 2, MFX_CODINGOPTION_ON|MFX_CODINGOPTION_OFF, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOption.AUDelimiter, output.extCodingOption.AUDelimiter, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }

    // test ExtCodingOption2 field by field
    input.extCodingOption2 = MakeExtBuffer<mfxExtCodingOption2>();
    input.videoParam.NumExtParam = 1;
    input.videoParam.ExtParam[0] = &input.extCodingOption2.Header;
    output.videoParam.NumExtParam = 1;
    output.videoParam.ExtParam[0] = &output.extCodingOption2.Header;

    { SCOPED_TRACE("Test DisableVUI");
        const Ipp16u supported[] = {MFX_CODINGOPTION_ON, MFX_CODINGOPTION_OFF};
        TestOneFieldOk(input.extCodingOption2.DisableVUI, output.extCodingOption2.DisableVUI, supported);
        const Ipp16u unsupported[] = {1, 2, MFX_CODINGOPTION_ON|MFX_CODINGOPTION_OFF, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOption2.DisableVUI, output.extCodingOption2.DisableVUI, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }

    { SCOPED_TRACE("Test AdaptiveI");
        const Ipp16u supported[] = {MFX_CODINGOPTION_ON, MFX_CODINGOPTION_OFF};
        TestOneFieldOk(input.extCodingOption2.AdaptiveI, output.extCodingOption2.AdaptiveI, supported);
        const Ipp16u unsupported[] = {1, 2, MFX_CODINGOPTION_ON|MFX_CODINGOPTION_OFF, 0xff, 0xffff};
        TestOneFieldErr(input.extCodingOption2.AdaptiveI, output.extCodingOption2.AdaptiveI, 0, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, unsupported);
    }

    // test ExtDumpFiles
    input.extDumpFiles = MakeExtBuffer<mfxExtDumpFiles>();
    input.videoParam.NumExtParam = 1;
    input.videoParam.ExtParam[0] = &input.extDumpFiles.Header;
    output.videoParam.NumExtParam = 1;
    output.videoParam.ExtParam[0] = &output.extDumpFiles.Header;

    vm_char pattern[] = VM_STRING("file_name-to-store_reconstructed frames");
    // short file name
    vm_char name1[] = VM_STRING("dssdhgjkl");
    // longest possible file name
    vm_char name2[260] = {};
    for (Ipp32s i = 0; i < 259; i++)
        name2[i] = pattern[i % vm_string_strlen(pattern)];
    // file name longer than possible
    vm_char name3[260] = {};
    for (Ipp32s i = 0; i < 260; i++)
        name3[i] = pattern[i % vm_string_strlen(pattern)];
    // file name with /0 in the middle
    vm_char name4[260] = {};
    memcpy(name4, pattern, sizeof(pattern));
    memcpy(name4 + vm_string_strlen(pattern) + 1, pattern, sizeof(pattern));

    memcpy(input.extDumpFiles.ReconFilename, name1, sizeof(name1));
    memcpy(input.extDumpFiles.InputFramesFilename, name1, sizeof(name1));
    EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(0, memcmp(input.extDumpFiles.ReconFilename, name1, sizeof(name1)));
    EXPECT_EQ(0, memcmp(output.extDumpFiles.ReconFilename, name1, sizeof(name1)));
    EXPECT_EQ(0, memcmp(input.extDumpFiles.InputFramesFilename, name1, sizeof(name1)));
    EXPECT_EQ(0, memcmp(output.extDumpFiles.InputFramesFilename, name1, sizeof(name1)));

    memcpy(input.extDumpFiles.ReconFilename, name2, sizeof(name2));
    memcpy(input.extDumpFiles.InputFramesFilename, name2, sizeof(name2));
    EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(0, memcmp(input.extDumpFiles.ReconFilename, name2, sizeof(name2)));
    EXPECT_EQ(0, memcmp(output.extDumpFiles.ReconFilename, name2, sizeof(name2)));
    EXPECT_EQ(0, memcmp(input.extDumpFiles.InputFramesFilename, name2, sizeof(name2)));
    EXPECT_EQ(0, memcmp(output.extDumpFiles.InputFramesFilename, name2, sizeof(name2)));

    memcpy(input.extDumpFiles.ReconFilename, name3, sizeof(name3));
    memcpy(input.extDumpFiles.InputFramesFilename, name3, sizeof(name3));
    EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(0, memcmp(input.extDumpFiles.ReconFilename, name3, sizeof(name3)));
    EXPECT_EQ(0, output.extDumpFiles.ReconFilename[0]); // output name should be trunkated to null-string
    EXPECT_EQ(0, memcmp(input.extDumpFiles.InputFramesFilename, name3, sizeof(name3)));
    EXPECT_EQ(0, output.extDumpFiles.InputFramesFilename[0]); // output name should be trunkated to null-string

    memcpy(input.extDumpFiles.ReconFilename, name4, sizeof(name4));
    memcpy(input.extDumpFiles.InputFramesFilename, name4, sizeof(name4));
    EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(0, memcmp(input.extDumpFiles.ReconFilename, name4, sizeof(name4)));
    EXPECT_EQ(0, memcmp(output.extDumpFiles.ReconFilename, pattern, sizeof(pattern)));
    EXPECT_EQ(0, memcmp(input.extDumpFiles.InputFramesFilename, name4, sizeof(name4)));
    EXPECT_EQ(0, memcmp(output.extDumpFiles.InputFramesFilename, pattern, sizeof(pattern)));
}

TEST_F(QueryTest, Conflicts_framerate_too_high) {
    Ipp32u ok[][2] = { {0xffffffff, 0xda740e}, {0xffffff3c, 0xda740d}, {300, 1}, {1, 1}, {1, 0xffffffff}, {30, 1}, {30000, 1001}, {60000, 1001}, {120000, 1000} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[0];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.FrameRateExtD);
    }
    Ipp32u error[][2] = { {1, 0}, {0, 1}, {30, 0}, {0xffffffff, 0xda740d}, {301, 1}, {300001, 1000} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[0];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[1];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameRateExtD);
    }
}

TEST_F(QueryTest, Conflicts_mismatching_FourCC_and_ChromaFormat) {
    Ipp32u ok[][2] = { {NV12, YUV420}, {P010, YUV420}, {NV16, YUV422}, {P210, YUV422} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
    }
    Ipp32u error[][2] = { {0, YUV420}, {0, YUV422}, {NV12, 0}, {NV16, 0}, {P010, 0}, {P210, 0}, {NV12, YUV422}, {P010, YUV422}, {NV16, YUV420}, {P210, YUV420} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.ChromaFormat);
    }
}

TEST_F(QueryTest, Conflicts_PicWidthInLumaSamples_gt_Width) {
    Ipp32u ok[][2] = { {16, 8}, {1920, 1912}, {1920, 1920}, {3840, 3832}, {3840, 3840}, {720, 712} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], output.extHevcParam.PicWidthInLumaSamples);
    }
    Ipp32u error[][2] = { {16, 24}, {1920, 1928}, {3824, 3832}, {720, 728} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(0, output.extHevcParam.PicWidthInLumaSamples);
    }
}

TEST_F(QueryTest, Conflicts_PicHeightInLumaSamples_gt_Height) {
    Ipp32u ok[][2] = { {16, 8}, {1088, 1080}, {1088, 1088}, {2160, 2152}, {2160, 2160}, {480, 472} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
    }
    Ipp32u error[][2] = { {16, 24}, {1072, 1080}, {2160, 2168}, {352, 360} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(0, output.extHevcParam.PicHeightInLumaSamples);
    }
}

TEST_F(QueryTest, Conflicts_misaligned_CropX) {
    Ipp32u ok[][3] = { {NV12, YUV420, 2}, {NV16, YUV422, 100}, {P010, YUV420, 8}, {P210, YUV422, 256} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropX = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropX);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropX);
    }
    Ipp32u warning[][3] = { {NV12, YUV420, 1}, {NV16, YUV422, 99}, {P010, YUV420, 15}, {P210, YUV422, 5} };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropX = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropX);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2] - 1, output.videoParam.mfx.FrameInfo.CropX);
    }
}

TEST_F(QueryTest, Conflicts_misaligned_CropW) {
    Ipp32u ok[][3] = { {NV12, YUV420, 2}, {NV16, YUV422, 100}, {P010, YUV420, 8}, {P210, YUV422, 256} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropW = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropW);
    }
    Ipp32u warning[][3] = { {NV12, YUV420, 1}, {NV16, YUV422, 99}, {P010, YUV420, 15}, {P210, YUV422, 5} };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropW = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2] + 1, output.videoParam.mfx.FrameInfo.CropW);
    }
}

TEST_F(QueryTest, Conflicts_CropX_ge_picWidth) {
    Ipp32u ok[][3] = { {1920, 1912, 2}, {1920, 0, 2}, {0, 1912, 2}, {3840, 3832, 3830}, {3840, 0, 3830}, {0, 3832, 3830} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropX = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropX);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropX);
    }
    Ipp32u error[][3] = { {720, 712, 722}, {720, 0, 722}, {0, 712, 722}, {720, 712, 715}, {0, 712, 715}, {720, 720, 720}, {720, 712, 712} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropX = p[2];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropX);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.CropX);
    }
}

TEST_F(QueryTest, Conflicts_CropW_gt_picWidth) {
    Ipp32u ok[][3] = { {1920, 1912, 2}, {1920, 0, 2}, {0, 1912, 2}, {3840, 3832, 3830}, {3840, 0, 3830}, {0, 3832, 3830}, {720, 720, 720}, {720, 712, 712} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropW = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropW);
    }
    Ipp32u error[][3] = { {1280, 1280, 1722}, {0, 1280, 1722}, {1280, 0, 1722}, {352, 344, 346}, {0, 344, 346}  };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropW = p[2];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.CropW);
    }
}

TEST_F(QueryTest, Conflicts_CropX_plus_CropW_gt_picWidth) {
    Ipp32u ok[][4] = { {1920, 1912, 1912, 0}, {0, 1912, 1912, 0}, {1920, 0, 1912, 0}, {3840, 3840, 3838, 2}, {0, 3840, 3838, 2}, {3840, 0, 3838, 2}, {720, 712, 2, 710}, {0, 712, 2, 710}, {720, 0, 2, 710} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropW = p[2];
        input.videoParam.mfx.FrameInfo.CropX = p[3];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.CropX);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.CropX);
    }
    Ipp32u error[][4] = { {1280, 1280, 1280, 124}, {0, 1280, 1280, 124}, {1280, 0, 1280, 124}, {352, 344, 200, 146}, {0, 344, 200, 146} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Width = p[0];
        input.extHevcParam.PicWidthInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropW = p[2];
        input.videoParam.mfx.FrameInfo.CropX = p[3];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.CropX);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[1], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropW);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.CropX);
    }
}
 
TEST_F(QueryTest, Conflicts_misaligned_CropY) {
    Ipp32u ok[][3] = {
        {NV12, YUV420, 2}, {NV12, YUV420, 22}, {NV16, YUV422, 1}, {NV16, YUV422, 2}, {NV16, YUV422, 50}, {NV16, YUV422, 51},
        {P010, YUV420, 8}, {P010, YUV420, 28}, {P210, YUV422, 1}, {P210, YUV422, 2}, {P210, YUV422, 255}, {P210, YUV422, 256} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropY = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropY);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropY);
    }
    Ipp32u warning[][3] = { {NV12, YUV420, 1}, {NV12, YUV420, 99}, {P010, YUV420, 15}, {P010, YUV420, 5} };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropY = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropY);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2] - 1, output.videoParam.mfx.FrameInfo.CropY);
    }
}

TEST_F(QueryTest, Conflicts_misaligned_CropH) {
    Ipp32u ok[][3] = {
        {NV12, YUV420, 2}, {NV12, YUV420, 22}, {NV16, YUV422, 1}, {NV16, YUV422, 2}, {NV16, YUV422, 50}, {NV16, YUV422, 51},
        {P010, YUV420, 8}, {P010, YUV420, 28}, {P210, YUV422, 1}, {P210, YUV422, 2}, {P210, YUV422, 255}, {P210, YUV422, 256} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropH = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropH);
    }
    Ipp32u warning[][3] = { {NV12, YUV420, 1}, {NV12, YUV420, 99}, {P010, YUV420, 15}, {P010, YUV420, 5} };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.FrameInfo.CropH = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2] + 1, output.videoParam.mfx.FrameInfo.CropH);
    }
}

TEST_F(QueryTest, Conflicts_CropY_ge_picHeight) {
    Ipp32u ok[][3] = { {1088, 1080, 2}, {0, 1080, 2}, {1088, 0, 2}, {1088, 1080, 1078}, {0, 1080, 1078}, {1088, 0, 1078} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropY = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropY);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropY);
    }
    Ipp32u error[][3] = { {480, 480, 480}, {0, 480, 480}, {480, 0, 480}, {480, 472, 472}, {0, 472, 472}, {480, 472, 475}, {0, 472, 475} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropY = p[2];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropY);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.CropY);
    }
}

TEST_F(QueryTest, Conflicts_CropH_gt_picHeight) {
    Ipp32u ok[][3] = { {1088, 1080, 2}, {0, 1080, 2}, {1088, 0, 2}, {1088, 1080, 1078}, {0, 1080, 1078}, {1088, 0, 1078}, {576, 576, 576}, {0, 576, 576}, {576, 0, 576} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropH = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropH);
    }
    Ipp32u error[][3] = { {720, 720, 722}, {0, 720, 722}, {720, 0, 722}, {288, 280, 283}, {0, 280, 283} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropH = p[2];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.CropH);
    }
}

TEST_F(QueryTest, Conflicts_CropY_plus_CropH_gt_picHeight) {
    Ipp32u ok[][4] = { {1088, 1080, 1080, 0}, {0, 1080, 1080, 0}, {1088, 0, 1080, 0}, {1088, 1080, 1078, 2}, {0, 1080, 1078, 2}, {1088, 0, 1078, 2}, {720, 720, 2, 718}, {0, 720, 2, 718}, {720, 0, 2, 718} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropH = p[2];
        input.videoParam.mfx.FrameInfo.CropY = p[3];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.CropY);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.CropY);
    }
    Ipp32u error[][4] = { {1280, 1280, 1280, 124}, {0, 1280, 1280, 124}, {1280, 0, 1280, 124}, {288, 280, 142, 142}, {0, 280, 142, 142} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.FrameInfo.CropH = p[2];
        input.videoParam.mfx.FrameInfo.CropY = p[3];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.CropY);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.CropH);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.CropY);
    }
}

TEST_F(QueryTest, Conflicts_GopRefDist_gt_GopPicSize) {
    Ipp32u ok[][2] = { {1, 1}, {2, 1}, {6, 6}, {8, 8}, {13, 13}, {16, 16}, {64, 8}, {60, 8}, {120, 7}, {0xffff, 11} };
    for (auto p: ok) {
        input.videoParam.mfx.GopPicSize = p[0];
        input.videoParam.mfx.GopRefDist = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.GopPicSize);
        EXPECT_EQ(p[1], input.videoParam.mfx.GopRefDist);
        EXPECT_EQ(p[0], output.videoParam.mfx.GopPicSize);
        EXPECT_EQ(p[1], output.videoParam.mfx.GopRefDist);
    }
    Ipp32u warning[][2] = { {1, 2}, {3, 5}, {5, 8}, {3, 11}, {12, 13}, {15, 16} };
    for (auto p: warning) {
        input.videoParam.mfx.GopPicSize = p[0];
        input.videoParam.mfx.GopRefDist = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.GopPicSize);
        EXPECT_EQ(p[1], input.videoParam.mfx.GopRefDist);
        EXPECT_EQ(p[0], output.videoParam.mfx.GopPicSize);
        EXPECT_EQ(p[0], output.videoParam.mfx.GopRefDist);
    }
}

TEST_F(QueryTest, Conflicts_mismatching_FourCC_and_CodecProfile) {
    Ipp32u ok[][3] = { {NV12, YUV420, MAIN}, {NV12, YUV420, MAIN10}, {NV12, YUV420, REXT}, {P010, YUV420, MAIN10}, {P010, YUV420, REXT}, {NV16, YUV422, REXT}, {P210, YUV422, REXT} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.CodecProfile = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecProfile);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecProfile);
    }
    Ipp32u warning[][4] = { {P010, YUV420, MAIN, MAIN10}, {NV16, YUV422, MAIN, REXT}, {NV16, YUV422, MAIN10, REXT}, {P210, YUV422, MAIN, REXT}, {P210, YUV422, MAIN10, REXT} };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = p[1];
        input.videoParam.mfx.CodecProfile = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecProfile);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(p[3], output.videoParam.mfx.CodecProfile);
    }
}

TEST_F(QueryTest, Conflicts_mismatching_FourCC_and_GeneralConstraintFlags) {
    Ipp32u ok[][2] = { {NV12, 0}, {P010, 0}, {NV16, MAIN_422_10}, {P210, MAIN_422_10} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.FourCC = p[0];
        input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(p[0]);
        input.extHevcParam.GeneralConstraintFlags = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], input.extHevcParam.GeneralConstraintFlags);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
        EXPECT_EQ(p[1], output.extHevcParam.GeneralConstraintFlags);
    }
}

TEST_F(QueryTest, Conflicts_mismatching_CodecProfile_and_GeneralConstraintFlags) {
    Ipp32u ok[][2] = { {MAIN, 0}, {MAIN10, 0}, {REXT, MAIN_422_10} };
    for (auto p: ok) {
        input.videoParam.mfx.CodecProfile = p[0];
        input.extHevcParam.GeneralConstraintFlags = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.CodecProfile);
        EXPECT_EQ(p[1], input.extHevcParam.GeneralConstraintFlags);
        EXPECT_EQ(p[0], output.videoParam.mfx.CodecProfile);
        EXPECT_EQ(p[1], output.extHevcParam.GeneralConstraintFlags);
    }
    Ipp32u warning[][2] = { {MAIN, MAIN_422_10}, {MAIN10, MAIN_422_10}, {REXT, 11}, {REXT, 12} };
    for (auto p: warning) {
        input.videoParam.mfx.CodecProfile = p[0];
        input.extHevcParam.GeneralConstraintFlags = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.CodecProfile);
        EXPECT_EQ(p[1], input.extHevcParam.GeneralConstraintFlags);
        EXPECT_EQ(p[0], output.videoParam.mfx.CodecProfile);
        EXPECT_EQ(0, output.extHevcParam.GeneralConstraintFlags);
    }
}

TEST_F(QueryTest, Conflicts_picWidth_too_large_for_Level) {
    Ipp32u ok[][2] = {
        {8, M10}, {536, M10}, {544, M20}, {984, M20}, {992, M21}, {1400, M21}, {1408, M30}, {2096, M30}, {2112, M31}, {2800, M31},
        {2816, M40}, {4216, M40}, {2816, H40}, {4216, H40}, {2816, M41}, {4216, M41}, {2816, H41}, {4216, H41},
        {4224, M50}, {8192, M50}, {4224, H50}, {8192, H50}, {4224, M51}, {8192, M51}, {4224, H51}, {8192, H51}, {4224, M52}, {8192, M52}, {4224, H52}, {8192, H52},
        {8192, M60}, {16, M60}, {8192, H60}, {560, H60}, {8192, M61}, {1008, M61}, {8192, H61}, {1504, H61}, {8192, M62}, {2496, M62}, {8192, H62}, {3008, H62}
    };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Width = 8192;
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.videoParam.mfx.CodecLevel = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(8192, input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(8192, output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.videoParam.mfx.CodecLevel);
        if ((p[0] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Width = p[0];
            input.videoParam.mfx.CodecLevel = p[1];
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
            EXPECT_EQ(p[1], output.videoParam.mfx.CodecLevel);
        }
    }
    Ipp32u warning[][3] = {
        {544, M10, M20}, {976, M10, M20}, {992, M10, M21}, {1392, M10, M21}, {1408, M10, M30}, {2096, M10, M30},
        {2112, M10, M31}, {2800, M10, M31}, {2816, M30, M40}, {4208, M20, M40}, {2816, M21, M40}, {4208, M21, M40},
        {4224, M40, M50}, {4224, M41, M50}, {8192, M41, M50}, {4224, H40, H50}, {4224, H41, H50}, {8192, H41, H50}
    };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.Width = 8192;
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.videoParam.mfx.CodecLevel = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(8192, input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(8192, output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        if ((p[0] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Width = p[0];
            input.videoParam.mfx.CodecLevel = p[1];
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
            EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        }
    }
}

TEST_F(QueryTest, Conflicts_picHeight_too_large_for_Level) {
    Ipp32u ok[][2] = {
        {8, M10}, {536, M10}, {544, M20}, {984, M20}, {992, M21}, {1400, M21}, {1408, M30}, {2096, M30}, {2112, M31}, {2800, M31},
        {2816, M40}, {4216, M40}, {2816, H40}, {4216, H40}, {2816, M41}, {4216, M41}, {2816, H41}, {4216, H41},
        {4224, M50}, {4320, M50}, {4224, H50}, {4320, H50}, {4224, M51}, {4320, M51}, {4224, H51}, {4320, H51}, {4224, M52}, {4320, M52}, {4224, H52}, {4320, H52},
        {4320, M60}, {16, M60}, {4320, H60}, {560, H60}, {4320, M61}, {1008, M61}, {4320, H61}, {1504, H61}, {4320, M62}, {2496, M62}, {4320, H62}, {3008, H62}
    };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Height = 4320;
        input.extHevcParam.PicHeightInLumaSamples = p[0];
        input.videoParam.mfx.CodecLevel = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(4320, input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(4320, output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], output.videoParam.mfx.CodecLevel);
        if ((p[0] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Height = p[0];
            input.videoParam.mfx.CodecLevel = p[1];
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
            EXPECT_EQ(p[1], output.videoParam.mfx.CodecLevel);
        }
    }
    Ipp32u warning[][3] = {
        {544, M10, M20}, {976, M10, M20}, {992, M10, M21}, {1392, M10, M21}, {1408, M10, M30}, {2096, M10, M30},
        {2112, M10, M31}, {2800, M10, M31}, {2816, M30, M40}, {4208, M20, M40}, {2816, M21, M40}, {4208, M21, M40},
        {4224, M40, M50}, {4224, M41, M50}, {4320, M41, M50}, {4224, H40, H50}, {4224, H41, H50}, {4320, H41, H50}
    };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.Height = 4320;
        input.extHevcParam.PicHeightInLumaSamples = p[0];
        input.videoParam.mfx.CodecLevel = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(4320, input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(4320, output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        if ((p[0] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Height = p[0];
            input.videoParam.mfx.CodecLevel = p[1];
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
            EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        }
    }
}

TEST_F(QueryTest, Conflicts_LumaPs_too_large_for_Level) {
    Ipp32u ok[][3] = {
        {536,     64,   M10}, {256,     144,  M10}, {144,     256,  M10}, {64,     536,  M10}, {64,     536,  M20}, // max for level10 MainTier
        {536+8,   64,   M20}, {256+16,  144,  M20}, {144+16,  256,  M20}, {64+8,   536,  M20}, {64+8,   536,  M30}, // min for level20 MainTier
        {984,     112,  M20}, {512,     240,  M20}, {240,     512,  M20}, {112,    984,  M20}, {112,    984,  M21}, // max --/--
        {984+8,   120,  M21}, {512+16,  240,  M21}, {240+16,  512,  M21}, {120+8,  984,  M21}, {120+8,  984,  M30}, // min for level21 MainTier
        {1400,    168,  M21}, {512,     480,  M21}, {480,     512,  M21}, {168,    1400, M21}, {168,    1400, M30}, // max --/--
        {1400+8,  168,  M30}, {512+16,  480,  M30}, {480+16,  512,  M30}, {168+8,  1400, M30}, {168+8,  1400, M31}, // min for level30 MainTier
        {2096,    256,  M30}, {864,     640,  M30}, {640,     864,  M30}, {256,    2096, M30}, {256,    2096, M31}, // max --/--
        {2096+8,  256,  M31}, {864+16,  640,  M31}, {640+16,  864,  M31}, {256+8,  2096, M31}, {256+8,  2096, M40}, // min for level31 MainTier
        {2800,    344,  M31}, {1280,    768,  M31}, {768,     1280, M31}, {344,    2800, M31}, {344,    2800, H40}, // max --/--
        {2800+8,  344,  M40}, {1280+16, 768,  M40}, {768+16,  1280, M40}, {344+8,  2800, M40}, {344+8,  2800, M41}, // min for level40 MainTier
        {4216,    528,  M40}, {2048,    1088, M40}, {1088,    2048, M40}, {528,    4216, M40}, {528,    4216, M50}, // max --/--
        {2800+8,  344,  M41}, {1280+16, 768,  M41}, {768+16,  1280, M41}, {344+8,  2800, M41}, {344+8,  2800, H50}, // min for level41 MainTier
        {4216,    528,  M41}, {2048,    1088, M41}, {1088,    2048, M41}, {528,    4216, M41}, {528,    4216, M51}, // max --/--
        {2800+8,  344,  H40}, {1280+16, 768,  H40}, {768+16,  1280, H40}, {344+8,  2800, H40}, {344+8,  2800, H51}, // min for level40 HighTier
        {4216,    528,  H40}, {2048,    1088, H40}, {1088,    2048, H40}, {528,    4216, H40}, {528,    4216, M52}, // max --/--
        {2800+8,  344,  H41}, {1280+16, 768,  H41}, {768+16,  1280, H41}, {344+8,  2800, H41}, {344+8,  2800, H52}, // min for level41 HighTier
        {4216,    528,  H41}, {2048,    1088, H41}, {1088,    2048, H41}, {528,    4216, H41}, {528,    4216, M60}, // max --/--
        {4216+8,  528,  M50}, {2048+16, 1088, M50}, {1088+16, 2048, M50}, {528+8,  4216, M50}, {528+8,  4216, H60}, // min for level50 MainTier
        {8192,    1088, M50}, {4096,    2176, M50}, {2176,    4096, M50},                      {2176,   4096, M61}, // max --/--
        {4216+8,  528,  M51}, {2048+16, 1088, M51}, {1088+16, 2048, M51}, {528+8,  4216, M51}, {528+8,  4216, H61}, // min for level51 MainTier
        {8192,    1088, M51}, {4096,    2176, M51}, {2176,    4096, M51},                      {2176,   4096, M62}, // max --/--
        {4216+8,  528,  M52}, {2048+16, 1088, M52}, {1088+16, 2048, M52}, {528+8,  4216, M52}, {528+8,  4216, H62}, // min for level50 MainTier
        {8192,    1088, M52}, {4096,    2176, M52}, {2176,    4096, M52},                      {2176,   4096, M60}, // max --/--
        {4216+8,  528,  H50}, {2048+16, 1088, H50}, {1088+16, 2048, H50}, {528+8,  4216, H50}, {528+8,  4216, H60}, // min for level51 HighTier
        {8192,    1088, H50}, {4096,    2176, H50}, {2176,    4096, H50},                      {2176,   4096, M61}, // max --/--
        {4216+8,  528,  H51}, {2048+16, 1088, H51}, {1088+16, 2048, H51}, {528+8,  4216, H51}, {528+8,  4216, H61}, // min for level50 HighTier
        {8192,    1088, H51}, {4096,    2176, H51}, {2176,    4096, H51},                      {2176,   4096, M62}, // max --/--
        {4216+8,  528,  H52}, {2048+16, 1088, H52}, {1088+16, 2048, H52}, {528+8,  4216, H52}, {528+8,  4216, H62}, // min for level51 HighTier
        {8192,    1088, H52}, {4096,    2176, H52}, {2176,    4096, H52},                      {2176,   4096, H62}, // max --/--
        {8192,    4320, M60}, {8192,    4320, H60}, {8192,    4320, M61}, {8192,   4320, H61}, {8192,   4320, M62}, {8192, 4320, H62}, // max supported WxH fits levels 6X
    };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Width = 8192;
        input.videoParam.mfx.FrameInfo.Height = 4320;
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.CodecLevel = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(8192, input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(4320, input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(8192, output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(4320, output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
    }
    Ipp32u warning[][4] = {
        {536+8,  64,   M10, M20}, {256+16,  144,  M10, M20}, {144+16,  256,  M10, M20}, {64+8,  536,  M10, M20},
        {984+8,  120,  M10, M21}, {512+16,  240,  M20, M21}, {240+16,  512,  M10, M21}, {120+8, 976,  M20, M21},
        {1400+8, 168,  M10, M30}, {512+16,  480,  M20, M30}, {480+16,  512,  M21, M30}, {168+8, 1400, M10, M30},
        {2096+8, 256,  M10, M31}, {864+16,  640,  M20, M31}, {640+16,  864,  M21, M31}, {256+8, 2096, M30, M31},
        {2800+8, 344,  M20, M40}, {1280+16, 768,  M21, M40}, {768+16,  1280, M30, M40}, {344+8, 2800, M31, M40},
        {4216+8, 528,  M10, M50}, {2048+16, 1088, M20, M50}, {1088+16, 2048, M21, M50}, {528+8, 4216, M30, M50},
        {4216+8, 528,  H40, H50}, {2048+16, 1088, H41, H50}, {1088+16, 2048, H40, H50}, {528+8, 4216, H41, H50},
        {8192,   4320, M10, M60}, {8192,    4320, H40, H60}
    };
    for (auto p: warning) {
        input.videoParam.mfx.FrameInfo.Width = 8192;
        input.videoParam.mfx.FrameInfo.Height = 4320;
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.CodecLevel = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(8192, input.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(4320, input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(8192, output.videoParam.mfx.FrameInfo.Width);
        EXPECT_EQ(4320, output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[3], output.videoParam.mfx.CodecLevel);
        if ((p[0] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Width = p[0];
            input.videoParam.mfx.FrameInfo.Height = 4320;
            input.extHevcParam.PicWidthInLumaSamples = 0;
            input.extHevcParam.PicHeightInLumaSamples = p[1];
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[3], output.videoParam.mfx.CodecLevel);
        }
        if ((p[1] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Width = 8192;
            input.videoParam.mfx.FrameInfo.Height = p[1];
            input.extHevcParam.PicWidthInLumaSamples = p[0];
            input.extHevcParam.PicHeightInLumaSamples = 0;
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[3], output.videoParam.mfx.CodecLevel);
        }
        if ((p[0] & 15) == 0 && (p[1] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Width = p[0];
            input.videoParam.mfx.FrameInfo.Height = p[1];
            input.extHevcParam.PicWidthInLumaSamples = 0;
            input.extHevcParam.PicHeightInLumaSamples = 0;
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[3], output.videoParam.mfx.CodecLevel);
        }
    }
}

TEST_F(QueryTest, Conflicts_LumaSr_too_large_for_Level) {
    Ipp32u ok[][5] = { // Width, Height, FrameRateExtN, FrameRateExtD, CodecLevel
                                                                                            {256,  144,  15,  1, M10}, { 128,  272,  3610721594, 227341730, M10}, // max/max for level10 MainTier
        {128,  288,  3610721594, 227341730, M20}, {128,  272,  3610721595, 227341730, M20}, {512,  240,  30,  1, M20}, { 128,  816,  1175061035, 33293396,  M20}, // min/min/max/max for level20 MainTier
        {128,  832,  1175061035, 33293396,  M21}, {128,  816,  1175061036, 33293396,  M21}, {512,  480,  30,  1, M21}, { 96,   1312, 2168251726, 37040967,  M21}, // so on
        {96,   1328, 2168251726, 37040967,  M30}, {96,   1312, 2168251727, 37040967,  M30}, {864,  640,  30,  1, M30}, { 1840, 80,   1726164601, 15317047,  M30},  
        {1840, 96,   1726164601, 15317047,  M31}, {1840, 80,   1726164602, 15317047,  M31}, {1280, 768,  135, 4, M31}, { 976,  800,  3913851035, 92108377,  M31},  
        {976,  816,  3913851035, 92108377,  M40}, {976,  800,  3913851036, 92108377,  M40}, {2048, 1088, 30,  1, M40}, { 1808, 944,  4226773213, 107919336, M40}, 
        {1808, 960,  4226773213, 107919336, M41}, {1808, 944,  4226773214, 107919336, M41}, {2048, 1088, 60,  1, M41}, { 1616, 1296, 1308952332, 20505015,  M41},  
        {1616, 1312, 1308952332, 20505015,  M50}, {1616, 1296, 1308952333, 20505015,  M50}, {4096, 2176, 30,  1, M50}, { 7264, 896,  2648775603, 64474639,  M50},  
        {7264, 912,  2648775603, 64474639,  M51}, {7264, 896,  2648775604, 64474639,  M51}, {4096, 2176, 60,  1, M51}, { 6800, 1120, 3651609512, 52004530,  M51},  
        {6800, 1136, 3651609512, 52004530,  M52}, {6800, 1120, 3651609513, 52004530,  M52}, {4096, 2176, 120, 1, M52}, { 3296, 1664, 510306484,  2616806,   M52},   
        {6800, 1136, 3651609512, 52004530,  M60}, {6800, 1120, 3651609513, 52004530,  M60}, {8192, 4320, 30,  1, M60}, { 3696, 1728, 1640740758, 9797507,   M60},   
        {3696, 1744, 1640740758, 9797507,   M61}, {3696, 1728, 1640740759, 9797507,   M61}, {8192, 4320, 60,  1, M61}, { 5648, 3344, 196409978,  1734181,   M61},   
        {5648, 3360, 196409978,  1734181,   M62}, {5648, 3344, 196409979,  1734181,   M62}, {8192, 4320, 120, 1, M62}, { 4624, 3904, 4215606596, 17788021,  M62},  
        {976,  816,  3913851035, 92108377,  H40}, {976,  800,  3913851036, 92108377,  H40}, {2048, 1088, 30,  1, H40}, { 1808, 944,  4226773213, 107919336, H40}, 
        {1808, 960,  4226773213, 107919336, H41}, {1808, 944,  4226773214, 107919336, H41}, {2048, 1088, 60,  1, H41}, { 1616, 1296, 1308952332, 20505015,  H41},  
        {1616, 1312, 1308952332, 20505015,  H50}, {1616, 1296, 1308952333, 20505015,  H50}, {4096, 2176, 30,  1, H50}, { 7264, 896,  2648775603, 64474639,  H50},  
        {7264, 912,  2648775603, 64474639,  H51}, {7264, 896,  2648775604, 64474639,  H51}, {4096, 2176, 60,  1, H51}, { 6800, 1120, 3651609512, 52004530,  H51},  
        {6800, 1136, 3651609512, 52004530,  H52}, {6800, 1120, 3651609513, 52004530,  H52}, {4096, 2176, 120, 1, H52}, { 3296, 1664, 510306484,  2616806,   H52},   
        {6800, 1136, 3651609512, 52004530,  H60}, {6800, 1120, 3651609513, 52004530,  H60}, {8192, 4320, 30,  1, H60}, { 3696, 1728, 1640740758, 9797507,   H60},   
        {3696, 1744, 1640740758, 9797507,   H61}, {3696, 1728, 1640740759, 9797507,   H61}, {8192, 4320, 60,  1, H61}, { 5648, 3344, 196409978,  1734181,   H61},   
        {5648, 3360, 196409978,  1734181,   H62}, {5648, 3344, 196409979,  1734181,   H62}, {8192, 4320, 120, 1, H62}, { 4624, 3904, 4215606596, 17788021,  H62},  
    };
    for (auto p: ok) {
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.videoParam.mfx.FrameInfo.Height = p[1];
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[2];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[3];
        input.videoParam.mfx.CodecLevel = p[4];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[4], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[4], output.videoParam.mfx.CodecLevel);
    }
    Ipp32u warning[][6] = { // Width, Height, FrameRateExtN, FrameRateExtD, in.CodecLevel, out.CodecLevel
        {128,  288,  3610721594, 227341730, M10, M20}, {128,  272,  3610721595, 227341730, M10, M20},
        {128,  832,  1175061035, 33293396,  M10, M21}, {128,  816,  1175061036, 33293396,  M20, M21},
        {96,   1328, 2168251726, 37040967,  M20, M30}, {96,   1312, 2168251727, 37040967,  M21, M30},
        {1840, 96,   1726164601, 15317047,  M21, M31}, {1840, 80,   1726164602, 15317047,  M30, M31},
        {976,  816,  3913851035, 92108377,  M30, M40}, {976,  800,  3913851036, 92108377,  M31, M40},
        {1808, 960,  4226773213, 107919336, M31, M41}, {1808, 944,  4226773214, 107919336, M40, M41},
        {1616, 1312, 1308952332, 20505015,  M40, M50}, {1616, 1296, 1308952333, 20505015,  M41, M50},
        {7264, 912,  2648775603, 64474639,  M41, M51}, {7264, 896,  2648775604, 64474639,  M50, M51},
        {6800, 1136, 3651609512, 52004530,  M50, M52}, {6800, 1120, 3651609513, 52004530,  M51, M52},
        {3696, 1744, 1640740758, 9797507,   M52, M61}, {3696, 1728, 1640740759, 9797507,   M60, M61},
        {5648, 3360, 196409978,  1734181,   M60, M62}, {5648, 3344, 196409979,  1734181,   M61, M62},
        {1808, 960,  4226773213, 107919336, H40, H41}, {1808, 944,  4226773214, 107919336, H40, H41},
        {1616, 1312, 1308952332, 20505015,  H40, H50}, {1616, 1296, 1308952333, 20505015,  H41, H50},
        {7264, 912,  2648775603, 64474639,  H41, H51}, {7264, 896,  2648775604, 64474639,  H50, H51},
        {6800, 1136, 3651609512, 52004530,  H50, H52}, {6800, 1120, 3651609513, 52004530,  H51, H52},
        {3696, 1744, 1640740758, 9797507,   H52, H61}, {3696, 1728, 1640740759, 9797507,   H60, H61},
        {5648, 3360, 196409978,  1734181,   H60, H62}, {5648, 3344, 196409979,  1734181,   H61, H62},
    };
    for (auto p: warning) {
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.videoParam.mfx.FrameInfo.Height = p[1];
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[2];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[3];
        input.videoParam.mfx.CodecLevel = p[4];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[4], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[5], output.videoParam.mfx.CodecLevel);
    }
    Ipp32u error[][5] = { // Width, Height, FrameRateExtN, FrameRateExtD, CodecLevel
        {8192,  4320,  121, 1, M10}, {8192,  4320,  4294967295, 35528222, M10},
        {8192,  4320,  121, 1, M62}, {8192,  4320,  4294967295, 35528222, H62},
    };
    for (auto p: error) {
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.videoParam.mfx.FrameInfo.Height = p[1];
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[2];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[3];
        input.videoParam.mfx.CodecLevel = p[4];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[4], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(0, output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(0, output.videoParam.mfx.FrameInfo.FrameRateExtD);
    }
}

TEST_F(QueryTest, Conflicts_NumRefFrame_too_large_for_MAX_Level) {
    Ipp32u unsupported[][3] = { // Width, Height, MaxDpbSize at level 62
        {4208, 4208, 12}, {7296, 3648, 8}, {8192, 4320, 6},
    };
    for (auto p: unsupported) {
        for (Ipp32u numRefFrame = 1; numRefFrame <= 16; numRefFrame++) {
            input.videoParam.mfx.FrameInfo.Width = p[0];
            input.extHevcParam.PicHeightInLumaSamples = p[1];
            input.videoParam.mfx.NumRefFrame = numRefFrame;
            const Ipp32u maxDpbSize = p[2];
            mfxStatus expectedSts = (numRefFrame <= maxDpbSize) ? MFX_ERR_NONE : MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            Ipp32u expectedNumRefFrame = IPP_MIN(maxDpbSize, numRefFrame);
            EXPECT_EQ(expectedSts, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Width);
            EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(numRefFrame, input.videoParam.mfx.NumRefFrame);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Width);
            EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(expectedNumRefFrame, output.videoParam.mfx.NumRefFrame);
        }
    }
}

TEST_F(QueryTest, Conflicts_NumRefFrame_too_large_for_Level) {
    const Ipp32u CodecLevelsToTest[] = { M10, M20, M21, M30, M31, M40, M51, M62, H41, H52, H60 };

    Ipp32u ok[][5] = { // Width(==Height), min required levels for NumRefFrame<=6, NumRefFrame<=8, NumRefFrame<=12, NumRefFrame<=16
        {96,   M10, M10, M10, M10}, {128,  M10, M10, M10, M20}, {160,  M10, M10, M20, M20}, {192,  M10, M20, M20, M21},
                                    {240,  M20, M20, M20, M21}, {288,  M20, M20, M21, M30}, {336,  M20, M21, M21, M30},
                                                                {416,  M21, M21, M30, M31}, {480,  M21, M30, M30, M31},
                                    {512,  M30, M30, M30, M40}, {640,  M30, M30, M31, M40}, {736,  M30, M31, M40, M40},
                                                                {848,  M31, M31, M40, M50}, {976,  M31, M40, M40, M50},
                                    {1040, M40, M40, M40, M50}, {1280, M40, M40, M50, M50}, {1488, M40, M50, M50, M50},
                                    {2096, M50, M50, M50, M60}, {2576, M50, M50, M60, M60}, {2976, M50, M60, M60, M60},
    };
    for (auto p: ok) {
        for (Ipp32u numRefFrame = 1; numRefFrame <= 16; numRefFrame++) {
            input.extHevcParam.PicWidthInLumaSamples = p[0];
            input.videoParam.mfx.FrameInfo.Height = p[0];
            input.videoParam.mfx.NumRefFrame = numRefFrame;
            Ipp32u reqLevel = numRefFrame <= 6 ? p[1] : numRefFrame <= 8 ? p[2] : numRefFrame <= 12 ? p[3] : p[4];
            for (Ipp32u codecLevel: CodecLevelsToTest) {
                input.videoParam.mfx.CodecLevel = codecLevel;
                mfxStatus expectedSts = ((codecLevel & 0xff) >= (reqLevel & 0xff)) ? MFX_ERR_NONE : MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                Ipp32u expectedLevel = IPP_MAX(reqLevel & 0xff, codecLevel & 0xff) | (codecLevel & 0xff00);
                EXPECT_EQ(expectedSts, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
                EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
                EXPECT_EQ(codecLevel, input.videoParam.mfx.CodecLevel);
                EXPECT_EQ(numRefFrame, input.videoParam.mfx.NumRefFrame);
                EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
                EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
                EXPECT_EQ(expectedLevel, output.videoParam.mfx.CodecLevel);
                EXPECT_EQ(numRefFrame, output.videoParam.mfx.NumRefFrame);
            }
        }
    }
}

TEST_F(QueryTest, Conflicts_NumRefFrameB_gt_NumRefFrame) {
    Ipp32u ok[][2] = { {1, 1}, {2, 1}, {2, 2}, {16, 1}, {16, 8}, {16, 15}, {16, 16} };
    for (auto p: ok) {
        input.videoParam.mfx.NumRefFrame = p[0];
        input.extCodingOptionHevc.NumRefFrameB = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumRefFrame);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.NumRefFrameB);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumRefFrame);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.NumRefFrameB);
    }
    Ipp32u warning[][2] = { {1, 2}, {2, 3}, {8, 9}, {16, 17} };
    for (auto p: warning) {
        input.videoParam.mfx.NumRefFrame = p[0];
        input.extCodingOptionHevc.NumRefFrameB = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumRefFrame);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.NumRefFrameB);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumRefFrame);
        EXPECT_EQ(0, output.extCodingOptionHevc.NumRefFrameB);
    }
}

TEST_F(QueryTest, Conflicts_multi_slice_and_multi_tile_at_once) {
    Ipp32u ok[][3] = { {1, 1, 1}, {1, 1, 2}, {1, 2, 1}, {1, 4, 8}, {1, 22, 20}, {2, 1, 1}, {4, 1, 1}, {68, 1, 1} };
    for (auto p: ok) {
        input.videoParam.mfx.NumSlice = p[0];
        input.extHevcTiles.NumTileRows = p[1];
        input.extHevcTiles.NumTileColumns = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], output.extHevcTiles.NumTileColumns);
    }
    Ipp32u warning[][3] = { {2, 1, 2}, {5, 2, 1}, {10, 5, 5} };
    for (auto p: warning) {
        input.videoParam.mfx.NumSlice = p[0];
        input.extHevcTiles.NumTileRows = p[1];
        input.extHevcTiles.NumTileColumns = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(1, output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], output.extHevcTiles.NumTileColumns);
    }
}

TEST_F(QueryTest, Conflicts_NumSlice_gt_MAX_number_of_CTB_rows) {
    Ipp32u ok[][3] = { {2160, 184, 3}, {176, 0, 3}, {2160, 248, 4}, {240, 240, 4}, {2160, 488, 8}, {480, 480, 8}, {720, 712, 12}, {720, 0, 12},
                       {1072, 0, 17}, {1088, 1088, 17}, {1088, 1080, 17}, {2176, 2168, 34}, {2160, 2152, 34}, {4320, 4312, 68} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.NumSlice = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.videoParam.mfx.NumSlice);
        if (p[2] < 68) {
            input.videoParam.mfx.NumSlice = p[2]+1;
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[2]+1, input.videoParam.mfx.NumSlice);
            EXPECT_EQ(p[2], output.videoParam.mfx.NumSlice);
        }
    }
    Ipp32u error[][3] = { {720, 720, 600}, {1072, 1072, 500}, {1088, 1080, 300}, {2160, 8, 200}, {4320, 176, 136} };
    for (auto p: error) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.videoParam.mfx.NumSlice = p[2];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(0, output.videoParam.mfx.NumSlice);
    }
}

TEST_F(QueryTest, Conflicts_NumSlice_gt_number_of_CTB_rows) {
    Ipp32u configs[][4] = { {2160, 184, 5, 3}, {176, 176, 5, 3}, {2160, 184, 6, 3}, {176, 176, 6, 3}, {2160, 728, 5, 12}, {720, 720, 5, 12}, {2160, 712, 6, 12}, {720, 720, 6, 12} }; // Height, Log2MaxCuSize, max NumSlice
    for (auto p: configs) {
        input.videoParam.mfx.FrameInfo.Height = p[0];
        input.extHevcParam.PicHeightInLumaSamples = p[1];
        input.extCodingOptionHevc.Log2MaxCUSize = p[2];
        Ipp32u NumSlices[] = {1, p[3]/2, p[3]};
        for (auto numSlice: NumSlices) {
            input.videoParam.mfx.NumSlice = numSlice;
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
            EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[2], input.extCodingOptionHevc.Log2MaxCUSize);
            EXPECT_EQ(numSlice, input.videoParam.mfx.NumSlice);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
            EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[2], output.extCodingOptionHevc.Log2MaxCUSize);
            EXPECT_EQ(numSlice, output.videoParam.mfx.NumSlice);
        }
        input.videoParam.mfx.NumSlice = p[3]+1;
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[3]+1, input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.Height);
        EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[2], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[3], output.videoParam.mfx.NumSlice);
    }
}

TEST_F(QueryTest, Conflicts_NumSlice_too_large_for_Level) {
    Ipp32u ok[][3] = {
        {1, 16, M10}, {1, 16, M20}, {17, 20, M21}, {21, 30, M30}, {31, 40, M31}, {41, 68, M40}, {41, 68, M41} };
        //{76, 200, M50}, {76, 200, M51}, {76, 200, M52}, {201, 600, M60}, {201, 600, M61}, {201, 600, M62},
        //{41, 75, H40}, {41, 75, H41}, {76, 200, H50}, {76, 200, H51}, {76, 200, H52}, {201, 600, H60}, {201, 600, H61}, {201, 600, H62} };
    for (auto p: ok) {
        input.videoParam.mfx.NumSlice = p[0];
        input.videoParam.mfx.CodecLevel = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        input.videoParam.mfx.NumSlice = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[1], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[1], output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        input.videoParam.mfx.NumSlice = IPP_MAX(1, p[0] - 1);
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(IPP_MAX(1, p[0] - 1), input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(IPP_MAX(1, p[0] - 1), output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
    }
    Ipp32u warning[][3] = {
        {17, M10, M21}, {21, M10, M30}, {31, M10, M31}, {41, M10, M40} };
        //, {76, M10, M50}, {76, H40, H50}, {201, M10, M60}, {201, H41, H60} };
    for (auto p: warning) {
        input.videoParam.mfx.NumSlice = p[0];
        input.videoParam.mfx.CodecLevel = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
    }
}

TEST_F(QueryTest, Conflicts_Tile_width_too_small) {
    Ipp32u configs[][2] = { {8, 1}, {256, 1}, {504, 1}, {512, 2}, {720, 2}, {1272, 4}, {1280, 5}, {1920, 7}, {2296, 8}, {3832, 14}, {3840, 15}, {5112, 19}, {5128, 20}, {8176, 20} };
    for (auto p: configs) {
        input.videoParam.mfx.FrameInfo.Width = (p[0] + 15) & ~15;
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.extHevcTiles.NumTileColumns = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileColumns);
        input.extHevcTiles.NumTileColumns++;
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1]+1, input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileColumns);
    }
}

TEST_F(QueryTest, Conflicts_Tile_height_too_small) {
    Ipp32u configs[][2] = { {8, 1}, {64, 1}, {120, 1}, {128, 2}, {456, 7}, {504, 7}, {720, 11}, {1072, 16}, {1080, 16}, {1088, 17}, {1336, 20}, {1344, 21}, {2160, 22}, {4320, 22} };
    for (auto p: configs) {
        input.videoParam.mfx.FrameInfo.Height = (p[0] + 15) & ~15;
        input.extHevcParam.PicHeightInLumaSamples = p[0];
        input.extHevcTiles.NumTileRows = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[0], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileRows);
        input.extHevcTiles.NumTileRows++;
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1]+1, input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[0], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileRows);
    }
}

TEST_F(QueryTest, Conflicts_NumTileRows_too_large_for_Level) {
    Ipp32u ok[][3] = {
        {1, 1, M10}, {1, 1, M20}, {1, 1, M21}, {2, 2, M30}, {3, 3, M31}, {4, 5, M40}, {4, 5, M41},
        {6, 11, M50}, {6, 11, M51}, {6, 11, M52}, {12, 22, M60}, {12, 22, M61}, {12, 22, M62},
        {4, 5, H40}, {4, 5, H41}, {6, 11, H50}, {6, 11, H51}, {6, 11, H52}, {12, 22, H60}, {12, 22, H61}, {12, 22, H62} };
    for (auto p: ok) {
        input.extHevcTiles.NumTileRows = p[0];
        input.videoParam.mfx.CodecLevel = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        input.extHevcTiles.NumTileRows = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        input.extHevcTiles.NumTileRows = IPP_MAX(1, p[0] - 1);
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(IPP_MAX(1, p[0] - 1), input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(IPP_MAX(1, p[0] - 1), output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
    }
    Ipp32u warning[][3] = {
        {2, M10, M30}, {3, M10, M31}, {4, M10, M40}, {6, M10, M50}, {12, M10, M60}, {6, H40, H50}, {12, H41, H60} };
    for (auto p: warning) {
        input.extHevcTiles.NumTileRows = p[0];
        input.videoParam.mfx.CodecLevel = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
    }
}

TEST_F(QueryTest, Conflicts_NumTileColumns_too_large_for_Level) {
    Ipp32u ok[][3] = {
        {1, 1, M10}, {1, 1, M20}, {1, 1, M21}, {2, 2, M30}, {3, 3, M31}, {4, 5, M40}, {4, 5, M41},
        {6, 10, M50}, {6, 10, M51}, {6, 10, M52}, {11, 20, M60}, {11, 20, M61}, {11, 20, M62},
        {4, 5, H40}, {4, 5, H41}, {6, 10, H50}, {6, 10, H51}, {6, 10, H52}, {11, 20, H60}, {11, 20, H61}, {11, 20, H62} };
    for (auto p: ok) {
        input.extHevcTiles.NumTileColumns = p[0];
        input.videoParam.mfx.CodecLevel = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        input.extHevcTiles.NumTileColumns = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
        input.extHevcTiles.NumTileColumns = IPP_MAX(1, p[0] - 1);
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(IPP_MAX(1, p[0] - 1), input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(IPP_MAX(1, p[0] - 1), output.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
    }
    Ipp32u warning[][3] = {
        {2, M10, M30}, {3, M10, M31}, {4, M10, M40}, {6, M10, M50}, {12, M10, M60}, {6, H40, H50}, {12, H41, H60} };
    for (auto p: warning) {
        input.extHevcTiles.NumTileColumns = p[0];
        input.videoParam.mfx.CodecLevel = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[1], input.videoParam.mfx.CodecLevel);
        EXPECT_EQ(p[0], output.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], output.videoParam.mfx.CodecLevel);
    }
}

TEST_F(QueryTest, Conflicts_TargetKbps_gt_uncompressed_rate) {
    Ipp32s rcMethods[] = { CBR, VBR, AVBR, LA_EXT };
    Ipp32u ok[][8] = {
        {8,    8,    0xffffffff, 0xa3d70a3,  NV12, 1,  19},    {8,    8,    0xffffffff, 0xa3d70a3,  NV12, 19,    1},
        {16,   16,   0xffffffff, 0xa3d70a3,  NV12, 1,  76},    {16,   16,   0xffffffff, 0xa3d70a3,  NV12, 76,    1},
        {1920, 1080, 0xffffffff, 0xa3d70a3,  NV16, 13, 63803}, {1920, 1080, 0xffffffff, 0xa3d70a3,  NV16, 63803, 13},
        {1920, 1088, 0xffffffff, 0xa3d70a3,  NV16, 13, 64275}, {1920, 1088, 0xffffffff, 0xa3d70a3,  NV16, 64275, 13},
        {3840, 2160, 0xffffffff, 0x24924924, P010, 14, 62208}, {3840, 2160, 0xffffffff, 0x24924924, P010, 62208, 14},
        {512,  240,  0xffffffff, 0xda740e,   P210, 12, 61439}, {512,   240, 0xffffffff, 0xda740e,   P210, 61439, 12}
    };
    for (auto rcMethod: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcMethod;
        for (auto p: ok) {
            input.videoParam.mfx.FrameInfo.Width = (p[0] + 15) & ~15;
            input.videoParam.mfx.FrameInfo.Height = (p[1] + 15) & ~15;
            input.extHevcParam.PicWidthInLumaSamples = p[0];
            input.extHevcParam.PicHeightInLumaSamples = p[1];
            input.videoParam.mfx.FrameInfo.FrameRateExtN = p[2];
            input.videoParam.mfx.FrameInfo.FrameRateExtD = p[3];
            input.videoParam.mfx.FrameInfo.FourCC = p[4];
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(p[4]);
            input.videoParam.mfx.BRCParamMultiplier = p[5];
            input.videoParam.mfx.TargetKbps = p[6];
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
            EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.FrameRateExtN);
            EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtD);
            EXPECT_EQ(p[4], input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[5], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[6], input.videoParam.mfx.TargetKbps);
            EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
            EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.FrameRateExtN);
            EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtD);
            EXPECT_EQ(p[4], output.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[5], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[6], output.videoParam.mfx.TargetKbps);
        }
    }

    Ipp32u warning[][10] = {
        {8,    8,    0xffffffff, 0xa3d70a3,  NV12, 1,  20,    1,  19},    {8,    8,    0xffffffff, 0xa3d70a3,  NV12, 5,     4,  1,  19},
        {16,   16,   0xffffffff, 0xa3d70a3,  NV12, 1,  77,    1,  76},    {16,   16,   0xffffffff, 0xa3d70a3,  NV12, 11,    7,  1,  76},
        {1920, 1080, 0xffffffff, 0xa3d70a3,  NV16, 13, 63804, 13, 63803}, {1920, 1080, 0xffffffff, 0xa3d70a3,  NV16, 15951, 52, 13, 63803},
        {1920, 1088, 0xffffffff, 0xa3d70a3,  NV16, 13, 64276, 13, 64275}, {1920, 1088, 0xffffffff, 0xa3d70a3,  NV16, 16069, 52, 13, 64275},
        {3840, 2160, 0xffffffff, 0x24924924, P010, 14, 62209, 14, 62208}, {3840, 2160, 0xffffffff, 0x24924924, P010, 8887,  98, 14, 62208},
        {512,  240,  0xffffffff, 0xda740e,   P210, 12, 61440, 12, 61439}, {512,  240,  0xffffffff, 0xda740e,   P210, 15360, 48, 12, 61439}
    };
    for (auto rcMethod: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcMethod;
        for (auto p: warning) {
            input.videoParam.mfx.FrameInfo.Width = (p[0] + 15) & ~15;
            input.videoParam.mfx.FrameInfo.Height = (p[1] + 15) & ~15;
            input.extHevcParam.PicWidthInLumaSamples = p[0];
            input.extHevcParam.PicHeightInLumaSamples = p[1];
            input.videoParam.mfx.FrameInfo.FrameRateExtN = p[2];
            input.videoParam.mfx.FrameInfo.FrameRateExtD = p[3];
            input.videoParam.mfx.FrameInfo.FourCC = p[4];
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(p[4]);
            input.videoParam.mfx.BRCParamMultiplier = p[5];
            input.videoParam.mfx.TargetKbps = p[6];
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
            EXPECT_EQ(p[1], input.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[2], input.videoParam.mfx.FrameInfo.FrameRateExtN);
            EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtD);
            EXPECT_EQ(p[4], input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[5], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[6], input.videoParam.mfx.TargetKbps);
            EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
            EXPECT_EQ(p[1], output.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[2], output.videoParam.mfx.FrameInfo.FrameRateExtN);
            EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtD);
            EXPECT_EQ(p[4], output.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[7], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[8], output.videoParam.mfx.TargetKbps);
        }
    }
}

TEST_F(QueryTest, Conflicts_MaxKbps_lt_TargetKbps) {
    input.videoParam.mfx.RateControlMethod = VBR;
    Ipp32u ok[][3] = { {0, 1000, 1000}, {1, 1000, 1000}, {2, 1000, 1000}, {4, 1000, 1000} };
    for (auto p: ok) {
        input.videoParam.mfx.BRCParamMultiplier = p[0];
        input.videoParam.mfx.MaxKbps = p[1];
        input.videoParam.mfx.TargetKbps = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[1], input.videoParam.mfx.MaxKbps);
        EXPECT_EQ(p[2], input.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[0], output.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[1], output.videoParam.mfx.MaxKbps);
        EXPECT_EQ(p[2], output.videoParam.mfx.TargetKbps);
    }
    Ipp32u warning[][3] = { {0, 1000, 1001}, {1, 1000, 2000}, {2, 1000, 3000}, {4, 1000, 4000} };
    for (auto p: warning) {
        input.videoParam.mfx.BRCParamMultiplier = p[0];
        input.videoParam.mfx.MaxKbps = p[1];
        input.videoParam.mfx.TargetKbps = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[1], input.videoParam.mfx.MaxKbps);
        EXPECT_EQ(p[2], input.videoParam.mfx.TargetKbps);
        EXPECT_EQ(1, output.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(IPP_MAX(1, p[0]) * p[2], output.videoParam.mfx.MaxKbps);
        EXPECT_EQ(IPP_MAX(1, p[0]) * p[2], output.videoParam.mfx.TargetKbps);
    }
}

TEST_F(QueryTest, Conflicts_TargetKbps_MaxKbps_too_large_for_HighTier_and_MAX_Level) {
    Ipp32s rcMethods[] = { CBR, VBR };
    Ipp32u tooHighBitrate[][6] = { // FourCC, Profile, in.Multiplier, in.TargetKbps, out.Multiplier, out.TargetKbps
        {NV12, 0, 16, 55001, 14, 62857}, {0, MAIN, 55001, 16, 14, 62857}, {P010, 0, 1000, 881, 14, 62857}, {0, MAIN10, 880, 1001, 14, 62857},
    };
    enum { TEST_TARGET=1, TEST_MAX=2, TEST_BOTH=TEST_TARGET|TEST_MAX };
    for (auto rcMethod: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcMethod;
        for (Ipp32s mode = TEST_TARGET; mode <= (rcMethod == CBR ? TEST_TARGET : TEST_BOTH); mode++) {
            for (auto p: tooHighBitrate) {
                input.videoParam.mfx.FrameInfo.FourCC = p[0];
                input.videoParam.mfx.CodecProfile = p[1];
                input.videoParam.mfx.BRCParamMultiplier = p[2];
                input.videoParam.mfx.TargetKbps = (mode & TEST_TARGET) ? p[3] : 0;
                input.videoParam.mfx.MaxKbps = (mode & TEST_MAX) ? p[3] : 0;
                (rcMethod == VBR ? input.videoParam.mfx.MaxKbps : input.videoParam.mfx.TargetKbps) = p[3];
                input.videoParam.mfx.CodecLevel = M10;
                input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(input.videoParam.mfx.FrameInfo.FourCC);
                EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
                EXPECT_EQ(p[1], input.videoParam.mfx.CodecProfile);
                EXPECT_EQ(p[2], input.videoParam.mfx.BRCParamMultiplier);
                if (mode & TEST_TARGET) EXPECT_EQ(p[3], input.videoParam.mfx.TargetKbps);
                if (mode & TEST_MAX)    EXPECT_EQ(p[3], input.videoParam.mfx.MaxKbps);
                EXPECT_EQ(M10, input.videoParam.mfx.CodecLevel);
                EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
                EXPECT_EQ(p[1], output.videoParam.mfx.CodecProfile);
                EXPECT_EQ(p[4], output.videoParam.mfx.BRCParamMultiplier);
                if (mode & TEST_TARGET) EXPECT_EQ(p[5], output.videoParam.mfx.TargetKbps);
                if (mode & TEST_MAX)    EXPECT_EQ(p[5], output.videoParam.mfx.MaxKbps);
                EXPECT_EQ(H62, output.videoParam.mfx.CodecLevel);
            }
        }
    }
}

TEST_F(QueryTest, Conflicts_TargetKbps_MaxKbps_too_large_for_Tier_Level) {
    Ipp32s rcMethods[] = { CBR, VBR };
    Ipp32u ok[][5] = { // FourCC, Profile, Multiplier, TargetKbps, Level
        {NV12, 0, 1, 140,   M10}, {0, MAIN, 140,   1, M10}, {P010, 0, 7,   20,  M10}, {0, MAIN10, 14,  10,  M10}, {NV16, 0, 1, 234,   M10}, {P210, 0, 234,   1, M10}, {0, REXT, 18,  13,  M10},
        {NV12, 0, 1, 141,   M20}, {0, MAIN, 141,   1, M20}, {P010, 0, 7,   21,  M20}, {0, MAIN10, 14,  11,  M20}, {NV16, 0, 1, 235,   M20}, {P210, 0, 235,   1, M20}, {0, REXT, 18,  14,  M20},
        {NV12, 0, 1, 1650,  M20}, {0, MAIN, 1650,  1, M20}, {P010, 0, 5,   330, M20}, {0, MAIN10, 50,  33,  M20}, {NV16, 0, 1, 2749,  M20}, {P210, 0, 2749,  1, M20}, {0, REXT, 12,  229, M20},
        {NV12, 0, 1, 1651,  M21}, {0, MAIN, 1651,  1, M21}, {P010, 0, 5,   331, M21}, {0, MAIN10, 51,  33,  M21}, {NV16, 0, 1, 2750,  M21}, {P210, 0, 2750,  1, M21}, {0, REXT, 12,  230, M21},
        {NV12, 0, 1, 3300,  M21}, {0, MAIN, 3300,  1, M21}, {P010, 0, 25,  132, M21}, {0, MAIN10, 66,  50,  M21}, {NV16, 0, 1, 5499,  M21}, {P210, 0, 5499,  1, M21}, {0, REXT, 9,   611, M21},
        {NV12, 0, 1, 3301,  M30}, {0, MAIN, 3301,  1, M30}, {P010, 0, 25,  133, M30}, {0, MAIN10, 67,  50,  M30}, {NV16, 0, 1, 5500,  M30}, {P210, 0, 5500,  1, M30}, {0, REXT, 9,   612, M30},
        {NV12, 0, 1, 6600,  M30}, {0, MAIN, 6600,  1, M30}, {P010, 0, 25,  264, M30}, {0, MAIN10, 132, 50,  M30}, {NV16, 0, 1, 10998, M30}, {P210, 0, 10998, 1, M30}, {0, REXT, 18,  611, M30},
        {NV12, 0, 1, 6601,  M31}, {0, MAIN, 6601,  1, M31}, {P010, 0, 25,  265, M31}, {0, MAIN10, 133, 50,  M31}, {NV16, 0, 1, 10999, M31}, {P210, 0, 10999, 1, M31}, {0, REXT, 18,  612, M31},
        {NV12, 0, 1, 11000, M31}, {0, MAIN, 11000, 1, M31}, {P010, 0, 50,  220, M31}, {0, MAIN10, 220, 50,  M31}, {NV16, 0, 1, 18330, M31}, {P210, 0, 18330, 1, M31}, {0, REXT, 30,  611, M31},
        {NV12, 0, 1, 11001, M40}, {0, MAIN, 11001, 1, M40}, {P010, 0, 50,  221, M40}, {0, MAIN10, 221, 50,  M40}, {NV16, 0, 1, 18331, M40}, {P210, 0, 18331, 1, M40}, {0, REXT, 30,  612, M40},
        {NV12, 0, 1, 13200, M40}, {0, MAIN, 13200, 1, M40}, {P010, 0, 60,  220, M40}, {0, MAIN10, 220, 60,  M40}, {NV16, 0, 1, 21996, M40}, {P210, 0, 21996, 1, M40}, {0, REXT, 36,  611, M40},
        {NV12, 0, 1, 13201, M41}, {0, MAIN, 13201, 1, M41}, {P010, 0, 60,  221, M41}, {0, MAIN10, 221, 60,  M41}, {NV16, 0, 1, 21997, M41}, {P210, 0, 21997, 1, M41}, {0, REXT, 36,  612, M41},
        {NV12, 0, 1, 22000, M41}, {0, MAIN, 22000, 1, M41}, {P010, 0, 50,  440, M41}, {0, MAIN10, 440, 50,  M41}, {NV16, 0, 1, 36660, M41}, {P210, 0, 36660, 1, M41}, {0, REXT, 60,  611, M41},
        {NV12, 0, 1, 22001, M50}, {0, MAIN, 22001, 1, M50}, {P010, 0, 50,  441, M50}, {0, MAIN10, 441, 50,  M50}, {NV16, 0, 1, 36661, M50}, {P210, 0, 36661, 1, M50}, {0, REXT, 60,  612, M50},
        {NV12, 0, 1, 27500, M50}, {0, MAIN, 27500, 1, M50}, {P010, 0, 50,  550, M50}, {0, MAIN10, 550, 50,  M50}, {NV16, 0, 1, 45825, M50}, {P210, 0, 45825, 1, M50}, {0, REXT, 75,  611, M50},
        {NV12, 0, 1, 27501, M51}, {0, MAIN, 27501, 1, M51}, {P010, 0, 50,  551, M51}, {0, MAIN10, 551, 50,  M51}, {NV16, 0, 1, 45826, M51}, {P210, 0, 45826, 1, M51}, {0, REXT, 75,  612, M51},
        {NV12, 0, 1, 44000, M51}, {0, MAIN, 44000, 1, M51}, {P010, 0, 50,  880, M51}, {0, MAIN10, 880, 50,  M51}, {NV16, 0, 2, 36660, M51}, {P210, 0, 36660, 2, M51}, {0, REXT, 120, 611, M51},
        {NV12, 0, 1, 44001, M52}, {0, MAIN, 44001, 1, M52}, {P010, 0, 50,  881, M52}, {0, MAIN10, 881, 50,  M52}, {NV16, 0, 2, 36661, M52}, {P210, 0, 36661, 2, M52}, {0, REXT, 120, 612, M52},
        {NV12, 0, 2, 33000, M52}, {0, MAIN, 33000, 2, M52}, {P010, 0, 75,  880, M52}, {0, MAIN10, 880, 75,  M52}, {NV16, 0, 2, 54990, M52}, {P210, 0, 54990, 2, M52}, {0, REXT, 180, 611, M52},
        {NV12, 0, 1, 44001, M60}, {0, MAIN, 44001, 1, M60}, {P010, 0, 50,  881, M60}, {0, MAIN10, 881, 50,  M60}, {NV16, 0, 2, 36661, M60}, {P210, 0, 36661, 2, M60}, {0, REXT, 120, 612, M60},
        {NV12, 0, 2, 33000, M60}, {0, MAIN, 33000, 2, M60}, {P010, 0, 75,  880, M60}, {0, MAIN10, 880, 75,  M60}, {NV16, 0, 2, 54990, M60}, {P210, 0, 54990, 2, M60}, {0, REXT, 180, 611, M60},
        {NV12, 0, 2, 33001, M61}, {0, MAIN, 33001, 2, M61}, {P010, 0, 75,  881, M61}, {0, MAIN10, 881, 75,  M61}, {NV16, 0, 2, 54991, M61}, {P210, 0, 54991, 2, M61}, {0, REXT, 180, 612, M61},
        {NV12, 0, 3, 44000, M61}, {0, MAIN, 44000, 3, M61}, {P010, 0, 150, 880, M61}, {0, MAIN10, 880, 150, M61}, {NV16, 0, 4, 54990, M61}, {P210, 0, 54990, 4, M61}, {0, REXT, 360, 611, M61},
        {NV12, 0, 3, 44001, M62}, {0, MAIN, 44001, 3, M62}, {P010, 0, 150, 881, M62}, {0, MAIN10, 881, 150, M62}, {NV16, 0, 4, 54991, M62}, {P210, 0, 54991, 4, M62}, {0, REXT, 360, 612, M62},
        {NV12, 0, 5, 52800, M62}, {0, MAIN, 52800, 5, M62}, {P010, 0, 300, 880, M62}, {0, MAIN10, 880, 300, M62}, {NV16, 0, 8, 54990, M62}, {P210, 0, 54990, 8, M62}, {0, REXT, 720, 611, M62},
        {NV12, 0, 1, 140,   H40}, {0, MAIN, 140,   1, H40}, {P010, 0, 7,   20,  H40}, {0, MAIN10, 14,  10,  H40}, {NV16, 0, 1, 234,   H40}, {P210, 0, 234,   1, H40}, {0, REXT, 18,  13,  H40},
        {NV12, 0, 1, 33000, H40}, {0, MAIN, 33000, 1, H40}, {P010, 0, 50,  660, H40}, {0, MAIN10, 660, 50,  H40}, {NV16, 0, 1, 54990, H40}, {P210, 0, 54990, 1, H40}, {0, REXT, 90,  611, H40},
        {NV12, 0, 1, 33001, H41}, {0, MAIN, 33001, 1, H41}, {P010, 0, 50,  661, H41}, {0, MAIN10, 661, 50,  H41}, {NV16, 0, 1, 54991, H41}, {P210, 0, 54991, 1, H41}, {0, REXT, 90,  612, H41},
        {NV12, 0, 1, 55000, H41}, {0, MAIN, 55000, 1, H41}, {P010, 0, 100, 550, H41}, {0, MAIN10, 550, 100, H41}, {NV16, 0, 2, 45825, H41}, {P210, 0, 45825, 2, H41}, {0, REXT, 150, 611, H41},
        {NV12, 0, 1, 55001, H50}, {0, MAIN, 55001, 1, H50}, {P010, 0, 100, 551, H50}, {0, MAIN10, 551, 100, H50}, {NV16, 0, 2, 45826, H50}, {P210, 0, 45826, 2, H50}, {0, REXT, 150, 612, H50},
        {NV12, 0, 2, 55000, H50}, {0, MAIN, 55000, 2, H50}, {P010, 0, 200, 550, H50}, {0, MAIN10, 550, 200, H50}, {NV16, 0, 3, 61100, H50}, {P210, 0, 61100, 3, H50}, {0, REXT, 300, 611, H50},
        {NV12, 0, 2, 55001, H51}, {0, MAIN, 55001, 2, H51}, {P010, 0, 200, 551, H51}, {0, MAIN10, 551, 200, H51}, {NV16, 0, 3, 61101, H51}, {P210, 0, 61101, 3, H51}, {0, REXT, 300, 612, H51},
        {NV12, 0, 4, 44000, H51}, {0, MAIN, 44000, 4, H51}, {P010, 0, 200, 880, H51}, {0, MAIN10, 880, 200, H51}, {NV16, 0, 5, 58656, H51}, {P210, 0, 58656, 5, H51}, {0, REXT, 480, 611, H51},
        {NV12, 0, 4, 44001, H52}, {0, MAIN, 44001, 4, H52}, {P010, 0, 200, 881, H52}, {0, MAIN10, 881, 200, H52}, {NV16, 0, 5, 58657, H52}, {P210, 0, 58657, 5, H52}, {0, REXT, 480, 612, H52},
        {NV12, 0, 5, 52800, H52}, {0, MAIN, 52800, 5, H52}, {P010, 0, 300, 880, H52}, {0, MAIN10, 880, 300, H52}, {NV16, 0, 7, 62845, H52}, {P210, 0, 62845, 7, H52}, {0, REXT, 720, 611, H52},
        {NV12, 0, 4, 44001, H60}, {0, MAIN, 44001, 4, H60}, {P010, 0, 200, 881, H60}, {0, MAIN10, 881, 200, H60}, {NV16, 0, 5, 58657, H60}, {P210, 0, 58657, 5, H60}, {0, REXT, 480, 612, H60},
        {NV12, 0, 5, 52800, H60}, {0, MAIN, 52800, 5, H60}, {P010, 0, 300, 880, H60}, {0, MAIN10, 880, 300, H60}, {NV16, 0, 8, 54990, H60}, {P210, 0, 54990, 8, H60}, {0, REXT, 720, 611, H60},
        {NV12, 0, 5, 52801, H61}, {0, MAIN, 52801, 5, H61}, {P010, 0, 300, 881, H61}, {0, MAIN10, 881, 300, H61}, {NV16, 0, 8, 54991, H61}, {P210, 0, 54991, 8, H61}, {0, REXT, 720, 612, H61},
        {NV12, 0, 10,52800, H61}, {0, MAIN, 52800, 10,H61}, {P010, 0, 600, 880, H61}, {0, MAIN10, 880, 600, H61}, {NV16, 0, 16,54990, H61}, {P210, 0, 54990, 16,H61}, {0, REXT, 720, 611, H61},
        {NV12, 0, 10,52801, H62}, {0, MAIN, 52801, 10,H62}, {P010, 0, 600, 881, H62}, {0, MAIN10, 881, 600, H62}, {NV16, 0, 16,54991, H62}, {P210, 0, 54991, 16,H62}, {0, REXT, 720, 612, H62},
        {NV12, 0, 16,55000, H62}, {0, MAIN, 55000, 16,H62}, {P010, 0, 1000,880, H62}, {0, MAIN10, 880, 1000,H62}, {NV16, 0, 24,61100, H62}, {P210, 0, 61100, 24,H62}, {0, REXT, 1200,1222,H62},
    };
    for (auto rcMethod: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcMethod;
        for (auto p: ok) {
            input.videoParam.mfx.FrameInfo.FourCC = p[0];
            input.videoParam.mfx.CodecProfile = p[1];
            input.videoParam.mfx.BRCParamMultiplier = p[2];
            input.videoParam.mfx.TargetKbps = p[3];
            input.videoParam.mfx.MaxKbps = p[3];
            input.videoParam.mfx.CodecLevel = p[4];
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], input.videoParam.mfx.TargetKbps);
            EXPECT_EQ(p[3], input.videoParam.mfx.MaxKbps);
            EXPECT_EQ(p[4], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], output.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], output.videoParam.mfx.TargetKbps);
            if (rcMethod == VBR) EXPECT_EQ(p[3], output.videoParam.mfx.MaxKbps);
            EXPECT_EQ(p[4], output.videoParam.mfx.CodecLevel);
        }
    }

    Ipp32u correctableLevel[][6] = { // FourCC, Profile, Multiplier, TargetKbps, in.Level, out.Level
        {NV12, 0, 1, 141,   M10, M20}, {0, MAIN, 141,   1, M10, M20}, {P010, 0, 7,   21,  M10, M20}, {0, MAIN10, 14,  11,  M10, M20}, {NV16, 0, 1, 235,   M10, M20}, {P210, 0, 235,   1, M10, M20}, {0, REXT, 18,  14,  M10, M20},
        {NV12, 0, 1, 1651,  M10, M21}, {0, MAIN, 1651,  1, M10, M21}, {P010, 0, 5,   331, M10, M21}, {0, MAIN10, 51,  33,  M10, M21}, {NV16, 0, 1, 2750,  M10, M21}, {P210, 0, 2750,  1, M10, M21}, {0, REXT, 12,  230, M10, M21},
        {NV12, 0, 1, 3301,  M10, M30}, {0, MAIN, 3301,  1, M10, M30}, {P010, 0, 25,  133, M10, M30}, {0, MAIN10, 67,  50,  M10, M30}, {NV16, 0, 1, 5500,  M10, M30}, {P210, 0, 5500,  1, M10, M30}, {0, REXT, 9,   612, M10, M30},
        {NV12, 0, 1, 6601,  M10, M31}, {0, MAIN, 6601,  1, M10, M31}, {P010, 0, 25,  265, M10, M31}, {0, MAIN10, 133, 50,  M10, M31}, {NV16, 0, 1, 10999, M10, M31}, {P210, 0, 10999, 1, M10, M31}, {0, REXT, 18,  612, M10, M31},
        {NV12, 0, 1, 11001, M10, M40}, {0, MAIN, 11001, 1, M10, M40}, {P010, 0, 50,  221, M10, M40}, {0, MAIN10, 221, 50,  M10, M40}, {NV16, 0, 1, 18331, M10, M40}, {P210, 0, 18331, 1, M10, M40}, {0, REXT, 30,  612, M10, M40},
        {NV12, 0, 1, 13201, M10, H40}, {0, MAIN, 13201, 1, M10, H40}, {P010, 0, 60,  221, M10, H40}, {0, MAIN10, 221, 60,  M10, H40}, {NV16, 0, 1, 21997, M10, H40}, {P210, 0, 21997, 1, M10, H40}, {0, REXT, 36,  612, M10, H40},
        {NV12, 0, 1, 33000, M50, H50}, {0, MAIN, 33000, 1, M50, H50}, {P010, 0, 50,  660, M50, H50}, {0, MAIN10, 660, 50,  M50, H50}, {NV16, 0, 1, 54990, M50, H50}, {P210, 0, 54990, 1, M50, H50}, {0, REXT, 90,  611, M50, H50},
        {NV12, 0, 1, 33000, M50, H50}, {0, MAIN, 33000, 1, M50, H50}, {P010, 0, 50,  660, M50, H50}, {0, MAIN10, 660, 50,  M50, H50}, {NV16, 0, 1, 54990, M50, H50}, {P210, 0, 54990, 1, M50, H50}, {0, REXT, 90,  611, M50, H50},
        {NV12, 0, 2, 55000, M60, H60}, {0, MAIN, 55000, 2, M60, H60}, {P010, 0, 200, 550, M60, H60}, {0, MAIN10, 550, 200, M60, H60}, {NV16, 0, 3, 61100, M60, H60}, {P210, 0, 61100, 3, M60, H60}, {0, REXT, 300, 611, M60, H60},
    };
    for (auto rcMethod: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcMethod;
        for (auto p: correctableLevel) {
            input.videoParam.mfx.FrameInfo.FourCC = p[0];
            input.videoParam.mfx.CodecProfile = p[1];
            input.videoParam.mfx.BRCParamMultiplier = p[2];
            input.videoParam.mfx.TargetKbps = p[3];
            input.videoParam.mfx.MaxKbps = p[3];
            input.videoParam.mfx.CodecLevel = p[4];
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], input.videoParam.mfx.TargetKbps);
            EXPECT_EQ(p[3], input.videoParam.mfx.MaxKbps);
            EXPECT_EQ(p[4], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], output.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], output.videoParam.mfx.TargetKbps);
            if (rcMethod == VBR) EXPECT_EQ(p[3], output.videoParam.mfx.MaxKbps);
            EXPECT_EQ(p[5], output.videoParam.mfx.CodecLevel);
        }
    }
}

TEST_F(QueryTest, Conflicts_BufferSizeInKB_too_small) {
    Ipp32u ok[][6] = { // RateControlMethod, BRCParamMultiplier, TargetKbps, FrameRateExtN, FrameRateExtD, BufferSizeInKB
        {CBR, 0, 1000, 0xffffffeb, 0xa3d70a3, 10}, {VBR, 0, 6000, 0xfffffff0, 0x8888888, 50},
        {CBR, 2, 1000, 0xffffffeb, 0xa3d70a3, 10}, {VBR, 2, 6000, 0xfffffff0, 0x8888888, 50} };
    for (auto p: ok) {
        input.videoParam.mfx.RateControlMethod = p[0];
        input.videoParam.mfx.BRCParamMultiplier = p[1];
        input.videoParam.mfx.TargetKbps = p[2];
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[3];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[4];
        input.videoParam.mfx.BufferSizeInKB = p[5];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(p[1], input.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[2], input.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[5], input.videoParam.mfx.BufferSizeInKB);
        EXPECT_EQ(p[0], output.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(p[1], output.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[2], output.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], output.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[5], output.videoParam.mfx.BufferSizeInKB);
    }
    Ipp32u warning[][8] = { // RateControlMethod, BRCParamMultiplier, TargetKbps, FrameRateExtN, FrameRateExtD, BufferSizeInKB, out.TargetKbs, out.BufferSizeInKB
        {CBR, 0, 1000, 0xffffffeb, 0xa3d70a3, 9, 1000, 10}, {VBR, 0, 6000, 0xfffffff0, 0x8888888, 49, 6000, 50},
        {CBR, 2, 1000, 0xffffffeb, 0xa3d70a3, 9, 2000, 20}, {VBR, 2, 6000, 0xfffffff0, 0x8888888, 49, 12000,100} };
    for (auto p: warning) {
        input.videoParam.mfx.RateControlMethod = p[0];
        input.videoParam.mfx.BRCParamMultiplier = p[1];
        input.videoParam.mfx.TargetKbps = p[2];
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[3];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[4];
        input.videoParam.mfx.BufferSizeInKB = p[5];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(p[1], input.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[2], input.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[5], input.videoParam.mfx.BufferSizeInKB);
        EXPECT_EQ(p[0], output.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(1, output.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[6], output.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], output.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[7], output.videoParam.mfx.BufferSizeInKB);
    }
}

TEST_F(QueryTest, Conflicts_InitialDelayInKB_too_small) {
    Ipp32u ok[][6] = { // RateControlMethod, BRCParamMultiplier, TargetKbps, FrameRateExtN, FrameRateExtD, BufferSizeInKB
        {CBR, 0, 1000, 0xffffffeb, 0xa3d70a3, 5}, {VBR, 0, 6000, 0xfffffff0, 0x8888888, 25},
        {CBR, 2, 1000, 0xffffffeb, 0xa3d70a3, 5}, {VBR, 2, 6000, 0xfffffff0, 0x8888888, 25} };
    for (auto p: ok) {
        input.videoParam.mfx.RateControlMethod = p[0];
        input.videoParam.mfx.BRCParamMultiplier = p[1];
        input.videoParam.mfx.TargetKbps = p[2];
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[3];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[4];
        input.videoParam.mfx.InitialDelayInKB = p[5];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(p[1], input.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[2], input.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[5], input.videoParam.mfx.InitialDelayInKB);
        EXPECT_EQ(p[0], output.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(p[1], output.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[2], output.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], output.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[5], output.videoParam.mfx.InitialDelayInKB);
    }
    Ipp32u warning[][8] = { // RateControlMethod, BRCParamMultiplier, TargetKbps, FrameRateExtN, FrameRateExtD, BufferSizeInKB, out.TargetKbs, out.BufferSizeInKB
        {CBR, 0, 2000, 0xffffffeb, 0xa3d70a3, 9, 2000, 10}, {VBR, 0, 6000, 0xfffffff0, 0x8888888, 24, 6000, 25},
        {CBR, 2, 2000, 0xffffffeb, 0xa3d70a3, 9, 4000, 20}, {VBR, 2, 6000, 0xfffffff0, 0x8888888, 24, 12000,50} };
    for (auto p: warning) {
        input.videoParam.mfx.RateControlMethod = p[0];
        input.videoParam.mfx.BRCParamMultiplier = p[1];
        input.videoParam.mfx.TargetKbps = p[2];
        input.videoParam.mfx.FrameInfo.FrameRateExtN = p[3];
        input.videoParam.mfx.FrameInfo.FrameRateExtD = p[4];
        input.videoParam.mfx.InitialDelayInKB = p[5];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(p[1], input.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[2], input.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], input.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], input.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[5], input.videoParam.mfx.InitialDelayInKB);
        EXPECT_EQ(p[0], output.videoParam.mfx.RateControlMethod);
        EXPECT_EQ(1, output.videoParam.mfx.BRCParamMultiplier);
        EXPECT_EQ(p[6], output.videoParam.mfx.TargetKbps);
        EXPECT_EQ(p[3], output.videoParam.mfx.FrameInfo.FrameRateExtN);
        EXPECT_EQ(p[4], output.videoParam.mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(p[7], output.videoParam.mfx.InitialDelayInKB);
    }
}

TEST_F(QueryTest, Conflicts_BufferSizeInKB_and_InitialDelayInKB_too_large_for_HighTier_and_MAX_Level) {
    enum { BUFSIZE, INITDELAY };
    const struct { Ipp32s rcMethod, testField; } testmodes[4] = {
        {CBR, BUFSIZE}, {CBR, INITDELAY}, {VBR, BUFSIZE}, {VBR, INITDELAY} }; 

    Ipp32u tooHighBuffer[][6] = { // FourCC, Profile, in.Multiplier, in.BufferSizeInKB, out.Multiplier, out.BufferSizeInKB
        {NV12, 0, 16, 6876, 2, 55000}, {0, MAIN, 6876, 16, 2, 55000}, {P010, 0, 1000, 111, 2, 55000}, {0, MAIN10, 111, 1000, 2, 55000},
    };
    for (auto mode: testmodes) {
        input.videoParam.mfx.RateControlMethod = mode.rcMethod;
        input.videoParam.mfx.BufferSizeInKB = 0;
        input.videoParam.mfx.InitialDelayInKB = 0;
        Ipp16u &inTestField  = (mode.testField == BUFSIZE) ? input.videoParam.mfx.BufferSizeInKB  : input.videoParam.mfx.InitialDelayInKB;
        Ipp16u &outTestField = (mode.testField == BUFSIZE) ? output.videoParam.mfx.BufferSizeInKB : output.videoParam.mfx.InitialDelayInKB;
        for (auto p: tooHighBuffer) {
            input.videoParam.mfx.FrameInfo.FourCC = p[0];
            input.videoParam.mfx.CodecProfile = p[1];
            input.videoParam.mfx.BRCParamMultiplier = p[2];
            inTestField = p[3];
            input.videoParam.mfx.CodecLevel = M10;
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], inTestField);
            EXPECT_EQ(M10, input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], output.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[4], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[5], outTestField);
            EXPECT_EQ(H62, output.videoParam.mfx.CodecLevel);
        }
    }
}

TEST_F(QueryTest, Conflicts_BufferSizeInKB_and_InitialDelayInKB_too_large_for_Tier_Level) {
    enum { BUFSIZE, INITDELAY };
    const struct { Ipp32s rcMethod, testField; } testmodes[4] = {
        {CBR, BUFSIZE}, {CBR, INITDELAY}, {VBR, BUFSIZE}, {VBR, INITDELAY} }; 

    Ipp32u ok[][5] = { // FourCC, Profile, Multiplier, BufferSizeInKB, Level
        {NV12, 0, 1, 48,    M10}, {0, MAIN, 48,    1, M10}, {P010, 0, 3,   16, M10}, {0, MAIN10,  16, 3,  M10}, {NV16, 0, 1, 80,    M10}, {P210, 0, 80,    1, M10}, {0, REXT, 8,   10, M10},
        {NV12, 0, 1, 49,    M20}, {0, MAIN, 49,    1, M20}, {P010, 0, 3,   17, M20}, {0, MAIN10,  17, 3,  M20}, {NV16, 0, 1, 81,    M20}, {P210, 0, 81,    1, M20}, {0, REXT, 8,   11, M20},
        {NV12, 0, 1, 206,   M20}, {0, MAIN, 206,   1, M20}, {P010, 0, 2,  103, M20}, {0, MAIN10, 103, 2,  M20}, {NV16, 0, 1, 343,   M20}, {P210, 0, 343,   1, M20}, {0, REXT, 7,   49, M20},
        {NV12, 0, 1, 207,   M21}, {0, MAIN, 207,   1, M21}, {P010, 0, 2,  104, M21}, {0, MAIN10, 104, 2,  M21}, {NV16, 0, 1, 344,   M21}, {P210, 0, 344,   1, M21}, {0, REXT, 7,   50, M21},
        {NV12, 0, 1, 412,   M21}, {0, MAIN, 412,   1, M21}, {P010, 0, 4,  103, M21}, {0, MAIN10, 103, 4,  M21}, {NV16, 0, 1, 687,   M21}, {P210, 0, 687,   1, M21}, {0, REXT, 3,  229, M21},
        {NV12, 0, 1, 413,   M30}, {0, MAIN, 413,   1, M30}, {P010, 0, 4,  104, M30}, {0, MAIN10, 104, 4,  M30}, {NV16, 0, 1, 688,   M30}, {P210, 0, 688,   1, M30}, {0, REXT, 3,  230, M30},
        {NV12, 0, 1, 825,   M30}, {0, MAIN, 825,   1, M30}, {P010, 0, 5,  165, M30}, {0, MAIN10, 165, 5,  M30}, {NV16, 0, 1, 1374,  M30}, {P210, 0, 1374,  1, M30}, {0, REXT, 6,  229, M30},
        {NV12, 0, 1, 826,   M31}, {0, MAIN, 826,   1, M31}, {P010, 0, 5,  166, M31}, {0, MAIN10, 166, 5,  M31}, {NV16, 0, 1, 1375,  M31}, {P210, 0, 1375,  1, M31}, {0, REXT, 6,  230, M31},
        {NV12, 0, 1, 1375,  M31}, {0, MAIN, 1375,  1, M31}, {P010, 0, 5,  275, M31}, {0, MAIN10, 275, 5,  M31}, {NV16, 0, 1, 2291,  M31}, {P210, 0, 2291,  1, M31}, {0, REXT, 29,  79, M31},
        {NV12, 0, 1, 1376,  M40}, {0, MAIN, 1376,  1, M40}, {P010, 0, 5,  276, M40}, {0, MAIN10, 276, 5,  M40}, {NV16, 0, 1, 2292,  M40}, {P210, 0, 2292,  1, M40}, {0, REXT, 29,  80, M40},
        {NV12, 0, 1, 1650,  M40}, {0, MAIN, 1650,  1, M40}, {P010, 0, 5,  330, M40}, {0, MAIN10, 330, 5,  M40}, {NV16, 0, 1, 2749,  M40}, {P210, 0, 2749,  1, M40}, {0, REXT, 12, 229, M40},
        {NV12, 0, 1, 1651,  M41}, {0, MAIN, 1651,  1, M41}, {P010, 0, 5,  331, M41}, {0, MAIN10, 331, 5,  M41}, {NV16, 0, 1, 2750,  M41}, {P210, 0, 2750,  1, M41}, {0, REXT, 12, 230, M41},
        {NV12, 0, 1, 2750,  M41}, {0, MAIN, 2750,  1, M41}, {P010, 0, 5,  550, M41}, {0, MAIN10, 550, 5,  M41}, {NV16, 0, 1, 4582,  M41}, {P210, 0, 4582,  1, M41}, {0, REXT, 29, 158, M41},
        {NV12, 0, 1, 2751,  M50}, {0, MAIN, 2751,  1, M50}, {P010, 0, 5,  551, M50}, {0, MAIN10, 551, 5,  M50}, {NV16, 0, 1, 4583,  M50}, {P210, 0, 4583,  1, M50}, {0, REXT, 29, 159, M50},
        {NV12, 0, 1, 3437,  M50}, {0, MAIN, 3437,  1, M50}, {P010, 0, 7,  491, M50}, {0, MAIN10, 491, 7,  M50}, {NV16, 0, 1, 5728,  M50}, {P210, 0, 5728,  1, M50}, {0, REXT, 16, 358, M50},
        {NV12, 0, 1, 3438,  M51}, {0, MAIN, 3438,  1, M51}, {P010, 0, 7,  492, M51}, {0, MAIN10, 492, 7,  M51}, {NV16, 0, 1, 5729,  M51}, {P210, 0, 5729,  1, M51}, {0, REXT, 16, 359, M51},
        {NV12, 0, 1, 5500,  M51}, {0, MAIN, 5500,  1, M51}, {P010, 0, 11, 500, M51}, {0, MAIN10, 500, 11, M51}, {NV16, 0, 1, 9165,  M51}, {P210, 0, 9165,  1, M51}, {0, REXT, 15, 611, M51},
        {NV12, 0, 1, 5501,  M52}, {0, MAIN, 5501,  1, M52}, {P010, 0, 11, 501, M52}, {0, MAIN10, 501, 11, M52}, {NV16, 0, 1, 9166,  M52}, {P210, 0, 9166,  1, M52}, {0, REXT, 15, 612, M52},
        {NV12, 0, 1, 8250,  M52}, {0, MAIN, 8250,  1, M52}, {P010, 0, 11, 750, M52}, {0, MAIN10, 750, 11, M52}, {NV16, 0, 1, 13747, M52}, {P210, 0, 13747, 1, M52}, {0, REXT, 59, 233, M52},
        {NV12, 0, 1, 5501,  M60}, {0, MAIN, 5501,  1, M60}, {P010, 0, 11, 501, M60}, {0, MAIN10, 501, 11, M60}, {NV16, 0, 1, 9166,  M60}, {P210, 0, 9166,  1, M60}, {0, REXT, 15, 612, M60},
        {NV12, 0, 1, 8250,  M60}, {0, MAIN, 8250,  1, M60}, {P010, 0, 11, 750, M60}, {0, MAIN10, 750, 11, M60}, {NV16, 0, 1, 13747, M60}, {P210, 0, 13747, 1, M60}, {0, REXT, 59, 233, M60},
        {NV12, 0, 1, 8251,  M61}, {0, MAIN, 8251,  1, M61}, {P010, 0, 11, 751, M61}, {0, MAIN10, 751, 11, M61}, {NV16, 0, 1, 13748, M61}, {P210, 0, 13748, 1, M61}, {0, REXT, 59, 234, M61},
        {NV12, 0, 1, 16500, M61}, {0, MAIN, 16500, 1, M61}, {P010, 0, 25, 660, M61}, {0, MAIN10, 660, 25, M61}, {NV16, 0, 1, 27495, M61}, {P210, 0, 27495, 1, M61}, {0, REXT, 45, 611, M61},
        {NV12, 0, 1, 16501, M62}, {0, MAIN, 16501, 1, M62}, {P010, 0, 25, 661, M62}, {0, MAIN10, 661, 25, M62}, {NV16, 0, 1, 27496, M62}, {P210, 0, 27496, 1, M62}, {0, REXT, 45, 612, M62},
        {NV12, 0, 1, 33000, M62}, {0, MAIN, 33000, 1, M62}, {P010, 0, 50, 660, M62}, {0, MAIN10, 660, 50, M62}, {NV16, 0, 1, 54990, M62}, {P210, 0, 54990, 1, M62}, {0, REXT, 90, 611, M62},
        {NV12, 0, 1, 48,    H40}, {0, MAIN, 48,    1, H40}, {P010, 0, 4,  12,  H40}, {0, MAIN10, 12,  4,  H40}, {NV16, 0, 1, 80,    H40}, {P210, 0, 80,    1, H40}, {0, REXT, 4,  20, H40},
        {NV12, 0, 1, 4125,  H40}, {0, MAIN, 4125,  1, H40}, {P010, 0, 25, 165, H40}, {0, MAIN10, 165, 25, H40}, {NV16, 0, 1, 6873,  H40}, {P210, 0, 6873,  1, H40}, {0, REXT, 29, 237, H40},
        {NV12, 0, 1, 4126,  H41}, {0, MAIN, 4126,  1, H41}, {P010, 0, 25, 166, H41}, {0, MAIN10, 166, 25, H41}, {NV16, 0, 1, 6874,  H41}, {P210, 0, 6874,  1, H41}, {0, REXT, 29, 238, H41},
        {NV12, 0, 1, 6875,  H41}, {0, MAIN, 6875,  1, H41}, {P010, 0, 25, 275, H41}, {0, MAIN10, 275, 25, H41}, {NV16, 0, 1, 11456, H41}, {P210, 0, 11456, 1, H41}, {0, REXT, 16, 716, H41},
        {NV12, 0, 1, 6876,  H50}, {0, MAIN, 6876,  1, H50}, {P010, 0, 25, 276, H50}, {0, MAIN10, 276, 25, H50}, {NV16, 0, 1, 11457, H50}, {P210, 0, 11457, 1, H50}, {0, REXT, 16, 717, H50},
        {NV12, 0, 1, 13750, H50}, {0, MAIN, 13750, 1, H50}, {P010, 0, 25, 550, H50}, {0, MAIN10, 550, 25, H50}, {NV16, 0, 1, 22912, H50}, {P210, 0, 22912, 1, H50}, {0, REXT, 32, 716, H50},
        {NV12, 0, 1, 13751, H51}, {0, MAIN, 13751, 1, H51}, {P010, 0, 25, 551, H51}, {0, MAIN10, 551, 25, H51}, {NV16, 0, 1, 22913, H51}, {P210, 0, 22913, 1, H51}, {0, REXT, 32, 717, H51},
        {NV12, 0, 1, 22000, H51}, {0, MAIN, 22000, 1, H51}, {P010, 0, 25, 880, H51}, {0, MAIN10, 880, 25, H51}, {NV16, 0, 1, 36660, H51}, {P210, 0, 36660, 1, H51}, {0, REXT, 60, 611, H51},
        {NV12, 0, 1, 22001, H52}, {0, MAIN, 22001, 1, H52}, {P010, 0, 25, 881, H52}, {0, MAIN10, 881, 25, H52}, {NV16, 0, 1, 36661, H52}, {P210, 0, 36661, 1, H52}, {0, REXT, 60, 612, H52},
        {NV12, 0, 1, 33000, H52}, {0, MAIN, 33000, 1, H52}, {P010, 0, 50, 660, H52}, {0, MAIN10, 660, 50, H52}, {NV16, 0, 1, 54990, H52}, {P210, 0, 54990, 1, H52}, {0, REXT, 90, 611, H52},
        {NV12, 0, 1, 22001, H60}, {0, MAIN, 22001, 1, H60}, {P010, 0, 25, 881, H60}, {0, MAIN10, 881, 25, H60}, {NV16, 0, 1, 36661, H60}, {P210, 0, 36661, 1, H60}, {0, REXT, 60, 612, H60},
        {NV12, 0, 1, 33000, H60}, {0, MAIN, 33000, 1, H60}, {P010, 0, 50, 660, H60}, {0, MAIN10, 660, 50, H60}, {NV16, 0, 1, 54990, H60}, {P210, 0, 54990, 1, H60}, {0, REXT, 90, 611, H60},
        {NV12, 0, 1, 33001, H61}, {0, MAIN, 33001, 1, H61}, {P010, 0, 50, 661, H61}, {0, MAIN10, 661, 50, H61}, {NV16, 0, 1, 54991, H61}, {P210, 0, 54991, 1, H61}, {0, REXT, 90, 612, H61},
        {NV12, 0, 2, 33000, H61}, {0, MAIN, 33000, 2, H61}, {P010, 0, 75, 880, H61}, {0, MAIN10, 880, 75, H61}, {NV16, 0, 2, 54990, H61}, {P210, 0, 54990, 2, H61}, {0, REXT, 90, 1222,H61},
        {NV12, 0, 2, 33001, H62}, {0, MAIN, 33001, 2, H62}, {P010, 0, 75, 881, H62}, {0, MAIN10, 881, 75, H62}, {NV16, 0, 2, 54991, H62}, {P210, 0, 54991, 2, H62}, {0, REXT, 90, 1223,H62},
        {NV12, 0, 2, 55000, H62}, {0, MAIN, 55000, 2, H62}, {P010, 0, 50, 2200,H62}, {0, MAIN10, 2200,50, H62}, {NV16, 0, 3, 61100, H62}, {P210, 0, 61100, 3, H62}, {0, REXT, 75, 2444,H62},
    };

    for (auto mode: testmodes) {
        input.videoParam.mfx.RateControlMethod = mode.rcMethod;
        input.videoParam.mfx.BufferSizeInKB = 0;
        input.videoParam.mfx.InitialDelayInKB = 0;
        Ipp16u &inTestField  = (mode.testField == BUFSIZE) ? input.videoParam.mfx.BufferSizeInKB  : input.videoParam.mfx.InitialDelayInKB;
        Ipp16u &outTestField = (mode.testField == BUFSIZE) ? output.videoParam.mfx.BufferSizeInKB : output.videoParam.mfx.InitialDelayInKB;
        for (auto p: ok) {
            input.videoParam.mfx.FrameInfo.FourCC = p[0];
            input.videoParam.mfx.CodecProfile = p[1];
            input.videoParam.mfx.BRCParamMultiplier = p[2];
            inTestField = p[3];
            input.videoParam.mfx.CodecLevel = p[4];
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], inTestField);
            EXPECT_EQ(p[4], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], output.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], outTestField);
            EXPECT_EQ(p[4], output.videoParam.mfx.CodecLevel);
        }
    }

    Ipp32u correctableLevel[][6] = { // FourCC, Profile, Multiplier, BufferSizeInKB, in.Level, out.Level
        {NV12, 0, 1, 49,    M10, M20}, {0, MAIN, 49,    1, M10, M20}, {P010, 0, 7,  7,   M10, M20}, {0, MAIN10, 7,   7,  M10, M20}, {NV16, 0, 1, 81,    M10, M20}, {P210, 0, 81,    1, M10, M20}, {0, REXT, 3,  27,  M10, M20},
        {NV12, 0, 1, 207,   M10, M21}, {0, MAIN, 207,   1, M10, M21}, {P010, 0, 3,  69,  M10, M21}, {0, MAIN10, 69,  3,  M10, M21}, {NV16, 0, 1, 344,   M10, M21}, {P210, 0, 344,   1, M10, M21}, {0, REXT, 4,  86,  M10, M21},
        {NV12, 0, 1, 413,   M10, M30}, {0, MAIN, 413,   1, M10, M30}, {P010, 0, 7,  59,  M10, M30}, {0, MAIN10, 59,  7,  M10, M30}, {NV16, 0, 1, 688,   M10, M30}, {P210, 0, 688,   1, M10, M30}, {0, REXT, 4,  172, M10, M30},
        {NV12, 0, 1, 826,   M10, M31}, {0, MAIN, 826,   1, M10, M31}, {P010, 0, 7,  118, M10, M31}, {0, MAIN10, 118, 7,  M10, M31}, {NV16, 0, 1, 1375,  M10, M31}, {P210, 0, 1375,  1, M10, M31}, {0, REXT, 5,  275, M10, M31},
        {NV12, 0, 1, 1376,  M10, M40}, {0, MAIN, 1376,  1, M10, M40}, {P010, 0, 8,  172, M10, M40}, {0, MAIN10, 172, 8,  M10, M40}, {NV16, 0, 1, 2292,  M10, M40}, {P210, 0, 2292,  1, M10, M40}, {0, REXT, 6,  382, M10, M40},
        {NV12, 0, 1, 1651,  M10, H40}, {0, MAIN, 1651,  1, M10, H40}, {P010, 0, 13, 127, M10, H40}, {0, MAIN10, 127, 13, M10, H40}, {NV16, 0, 1, 2750,  M10, H40}, {P210, 0, 2750,  1, M10, H40}, {0, REXT, 5,  550, M10, H40},
        {NV12, 0, 1, 13750, M50, H50}, {0, MAIN, 13750, 1, M50, H50}, {P010, 0, 50, 275, M50, H50}, {0, MAIN10, 275, 50, M50, H50}, {NV16, 0, 1, 22912, M50, H50}, {P210, 0, 22912, 1, M50, H50}, {0, REXT, 32, 716, M50, H50},
        {NV12, 0, 1, 22000, M50, H51}, {0, MAIN, 22000, 1, M50, H51}, {P010, 0, 50, 440, M50, H51}, {0, MAIN10, 440, 50, M50, H51}, {NV16, 0, 1, 36660, M50, H51}, {P210, 0, 36660, 1, M50, H51}, {0, REXT, 60, 611, M50, H51},
        {NV12, 0, 1, 33000, M60, H60}, {0, MAIN, 33000, 1, M60, H60}, {P010, 0, 50, 660, M60, H60}, {0, MAIN10, 660, 50, M60, H60}, {NV16, 0, 1, 36660, M60, H60}, {P210, 0, 36660, 1, M60, H60}, {0, REXT, 60, 611, M60, H60},
    };
    for (auto mode: testmodes) {
        input.videoParam.mfx.RateControlMethod = mode.rcMethod;
        input.videoParam.mfx.BufferSizeInKB = 0;
        input.videoParam.mfx.InitialDelayInKB = 0;
        Ipp16u &inTestField  = (mode.testField == BUFSIZE) ? input.videoParam.mfx.BufferSizeInKB  : input.videoParam.mfx.InitialDelayInKB;
        Ipp16u &outTestField = (mode.testField == BUFSIZE) ? output.videoParam.mfx.BufferSizeInKB : output.videoParam.mfx.InitialDelayInKB;
        for (auto p: correctableLevel) {
            // BrcParamMultiplier will be recalibrated
            input.videoParam.mfx.FrameInfo.FourCC = p[0];
            input.videoParam.mfx.CodecProfile = p[1];
            input.videoParam.mfx.BRCParamMultiplier = p[2];
            inTestField = p[3];
            input.videoParam.mfx.CodecLevel = p[4];
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], input.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], inTestField);
            EXPECT_EQ(p[4], input.videoParam.mfx.CodecLevel);
            EXPECT_EQ(p[0], output.videoParam.mfx.FrameInfo.FourCC);
            EXPECT_EQ(p[1], output.videoParam.mfx.CodecProfile);
            EXPECT_EQ(p[2], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[3], outTestField);
            EXPECT_EQ(p[5], output.videoParam.mfx.CodecLevel);
        }
    }
}

TEST_F(QueryTest, Conflicts_InitialDelayInKB_gt_BufferSizeInKB) {
    Ipp32s rcMethods[] = { CBR, VBR };
    Ipp32u ok[][3] = { {0, 1000, 1000}, {1, 1000, 1000}, {2, 1000, 250}, {4, 1000, 500}, {2, 55000, 41250} };
    for (auto rcMethod: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcMethod;
        for (auto p: ok) {
            input.videoParam.mfx.BRCParamMultiplier = p[0];
            input.videoParam.mfx.BufferSizeInKB = p[1];
            input.videoParam.mfx.InitialDelayInKB = p[2];
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[1], input.videoParam.mfx.BufferSizeInKB);
            EXPECT_EQ(p[2], input.videoParam.mfx.InitialDelayInKB);
            EXPECT_EQ(p[0], output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[1], output.videoParam.mfx.BufferSizeInKB);
            EXPECT_EQ(p[2], output.videoParam.mfx.InitialDelayInKB);
        }
    }
    Ipp32u warning[][3] = { {0, 1000, 2000}, {1, 1000, 2000}, {2, 1000, 33000}, {4, 1000, 16500}, {8, 1000, 13200} };
    for (auto rcMethod: rcMethods) {
        input.videoParam.mfx.RateControlMethod = rcMethod;
        for (auto p: warning) {
            input.videoParam.mfx.BRCParamMultiplier = p[0];
            input.videoParam.mfx.BufferSizeInKB = p[1];
            input.videoParam.mfx.InitialDelayInKB = p[2];
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(p[1], input.videoParam.mfx.BufferSizeInKB);
            EXPECT_EQ(p[2], input.videoParam.mfx.InitialDelayInKB);
            EXPECT_EQ(1, output.videoParam.mfx.BRCParamMultiplier);
            EXPECT_EQ(IPP_MAX(1, p[0]) * p[1], output.videoParam.mfx.BufferSizeInKB);
            EXPECT_EQ(IPP_MAX(1, p[0]) * p[1], output.videoParam.mfx.InitialDelayInKB);
        }
    }
}

TEST_F(QueryTest, Conflicts_multi_tile_and_wavefront_at_once) {
    Ipp32u ok[][3] = { {1, 1, ON}, {1, 1, OFF}, {1, 1, 0}, {2, 2, OFF}, {2, 2, 0} };
    for (auto p: ok) {
        input.extHevcTiles.NumTileRows = p[0];
        input.extHevcTiles.NumTileColumns = p[1];
        input.extCodingOptionHevc.WPP = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], input.extCodingOptionHevc.WPP);
        EXPECT_EQ(p[0], output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], output.extCodingOptionHevc.WPP);
    }
    input.extHevcTiles.NumTileRows = 2;
    input.extHevcTiles.NumTileColumns = 2;
    input.extCodingOptionHevc.WPP = ON;
    EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(2, input.extHevcTiles.NumTileRows);
    EXPECT_EQ(2, input.extHevcTiles.NumTileColumns);
    EXPECT_EQ(ON, input.extCodingOptionHevc.WPP);
    EXPECT_EQ(2, output.extHevcTiles.NumTileRows);
    EXPECT_EQ(2, output.extHevcTiles.NumTileColumns);
    EXPECT_EQ(OFF, output.extCodingOptionHevc.WPP);
}

TEST_F(QueryTest, Conflicts_multi_slice_and_frame_threading_at_once) {
    Ipp32u ok[][2] = { {1, 1}, {1, 5}, {5, 1} };
    for (auto p: ok) {
        input.videoParam.mfx.NumSlice = p[0];
        input.extCodingOptionHevc.FramesInParallel = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.FramesInParallel);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.FramesInParallel);
    }

    input.videoParam.mfx.NumSlice = 5;
    input.extCodingOptionHevc.FramesInParallel = 5;
    EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(5, input.videoParam.mfx.NumSlice);
    EXPECT_EQ(5, input.extCodingOptionHevc.FramesInParallel);
    EXPECT_EQ(5, output.videoParam.mfx.NumSlice);
    EXPECT_EQ(1, output.extCodingOptionHevc.FramesInParallel);
}

TEST_F(QueryTest, Conflicts_multi_tile_and_frame_threading_at_once) {
    Ipp32u ok[][3] = { {1, 0, 1}, {0, 1, 1}, {1, 0, 5}, {0, 1, 5}, {1, 0, 0}, {0, 1, 0}, {2, 0, 1}, {0, 2, 1}, {2, 2, 0} };
    for (auto p: ok) {
        input.extHevcTiles.NumTileRows = p[0];
        input.extHevcTiles.NumTileColumns = p[1];
        input.extCodingOptionHevc.FramesInParallel = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[1], input.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], input.extCodingOptionHevc.FramesInParallel);
        EXPECT_EQ(p[0], output.extHevcTiles.NumTileRows);
        EXPECT_EQ(p[1], output.extHevcTiles.NumTileColumns);
        EXPECT_EQ(p[2], output.extCodingOptionHevc.FramesInParallel);
    }
    input.extHevcTiles.NumTileRows = 2;
    input.extHevcTiles.NumTileColumns = 2;
    input.extCodingOptionHevc.FramesInParallel = 5;
    EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(2, input.extHevcTiles.NumTileRows);
    EXPECT_EQ(2, input.extHevcTiles.NumTileColumns);
    EXPECT_EQ(5, input.extCodingOptionHevc.FramesInParallel);
    EXPECT_EQ(2, output.extHevcTiles.NumTileRows);
    EXPECT_EQ(2, output.extHevcTiles.NumTileColumns);
    EXPECT_EQ(1, output.extCodingOptionHevc.FramesInParallel);
}

TEST_F(QueryTest, Conflicts_gacc_and_frame_threading_at_once) {
    Ipp32u ok[][2] = { {OFF, 1}, {OFF, 5}, {0, 1}, {0, 5} };
    for (auto p: ok) {
        input.extCodingOptionHevc.EnableCm = p[0];
        input.extCodingOptionHevc.FramesInParallel = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.EnableCm);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.FramesInParallel);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.EnableCm);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.FramesInParallel);
    }
    input.extCodingOptionHevc.EnableCm = ON;
    input.extCodingOptionHevc.FramesInParallel = 5;
    EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
    EXPECT_EQ(ON, input.extCodingOptionHevc.EnableCm);
    EXPECT_EQ(5, input.extCodingOptionHevc.FramesInParallel);
    EXPECT_EQ(0, output.extCodingOptionHevc.EnableCm);
    EXPECT_EQ(5, output.extCodingOptionHevc.FramesInParallel);
}

TEST_F(QueryTest, Conflicts_RegionId_ge_NumSlice) {
    enum {REG_ON=MFX_HEVC_REGION_ENCODING_ON, REG_OFF=MFX_HEVC_REGION_ENCODING_OFF};
    input.extHevcRegion.RegionType = MFX_HEVC_REGION_SLICE;
    Ipp32u ok[][3] = { {1, REG_ON, 0}, {10, REG_ON, 9}, {20, REG_ON, 5}, {1, REG_OFF, 10}, {10, REG_OFF, 19}, {20, REG_OFF, 25} };
    for (auto p: ok) {
        input.videoParam.mfx.NumSlice = p[0];
        input.extHevcRegion.RegionEncoding = p[1];
        input.extHevcRegion.RegionId = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], input.extHevcRegion.RegionEncoding);
        EXPECT_EQ(p[2], input.extHevcRegion.RegionId);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], output.extHevcRegion.RegionEncoding);
        EXPECT_EQ(p[2], output.extHevcRegion.RegionId);
    }
    Ipp32u error[][3] = { {1, REG_ON, 1}, {10, REG_ON, 10}, {20, REG_ON, 30} };
    for (auto p: error) {
        input.videoParam.mfx.NumSlice = p[0];
        input.extHevcRegion.RegionEncoding = p[1];
        input.extHevcRegion.RegionId = p[2];
        EXPECT_EQ(MFX_ERR_UNSUPPORTED, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], input.extHevcRegion.RegionEncoding);
        EXPECT_EQ(p[2], input.extHevcRegion.RegionId);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumSlice);
        EXPECT_EQ(p[1], output.extHevcRegion.RegionEncoding);
        EXPECT_EQ(0, output.extHevcRegion.RegionId);
    }
}

TEST_F(QueryTest, Conflicts_MinCUSize_too_small) {
    Ipp32u ok[][2] = { {5, 3}, {5, 2}, {5, 1}, {6, 4}, {6, 3}, {6, 2}, {6, 1} };
    for (auto p: ok) {
        input.extCodingOptionHevc.Log2MaxCUSize = p[0];
        input.extCodingOptionHevc.MaxCUDepth = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.MaxCUDepth);
    }
    Ipp32u warning[][2] = { {5, 4}  };
    for (auto p: warning) {
        input.extCodingOptionHevc.Log2MaxCUSize = p[0];
        input.extCodingOptionHevc.MaxCUDepth = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[0]-2, output.extCodingOptionHevc.MaxCUDepth);
    }
}

TEST_F(QueryTest, Conflicts_Width_not_divisible_by_MinCUSize) {
    Ipp32u ok[][3] = { {712, 5, 3}, {712, 6, 4}, {720, 5, 2}, {720, 6, 3}, {352, 6, 2} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Width = 0;
        input.extHevcParam.PicWidthInLumaSamples = p[0];
        input.extCodingOptionHevc.Log2MaxCUSize = p[1];
        input.extCodingOptionHevc.MaxCUDepth = p[2];
        output.extCodingOptionHevc.MaxCUDepth = 0;
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[2], input.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[2], output.extCodingOptionHevc.MaxCUDepth);
        if ((p[0] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Width = p[0];
            input.extHevcParam.PicWidthInLumaSamples = 0;
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        }

        for (Ipp32u depth = 1; depth < p[2]; depth++) {
            input.videoParam.mfx.FrameInfo.Width = 0;
            input.extHevcParam.PicWidthInLumaSamples = p[0];
            input.extCodingOptionHevc.Log2MaxCUSize = p[1];
            input.extCodingOptionHevc.MaxCUDepth = depth;
            output.extCodingOptionHevc.MaxCUDepth = 0;
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.extHevcParam.PicWidthInLumaSamples);
            EXPECT_EQ(p[1], input.extCodingOptionHevc.Log2MaxCUSize);
            EXPECT_EQ(depth, input.extCodingOptionHevc.MaxCUDepth);
            EXPECT_EQ(p[0], output.extHevcParam.PicWidthInLumaSamples);
            EXPECT_EQ(p[1], output.extCodingOptionHevc.Log2MaxCUSize);
            EXPECT_EQ(p[2], output.extCodingOptionHevc.MaxCUDepth);
            if ((p[0] & 15) == 0) {
                input.videoParam.mfx.FrameInfo.Width = p[0];
                input.extHevcParam.PicWidthInLumaSamples = 0;
                output.extCodingOptionHevc.MaxCUDepth = 0;
                EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                EXPECT_EQ(p[2], output.extCodingOptionHevc.MaxCUDepth);
            }
        }
    }
}

TEST_F(QueryTest, Conflicts_Height_not_divisible_by_MinCUSize) {
    Ipp32u ok[][3] = { {712, 5, 3}, {712, 6, 4}, {720, 5, 2}, {720, 6, 3}, {352, 6, 2} };
    for (auto p: ok) {
        input.videoParam.mfx.FrameInfo.Height = 0;
        input.extHevcParam.PicHeightInLumaSamples = p[0];
        input.extCodingOptionHevc.Log2MaxCUSize = p[1];
        input.extCodingOptionHevc.MaxCUDepth = p[2];
        output.extCodingOptionHevc.MaxCUDepth = 0;
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[2], input.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[0], output.extHevcParam.PicHeightInLumaSamples);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[2], output.extCodingOptionHevc.MaxCUDepth);
        if ((p[0] & 15) == 0) {
            input.videoParam.mfx.FrameInfo.Height = p[0];
            input.extHevcParam.PicHeightInLumaSamples = 0;
            EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        }

        for (Ipp32u depth = 1; depth < p[2]; depth++) {
            input.videoParam.mfx.FrameInfo.Height = 0;
            input.extHevcParam.PicHeightInLumaSamples = p[0];
            input.extCodingOptionHevc.Log2MaxCUSize = p[1];
            input.extCodingOptionHevc.MaxCUDepth = depth;
            output.extCodingOptionHevc.MaxCUDepth = 0;
            EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
            EXPECT_EQ(p[0], input.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[1], input.extCodingOptionHevc.Log2MaxCUSize);
            EXPECT_EQ(depth, input.extCodingOptionHevc.MaxCUDepth);
            EXPECT_EQ(p[0], output.extHevcParam.PicHeightInLumaSamples);
            EXPECT_EQ(p[1], output.extCodingOptionHevc.Log2MaxCUSize);
            EXPECT_EQ(p[2], output.extCodingOptionHevc.MaxCUDepth);
            if ((p[0] & 15) == 0) {
                input.videoParam.mfx.FrameInfo.Height = p[0];
                input.extHevcParam.PicHeightInLumaSamples = 0;
                output.extCodingOptionHevc.MaxCUDepth = 0;
                EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                EXPECT_EQ(p[2], output.extCodingOptionHevc.MaxCUDepth);
            }
        }
    }
}

TEST_F(QueryTest, Conflicts_MaxTUSize_gt_MaxCUSize) {
    Ipp32u ok[][2] = { {5, 5}, {5, 4}, {5, 3}, {5, 2}, {6, 5}, {6, 4}, {6, 3}, {6, 2} };
    for (auto p: ok) {
        input.extCodingOptionHevc.Log2MaxCUSize = p[0];
        input.extCodingOptionHevc.QuadtreeTULog2MaxSize = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.QuadtreeTULog2MaxSize);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.QuadtreeTULog2MaxSize);
    }
}

TEST_F(QueryTest, Conflicts_MinTUSize_gt_MaxCUSize) {
    Ipp32u ok[][2] = { {5, 5}, {5, 4}, {5, 3}, {5, 2}, {6, 5}, {6, 4}, {6, 3}, {6, 2} };
    for (auto p: ok) {
        input.extCodingOptionHevc.Log2MaxCUSize = p[0];
        input.extCodingOptionHevc.QuadtreeTULog2MinSize = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.QuadtreeTULog2MinSize);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.QuadtreeTULog2MinSize);
    }
}

TEST_F(QueryTest, Conflicts_MinTUSize_gt_MinCUSize) {
    Ipp32u ok[][3] = {
        {6, 4, 3}, {6, 4, 2}, {6, 3, 4}, {6, 3, 3}, {6, 3, 2}, {6, 2, 5}, {6, 2, 4}, {6, 2, 3}, {6, 2, 2}, {6, 1, 5}, {6, 1, 4}, {6, 1, 3}, {6, 1, 2},
        {5, 3, 3}, {5, 3, 2}, {5, 2, 4}, {5, 2, 3}, {5, 2, 2}, {5, 1, 5}, {5, 1, 4}, {5, 1, 3}, {5, 1, 2} };
    for (auto p: ok) {
        input.extCodingOptionHevc.Log2MaxCUSize = p[0];
        input.extCodingOptionHevc.MaxCUDepth = p[1];
        input.extCodingOptionHevc.QuadtreeTULog2MinSize = p[2];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[2], input.extCodingOptionHevc.QuadtreeTULog2MinSize);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[2], output.extCodingOptionHevc.QuadtreeTULog2MinSize);
    }
    Ipp32u warning[][4] = { {6, 4, 5, 3}, {6, 4, 4, 3}, {6, 3, 5, 4}, {5, 3, 5, 3}, {5, 3, 4, 3}, {5, 2, 5, 4} };
    for (auto p: warning) {
        input.extCodingOptionHevc.Log2MaxCUSize = p[0];
        input.extCodingOptionHevc.MaxCUDepth = p[1];
        input.extCodingOptionHevc.QuadtreeTULog2MinSize = p[2];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[2], input.extCodingOptionHevc.QuadtreeTULog2MinSize);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.Log2MaxCUSize);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.MaxCUDepth);
        EXPECT_EQ(p[3], output.extCodingOptionHevc.QuadtreeTULog2MinSize);
    }
}

TEST_F(QueryTest, Conflicts_MinTUSize_gt_MaxTUSize) {
    Ipp32u ok[][2] = { {2, 2}, {2, 3}, {2, 4}, {2, 5}, {3, 3}, {3, 4}, {3, 5}, {4, 4}, {4, 5}, {5, 5}  };
    for (auto p: ok) {
        input.extCodingOptionHevc.QuadtreeTULog2MinSize = p[0];
        input.extCodingOptionHevc.QuadtreeTULog2MaxSize = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.QuadtreeTULog2MinSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.QuadtreeTULog2MaxSize);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.QuadtreeTULog2MinSize);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.QuadtreeTULog2MaxSize);
    }
    Ipp32u warning[][2] = { {3, 2}, {4, 3}, {4, 2}, {5, 4}, {5, 3}, {5, 2} };
    for (auto p: warning) {
        input.extCodingOptionHevc.QuadtreeTULog2MinSize = p[0];
        input.extCodingOptionHevc.QuadtreeTULog2MaxSize = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOptionHevc.QuadtreeTULog2MinSize);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.QuadtreeTULog2MaxSize);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.QuadtreeTULog2MinSize);
        EXPECT_EQ(p[0], output.extCodingOptionHevc.QuadtreeTULog2MaxSize);
    }
}

TEST_F(QueryTest, Conflicts_NumThread_ne_ForceNumThread) {
    Ipp32u ok[][2] = { {0, 0}, {1, 1}, {2, 2}, {8, 8}, {56, 56}, {0xffff, 0xffff} };
    for (auto p: ok) {
        input.videoParam.mfx.NumThread = p[0];
        input.extCodingOptionHevc.ForceNumThread = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumThread);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.ForceNumThread);
        EXPECT_EQ(p[0], output.videoParam.mfx.NumThread);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.ForceNumThread);
    }
    Ipp32u warning[][2] = { {1, 2}, {3, 2}, {7, 8}, {9, 8}, {55, 56}, {57, 56}, {0xfffe, 0xffff}, {0xffff, 0xfffe} };
    for (auto p: warning) {
        input.videoParam.mfx.NumThread = p[0];
        input.extCodingOptionHevc.ForceNumThread = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.videoParam.mfx.NumThread);
        EXPECT_EQ(p[1], input.extCodingOptionHevc.ForceNumThread);
        EXPECT_EQ(0, output.videoParam.mfx.NumThread);
        EXPECT_EQ(p[1], output.extCodingOptionHevc.ForceNumThread);
    }
}

TEST_F(QueryTest, Conflicts_AdaptiveI_and_StrictGOP) {
    Ipp32u ok[][2] = { {OFF, MFX_GOP_STRICT}, {OFF, 0}, {ON, 0} };
    for (auto p: ok) {
        input.extCodingOption2.AdaptiveI = p[0];
        input.videoParam.mfx.GopOptFlag = p[1];
        EXPECT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOption2.AdaptiveI);
        EXPECT_EQ(p[1], input.videoParam.mfx.GopOptFlag);
        EXPECT_EQ(p[0], output.extCodingOption2.AdaptiveI);
        EXPECT_EQ(p[1], output.videoParam.mfx.GopOptFlag);
    }
    Ipp32u warning[][2] = { {ON, MFX_GOP_STRICT} };
    for (auto p: warning) {
        input.extCodingOption2.AdaptiveI = p[0];
        input.videoParam.mfx.GopOptFlag = p[1];
        EXPECT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
        EXPECT_EQ(p[0], input.extCodingOption2.AdaptiveI);
        EXPECT_EQ(p[1], input.videoParam.mfx.GopOptFlag);
        EXPECT_EQ(0, output.extCodingOption2.AdaptiveI);
        EXPECT_EQ(p[1], output.videoParam.mfx.GopOptFlag);
    }
}

TEST_F(QueryTest, Conflicts_FourCC_and_DeltaQpMode) {
    Ipp32u fourccs[] = { NV12, NV16, P010, P210 };
    Ipp32u unsupported = H265Enc::AMT_DQP_CAL | H265Enc::AMT_DQP_PAQ;
    for (auto fourcc: fourccs) {
        for (Ipp32s dqp = 0; dqp <= 7; dqp++) {
            input.videoParam.mfx.FrameInfo.FourCC = fourcc;
            input.videoParam.mfx.FrameInfo.ChromaFormat = Fourcc2ChromaFormat(fourcc);
            input.extCodingOptionHevc.DeltaQpMode = dqp + 1;
            if ((fourcc == P010 || fourcc == P210) && (dqp & unsupported)) {
                ASSERT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                ASSERT_EQ((dqp & H265Enc::AMT_DQP_CAQ) + 1, output.extCodingOptionHevc.DeltaQpMode);
            } else {
                ASSERT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                ASSERT_EQ(dqp + 1, output.extCodingOptionHevc.DeltaQpMode);
            }
            ASSERT_EQ(fourcc, input.videoParam.mfx.FrameInfo.FourCC);
            ASSERT_EQ(dqp + 1, input.extCodingOptionHevc.DeltaQpMode);
            ASSERT_EQ(fourcc, output.videoParam.mfx.FrameInfo.FourCC);
        }
    }
}

TEST_F(QueryTest, Conflicts_BPyramid_and_DeltaQpMode) {
    Ipp32u bpyramids[] = { ON, OFF };
    for (auto bpyramid: bpyramids) {
        for (Ipp32s gopRefDist = 1; gopRefDist <= 8; gopRefDist++) {
            for (Ipp32s dqp = 0; dqp <= 7; dqp++) {
                input.videoParam.mfx.GopRefDist = gopRefDist;
                input.extCodingOptionHevc.BPyramid = bpyramid;
                input.extCodingOptionHevc.DeltaQpMode = dqp + 1;
                if ((bpyramid == OFF || gopRefDist == 1) && dqp > 0) {
                    ASSERT_EQ(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                    ASSERT_EQ(1, output.extCodingOptionHevc.DeltaQpMode);
                } else {
                    ASSERT_EQ(MFX_ERR_NONE, MFXVideoENCODEH265::Query(nullptr, &input.videoParam, &output.videoParam));
                    ASSERT_EQ(dqp + 1, output.extCodingOptionHevc.DeltaQpMode);
                }
                ASSERT_EQ(gopRefDist, input.videoParam.mfx.GopRefDist);
                ASSERT_EQ(bpyramid, input.extCodingOptionHevc.BPyramid);
                ASSERT_EQ(dqp + 1, input.extCodingOptionHevc.DeltaQpMode);
                ASSERT_EQ(gopRefDist, output.videoParam.mfx.GopRefDist);
                ASSERT_EQ(bpyramid, output.extCodingOptionHevc.BPyramid);
            }
        }
    }
}
