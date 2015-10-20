#include "gtest/gtest.h"

#include "hevce_common_api.h"

using namespace ApiTestCommon;

ParamSet::ParamSet()
{
    extOpaqueSurfaceAlloc = MakeExtBuffer<mfxExtOpaqueSurfaceAlloc>();
    extCodingOptionHevc   = MakeExtBuffer<mfxExtCodingOptionHEVC>();
    extDumpFiles          = MakeExtBuffer<mfxExtDumpFiles>();
    extHevcTiles          = MakeExtBuffer<mfxExtHEVCTiles>();
    extHevcParam          = MakeExtBuffer<mfxExtHEVCParam>();
    extHevcRegion         = MakeExtBuffer<mfxExtHEVCRegion>();
    extCodingOption       = MakeExtBuffer<mfxExtCodingOption>();
    extCodingOption2      = MakeExtBuffer<mfxExtCodingOption2>();
    extEncoderROI         = MakeExtBuffer<mfxExtEncoderROI>();
    Zero(videoParam);
    assert(sizeof(extBuffers) / sizeof(extBuffers[0]) >= 9+1); // plus one for unknown extbuffer test
    videoParam.ExtParam = extBuffers;
    videoParam.NumExtParam = 9;
    videoParam.ExtParam[0] = &extOpaqueSurfaceAlloc.Header;
    videoParam.ExtParam[1] = &extCodingOptionHevc.Header;
    videoParam.ExtParam[2] = &extDumpFiles.Header;
    videoParam.ExtParam[3] = &extHevcTiles.Header;
    videoParam.ExtParam[4] = &extHevcParam.Header;
    videoParam.ExtParam[5] = &extHevcRegion.Header;
    videoParam.ExtParam[6] = &extCodingOption.Header;
    videoParam.ExtParam[7] = &extCodingOption2.Header;
    videoParam.ExtParam[8] = &extEncoderROI.Header;
}

void ApiTestCommon::CopyParamSet(ParamSet &dst, const ParamSet &src) {
    dst.extOpaqueSurfaceAlloc = src.extOpaqueSurfaceAlloc;
    dst.extCodingOptionHevc = src.extCodingOptionHevc;
    dst.extDumpFiles = src.extDumpFiles;
    dst.extHevcTiles = src.extHevcTiles;
    dst.extHevcRegion = src.extHevcRegion;
    dst.extCodingOption = src.extCodingOption;
    dst.extCodingOption2 = src.extCodingOption2;
    dst.extEncoderROI = src.extEncoderROI;
    dst.videoParam.mfx = src.videoParam.mfx;
    dst.videoParam.AsyncDepth = src.videoParam.AsyncDepth;
    dst.videoParam.Protected = src.videoParam.Protected;
    dst.videoParam.IOPattern = src.videoParam.IOPattern;
}

void ApiTestCommon::InitParamSetMandated(ParamSet &paramset) {
    paramset.videoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    paramset.videoParam.mfx.FrameInfo.Width = 1920;
    paramset.videoParam.mfx.FrameInfo.Height = 1088;
    paramset.videoParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    paramset.videoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    paramset.videoParam.mfx.FrameInfo.FrameRateExtN = 30;
    paramset.videoParam.mfx.FrameInfo.FrameRateExtD = 1;
    paramset.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    paramset.videoParam.mfx.TargetKbps = 5000;
}

