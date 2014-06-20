#include "dump.h"
#include "../loggers/log.h"

std::string dump_mfxDecodeStat(const std::string structName, mfxDecodeStat *decodeStat)
{
    std::string str = "mfxDecodeStat* " + structName + "=" + ToString(decodeStat) + "\n";
    if(decodeStat){
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(decodeStat->reserved) + "\n";
      str += structName + ".NumFrame=" + ToString(decodeStat->NumFrame) + "\n";
      str += structName + ".NumSkippedFrame=" + ToString(decodeStat->NumSkippedFrame) + "\n";
      str += structName + ".NumError=" + ToString(decodeStat->NumError) + "\n";
      str += structName + ".NumCachedFrame=" + ToString(decodeStat->NumCachedFrame);
    }

    return str;
}

std::string dump_mfxEncodeCtrl(const std::string structName, mfxEncodeCtrl *EncodeCtrl)
{
    std::string str = "mfxEncodeCtrl* " + structName + "=" + ToString(EncodeCtrl) + "\n";
    if(EncodeCtrl){
      str += dump_mfxExtBuffer(structName + ".Header=", &EncodeCtrl->Header) + "\n";
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(EncodeCtrl->reserved) + "\n";
      str += structName + ".SkipFrame=" + ToString(EncodeCtrl->SkipFrame) + "\n";
      str += structName + ".QP=" + ToString(EncodeCtrl->QP) + "\n";
      str += structName + ".FrameType=" + ToString(EncodeCtrl->FrameType) + "\n";
      str += structName + ".NumExtParam=" + ToString(EncodeCtrl->NumExtParam) + "\n";
      str += structName + ".NumPayload=" + ToString(EncodeCtrl->NumPayload) + "\n";
      str += structName + ".reserved2=" + ToString(EncodeCtrl->reserved2) + "\n";
      str += structName + ".ExtParam=" + ToString(EncodeCtrl->ExtParam) + "\n";
      str += structName + ".Payload=" + ToString(EncodeCtrl->Payload);
    }
    return str;
}

std::string dump_mfxEncodeStat(const std::string structName, mfxEncodeStat *encodeStat)
{
    std::string str = "mfxEncodeStat* " + structName + "=" + ToString(encodeStat) + "\n";
    if(encodeStat){
      str += structName + ".NumBit=" + ToString(encodeStat->NumBit) + "\n";
      str += structName + ".NumCachedFrame=" + ToString(encodeStat->NumCachedFrame) + "\n";
      str += structName + ".NumFrame=" + ToString(encodeStat->NumFrame) + "\n";
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encodeStat->reserved) + "\n";
    }
    return str;
}

std::string dump_mfxExtVppAuxData(const std::string structName, mfxExtVppAuxData *extVppAuxData)
{
    std::string str = "mfxExtVppAuxData* " + structName + "=" + ToString(extVppAuxData) + "\n";
    if(extVppAuxData){
      str += dump_mfxExtBuffer(structName + ".Header", &extVppAuxData->Header) + "\n";
      str += structName + ".SpatialComplexity=" + ToString(extVppAuxData->SpatialComplexity) + "\n";
      str += structName + ".TemporalComplexity=" + ToString(extVppAuxData->TemporalComplexity) + "\n";
      str += structName + ".PicStruct=" + ToString(extVppAuxData->PicStruct) + "\n";
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(extVppAuxData->reserved) + "\n";
      str += structName + ".SceneChangeRate=" + ToString(extVppAuxData->SceneChangeRate) + "\n";
      str += structName + ".RepeatedFrame=" + ToString(extVppAuxData->RepeatedFrame);
    }
    return str;
}

std::string dump_mfxFrameAllocRequest(const std::string structName, mfxFrameAllocRequest *frameAllocRequest)
{
    std::string str = "mfxFrameAllocRequest* " + structName + "=" + ToString(frameAllocRequest) + "\n";
    if(frameAllocRequest){
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameAllocRequest->reserved) + "\n";
      str += dump_mfxFrameInfo(structName+".Info", &frameAllocRequest->Info) + "\n";
      str += structName + ".Type=" +ToString(frameAllocRequest->Type) + "\n";
      str += structName + ".NumFrameMin=" +ToString(frameAllocRequest->NumFrameMin) + "\n";
      str += structName + ".NumFrameSuggested=" +ToString(frameAllocRequest->NumFrameSuggested) + "\n";
      str += structName + ".reserved2=" +ToString(frameAllocRequest->reserved2);
    }
    return str;
}

