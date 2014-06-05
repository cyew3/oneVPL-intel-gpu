#include "dump.h"

std::string dump_mfxStatus(const std::string structName, mfxStatus status)
{
    return std::string(structName + "=" + ToString(status));
}

std::string dump_mfxSession(const std::string structName, mfxSession session)
{
    return std::string("mfxSession " + structName + "=" + ToString(session));
}

std::string dump_mfxIMPL(const std::string structName, mfxIMPL *impl)
{
    return std::string("mfxIMPL " + structName + "=" + ToString(impl));
}

std::string dump_mfxVersion(const std::string structName, mfxVersion version)
{
    std::string str = "mfxVersion " + structName + ":\n" +
                      structName + ".Major=" + ToString(version.Major) + "\n" +
                      structName + ".Minor=" + ToString(version.Minor) + "\n" +
                      structName + ".Version=" + ToString(version.Version);

    return str;
}

std::string dump_mfxHDL(const std::string structName, mfxHDL *hdl)
{
    return std::string("mfxHDL " + structName + "=" + ToString(hdl));
}

std::string dump_mfxFrameId(const std::string structName, mfxFrameId frame)
{
    std::string str = "mfxFrameId " + structName + ":\n" +
                      structName + ".TemporalId=" + ToString(frame.TemporalId) + "\n" +
                      structName + ".PriorityId=" + ToString(frame.PriorityId) + "\n" +
                      structName + ".DependencyId=" + ToString(frame.DependencyId) + "\n" +
                      structName + ".QualityId=" + ToString(frame.QualityId) + "\n" +
                      structName + ".ViewId=" + ToString(frame.ViewId);

   return str;
}

std::string dump_mfxFrameInfo(const std::string structName, mfxFrameInfo info)
{
    std::string str = "mfxFrameInfo " + structName + ": \n" +
                      structName + ".reserved=" + ToString(info.reserved[0]) + ", " + ToString(info.reserved[1]) + ", " +
                      ToString(info.reserved[2]) + ", " + ToString(info.reserved[3]) + ", " +
                      ToString(info.reserved[4]) + ", " + ToString(info.reserved[5]) + "\n" +
                      dump_mfxFrameId(structName + ".mfxFrameId", info.FrameId) +
                      structName + ".FourCC=" + ToString(info.FourCC) + "\n" +
                      structName + ".Width=" + ToString(info.Width) + "\n" +
                      structName + ".Height=" + ToString(info.Height) + "\n" +
                      structName + ".CropX=" + ToString(info.CropX) + "\n" +
                      structName + ".CropY=" + ToString(info.CropY) + "\n" +
                      structName + ".CropW=" + ToString(info.CropW) + "\n" +
                      structName + ".CropH=" + ToString(info.CropH) + "\n" +
                      structName + ".FrameRateExtN=" + ToString(info.FrameRateExtN) + "\n" +
                      structName + ".FrameRateExtD=" + ToString(info.FrameRateExtD) + "\n" +
                      structName + ".reserved3=" + ToString(info.reserved3) + "\n" +
                      structName + ".AspectRatioW=" + ToString(info.AspectRatioW) + "\n" +
                      structName + ".AspectRatioH=" + ToString(info.AspectRatioH) + "\n" +
                      structName + ".PicStruct=" + ToString(info.PicStruct) + "\n" +
                      structName + ".ChromaFormat=" + ToString(info.ChromaFormat) + "\n" +
                      structName + ".reserved2=" + ToString(info.reserved2);

    return str;
}

std::string dump_mfxBufferAllocator(const std::string structName, mfxBufferAllocator *allocator)
{
    std::string str = "mfxBufferAllocator " + structName + ":\n" +
                      dump_mfxHDL(structName + ".pthis", &allocator->pthis) +
                      structName + ".reserved=" + ToString(allocator->reserved[0]) + ", " + ToString(allocator->reserved[1]) + ", " +
                      ToString(allocator->reserved[2]) + ", " + ToString(allocator->reserved[3]);
    return str;
}

std::string dump_mfxFrameAllocator(const std::string structName, mfxFrameAllocator *allocator)
{
    std::string str = "mfxFrameAllocator " + structName + ":\n" +
                      dump_mfxHDL(structName + ".pthis=", &allocator->pthis) +
                      structName + ".reserved=" + ToString(allocator->reserved[0]) + ", " + ToString(allocator->reserved[1]) + ", " +
                      ToString(allocator->reserved[2]) + ", " + ToString(allocator->reserved[3]);
    return str;
}