void ApiTestCommon::InitParamSetValid(ParamSet &paramset) {
    paramset.videoParam.AsyncDepth = 1;
    paramset.videoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    paramset.videoParam.Protected = 0;
    paramset.videoParam.mfx.BRCParamMultiplier = 1;
    paramset.videoParam.mfx.CodecId = MFX_CODEC_HEVC;
    paramset.videoParam.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
    paramset.videoParam.mfx.CodecLevel = MFX_LEVEL_HEVC_5;
    paramset.videoParam.mfx.NumThread = 8;
    paramset.videoParam.mfx.TargetUsage = MFX_TARGETUSAGE_1;
    paramset.videoParam.mfx.GopPicSize = 1000;
    paramset.videoParam.mfx.GopRefDist = 8;
    paramset.videoParam.mfx.GopOptFlag = 1;
    paramset.videoParam.mfx.IdrInterval = 1;
    paramset.videoParam.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    paramset.videoParam.mfx.BufferSizeInKB = 1000;
    paramset.videoParam.mfx.InitialDelayInKB = 500;
    paramset.videoParam.mfx.TargetKbps = 8000;
    paramset.videoParam.mfx.MaxKbps = 8000;
    paramset.videoParam.mfx.NumSlice = 1;
    paramset.videoParam.mfx.NumRefFrame = 4;
    paramset.videoParam.mfx.EncodedOrder = 0;
    paramset.videoParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    paramset.videoParam.mfx.FrameInfo.Width = 3840;
    paramset.videoParam.mfx.FrameInfo.Height = 2160;
    paramset.videoParam.mfx.FrameInfo.CropX = 0;
    paramset.videoParam.mfx.FrameInfo.CropY = 0;
    paramset.videoParam.mfx.FrameInfo.CropW = 3840;
    paramset.videoParam.mfx.FrameInfo.CropH = 2160;
    paramset.videoParam.mfx.FrameInfo.FrameRateExtN = 30;
    paramset.videoParam.mfx.FrameInfo.FrameRateExtD = 1;
    paramset.videoParam.mfx.FrameInfo.AspectRatioW = 1;
    paramset.videoParam.mfx.FrameInfo.AspectRatioH = 1;
    paramset.videoParam.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    paramset.videoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    paramset.extHevcRegion.RegionType = MFX_HEVC_REGION_SLICE;
    paramset.extHevcRegion.RegionId = 0;
    paramset.extHevcTiles.NumTileRows = 1;
    paramset.extHevcTiles.NumTileColumns = 1;
    paramset.extHevcParam.PicWidthInLumaSamples = 3840;
    paramset.extHevcParam.PicHeightInLumaSamples = 2160;
    paramset.extHevcParam.GeneralConstraintFlags = 0;
    paramset.extCodingOptionHevc.Log2MaxCUSize = 6;
    paramset.extCodingOptionHevc.MaxCUDepth = 4;
    paramset.extCodingOptionHevc.QuadtreeTULog2MaxSize = 5;
    paramset.extCodingOptionHevc.QuadtreeTULog2MinSize = 2;
    paramset.extCodingOptionHevc.QuadtreeTUMaxDepthIntra = 4;
    paramset.extCodingOptionHevc.QuadtreeTUMaxDepthInter = 4;
    paramset.extCodingOptionHevc.QuadtreeTUMaxDepthInterRD = 4;
    paramset.extCodingOptionHevc.AnalyzeChroma = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.SignBitHiding = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.RDOQuant = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.SAO = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.SplitThresholdStrengthCUIntra = 1;
    paramset.extCodingOptionHevc.SplitThresholdStrengthTUIntra = 1;
    paramset.extCodingOptionHevc.SplitThresholdStrengthCUInter = 1;
    paramset.extCodingOptionHevc.IntraNumCand0_2 = 4;
    paramset.extCodingOptionHevc.IntraNumCand0_3 = 4;
    paramset.extCodingOptionHevc.IntraNumCand0_4 = 4;
    paramset.extCodingOptionHevc.IntraNumCand0_5 = 4;
    paramset.extCodingOptionHevc.IntraNumCand0_6 = 4;
    paramset.extCodingOptionHevc.IntraNumCand1_2 = 3;
    paramset.extCodingOptionHevc.IntraNumCand1_3 = 3;
    paramset.extCodingOptionHevc.IntraNumCand1_4 = 3;
    paramset.extCodingOptionHevc.IntraNumCand1_5 = 3;
    paramset.extCodingOptionHevc.IntraNumCand1_6 = 3;
    paramset.extCodingOptionHevc.IntraNumCand2_2 = 2;
    paramset.extCodingOptionHevc.IntraNumCand2_3 = 2;
    paramset.extCodingOptionHevc.IntraNumCand2_4 = 2;
    paramset.extCodingOptionHevc.IntraNumCand2_5 = 2;
    paramset.extCodingOptionHevc.IntraNumCand2_6 = 2;
    paramset.extCodingOptionHevc.WPP = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.GPB = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.PartModes = 3;
    paramset.extCodingOptionHevc.CmIntraThreshold = 576;
    paramset.extCodingOptionHevc.TUSplitIntra = 1;
    paramset.extCodingOptionHevc.CUSplit = 1;
    paramset.extCodingOptionHevc.IntraAngModes = 1;
    paramset.extCodingOptionHevc.EnableCm = MFX_CODINGOPTION_OFF;
    paramset.extCodingOptionHevc.BPyramid = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.FastPUDecision = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.HadamardMe = 1;
    paramset.extCodingOptionHevc.TMVP = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.Deblocking = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.RDOQuantChroma = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.RDOQuantCGZ = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.SaoOpt = 1;
    paramset.extCodingOptionHevc.SaoSubOpt = 1;
    paramset.extCodingOptionHevc.CostChroma = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.PatternIntPel = 1;
    paramset.extCodingOptionHevc.FastSkip = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.PatternSubPel = 3;
    paramset.extCodingOptionHevc.ForceNumThread = 8;
    paramset.extCodingOptionHevc.FastCbfMode = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.PuDecisionSatd = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.MinCUDepthAdapt = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.MaxCUDepthAdapt = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.NumBiRefineIter = 10;
    paramset.extCodingOptionHevc.CUSplitThreshold = 256;
    paramset.extCodingOptionHevc.DeltaQpMode = 1;
    paramset.extCodingOptionHevc.Enable10bit = MFX_CODINGOPTION_OFF;
    paramset.extCodingOptionHevc.IntraAngModesP = 1;
    paramset.extCodingOptionHevc.IntraAngModesBRef = 1;
    paramset.extCodingOptionHevc.IntraAngModesBnonRef = 1;
    paramset.extCodingOptionHevc.IntraChromaRDO = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.FastInterp = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.SplitThresholdTabIndex = 1;
    paramset.extCodingOptionHevc.CpuFeature = 0;
    paramset.extCodingOptionHevc.TryIntra = 1;
    paramset.extCodingOptionHevc.FastAMPSkipME = 1;
    paramset.extCodingOptionHevc.FastAMPRD = 1;
    paramset.extCodingOptionHevc.SkipMotionPartition = 1;
    paramset.extCodingOptionHevc.SkipCandRD = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.FramesInParallel = 3;
    paramset.extCodingOptionHevc.AdaptiveRefs = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.FastCoeffCost = MFX_CODINGOPTION_OFF;
    paramset.extCodingOptionHevc.NumRefFrameB = 0;
    paramset.extCodingOptionHevc.IntraMinDepthSC = 11;
    paramset.extCodingOptionHevc.InterMinDepthSTC = 6;
    paramset.extCodingOptionHevc.MotionPartitionDepth = 3;
    paramset.extCodingOptionHevc.AnalyzeCmplx = 1;
    paramset.extCodingOptionHevc.RateControlDepth = 9;
    paramset.extCodingOptionHevc.LowresFactor = 3;
    paramset.extCodingOptionHevc.DeblockBorders = MFX_CODINGOPTION_ON;
    paramset.extCodingOptionHevc.SAOChroma = MFX_CODINGOPTION_OFF;
    paramset.extCodingOptionHevc.NumRefLayers = 2;
    paramset.extCodingOptionHevc.ConstQpOffset = 1;
    paramset.extCodingOption.AUDelimiter = MFX_CODINGOPTION_ON;
    paramset.extCodingOption2.DisableVUI = MFX_CODINGOPTION_ON;
    paramset.extCodingOption2.AdaptiveI = MFX_CODINGOPTION_ON;
    paramset.extEncoderROI.NumROI = 1;
    paramset.extEncoderROI.ROI[0].Left = paramset.extEncoderROI.ROI[0].Right = paramset.extEncoderROI.ROI[0].Top = paramset.extEncoderROI.ROI[0].Bottom = 0;
    paramset.extEncoderROI.ROI[0].Priority = 1;

}