std::string dump_mfxFrameData(const std::string structName, mfxFrameData *frameData)
{
    std::string str = "mfxFrameData* " + structName + "=" + ToString(frameData) + "\n";
    if(frameData) {
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameData->reserved) + "\n";
      str += structName + ".reserved1=" + ToString(frameData->reserved) + "\n";
      str += structName + ".PitchHigh=" + ToString(frameData->PitchHigh) + "\n";
      str += structName + ".TimeStamp=" + ToString(frameData->TimeStamp) + "\n";
      str += structName + ".FrameOrder=" + ToString(frameData->FrameOrder) + "\n";
      str += structName + ".Locked=" + ToString(frameData->Locked) + "\n";
      str += structName + ".Pitch=" + ToString(frameData->Pitch) + "\n";
      str += structName + ".PitchLow=" + ToString(frameData->PitchLow) + "\n";
      if(Log::GetLogLevel() == LOG_LEVEL_FULL) {
        str += structName + ".Y=" + ToHexFormatString(frameData->Y) + "\n";
        str += structName + ".R=" + ToHexFormatString(frameData->R) + "\n";
        str += structName + ".UV=" + ToHexFormatString(frameData->UV) + "\n";
        str += structName + ".VU=" + ToHexFormatString(frameData->VU) + "\n";
        str += structName + ".CbCr=" + ToHexFormatString(frameData->CbCr) + "\n";
        str += structName + ".CrCb=" + ToHexFormatString(frameData->CrCb) + "\n";
        str += structName + ".Cb=" + ToHexFormatString(frameData->Cb) + "\n";
        str += structName + ".U=" + ToHexFormatString(frameData->U) + "\n";
        str += structName + ".G=" + ToHexFormatString(frameData->G) + "\n";
        str += structName + ".Cr=" + ToHexFormatString(frameData->Cr) + "\n";
        str += structName + ".V=" + ToHexFormatString(frameData->V) + "\n";
        str += structName + ".B=" + ToHexFormatString(frameData->B) + "\n";
        str += structName + ".A=" + ToHexFormatString(frameData->A) + "\n";
      }
      str += structName + ".MemId=" + ToString(frameData->MemId) + "\n";
      str += structName + ".Corrupted=" + ToString(frameData->Corrupted) + "\n";
      str += structName + ".DataFlag=" + ToString(frameData->DataFlag);
    }

    return str;
}

std::string dump_mfxFrameId(const std::string structName, mfxFrameId *frame)
{
    std::string str = "mfxFrameId* " + structName + "= " + ToString(frame) + "\n";
    if(frame){
      str += structName + ".TemporalId=" + ToString(frame->TemporalId) + "\n" +
      structName + ".PriorityId=" + ToString(frame->PriorityId) + "\n" +
      structName + ".DependencyId=" + ToString(frame->DependencyId) + "\n" +
      structName + ".QualityId=" + ToString(frame->QualityId) + "\n" +
      structName + ".ViewId=" + ToString(frame->ViewId);
    }
    return str;
}

std::string dump_mfxFrameInfo(const std::string structName, mfxFrameInfo *info)
{
    std::string str = "mfxFrameInfo* " + structName + "=" + ToString(info) + "\n";
    if(info){
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(info->reserved) + "\n";
      str += dump_mfxFrameId(structName + ".mfxFrameId", &info->FrameId) + "\n";
      str += structName + ".FourCC=" + ToString(info->FourCC) + "\n";
      str += structName + ".Width=" + ToString(info->Width) + "\n";
      str += structName + ".Height=" + ToString(info->Height) + "\n";
      str += structName + ".CropX=" + ToString(info->CropX) + "\n";
      str += structName + ".CropY=" + ToString(info->CropY) + "\n";
      str += structName + ".CropW=" + ToString(info->CropW) + "\n";
      str += structName + ".CropH=" + ToString(info->CropH) + "\n";
      str += structName + ".FrameRateExtN=" + ToString(info->FrameRateExtN) + "\n";
      str += structName + ".FrameRateExtD=" + ToString(info->FrameRateExtD) + "\n";
      str += structName + ".reserved3=" + ToString(info->reserved3) + "\n";
      str += structName + ".AspectRatioW=" + ToString(info->AspectRatioW) + "\n";
      str += structName + ".AspectRatioH=" + ToString(info->AspectRatioH) + "\n";
      str += structName + ".PicStruct=" + ToString(info->PicStruct) + "\n";
      str += structName + ".ChromaFormat=" + ToString(info->ChromaFormat) + "\n";
      str += structName + ".reserved2=" + ToString(info->reserved2);
    }
    return str;
}