std::string dump_mfxFrameAllocRequest(const std::string structName, mfxFrameAllocRequest *frameAllocRequest)
{
    std::string str = "mfxFrameAllocRequest " + structName + ":\n" +
                      structName + ".reserved=" + ToString(frameAllocRequest->reserved[0]) + ", " + ToString(frameAllocRequest->reserved[1]) + ", " +
                      ToString(frameAllocRequest->reserved[2]) + ", " + ToString(frameAllocRequest->reserved[3]) + "\n" +
                      dump_mfxFrameInfo(structName+".Info", frameAllocRequest->Info) + "\n" +
                      structName + ".Type=" +ToString(frameAllocRequest->Type) + "\n" +
                      structName + ".NumFrameMin=" +ToString(frameAllocRequest->NumFrameMin) + "\n" +
                      structName + ".NumFrameSuggested=" +ToString(frameAllocRequest->NumFrameSuggested) + "\n" +
                      structName + ".reserved2=" +ToString(frameAllocRequest->reserved2);
    return str;
}
std::string dump_mfxHandleType(const std::string structName, mfxHandleType handleType)
{
    return std::string("mfxHandleType " + structName + "=" + ToString(handleType));
}

std::string dump_mfxSyncPoint(const std::string structName, mfxSyncPoint *syncPoint)
{
    return std::string("mfxSyncPoint " + structName + "=" + ToString(syncPoint));
}

std::string dump_mfxInfoVPP(const std::string structName, mfxInfoVPP *vpp)
{
    std::string str = "mfxInfoVPP " + structName + ":\n" +
                       structName + ".reserved=" + ToString(vpp->reserved[0]) + ", " + ToString(vpp->reserved[1]) + ", " +
                       ToString(vpp->reserved[2]) + ", " + ToString(vpp->reserved[3]) + ", " +
                       ToString(vpp->reserved[4]) + ", " + ToString(vpp->reserved[5]) + ", " +
                       ToString(vpp->reserved[6]) + ", " + ToString(vpp->reserved[7]) + "\n" +
                       dump_mfxFrameInfo(structName + ".In", vpp->In) + "\n" +
                       dump_mfxFrameInfo(structName + ".Out", vpp->Out);

    return str;
}