void ApiTestCommon::InitParamSetCorrectable(ParamSet &input, ParamSet &expectedOutput) {
    InitParamSetValid(input);
    CopyParamSet(expectedOutput, input);
#define SETUP(FIELD, INVAL, EXPVAL) input.FIELD = (INVAL); expectedOutput.FIELD = (EXPVAL)
    SETUP(videoParam.mfx.TargetUsage, 10, 7);
    SETUP(videoParam.mfx.GopRefDist, 20, 16);
    SETUP(videoParam.mfx.NumRefFrame, 20, 16);
    SETUP(videoParam.mfx.CodecProfile, 0xfff, 0);
    SETUP(videoParam.mfx.CodecLevel, 0xfff, 0);
    SETUP(videoParam.mfx.GopOptFlag, 10, 0);
    SETUP(videoParam.mfx.BRCParamMultiplier, 100, 14);
    SETUP(videoParam.mfx.BufferSizeInKB, 50000, 7857);
    SETUP(videoParam.mfx.InitialDelayInKB, 50000, 7857);
    SETUP(videoParam.mfx.TargetKbps, 50000, 62857);
    SETUP(videoParam.mfx.MaxKbps, 50000, 62857);
    SETUP(videoParam.mfx.FrameInfo.AspectRatioW, 1, 0);
    SETUP(videoParam.mfx.FrameInfo.AspectRatioH, 0, 0);
    SETUP(videoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, MFX_IOPATTERN_IN_SYSTEM_MEMORY);
    SETUP(extHevcTiles.NumTileRows, 40, 22);
    SETUP(extHevcTiles.NumTileColumns, 40, 15);
    SETUP(extCodingOptionHevc.Log2MaxCUSize, 100, 0);
    SETUP(extCodingOptionHevc.MaxCUDepth, 100, 0);
    SETUP(extCodingOptionHevc.QuadtreeTULog2MaxSize, 100, 0);
    SETUP(extCodingOptionHevc.QuadtreeTULog2MinSize, 100, 0);
    SETUP(extCodingOptionHevc.QuadtreeTUMaxDepthIntra, 100, 0);
    SETUP(extCodingOptionHevc.QuadtreeTUMaxDepthInter, 100, 0);
    SETUP(extCodingOptionHevc.QuadtreeTUMaxDepthInterRD, 100, 0);
    SETUP(extCodingOptionHevc.AnalyzeChroma, 100, 0);
    SETUP(extCodingOptionHevc.SignBitHiding, 100, 0);
    SETUP(extCodingOptionHevc.RDOQuant, 100, 0);
    SETUP(extCodingOptionHevc.SAO, 100, 0);
    SETUP(extCodingOptionHevc.SplitThresholdStrengthCUIntra, 100, 0);
    SETUP(extCodingOptionHevc.SplitThresholdStrengthTUIntra, 100, 0);
    SETUP(extCodingOptionHevc.SplitThresholdStrengthCUInter, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand1_2, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand1_3, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand1_4, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand1_5, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand1_6, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand2_2, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand2_3, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand2_4, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand2_5, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand2_6, 100, 0);
    SETUP(extCodingOptionHevc.WPP, 100, 0);
    SETUP(extCodingOptionHevc.GPB, 100, 0);
    SETUP(extCodingOptionHevc.PartModes, 100, 0);
    SETUP(extCodingOptionHevc.TUSplitIntra, 100, 0);
    SETUP(extCodingOptionHevc.CUSplit, 100, 0);
    SETUP(extCodingOptionHevc.IntraAngModes, 1000, 0);
    SETUP(extCodingOptionHevc.EnableCm, 100, 0);
    SETUP(extCodingOptionHevc.BPyramid, 100, 0);
    SETUP(extCodingOptionHevc.FastPUDecision, 100, 0);
    SETUP(extCodingOptionHevc.HadamardMe, 100, 0);
    SETUP(extCodingOptionHevc.TMVP, 100, 0);
    SETUP(extCodingOptionHevc.Deblocking, 100, 0);
    SETUP(extCodingOptionHevc.RDOQuantChroma, 100, 0);
    SETUP(extCodingOptionHevc.RDOQuantCGZ, 100, 0);
    SETUP(extCodingOptionHevc.SaoOpt, 100, 0);
    SETUP(extCodingOptionHevc.SaoSubOpt, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand0_2, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand0_3, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand0_4, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand0_5, 100, 0);
    SETUP(extCodingOptionHevc.IntraNumCand0_6, 100, 0);
    SETUP(extCodingOptionHevc.CostChroma, 100, 0);
    SETUP(extCodingOptionHevc.PatternIntPel, 1000, 0);
    SETUP(extCodingOptionHevc.FastSkip, 100, 0);
    SETUP(extCodingOptionHevc.PatternSubPel, 100, 0);
    SETUP(extCodingOptionHevc.FastCbfMode, 100, 0);
    SETUP(extCodingOptionHevc.PuDecisionSatd, 100, 0);
    SETUP(extCodingOptionHevc.MinCUDepthAdapt, 100, 0);
    SETUP(extCodingOptionHevc.MaxCUDepthAdapt, 100, 0);
    SETUP(extCodingOptionHevc.DeltaQpMode, 100, 0);
    SETUP(extCodingOptionHevc.Enable10bit, 100, 0);
    SETUP(extCodingOptionHevc.IntraAngModesP, 1000, 0);
    SETUP(extCodingOptionHevc.IntraAngModesBRef, 1000, 0);
    SETUP(extCodingOptionHevc.IntraAngModesBnonRef, 1000, 0);
    SETUP(extCodingOptionHevc.IntraChromaRDO, 100, 0);
    SETUP(extCodingOptionHevc.FastInterp, 100, 0);
    SETUP(extCodingOptionHevc.SplitThresholdTabIndex, 100, 0);
    SETUP(extCodingOptionHevc.CpuFeature, 100, 0);
    SETUP(extCodingOptionHevc.TryIntra, 100, 0);
    SETUP(extCodingOptionHevc.FastAMPSkipME, 100, 0);
    SETUP(extCodingOptionHevc.FastAMPRD, 100, 0);
    SETUP(extCodingOptionHevc.SkipMotionPartition, 100, 0);
    SETUP(extCodingOptionHevc.SkipCandRD, 100, 0);
    SETUP(extCodingOptionHevc.FramesInParallel, 3, 1);
    SETUP(extCodingOptionHevc.AdaptiveRefs, 100, 0);
    SETUP(extCodingOptionHevc.FastCoeffCost, 100, 0);
    SETUP(extCodingOptionHevc.NumRefFrameB, 100, 0);
    SETUP(extCodingOptionHevc.AnalyzeCmplx, 100, 0);
    SETUP(extCodingOptionHevc.LowresFactor, 100, 0);
    SETUP(extCodingOptionHevc.DeblockBorders, 100, 0);
    SETUP(extHevcParam.GeneralConstraintFlags, Ipp64u(-1), 0);
    SETUP(extCodingOption.AUDelimiter, 100, 0);
    SETUP(extCodingOption2.DisableVUI, 100, 0);
    SETUP(extCodingOption2.AdaptiveI, 100, 0);
    input.extEncoderROI.NumROI = expectedOutput.extEncoderROI.NumROI = 1;
    SETUP(extEncoderROI.ROI[0].Left, 1, 0);
    SETUP(extEncoderROI.ROI[0].Right, 3, 0);
    SETUP(extEncoderROI.ROI[0].Top, 64, 0);
    SETUP(extEncoderROI.ROI[0].Bottom, 0, 0);
    SETUP(extEncoderROI.ROI[0].Priority, 100, 3); // not CQP
#undef SETUP
}

