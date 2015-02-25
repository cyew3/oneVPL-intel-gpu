#include "dump.h"
#include "../loggers/log.h"

std::string DumpContext::dump(const std::string structName, const mfxDecodeStat &decodeStat)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(decodeStat.reserved) + "\n";
    str += structName + ".NumFrame=" + ToString(decodeStat.NumFrame) + "\n";
    str += structName + ".NumSkippedFrame=" + ToString(decodeStat.NumSkippedFrame) + "\n";
    str += structName + ".NumError=" + ToString(decodeStat.NumError) + "\n";
    str += structName + ".NumCachedFrame=" + ToString(decodeStat.NumCachedFrame);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxEncodeCtrl &EncodeCtrl)
{
    std::string str;
    str += dump(structName + ".Header=", EncodeCtrl.Header) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(EncodeCtrl.reserved) + "\n";
    str += structName + ".SkipFrame=" + ToString(EncodeCtrl.SkipFrame) + "\n";
    str += structName + ".QP=" + ToString(EncodeCtrl.QP) + "\n";
    str += structName + ".FrameType=" + ToString(EncodeCtrl.FrameType) + "\n";
    str += structName + ".NumPayload=" + ToString(EncodeCtrl.NumPayload) + "\n";
    str += structName + ".reserved2=" + ToString(EncodeCtrl.reserved2) + "\n";
    str += dump_mfxExtParams(structName, EncodeCtrl) + "\n";
    str += structName + ".Payload=" + ToString(EncodeCtrl.Payload);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxEncodeStat &encodeStat)
{
    std::string str;
    str += structName + ".NumBit=" + ToString(encodeStat.NumBit) + "\n";
    str += structName + ".NumCachedFrame=" + ToString(encodeStat.NumCachedFrame) + "\n";
    str += structName + ".NumFrame=" + ToString(encodeStat.NumFrame) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encodeStat.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCodingOption &_struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    str += structName + ".reserved1=" + ToString(_struct.reserved1) + "\n";
    str += structName + ".RateDistortionOpt=" + ToString(_struct.RateDistortionOpt) + "\n";
    str += structName + ".MECostType=" + ToString(_struct.MECostType) + "\n";
    str += structName + ".MESearchType=" + ToString(_struct.MESearchType) + "\n";
    //mfxI16Pair  MVSearchWindow; // TODO: dump the field
    str += structName + ".EndOfSequence=" + ToString(_struct.EndOfSequence) + "\n";
    str += structName + ".FramePicture=" + ToString(_struct.FramePicture) + "\n";

    str += structName + ".CAVLC=" + ToString(_struct.CAVLC) + "\n";
    str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(_struct.reserved2) + "\n";
    str += structName + ".RecoveryPointSEI=" + ToString(_struct.RecoveryPointSEI) + "\n";
    str += structName + ".ViewOutput=" + ToString(_struct.ViewOutput) + "\n";
    str += structName + ".NalHrdConformance=" + ToString(_struct.NalHrdConformance) + "\n";
    str += structName + ".SingleSeiNalUnit=" + ToString(_struct.SingleSeiNalUnit) + "\n";
    str += structName + ".VuiVclHrdParameters=" + ToString(_struct.VuiVclHrdParameters) + "\n";

    str += structName + ".RefPicListReordering=" + ToString(_struct.RefPicListReordering) + "\n";
    str += structName + ".ResetRefList=" + ToString(_struct.ResetRefList) + "\n";
    str += structName + ".RefPicMarkRep=" + ToString(_struct.RefPicMarkRep) + "\n";
    str += structName + ".FieldOutput=" + ToString(_struct.FieldOutput) + "\n";

    str += structName + ".IntraPredBlockSize=" + ToString(_struct.IntraPredBlockSize) + "\n";
    str += structName + ".InterPredBlockSize=" + ToString(_struct.InterPredBlockSize) + "\n";
    str += structName + ".MVPrecision=" + ToString(_struct.MVPrecision) + "\n";
    str += structName + ".MaxDecFrameBuffering=" + ToString(_struct.MaxDecFrameBuffering) + "\n";

    str += structName + ".AUDelimiter=" + ToString(_struct.AUDelimiter) + "\n";
    str += structName + ".EndOfStream=" + ToString(_struct.EndOfStream) + "\n";
    str += structName + ".PicTimingSEI=" + ToString(_struct.PicTimingSEI) + "\n";
    str += structName + ".VuiNalHrdParameters=" + ToString(_struct.VuiNalHrdParameters) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCodingOption2 &_struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(IntRefType);
    DUMP_FIELD(IntRefCycleSize);
    DUMP_FIELD(IntRefQPDelta);

    DUMP_FIELD(MaxFrameSize);
    DUMP_FIELD(MaxSliceSize);

    DUMP_FIELD(BitrateLimit);
    DUMP_FIELD(MBBRC);
    DUMP_FIELD(ExtBRC);
    DUMP_FIELD(LookAheadDepth);
    DUMP_FIELD(Trellis);
    DUMP_FIELD(RepeatPPS);
    DUMP_FIELD(BRefType);
    DUMP_FIELD(AdaptiveI);
    DUMP_FIELD(AdaptiveB);
    DUMP_FIELD(LookAheadDS);
    DUMP_FIELD(NumMbPerSlice);
    DUMP_FIELD(SkipFrame);
    DUMP_FIELD(MinQPI);
    DUMP_FIELD(MaxQPI);
    DUMP_FIELD(MinQPP);
    DUMP_FIELD(MaxQPP);
    DUMP_FIELD(MinQPB);
    DUMP_FIELD(MaxQPB);
    DUMP_FIELD(FixedFrameRate);
    DUMP_FIELD(DisableDeblockingIdc);
    DUMP_FIELD(DisableVUI);
    DUMP_FIELD(BufferingPeriodSEI);
    DUMP_FIELD(EnableMAD);
    DUMP_FIELD(UseRawRef);

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCodingOption3 &_struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(NumSliceI);
    DUMP_FIELD(NumSliceP);
    DUMP_FIELD(NumSliceB);

    DUMP_FIELD(WinBRCMaxAvgKbps);		
    DUMP_FIELD(WinBRCSize);		
 
    DUMP_FIELD(QVBRQuality);
    DUMP_FIELD(EnableMBQP);
    DUMP_FIELD(IntRefCycleDist);
    DUMP_FIELD(DirectBiasAdjustment);          /* tri-state option */
    DUMP_FIELD(GlobalMotionBiasAdjustment);    /* tri-state option */
    DUMP_FIELD(MVCostScalingFactor);
    DUMP_FIELD(MBDisableSkipMap);              /* tri-state option */

    DUMP_FIELD_RESERVED(reserved);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtEncoderResetOption &_struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(StartNewSequence);
    DUMP_FIELD_RESERVED(reserved);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVPPDoNotUse &_struct)
{
    std::string str;
    str += dump(structName + ".Header", _struct.Header) + "\n";
    // TODO dump list of algs
    DUMP_FIELD(NumAlg);
    DUMP_FIELD(AlgList);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtVppAuxData &extVppAuxData)
{
    std::string str;
    str += dump(structName + ".Header", extVppAuxData.Header) + "\n";
    str += structName + ".SpatialComplexity=" + ToString(extVppAuxData.SpatialComplexity) + "\n";
    str += structName + ".TemporalComplexity=" + ToString(extVppAuxData.TemporalComplexity) + "\n";
    str += structName + ".PicStruct=" + ToString(extVppAuxData.PicStruct) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(extVppAuxData.reserved) + "\n";
    str += structName + ".SceneChangeRate=" + ToString(extVppAuxData.SceneChangeRate) + "\n";
    str += structName + ".RepeatedFrame=" + ToString(extVppAuxData.RepeatedFrame);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameAllocRequest &frameAllocRequest)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameAllocRequest.reserved) + "\n";
    str += dump(structName+".Info", frameAllocRequest.Info) + "\n";
    str += structName + ".Type=" + ToString(frameAllocRequest.Type) + "\n";
    str += structName + ".NumFrameMin=" + ToString(frameAllocRequest.NumFrameMin) + "\n";
    str += structName + ".NumFrameSuggested=" + ToString(frameAllocRequest.NumFrameSuggested) + "\n";
    str += structName + ".reserved2=" + ToString(frameAllocRequest.reserved2);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameData &frameData)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameData.reserved) + "\n";
    str += structName + ".reserved1=" + ToString(frameData.reserved) + "\n";
    str += structName + ".PitchHigh=" + ToString(frameData.PitchHigh) + "\n";
    str += structName + ".TimeStamp=" + ToString(frameData.TimeStamp) + "\n";
    str += structName + ".FrameOrder=" + ToString(frameData.FrameOrder) + "\n";
    str += structName + ".Locked=" + ToString(frameData.Locked) + "\n";
    str += structName + ".Pitch=" + ToString(frameData.Pitch) + "\n";
    str += structName + ".PitchLow=" + ToString(frameData.PitchLow) + "\n";
    if (Log::GetLogLevel() == LOG_LEVEL_FULL) {
        str += structName + ".Y=" + ToHexFormatString(frameData.Y) + "\n";
        str += structName + ".R=" + ToHexFormatString(frameData.R) + "\n";
        str += structName + ".UV=" + ToHexFormatString(frameData.UV) + "\n";
        str += structName + ".VU=" + ToHexFormatString(frameData.VU) + "\n";
        str += structName + ".CbCr=" + ToHexFormatString(frameData.CbCr) + "\n";
        str += structName + ".CrCb=" + ToHexFormatString(frameData.CrCb) + "\n";
        str += structName + ".Cb=" + ToHexFormatString(frameData.Cb) + "\n";
        str += structName + ".U=" + ToHexFormatString(frameData.U) + "\n";
        str += structName + ".G=" + ToHexFormatString(frameData.G) + "\n";
        str += structName + ".Cr=" + ToHexFormatString(frameData.Cr) + "\n";
        str += structName + ".V=" + ToHexFormatString(frameData.V) + "\n";
        str += structName + ".B=" + ToHexFormatString(frameData.B) + "\n";
        str += structName + ".A=" + ToHexFormatString(frameData.A) + "\n";
    }
    str += structName + ".MemId=" + ToString(frameData.MemId) + "\n";
    str += structName + ".Corrupted=" + ToString(frameData.Corrupted) + "\n";
    str += structName + ".DataFlag=" + ToString(frameData.DataFlag);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameId &frame)
{
    std::string str;
    str += structName + ".TemporalId=" + ToString(frame.TemporalId) + "\n";
    str += structName + ".PriorityId=" + ToString(frame.PriorityId) + "\n";
    str += structName + ".DependencyId=" + ToString(frame.DependencyId) + "\n";
    str += structName + ".QualityId=" + ToString(frame.QualityId) + "\n";
    str += structName + ".ViewId=" + ToString(frame.ViewId);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameInfo &info)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(info.reserved) + "\n";
    str += dump(structName + ".mfxFrameId", info.FrameId) + "\n";
    str += structName + ".FourCC=" + GetFourCC(info.FourCC) + "\n";
    str += structName + ".Width=" + ToString(info.Width) + "\n";
    str += structName + ".Height=" + ToString(info.Height) + "\n";
    str += structName + ".CropX=" + ToString(info.CropX) + "\n";
    str += structName + ".CropY=" + ToString(info.CropY) + "\n";
    str += structName + ".CropW=" + ToString(info.CropW) + "\n";
    str += structName + ".CropH=" + ToString(info.CropH) + "\n";
    str += structName + ".FrameRateExtN=" + ToString(info.FrameRateExtN) + "\n";
    str += structName + ".FrameRateExtD=" + ToString(info.FrameRateExtD) + "\n";
    str += structName + ".reserved3=" + ToString(info.reserved3) + "\n";
    str += structName + ".AspectRatioW=" + ToString(info.AspectRatioW) + "\n";
    str += structName + ".AspectRatioH=" + ToString(info.AspectRatioH) + "\n";
    str += structName + ".PicStruct=" + ToString(info.PicStruct) + "\n";
    str += structName + ".ChromaFormat=" + ToString(info.ChromaFormat) + "\n";
    str += structName + ".reserved2=" + ToString(info.reserved2);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxFrameSurface1 &frameSurface1)
{
    std::string str;
    str += dump(structName + ".Data", frameSurface1.Data) + "\n";
    str += dump(structName + ".Info", frameSurface1.Info) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameSurface1.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxHandleType &handleType)
{
    return std::string("mfxHandleType " + structName + "=" + ToString(handleType));
}