std::string dump_mfxInfoMFX(const std::string structName, mfxInfoMFX *mfx)
{
    std::string str = "mfxInfoMFX " + structName + ":\n" +
                      structName + ".reserved=" + ToString(mfx->reserved[0]) + ", " + ToString(mfx->reserved[1]) + ", " + ToString(mfx->reserved[2]) + ", "
                              + ToString(mfx->reserved[3]) + ", " + ToString(mfx->reserved[4]) + ", " + ToString(mfx->reserved[5]) + ", "
                              + ToString(mfx->reserved[6]) + "\n" +
                      structName + ".reserved4=" + ToString(mfx->reserved4) + "\n" +
                      structName + ".BRCParamMultiplier=" + ToString(mfx->BRCParamMultiplier) + "\n" +
                      dump_mfxFrameInfo(structName + ".FrameInfo", mfx->FrameInfo) + "\n" +
                      structName + ".CodecId=" + ToString(mfx->CodecId) + "\n" +
                      structName + ".CodecProfile=" + ToString(mfx->CodecProfile) + "\n" +
                      structName + ".CodecLevel=" + ToString(mfx->CodecLevel) + "\n" +
                      structName + ".NumThread=" + ToString(mfx->NumThread) + "\n" +
                      structName + ".TargetUsage=" + ToString(mfx->TargetUsage) + "\n" +
                      structName + ".GopPicSize=" + ToString(mfx->GopPicSize) + "\n" +
                      structName + ".GopRefDist=" + ToString(mfx->GopRefDist) + "\n" +
                      structName + ".GopOptFlag=" + ToString(mfx->GopOptFlag) + "\n" +
                      structName + ".IdrInterval=" + ToString(mfx->IdrInterval) + "\n" +
                      structName + ".RateControlMethod=" + ToString(mfx->RateControlMethod) + "\n" +
                      structName + ".InitialDelayInKB=" + ToString(mfx->InitialDelayInKB) + "\n" +
                      structName + ".QPI=" + ToString(mfx->QPI) + "\n" +
                      structName + ".Accuracy=" + ToString(mfx->Accuracy) + "\n" +
                      structName + ".BufferSizeInKB=" + ToString(mfx->BufferSizeInKB) + "\n" +
                      structName + ".TargetKbps=" + ToString(mfx->TargetKbps) + "\n" +
                      structName + ".QPP=" + ToString(mfx->QPP) + "\n" +
                      structName + ".ICQQuality=" + ToString(mfx->ICQQuality) + "\n" +
                      structName + ".MaxKbps=" + ToString(mfx->MaxKbps) + "\n" +
                      structName + ".QPB=" + ToString(mfx->QPB) + "\n" +
                      structName + ".Convergence=" + ToString(mfx->Convergence) + "\n" +
                      structName + ".NumSlice=" + ToString(mfx->NumSlice) + "\n" +
                      structName + ".NumRefFrame=" + ToString(mfx->NumRefFrame) + "\n" +
                      structName + ".EncodedOrder=" + ToString(mfx->EncodedOrder) + "\n" +
                      structName + ".DecodedOrder=" + ToString(mfx->DecodedOrder) + "\n" +
                      structName + ".ExtendedPicStruct=" + ToString(mfx->ExtendedPicStruct) + "\n" +
                      structName + ".TimeStampCalc=" + ToString(mfx->TimeStampCalc) + "\n" +
                      structName + ".SliceGroupsPresent=" + ToString(mfx->SliceGroupsPresent) + "\n" +
                      structName + ".reserved2=" + ToString(mfx->reserved2[0]) + ", " + ToString(mfx->reserved2[1]) + ", " + ToString(mfx->reserved2[2]) + ", "
                               + ToString(mfx->reserved2[3]) + ", " + ToString(mfx->reserved2[4]) + ", " + ToString(mfx->reserved2[5]) + ", "
                               + ToString(mfx->reserved2[6]) + ", " + ToString(mfx->reserved2[7]) + ", " + ToString(mfx->reserved2[8]) + "\n" +
                      structName + ".JPEGChromaFormat=" + ToString(mfx->JPEGChromaFormat) + "\n" +
                      structName + ".Rotation=" + ToString(mfx->Rotation) + "\n" +
                      structName + ".JPEGColorFormat=" + ToString(mfx->JPEGColorFormat) + "\n" +
                      structName + ".InterleavedDec=" + ToString(mfx->InterleavedDec) + "\n" +
                      structName + ".reserved3=" + ToString(mfx->reserved3[0]) + ", " + ToString(mfx->reserved3[1]) + ", " + ToString(mfx->reserved3[2]) + ", "
                               + ToString(mfx->reserved3[3]) + ", " + ToString(mfx->reserved3[4]) + ", " + ToString(mfx->reserved3[5]) + ", "
                               + ToString(mfx->reserved3[6]) + ", " + ToString(mfx->reserved3[7]) + ", " + ToString(mfx->reserved3[8]) + "\n" +
                      structName + ".Interleaved=" + ToString(mfx->Interleaved) + "\n" +
                      structName + ".Quality=" + ToString(mfx->Quality) + "\n" +
                      structName + ".RestartInterval=" + ToString(mfx->RestartInterval) + "\n" +
                      structName + ".reserved5=" + ToString(mfx->reserved5[0]) + " " + ToString(mfx->reserved5[1]) + ", " + ToString(mfx->reserved5[2]) + ", "
                               + ToString(mfx->reserved5[3]) + ", " + ToString(mfx->reserved5[4]) + ", " + ToString(mfx->reserved5[5]) + ", "
                               + ToString(mfx->reserved5[6]) + ", " + ToString(mfx->reserved5[7]) + ", " + ToString(mfx->reserved5[8]) + ", "
                               + ToString(mfx->reserved5[9]);
    return str;
}

std::string dump_mfxVideoParam(const std::string structName, mfxVideoParam *videoParam)
{
    std::string str = "mfxVideoParam " + structName + ":\n" +
                      structName + ".reserved=" + ToString(videoParam->reserved[0]) + ", " + ToString(videoParam->reserved[1]) + ", " + ToString(videoParam->reserved[2]) + "\n" +
                      structName + ".reserved3=" + ToString(videoParam->reserved3) + "\n" +
                      structName + ".AsyncDepth=" + ToString(videoParam->AsyncDepth) + "\n" +
                      dump_mfxInfoMFX(structName + ".mfx", &videoParam->mfx) + "\n" +
                      dump_mfxInfoVPP(structName + ".vpp", &videoParam->vpp) + "\n" +
                      structName + ".Protected=" + ToString(videoParam->Protected) + "\n" +
                      structName + ".IOPattern=" + ToString(videoParam->IOPattern) + "\n" +
                      structName + ".ExtParam=" + ToString(videoParam->ExtParam) + "\n" +
                      structName + ".NumExtParam=" + ToString(videoParam->NumExtParam) + "\n" +
                      structName + ".reserved2=" + ToString(videoParam->reserved2);

    return str;
}