void ApiTestCommon::InitParamSetUnsupported(ParamSet &input, ParamSet &expectedOutput) {
    InitParamSetValid(input);
    CopyParamSet(expectedOutput, input);
#define SETUP(FIELD, INVAL) input.FIELD = (INVAL); expectedOutput.FIELD = 0
    SETUP(videoParam.Protected, 1);
    SETUP(videoParam.mfx.CodecId, 0xffffffff);
    SETUP(videoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF);
    SETUP(videoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1);
    SETUP(videoParam.mfx.NumSlice, 1000);
    SETUP(videoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_BGR4);
    SETUP(videoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422V);
    SETUP(videoParam.mfx.FrameInfo.CropX, 0xf000);
    SETUP(videoParam.mfx.FrameInfo.CropY, 0xf000);
    SETUP(videoParam.mfx.FrameInfo.CropW, 0xf000);
    SETUP(videoParam.mfx.FrameInfo.CropH, 0xf000);
    SETUP(videoParam.mfx.FrameInfo.Width, 0xf000);
    SETUP(videoParam.mfx.FrameInfo.Height, 0xf000);
    SETUP(videoParam.mfx.FrameInfo.FrameRateExtN, 500);
    SETUP(videoParam.mfx.FrameInfo.FrameRateExtD, 1);
    SETUP(videoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY);
    input.extHevcRegion.RegionEncoding = expectedOutput.extHevcRegion.RegionEncoding = MFX_HEVC_REGION_ENCODING_ON;
    SETUP(extHevcRegion.RegionType, 1000);
    SETUP(extHevcRegion.RegionId, 1000);
    SETUP(extHevcParam.PicWidthInLumaSamples, 0xff00);
    SETUP(extHevcParam.PicHeightInLumaSamples, 0xff00);
    input.extEncoderROI.NumROI = expectedOutput.extEncoderROI.NumROI = 1;
    SETUP(extEncoderROI.ROI[0].Right, 0xf000);
    SETUP(extEncoderROI.ROI[0].Bottom, 0xf000);
#undef SETUP
}