std::string DumpContext::dump(const std::string structName, const mfxInfoMFX &mfx)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(mfx.reserved) + "\n";
    str += structName + ".reserved5=" + ToString(mfx.reserved5) + "\n";
    str += structName + ".BRCParamMultiplier=" + ToString(mfx.BRCParamMultiplier) + "\n";
    str += dump(structName + ".FrameInfo", mfx.FrameInfo) + "\n";
    str += structName + ".CodecId=" + ToString(mfx.CodecId) + "\n";
    str += structName + ".CodecProfile=" + ToString(mfx.CodecProfile) + "\n";
    str += structName + ".CodecLevel=" + ToString(mfx.CodecLevel) + "\n";
    str += structName + ".NumThread=" + ToString(mfx.NumThread) + "\n";
    str += structName + ".TargetUsage=" + ToString(mfx.TargetUsage) + "\n";
    str += structName + ".GopPicSize=" + ToString(mfx.GopPicSize) + "\n";
    str += structName + ".GopRefDist=" + ToString(mfx.GopRefDist) + "\n";
    str += structName + ".GopOptFlag=" + ToString(mfx.GopOptFlag) + "\n";
    str += structName + ".IdrInterval=" + ToString(mfx.IdrInterval) + "\n";
    str += structName + ".RateControlMethod=" + ToString(mfx.RateControlMethod) + "\n";
    str += structName + ".InitialDelayInKB=" + ToString(mfx.InitialDelayInKB) + "\n";
    str += structName + ".QPI=" + ToString(mfx.QPI) + "\n";
    str += structName + ".Accuracy=" + ToString(mfx.Accuracy) + "\n";
    str += structName + ".BufferSizeInKB=" + ToString(mfx.BufferSizeInKB) + "\n";
    str += structName + ".TargetKbps=" + ToString(mfx.TargetKbps) + "\n";
    str += structName + ".QPP=" + ToString(mfx.QPP) + "\n";
    str += structName + ".ICQQuality=" + ToString(mfx.ICQQuality) + "\n";
    str += structName + ".MaxKbps=" + ToString(mfx.MaxKbps) + "\n";
    str += structName + ".QPB=" + ToString(mfx.QPB) + "\n";
    str += structName + ".Convergence=" + ToString(mfx.Convergence) + "\n";
    str += structName + ".NumSlice=" + ToString(mfx.NumSlice) + "\n";
    str += structName + ".NumRefFrame=" + ToString(mfx.NumRefFrame) + "\n";
    str += structName + ".EncodedOrder=" + ToString(mfx.EncodedOrder) + "\n";
    str += structName + ".DecodedOrder=" + ToString(mfx.DecodedOrder) + "\n";
    str += structName + ".ExtendedPicStruct=" + ToString(mfx.ExtendedPicStruct) + "\n";
    str += structName + ".TimeStampCalc=" + ToString(mfx.TimeStampCalc) + "\n";
    str += structName + ".SliceGroupsPresent=" + ToString(mfx.SliceGroupsPresent) + "\n";
    str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(mfx.reserved2) + "\n";
    str += structName + ".JPEGChromaFormat=" + ToString(mfx.JPEGChromaFormat) + "\n";
    str += structName + ".Rotation=" + ToString(mfx.Rotation) + "\n";
    str += structName + ".JPEGColorFormat=" + ToString(mfx.JPEGColorFormat) + "\n";
    str += structName + ".InterleavedDec=" + ToString(mfx.InterleavedDec) + "\n";
    str += structName + ".reserved3[]=" + DUMP_RESERVED_ARRAY(mfx.reserved3) + "\n";
    str += structName + ".Interleaved=" + ToString(mfx.Interleaved) + "\n";
    str += structName + ".Quality=" + ToString(mfx.Quality) + "\n";
    str += structName + ".RestartInterval=" + ToString(mfx.RestartInterval) + "\n";
    str += structName + ".reserved5[]=" + DUMP_RESERVED_ARRAY(mfx.reserved5) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxInfoVPP &vpp)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(vpp.reserved) + "\n";
    str += dump(structName + ".In", vpp.In) + "\n" +
    str += dump(structName + ".Out", vpp.Out);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxPayload &payload)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(payload.reserved) + "\n";
    str += structName + ".Data=" + ToString(payload.Data) + "\n";
    str += structName + ".NumBit=" + ToString(payload.NumBit) + "\n";
    str += structName + ".Type=" + ToString(payload.Type) + "\n";
    str += structName + ".BufSize=" + ToString(payload.BufSize);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxSkipMode &skipMode)
{
    return std::string("mfxSkipMode " + structName + "=" + ToString(skipMode));
}