std::string dump_mfxEncodeStat(const std::string structName, mfxEncodeStat *encodeStat)
{
    std::string str = "mfxEncodeStat " + structName + ":\n" +
                      structName + ".NumBit=" + ToString(encodeStat->NumBit) + "\n" +
                      structName + ".NumCachedFrame=" + ToString(encodeStat->NumCachedFrame) + "\n" +
                      structName + ".NumFrame=" + ToString(encodeStat->NumFrame) + "\n" +
                      structName + ".reserved=" + ToString(encodeStat->reserved[0]) + ", " + ToString(encodeStat->reserved[1]) + ", " + ToString(encodeStat->reserved[2]) + ", "
                              + ToString(encodeStat->reserved[3]) + ", " + ToString(encodeStat->reserved[4]) + ", " + ToString(encodeStat->reserved[5]) + ", "
                              + ToString(encodeStat->reserved[6]) + ", " + ToString(encodeStat->reserved[7]) + ", " + ToString(encodeStat->reserved[8]) + ", "
                              + ToString(encodeStat->reserved[9]) + ", " + ToString(encodeStat->reserved[10]) + ", " + ToString(encodeStat->reserved[11]) + ", "
                              + ToString(encodeStat->reserved[12]) + ", " + ToString(encodeStat->reserved[13]) + ", " + ToString(encodeStat->reserved[14]) + ", "
                              + ToString(encodeStat->reserved[15]);
    return str;
}

std::string dump_mfxExtBuffer(const std::string structName, mfxExtBuffer *extBuffer)
{
    std::string str = "mfxExtBuffer " + structName + ":\n" +
                      structName + ".BufferId=" + ToString(extBuffer->BufferId) + "\n" +
                      structName + ".BufferSz=" + ToString(extBuffer->BufferSz);

    return str;
}

std::string dump_mfxEncodeCtrl(const std::string structName, mfxEncodeCtrl *EncodeCtrl)
{
    std::string str = "mfxEncodeCtrl " + structName + ": \n" +
                      dump_mfxExtBuffer(structName + ".Header=", &EncodeCtrl->Header) + "\n" +
                      structName + ".reserved=" + ToString(EncodeCtrl->reserved[0]) + ", " + ToString(EncodeCtrl->reserved[1]) + ", " + ToString(EncodeCtrl->reserved[2]) + ", "
                                            + ToString(EncodeCtrl->reserved[3]) + ", " + ToString(EncodeCtrl->reserved[4]) + "\n" +
                      structName + ".SkipFrame=" + ToString(EncodeCtrl->SkipFrame) + "\n" +
                      structName + ".QP=" + ToString(EncodeCtrl->QP) + "\n" +
                      structName + ".FrameType=" + ToString(EncodeCtrl->FrameType) + "\n" +
                      structName + ".NumExtParam=" + ToString(EncodeCtrl->NumExtParam) + "\n" +
                      structName + ".NumPayload=" + ToString(EncodeCtrl->NumPayload) + "\n" +
                      structName + ".reserved2=" + ToString(EncodeCtrl->reserved2) + "\n" +
                      structName + ".ExtParam=" + ToString(EncodeCtrl->ExtParam) + "\n" +
                      structName + ".Payload=" + ToString(EncodeCtrl->Payload);
    return str;
}