// for debug purposes check each field individually instead of using memcmp
#define EXPECT(FIELD) EXPECT_EQ(expected.FIELD, actual.FIELD)
template <> void ApiTestCommon::ExpectEqual<mfxInfoMFX>(const mfxInfoMFX &expected, const mfxInfoMFX &actual) {
    EXPECT(BRCParamMultiplier);
    EXPECT(CodecId);
    EXPECT(CodecProfile);
    EXPECT(CodecLevel);
    EXPECT(NumThread);
    EXPECT(TargetUsage);
    EXPECT(GopPicSize);
    EXPECT(GopRefDist);
    EXPECT(GopOptFlag);
    EXPECT(IdrInterval);
    EXPECT(RateControlMethod);
    EXPECT(BufferSizeInKB);
    EXPECT(InitialDelayInKB);
    EXPECT(TargetKbps);
    EXPECT(MaxKbps);
    EXPECT(NumSlice);
    EXPECT(NumRefFrame);
    EXPECT(EncodedOrder);
    EXPECT(FrameInfo.FourCC);
    EXPECT(FrameInfo.Width);
    EXPECT(FrameInfo.Height);
    EXPECT(FrameInfo.CropX);
    EXPECT(FrameInfo.CropY);
    EXPECT(FrameInfo.CropW);
    EXPECT(FrameInfo.CropH);
    EXPECT(FrameInfo.FrameRateExtN);
    EXPECT(FrameInfo.FrameRateExtD);
    EXPECT(FrameInfo.AspectRatioW);
    EXPECT(FrameInfo.AspectRatioH);
    EXPECT(FrameInfo.PicStruct);
    EXPECT(FrameInfo.ChromaFormat);
    EXPECT_EQ(0, MemCompare(expected, actual)); // double check by full memcmp
}
template <> void ApiTestCommon::ExpectEqual<mfxExtHEVCRegion>(const mfxExtHEVCRegion &expected, const mfxExtHEVCRegion &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT(RegionEncoding);
    EXPECT(RegionId);
    EXPECT(RegionType);
    EXPECT_EQ(0, MemCompare(expected, actual)); // double check by full memcmp
}
template <> void ApiTestCommon::ExpectEqual<mfxExtHEVCTiles>(const mfxExtHEVCTiles &expected, const mfxExtHEVCTiles &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT(NumTileRows);
    EXPECT(NumTileColumns);
    EXPECT_EQ(0, MemCompare(expected, actual)); // double check by full memcmp
}
template <> void ApiTestCommon::ExpectEqual<mfxExtOpaqueSurfaceAlloc>(const mfxExtOpaqueSurfaceAlloc &expected, const mfxExtOpaqueSurfaceAlloc &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT(In.Type);
    EXPECT(In.NumSurface);
    EXPECT(Out.Type);
    EXPECT(Out.NumSurface);
}
template <> void ApiTestCommon::ExpectEqual<mfxExtDumpFiles>(const mfxExtDumpFiles &expected, const mfxExtDumpFiles &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT_EQ(0, MemCompare(expected.ReconFilename, actual.ReconFilename));
}
template <> void ApiTestCommon::ExpectEqual<mfxExtCodingOptionHEVC>(const mfxExtCodingOptionHEVC &expected, const mfxExtCodingOptionHEVC &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT(Log2MaxCUSize);
    EXPECT(MaxCUDepth);
    EXPECT(QuadtreeTULog2MaxSize);
    EXPECT(QuadtreeTULog2MinSize);
    EXPECT(QuadtreeTUMaxDepthIntra);
    EXPECT(QuadtreeTUMaxDepthInter);
    EXPECT(QuadtreeTUMaxDepthInterRD);
    EXPECT(AnalyzeChroma);
    EXPECT(SignBitHiding);
    EXPECT(RDOQuant);
    EXPECT(SAO);
    EXPECT(SplitThresholdStrengthCUIntra);
    EXPECT(SplitThresholdStrengthTUIntra);
    EXPECT(SplitThresholdStrengthCUInter);
    EXPECT(IntraNumCand0_2);
    EXPECT(IntraNumCand0_3);
    EXPECT(IntraNumCand0_4);
    EXPECT(IntraNumCand0_5);
    EXPECT(IntraNumCand0_6);
    EXPECT(IntraNumCand1_2);
    EXPECT(IntraNumCand1_3);
    EXPECT(IntraNumCand1_4);
    EXPECT(IntraNumCand1_5);
    EXPECT(IntraNumCand1_6);
    EXPECT(IntraNumCand2_2);
    EXPECT(IntraNumCand2_3);
    EXPECT(IntraNumCand2_4);
    EXPECT(IntraNumCand2_5);
    EXPECT(IntraNumCand2_6);
    EXPECT(WPP);
    EXPECT(GPB);
    EXPECT(PartModes);
    EXPECT(CmIntraThreshold);
    EXPECT(TUSplitIntra);
    EXPECT(CUSplit);
    EXPECT(IntraAngModes);
    EXPECT(EnableCm);
    EXPECT(BPyramid);
    EXPECT(FastPUDecision);
    EXPECT(HadamardMe);
    EXPECT(TMVP);
    EXPECT(Deblocking);
    EXPECT(RDOQuantChroma);
    EXPECT(RDOQuantCGZ);
    EXPECT(SaoOpt);
    EXPECT(CostChroma);
    EXPECT(PatternIntPel);
    EXPECT(FastSkip);
    EXPECT(PatternSubPel);
    EXPECT(ForceNumThread);
    EXPECT(FastCbfMode);
    EXPECT(PuDecisionSatd);
    EXPECT(MinCUDepthAdapt);
    EXPECT(MaxCUDepthAdapt);
    EXPECT(NumBiRefineIter);
    EXPECT(CUSplitThreshold);
    EXPECT(DeltaQpMode);
    EXPECT(Enable10bit);
    EXPECT(IntraAngModesP);
    EXPECT(IntraAngModesBRef);
    EXPECT(IntraAngModesBnonRef);
    EXPECT(IntraChromaRDO);
    EXPECT(FastInterp);
    EXPECT(SplitThresholdTabIndex);
    EXPECT(CpuFeature);
    EXPECT(TryIntra);
    EXPECT(FastAMPSkipME);
    EXPECT(FastAMPRD);
    EXPECT(SkipMotionPartition);
    EXPECT(SkipCandRD);
    EXPECT(FramesInParallel);
    EXPECT(AdaptiveRefs);
    EXPECT(FastCoeffCost);
    EXPECT(NumRefFrameB);
    EXPECT(IntraMinDepthSC);
    EXPECT(AnalyzeCmplx);
    EXPECT(RateControlDepth);
    EXPECT(LowresFactor);
    EXPECT(DeblockBorders);
    EXPECT_EQ(0, MemCompare(expected, actual)); // double check by full memcmp
}
template <> void ApiTestCommon::ExpectEqual<mfxExtCodingOption>(const mfxExtCodingOption &expected, const mfxExtCodingOption &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT(RateDistortionOpt);
    EXPECT(MECostType);
    EXPECT(MESearchType);
    EXPECT(MVSearchWindow.x);
    EXPECT(MVSearchWindow.y);
    EXPECT(EndOfSequence);
    EXPECT(FramePicture);
    EXPECT(CAVLC);
    EXPECT(RecoveryPointSEI);
    EXPECT(ViewOutput);
    EXPECT(NalHrdConformance);
    EXPECT(SingleSeiNalUnit);
    EXPECT(VuiVclHrdParameters);
    EXPECT(RefPicListReordering);
    EXPECT(ResetRefList);
    EXPECT(RefPicMarkRep);
    EXPECT(FieldOutput);
    EXPECT(IntraPredBlockSize);
    EXPECT(InterPredBlockSize);
    EXPECT(MVPrecision);
    EXPECT(MaxDecFrameBuffering);
    EXPECT(AUDelimiter);
    EXPECT(EndOfStream);
    EXPECT_EQ(0, MemCompare(expected, actual)); // double check by full memcmp
}
template <> void ApiTestCommon::ExpectEqual<mfxExtCodingOption2>(const mfxExtCodingOption2 &expected, const mfxExtCodingOption2 &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT(IntRefType);
    EXPECT(IntRefCycleSize);
    EXPECT(IntRefQPDelta);
    EXPECT(MaxFrameSize);
    EXPECT(MaxSliceSize);
    EXPECT(BitrateLimit);
    EXPECT(MBBRC);
    EXPECT(ExtBRC);
    EXPECT(LookAheadDepth);
    EXPECT(Trellis);
    EXPECT(RepeatPPS);
    EXPECT(BRefType);
    EXPECT(AdaptiveI);
    EXPECT(AdaptiveB);
    EXPECT(LookAheadDS);
    EXPECT(NumMbPerSlice);
    EXPECT(SkipFrame);
    EXPECT(MinQPI);
    EXPECT(MaxQPI);
    EXPECT(MinQPP);
    EXPECT(MaxQPP);
    EXPECT(MinQPB);
    EXPECT(MaxQPB);
    EXPECT(FixedFrameRate);
    EXPECT(DisableDeblockingIdc);
    EXPECT(DisableVUI);
    EXPECT(BufferingPeriodSEI);
    EXPECT(EnableMAD);
    EXPECT(UseRawRef);
    EXPECT_EQ(0, MemCompare(expected, actual));
}
template <> void ApiTestCommon::ExpectEqual<mfxExtEncoderROI>(const mfxExtEncoderROI &expected, const mfxExtEncoderROI &actual) {
    EXPECT(Header.BufferId);
    EXPECT(Header.BufferSz);
    EXPECT(NumROI);
    for (Ipp32s i = 0; i < expected.NumROI; i++) {
        EXPECT(ROI[i].Left);
        EXPECT(ROI[i].Right);
        EXPECT(ROI[i].Top);
        EXPECT(ROI[i].Bottom);
        EXPECT(ROI[i].Priority);
    }
    EXPECT_EQ(0, MemCompare(expected, actual));
}
#undef EXPECT