std::string dump_mfxFrameSurface1(const std::string structName, mfxFrameSurface1 *frameSurface1)
{
    std::string str = "mfxFrameSurface1* " + structName + "=" + ToString(frameSurface1) + "\n";
    if(frameSurface1){
      str += dump_mfxFrameData(structName + ".Data", &frameSurface1->Data) + "\n";
      str += dump_mfxFrameInfo(structName + ".Info", &frameSurface1->Info) + "\n";
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(frameSurface1->reserved) + "\n";
    }
    return str;
}

std::string dump_mfxHandleType(const std::string structName, mfxHandleType handleType)
{
    return std::string("mfxHandleType " + structName + "=" + ToString(handleType));
}

std::string dump_mfxInfoMFX(const std::string structName, mfxInfoMFX *mfx)
{
    std::string str = "mfxInfoMFX* " + structName + "=" + ToString(mfx) + "\n";
    if(mfx){
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(mfx->reserved) + "\n";
      str += structName + ".reserved4=" + ToString(mfx->reserved4) + "\n";
      str += structName + ".BRCParamMultiplier=" + ToString(mfx->BRCParamMultiplier) + "\n";
      str += dump_mfxFrameInfo(structName + ".FrameInfo", &mfx->FrameInfo) + "\n";
      str += structName + ".CodecId=" + ToString(mfx->CodecId) + "\n";
      str += structName + ".CodecProfile=" + ToString(mfx->CodecProfile) + "\n";
      str += structName + ".CodecLevel=" + ToString(mfx->CodecLevel) + "\n";
      str += structName + ".NumThread=" + ToString(mfx->NumThread) + "\n";
      str += structName + ".TargetUsage=" + ToString(mfx->TargetUsage) + "\n";
      str += structName + ".GopPicSize=" + ToString(mfx->GopPicSize) + "\n";
      str += structName + ".GopRefDist=" + ToString(mfx->GopRefDist) + "\n";
      str += structName + ".GopOptFlag=" + ToString(mfx->GopOptFlag) + "\n";
      str += structName + ".IdrInterval=" + ToString(mfx->IdrInterval) + "\n";
      str += structName + ".RateControlMethod=" + ToString(mfx->RateControlMethod) + "\n";
      str += structName + ".InitialDelayInKB=" + ToString(mfx->InitialDelayInKB) + "\n";
      str += structName + ".QPI=" + ToString(mfx->QPI) + "\n";
      str += structName + ".Accuracy=" + ToString(mfx->Accuracy) + "\n";
      str += structName + ".BufferSizeInKB=" + ToString(mfx->BufferSizeInKB) + "\n";
      str += structName + ".TargetKbps=" + ToString(mfx->TargetKbps) + "\n";
      str += structName + ".QPP=" + ToString(mfx->QPP) + "\n";
      str += structName + ".ICQQuality=" + ToString(mfx->ICQQuality) + "\n";
      str += structName + ".MaxKbps=" + ToString(mfx->MaxKbps) + "\n";
      str += structName + ".QPB=" + ToString(mfx->QPB) + "\n";
      str += structName + ".Convergence=" + ToString(mfx->Convergence) + "\n";
      str += structName + ".NumSlice=" + ToString(mfx->NumSlice) + "\n";
      str += structName + ".NumRefFrame=" + ToString(mfx->NumRefFrame) + "\n";
      str += structName + ".EncodedOrder=" + ToString(mfx->EncodedOrder) + "\n";
      str += structName + ".DecodedOrder=" + ToString(mfx->DecodedOrder) + "\n";
      str += structName + ".ExtendedPicStruct=" + ToString(mfx->ExtendedPicStruct) + "\n";
      str += structName + ".TimeStampCalc=" + ToString(mfx->TimeStampCalc) + "\n";
      str += structName + ".SliceGroupsPresent=" + ToString(mfx->SliceGroupsPresent) + "\n";
      str += structName + ".reserved2[]=" + DUMP_RESERVED_ARRAY(mfx->reserved2) + "\n";
      str += structName + ".JPEGChromaFormat=" + ToString(mfx->JPEGChromaFormat) + "\n";
      str += structName + ".Rotation=" + ToString(mfx->Rotation) + "\n";
      str += structName + ".JPEGColorFormat=" + ToString(mfx->JPEGColorFormat) + "\n";
      str += structName + ".InterleavedDec=" + ToString(mfx->InterleavedDec) + "\n";
      str += structName + ".reserved3[]=" + DUMP_RESERVED_ARRAY(mfx->reserved3) + "\n";
      str += structName + ".Interleaved=" + ToString(mfx->Interleaved) + "\n";
      str += structName + ".Quality=" + ToString(mfx->Quality) + "\n";
      str += structName + ".RestartInterval=" + ToString(mfx->RestartInterval) + "\n";
      str += structName + ".reserved5[]=" + DUMP_RESERVED_ARRAY(mfx->reserved5) + "\n";
    }
    return str;
}