std::string dump_mfxFrameData(const std::string structName, mfxFrameData *frameData)
{
    std::string str = "mfxFrameData " + structName + ":\n" +
                      structName + ".reserved=" + ToString(frameData->reserved[0]) + ", " + ToString(frameData->reserved[1]) + ", " + ToString(frameData->reserved[2]) + ", "
                                  + ToString(frameData->reserved[3]) + ", " + ToString(frameData->reserved[4]) + ", " + ToString(frameData->reserved[5]) + ", "
                                  + ToString(frameData->reserved[6]) + "\n" +
                      structName + ".reserved1=" + ToString(frameData->reserved) + "\n" +
                      structName + ".PitchHigh=" + ToString(frameData->PitchHigh) + "\n" +
                      structName + ".TimeStamp=" + ToString(frameData->TimeStamp) + "\n" +
                      structName + ".FrameOrder=" + ToString(frameData->FrameOrder) + "\n" +
                      structName + ".Locked=" + ToString(frameData->Locked) + "\n" +
                      structName + ".Pitch=" + ToString(frameData->Pitch) + "\n" +
                      structName + ".PitchLow=" + ToString(frameData->PitchLow) + "\n" +
                      structName + ".Y=" + ToString(frameData->Y) + "\n" +
                      structName + ".R=" + ToString(frameData->R) + "\n" +
                      structName + ".UV=" + ToString(frameData->UV) + "\n" +
                      structName + ".VU=" + ToString(frameData->VU) + "\n" +
                      structName + ".CbCr=" + ToString(frameData->CbCr) + "\n" +
                      structName + ".CrCb=" + ToString(frameData->CrCb) + "\n" +
                      structName + ".Cb=" + ToString(frameData->Cb) + "\n" +
                      structName + ".U=" + ToString(frameData->U) + "\n" +
                      structName + ".G=" + ToString(frameData->G) + "\n" +
                      structName + ".Cr=" + ToString(frameData->Cr) + "\n" +
                      structName + ".V=" + ToString(frameData->V) + "\n" +
                      structName + ".B=" + ToString(frameData->B) + "\n" +
                      structName + ".A=" + ToString(frameData->A) + "\n" +
                      structName + ".MemId=" + ToString(frameData->MemId) + "\n" +
                      structName + ".Corrupted=" + ToString(frameData->Corrupted) + "\n" +
                      structName + ".DataFlag=" + ToString(frameData->DataFlag);

    return str;
}

std::string dump_mfxFrameSurface1(const std::string structName, mfxFrameSurface1 *frameSurface1)
{
    std::string str = "mfxFrameSurface1 " + structName + ":\n" +
                      dump_mfxFrameData(structName + ".Data", &frameSurface1->Data) + "\n" +
                      dump_mfxFrameInfo(structName + ".Info", frameSurface1->Info) + "\n" +
                      structName + ".reserved=" + ToString(frameSurface1->reserved[0]) + ", " + ToString(frameSurface1->reserved[1]) + ", "
                                            + ToString(frameSurface1->reserved[2]) + ", " + ToString(frameSurface1->reserved[3]);
    return str;
}

std::string dump_mfxBitstream(const std::string structName, mfxBitstream *bitstream)
{
    std::string str = "mfxBitstream " + structName + ":\n" +
                      structName + ".EncryptedData=" + ToString(bitstream->EncryptedData) + "\n";
    if(bitstream->ExtParam){
        str = str + dump_mfxExtBuffer(structName + ".ExtParam=", (*bitstream->ExtParam)) + "\n";
    }
    str = structName + ".NumExtParam=" + ToString(bitstream->NumExtParam) + "\n" +
          structName + ".reserved=" + ToString(bitstream->reserved[0]) + ", " + ToString(bitstream->reserved[1]) + ", " + ToString(bitstream->reserved[2]) + ", "
                                            + ToString(bitstream->reserved[3]) + ", " + ToString(bitstream->reserved[4]) + ", " + ToString(bitstream->reserved[5]) + "\n" +
          structName + ".DecodeTimeStamp=" + ToString(bitstream->DecodeTimeStamp) + "\n" +
          structName + ".TimeStamp=" + ToString(bitstream->TimeStamp) + "\n" +
          structName + ".Data=" + ToString(bitstream->Data) + "\n" +
          structName + ".DataOffset=" + ToString(bitstream->DataOffset) + "\n" +
          structName + ".DataLength=" + ToString(bitstream->DataLength) + "\n" +
          structName + ".MaxLength=" + ToString(bitstream->MaxLength) + "\n" +
          structName + ".PicStruct=" + ToString(bitstream->PicStruct) + "\n" +
          structName + ".FrameType=" + ToString(bitstream->FrameType) + "\n" +
          structName + ".DataFlag=" + ToString(bitstream->DataFlag) + "\n" +
          structName + ".reserved2=" + ToString(bitstream->reserved2);

    return str;
}