template <> void ApiTestCommon::ExpectEqual<ParamSet>(const ParamSet &expected, const ParamSet &actual) {
    EXPECT_EQ(expected.videoParam.AsyncDepth, actual.videoParam.AsyncDepth);
    EXPECT_EQ(expected.videoParam.Protected, actual.videoParam.Protected);
    EXPECT_EQ(expected.videoParam.IOPattern, actual.videoParam.IOPattern);
    ExpectEqual(expected.videoParam.mfx, actual.videoParam.mfx);
    ExpectEqual(expected.extHevcRegion, actual.extHevcRegion);
    ExpectEqual(expected.extHevcTiles, actual.extHevcTiles);
    ExpectEqual(expected.extCodingOptionHevc, actual.extCodingOptionHevc);
    ExpectEqual(expected.extOpaqueSurfaceAlloc, actual.extOpaqueSurfaceAlloc);
    ExpectEqual(expected.extDumpFiles, actual.extDumpFiles);
    ExpectEqual(expected.extCodingOption, actual.extCodingOption);
    ExpectEqual(expected.extCodingOption2, actual.extCodingOption2);
    ExpectEqual(expected.extCodingOption2, actual.extCodingOption2);
    ExpectEqual(expected.extEncoderROI, actual.extEncoderROI);
}