std::string dump_mfxInfoVPP(const std::string structName, mfxInfoVPP *vpp)
{
    std::string str = "mfxInfoVPP* " + structName + "=" + ToString(vpp) + "\n";
    if(vpp){
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(vpp->reserved) + "\n";
      str += dump_mfxFrameInfo(structName + ".In", &vpp->In) + "\n" +
      str += dump_mfxFrameInfo(structName + ".Out", &vpp->Out);
    }
    return str;
}

std::string dump_mfxPayload(const std::string structName, mfxPayload *payload)
{
    std::string str = "mfxPayload* " + structName + "=" + ToString(payload) + "\n";
    if(payload){
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(payload->reserved) + "\n";
      str += structName + ".Data=" + ToString(payload->Data) + "\n";
      str += structName + ".NumBit=" + ToString(payload->NumBit) + "\n";
      str += structName + ".Type=" + ToString(payload->Type) + "\n";
      str += structName + ".BufSize=" + ToString(payload->BufSize);
    }
    return str;
}

std::string dump_mfxSkipMode(const std::string structName, mfxSkipMode skipMode)
{
    return std::string("mfxSkipMode " + structName + "=" + ToString(skipMode));
}

std::string dump_mfxVideoParam(const std::string structName, mfxVideoParam *videoParam)
{
    std::string str = "mfxVideoParam* " + structName + "=" + ToString(videoParam) + "\n";
    if(videoParam){
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(videoParam->reserved) + "\n";
      str += structName + ".AsyncDepth=" + ToString(videoParam->AsyncDepth) + "\n";
      str += dump_mfxInfoMFX(structName + ".mfx", &videoParam->mfx) + "\n";
      str += dump_mfxInfoVPP(structName + ".vpp", &videoParam->vpp) + "\n";
      str += structName + ".Protected=" + ToString(videoParam->Protected) + "\n";
      str += structName + ".IOPattern=" + ToString(videoParam->IOPattern) + "\n";
      str += structName + ".ExtParam=" + ToString(videoParam->ExtParam) + "\n";
      str += structName + ".NumExtParam=" + ToString(videoParam->NumExtParam) + "\n";
      str += structName + ".reserved2=" + ToString(videoParam->reserved2);
    }
    return str;
}

std::string dump_mfxVPPStat(const std::string structName, mfxVPPStat *vppStat)
{
    std::string str = "mfxVPPStat* " + structName + "=" + ToString(vppStat) + "\n";
    if(vppStat) {
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(vppStat->reserved) + "\n";
      str += structName + ".NumFrame=" + ToString(vppStat->NumFrame) + "\n";
      str += structName + ".NumCachedFrame=" + ToString(vppStat->NumCachedFrame);
    }
    return str;
}