std::string dump_mfxDecodeStat(const std::string structName, mfxDecodeStat *decodeStat)
{
    std::string str = "mfxDecodeStat " + structName + ":\n" +
                      structName + ".reserved=" + ToString(decodeStat->reserved[0]) + ", " + ToString(decodeStat->reserved[1]) + ", " + ToString(decodeStat->reserved[2]) + ", "
                                            + ToString(decodeStat->reserved[3]) + ", " + ToString(decodeStat->reserved[4]) + ", " + ToString(decodeStat->reserved[5]) + ", "
                                            + ToString(decodeStat->reserved[6]) + ", " + ToString(decodeStat->reserved[7]) + ", " + ToString(decodeStat->reserved[8]) + ", "
                                            + ToString(decodeStat->reserved[9]) + ", " + ToString(decodeStat->reserved[10]) + ", " + ToString(decodeStat->reserved[11]) + ", "
                                            + ToString(decodeStat->reserved[12]) + ", " + ToString(decodeStat->reserved[13]) + ", " + ToString(decodeStat->reserved[14]) + ", "
                                            + ToString(decodeStat->reserved[15]) + "\n" +
                      structName + ".NumFrame=" + ToString(decodeStat->NumFrame) + "\n" +
                      structName + ".NumSkippedFrame=" + ToString(decodeStat->NumSkippedFrame) + "\n" +
                      structName + ".NumError=" + ToString(decodeStat->NumError) + "\n" +
                      structName + ".NumCachedFrame=" + ToString(decodeStat->NumCachedFrame);

    return str;
}

std::string dump_mfxSkipMode(const std::string structName, mfxSkipMode skipMode)
{
    return std::string("mfxSkipMode " + structName + "=" + ToString(skipMode));
}

std::string dump_mfxPayload(const std::string structName, mfxPayload *payload)
{
    std::string str = "mfxPayload " + structName + ":\n" +
                      structName + ".reserved=" + ToString(payload->reserved[0]) + ", " + ToString(payload->reserved[1]) + ", "
                                            + ToString(payload->reserved[2]) + ", " + ToString(payload->reserved[3]) + "\n" +
                      structName + ".Data=" + ToString(payload->Data) + "\n" +
                      structName + ".NumBit=" + ToString(payload->NumBit) + "\n" +
                      structName + ".Type=" + ToString(payload->Type) + "\n" +
                      structName + ".BufSize=" + ToString(payload->BufSize);
}

std::string dump_mfxVPPStat(const std::string structName, mfxVPPStat *vppStat)
{
    std::string str = "mfxVPPStat " + structName + ":\n" +
                      structName + ".reserved=" + ToString(vppStat->reserved[0]) + ", " + ToString(vppStat->reserved[1]) + ", " + ToString(vppStat->reserved[2]) + ", "
                                            + ToString(vppStat->reserved[3]) + ", " + ToString(vppStat->reserved[4]) + ", " + ToString(vppStat->reserved[5]) + ", "
                                            + ToString(vppStat->reserved[6]) + ", " + ToString(vppStat->reserved[7]) + ", " + ToString(vppStat->reserved[8]) + ", "
                                            + ToString(vppStat->reserved[9]) + ", " + ToString(vppStat->reserved[10]) + ", " + ToString(vppStat->reserved[11]) + ", "
                                            + ToString(vppStat->reserved[12]) + ", " + ToString(vppStat->reserved[13]) + ", " + ToString(vppStat->reserved[14]) + ", "
                                            + ToString(vppStat->reserved[15]) + "\n" +
                      structName + ".NumFrame=" + ToString(vppStat->NumFrame) + "\n" +
                      structName + ".NumCachedFrame=" + ToString(vppStat->NumCachedFrame);

    return str;
}

std::string dump_mfxExtVppAuxData(const std::string structName, mfxExtVppAuxData *extVppAuxData)
{
    std::string str = "mfxExtVppAuxData " + structName + ":\n" +
                      dump_mfxExtBuffer(structName + ".Header", &extVppAuxData->Header) + "\n" +
                      structName + ".SpatialComplexity=" + ToString(extVppAuxData->SpatialComplexity) + "\n" +
                      structName + ".TemporalComplexity=" + ToString(extVppAuxData->TemporalComplexity) + "\n" +
                      structName + ".PicStruct=" + ToString(extVppAuxData->PicStruct) + "\n" +
                      structName + ".reserved=" + ToString(extVppAuxData->reserved[0]) + ToString(extVppAuxData->reserved[1]) + ToString(extVppAuxData->reserved[2]) + "\n" +
                      structName + ".SceneChangeRate=" + ToString(extVppAuxData->SceneChangeRate) + "\n" +
                      structName + ".RepeatedFrame=" + ToString(extVppAuxData->RepeatedFrame);
}

std::string dump_mfxPriority(const std::string structName, mfxPriority priority)
{
    return std::string("mfxPriority " + structName + "=" + ToString(priority));
}