std::string DumpContext::dump(const std::string structName, const mfxVideoParam& videoParam)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(videoParam.reserved) + "\n";
    str += structName + ".AsyncDepth=" + ToString(videoParam.AsyncDepth) + "\n";
    if(context == DUMPCONTEXT_MFX)
        str += dump(structName + ".mfx", videoParam.mfx) + "\n";
    else if(context == DUMPCONTEXT_VPP)
        str += dump(structName + ".vpp", videoParam.vpp) + "\n";
    else if(context == DUMPCONTEXT_ALL){
        str += dump(structName + ".mfx", videoParam.mfx) + "\n";
        str += dump(structName + ".vpp", videoParam.vpp) + "\n";
    }
    str += structName + ".Protected=" + ToString(videoParam.Protected) + "\n";
    str += structName + ".IOPattern=" + ToString(videoParam.IOPattern) + "\n";
    str += dump_mfxExtParams(structName, videoParam) + "\n";
    str += structName + ".reserved2=" + ToString(videoParam.reserved2);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxVPPStat &vppStat)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(vppStat.reserved) + "\n";
    str += structName + ".NumFrame=" + ToString(vppStat.NumFrame) + "\n";
    str += structName + ".NumCachedFrame=" + ToString(vppStat.NumCachedFrame);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamGammaCorrection &CamGammaCorrection)
{
    std::string str;
    str += dump(structName + ".Header=", CamGammaCorrection.Header) + "\n";
    str += structName + ".Mode=" + ToString(CamGammaCorrection.Mode) + "\n";
    str += structName + ".reserved1=" + ToString(CamGammaCorrection.reserved1) + "\n";
    str += structName + ".GammaValue=" + ToString(CamGammaCorrection.GammaValue) + "\n";
    str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(CamGammaCorrection.reserved2) + "\n";
    str += structName + ".NumPoints=" + ToString(CamGammaCorrection.NumPoints) + "\n";
    str += structName + ".GammaPoint=" + ToString(CamGammaCorrection.GammaPoint) + "\n";
    str += structName + ".GammaCorrected=" + ToString(CamGammaCorrection.GammaCorrected) + "\n";
    str += structName + ".reserved3[]=" + DUMP_RESERVED_ARRAY(CamGammaCorrection.reserved3) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamWhiteBalance &CamWhiteBalance)
{
    std::string str;
    str += dump(structName + ".Header=", CamWhiteBalance.Header) + "\n";
    str += structName + ".Mode=" + ToString(CamWhiteBalance.Mode) + "\n";
    str += structName + ".R=" + ToString(CamWhiteBalance.R) + "\n";
    str += structName + ".G0=" + ToString(CamWhiteBalance.G0) + "\n";
    str += structName + ".B=" + ToString(CamWhiteBalance.B) + "\n";
    str += structName + ".G1=" + ToString(CamWhiteBalance.G1) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CamWhiteBalance.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamHotPixelRemoval &CamHotPixelRemoval)
{
    std::string str;
    str += dump(structName + ".Header=", CamHotPixelRemoval.Header) + "\n";
    str += structName + ".PixelThresholdDifference=" + ToString(CamHotPixelRemoval.PixelThresholdDifference) + "\n";
    str += structName + ".PixelCountThreshold=" + ToString(CamHotPixelRemoval.PixelCountThreshold) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamBlackLevelCorrection &CamBlackLevelCorrection)
{
    std::string str;
    str += dump(structName + ".Header=", CamBlackLevelCorrection.Header) + "\n";
    str += structName + ".R=" + ToString(CamBlackLevelCorrection.R) + "\n";
    str += structName + ".G0=" + ToString(CamBlackLevelCorrection.G0) + "\n";
    str += structName + ".B=" + ToString(CamBlackLevelCorrection.B) + "\n";
    str += structName + ".G1=" + ToString(CamBlackLevelCorrection.G1) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CamBlackLevelCorrection.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxVignetteCorrectionParams &VignetteCorrectionParams)
{
    std::string str;
    str += structName + ".R=" + ToString(VignetteCorrectionParams.R) + "\n";
    str += structName + ".G0=" + ToString(VignetteCorrectionParams.G0) + "\n";
    str += structName + ".B=" + ToString(VignetteCorrectionParams.B) + "\n";
    str += structName + ".G1=" + ToString(VignetteCorrectionParams.G1) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamVignetteCorrection &CamVignetteCorrection)
{
    std::string str;
    str += dump(structName + ".Header=", CamVignetteCorrection.Header) + "\n";
    str += structName + ".Width=" + ToString(CamVignetteCorrection.Width) + "\n";
    str += structName + ".Height=" + ToString(CamVignetteCorrection.Height) + "\n";
    str += structName + ".Pitch=" + ToString(CamVignetteCorrection.Pitch) + "\n";
    str += structName + ".MaskPrecision=" + ToString(CamVignetteCorrection.MaskPrecision) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CamVignetteCorrection.reserved) + "\n";
    str += dump(structName + ".CorrectionMap", CamVignetteCorrection.CorrectionMap) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamBayerDenoise &CamBayerDenoise)
{
    std::string str;
    str += dump(structName + ".Header=", CamBayerDenoise.Header) + "\n";
    str += structName + ".Threshold=" + ToString(CamBayerDenoise.Threshold) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CamBayerDenoise.reserved) + "\n";
    str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(CamBayerDenoise.reserved2) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamColorCorrection3x3 &CamColorCorrection3x3)
{
    std::string str;
    str += dump(structName + ".Header=", CamColorCorrection3x3.Header) + "\n";
    str += structName + ".CCM*=" + ToString(CamColorCorrection3x3.CCM) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CamColorCorrection3x3.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamPadding &CamPadding)
{
    std::string str;
    str += dump(structName + ".Header=", CamPadding.Header) + "\n";
    str += structName + ".Top=" + ToString(CamPadding.Top) + "\n";
    str += structName + ".Bottom=" + ToString(CamPadding.Bottom) + "\n";
    str += structName + ".Left=" + ToString(CamPadding.Left) + "\n";
    str += structName + ".Right=" + ToString(CamPadding.Right) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CamPadding.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtCamPipeControl &CamPipeControl)
{
    std::string str;
    str += dump(structName + ".Header=", CamPipeControl.Header) + "\n";
    str += structName + ".RawFormat=" + ToString(CamPipeControl.RawFormat) + "\n";
    str += structName + ".reserved1=" + ToString(CamPipeControl.reserved1) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CamPipeControl.reserved) + "\n";
    return str;
}