std::vector<Nal> ApiTestCommon::SplitNals(const Ipp8u *start, const Ipp8u *end)
{
    std::vector<Nal> nals;
    for (; end - start > 4; ++start)
        if (start[0] == 0 && start[1] == 0 && (start[2] == 0 && start[3] == 1 || start[2] == 1))
            break;
    if (end - start <= 4)
        start = end;

    while (start < end) {
        Nal nal;
        nal.start = start;
        for (start += 3; end - start > 4; ++start)
            if (start[0] == 0 && start[1] == 0 && (start[2] == 0 && start[3] == 1 || start[2] == 1))
                break;
        if (end - start <= 4)
            start = end;
        nal.end = start;
        nal.type = (nal.start[(nal.start[2] == 0 ? 4 : 3)] >> 1) & 0x3f;
        nals.push_back(nal);
    }
    return nals;
}

std::vector<Ipp8u> ApiTestCommon::ExtractRbsp(const Ipp8u *inStart, const Ipp8u *inEnd)
{
    std::vector<Ipp8u> outArray(inEnd - inStart);

    Ipp8u *out = outArray.data();
    while (inEnd - inStart > 2) {
        *out++ = *inStart++;
        if (inStart[-1] == 0 && inStart[0] == 0 && inStart[1] == 3) {
            *out++ = *inStart++;
            ++inStart;
        }
    }
    while (inEnd != inStart)
        *out++ = *inStart++;

    outArray.resize(out - outArray.data());
    return outArray;
}
