#include "dump.h"

void dump_mfxStatus(const std::string structName, mfxStatus status)
{
    Log::WriteLog("mfxStatus " + structName + "=" + ToString(status) + "\n");
}

void dump_mfxSession(const std::string structName, mfxSession session)
{
    Log::WriteLog("mfxSession " + structName + "=" + ToString(session) + "\n");
}

void dump_mfxIMPL(const std::string structName, mfxIMPL *impl)
{
    Log::WriteLog("mfxIMPL " + structName + "=" + ToString(impl) + "\n");
}

void dump_mfxVersion(const std::string structName, mfxVersion version)
{
    Log::WriteLog("mfxVersion " + structName + ":");
    Log::WriteLog(structName+".Major=" + ToString(version.Major));
    Log::WriteLog(structName + ".Minor=" + ToString(version.Minor));
    Log::WriteLog(structName + ".Version=" + ToString(version.Version) + "\n");
}

void dump_mfxHDL(const std::string structName, mfxHDL *hdl)
{
    Log::WriteLog("mfxHDL " + structName + "=" + ToString(hdl) + "\n");
}

void dump_mfxFrameId(const std::string structName, mfxFrameId frame)
{
    Log::WriteLog("mfxFrameId " + structName + ":");
    Log::WriteLog(structName + ".TemporalId=" + ToString(frame.TemporalId));
    Log::WriteLog(structName + ".PriorityId=" + ToString(frame.PriorityId));
    Log::WriteLog(structName + ".DependencyId=" + ToString(frame.DependencyId));
    Log::WriteLog(structName + ".QualityId=" + ToString(frame.QualityId));
    Log::WriteLog(structName + ".ViewId=" + ToString(frame.ViewId) + "\n");
}

void dump_mfxFrameInfo(const std::string structName, mfxFrameInfo info)
{
    Log::WriteLog("mfxFrameInfo " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(info.reserved[0]) + ", " + ToString(info.reserved[1]) + ", " +
                                ToString(info.reserved[2]) + ", " + ToString(info.reserved[3]) + ", " +
                                ToString(info.reserved[4]) + ", " + ToString(info.reserved[5]));
    Log::WriteLog("\n");
    dump_mfxFrameId(structName + ".mfxFrameId", info.FrameId);

    Log::WriteLog(structName + ".FourCC=" + ToString(info.FourCC));
    Log::WriteLog(structName + ".Width=" + ToString(info.Width));
    Log::WriteLog(structName + ".Height=" + ToString(info.Height));
    Log::WriteLog(structName + ".CropX=" + ToString(info.CropX));
    Log::WriteLog(structName + ".CropY=" + ToString(info.CropY));
    Log::WriteLog(structName + ".CropW=" + ToString(info.CropW));
    Log::WriteLog(structName + ".CropH=" + ToString(info.CropH));
    Log::WriteLog(structName + ".FrameRateExtN=" + ToString(info.FrameRateExtN));
    Log::WriteLog(structName + ".FrameRateExtD=" + ToString(info.FrameRateExtD));
    Log::WriteLog(structName + ".reserved3=" + ToString(info.reserved3));
    Log::WriteLog(structName + ".AspectRatioW=" + ToString(info.AspectRatioW));
    Log::WriteLog(structName + ".AspectRatioH=" + ToString(info.AspectRatioH));
    Log::WriteLog(structName + ".PicStruct=" + ToString(info.PicStruct));
    Log::WriteLog(structName + ".ChromaFormat=" + ToString(info.ChromaFormat));
    Log::WriteLog(structName + ".reserved2=" + ToString(info.reserved2) + "\n");
}

void dump_mfxBufferAllocator(const std::string structName, mfxBufferAllocator *allocator)
{
    Log::WriteLog("mfxBufferAllocator " + structName + ":");
    Log::WriteLog("\n");
    dump_mfxHDL(structName + ".pthis", &allocator->pthis);
    Log::WriteLog(structName + ".reserved=" + ToString(allocator->reserved[0]) + ", " + ToString(allocator->reserved[1]) + ", " +
                                ToString(allocator->reserved[2]) + ", " + ToString(allocator->reserved[3]) + "\n");
}

void dump_mfxFrameAllocator(const std::string structName, mfxFrameAllocator *allocator)
{
    Log::WriteLog("mfxFrameAllocator " + structName + ":");
    Log::WriteLog("\n");
    dump_mfxHDL(structName + ".pthis=", &allocator->pthis);
    Log::WriteLog(structName + ".reserved=" + ToString(allocator->reserved[0]) + ", " + ToString(allocator->reserved[1]) + ", " +
                                ToString(allocator->reserved[2]) + ", " + ToString(allocator->reserved[3]) + "\n");
}

void dump_mfxFrameAllocRequest(const std::string structName, mfxFrameAllocRequest *frameAllocRequest)
{
    Log::WriteLog("mfxFrameAllocRequest " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(frameAllocRequest->reserved[0]) + ", " + ToString(frameAllocRequest->reserved[1]) + ", " +
                                ToString(frameAllocRequest->reserved[2]) + ", " + ToString(frameAllocRequest->reserved[3]));
    Log::WriteLog("\n");
    dump_mfxFrameInfo(structName+".Info", frameAllocRequest->Info);
    Log::WriteLog(structName + ".Type=" +ToString(frameAllocRequest->Type));
    Log::WriteLog(structName + ".NumFrameMin=" +ToString(frameAllocRequest->NumFrameMin));
    Log::WriteLog(structName + ".NumFrameSuggested=" +ToString(frameAllocRequest->NumFrameSuggested));
    Log::WriteLog(structName + ".reserved2=" +ToString(frameAllocRequest->reserved2) + "\n");
}
void dump_mfxHandleType(const std::string structName, mfxHandleType handleType)
{
    Log::WriteLog("mfxHandleType " + structName + "=" + ToString(handleType) + "\n");
}

void dump_mfxSyncPoint(const std::string structName, mfxSyncPoint *syncPoint)
{
    Log::WriteLog("mfxSyncPoint " + structName + "=" + ToString(syncPoint) + "\n");
}

void dump_mfxInfoVPP(const std::string structName, mfxInfoVPP *vpp)
{
    Log::WriteLog("mfxInfoVPP " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(vpp->reserved[0]) + ", " + ToString(vpp->reserved[1]) + ", " +
                                              ToString(vpp->reserved[2]) + ", " + ToString(vpp->reserved[3]) + ", " +
                                              ToString(vpp->reserved[4]) + ", " + ToString(vpp->reserved[5]) + ", " +
                                              ToString(vpp->reserved[6]) + ", " + ToString(vpp->reserved[7]));
    Log::WriteLog("\n");
    dump_mfxFrameInfo(structName + ".In", vpp->In);
    Log::WriteLog("\n");
    dump_mfxFrameInfo(structName + ".Out", vpp->Out);
}

void dump_mfxInfoMFX(const std::string structName, mfxInfoMFX *mfx)
{
    Log::WriteLog("mfxInfoMFX " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(mfx->reserved[0]) + ", " + ToString(mfx->reserved[1]) + ", " + ToString(mfx->reserved[2]) + ", "
                              + ToString(mfx->reserved[3]) + ", " + ToString(mfx->reserved[4]) + ", " + ToString(mfx->reserved[5]) + ", "
                              + ToString(mfx->reserved[6]));

    Log::WriteLog(structName + ".reserved4=" + ToString(mfx->reserved4));
    Log::WriteLog(structName + ".BRCParamMultiplier=" + ToString(mfx->BRCParamMultiplier));
    Log::WriteLog(structName + ".reserved4=" + ToString(mfx->reserved4));
    Log::WriteLog("\n");
    dump_mfxFrameInfo(structName + ".FrameInfo", mfx->FrameInfo);
    Log::WriteLog(structName + ".CodecId=" + ToString(mfx->CodecId));
    Log::WriteLog(structName + ".CodecProfile=" + ToString(mfx->CodecProfile));
    Log::WriteLog(structName + ".CodecLevel=" + ToString(mfx->CodecLevel));
    Log::WriteLog(structName + ".NumThread=" + ToString(mfx->NumThread));

    Log::WriteLog(structName + ".TargetUsage=" + ToString(mfx->TargetUsage));
    Log::WriteLog(structName + ".GopPicSize=" + ToString(mfx->GopPicSize));
    Log::WriteLog(structName + ".GopRefDist=" + ToString(mfx->GopRefDist));
    Log::WriteLog(structName + ".GopOptFlag=" + ToString(mfx->GopOptFlag));
    Log::WriteLog(structName + ".IdrInterval=" + ToString(mfx->IdrInterval));

    Log::WriteLog(structName + ".RateControlMethod=" + ToString(mfx->RateControlMethod));
    Log::WriteLog(structName + ".InitialDelayInKB=" + ToString(mfx->InitialDelayInKB));
    Log::WriteLog(structName + ".QPI=" + ToString(mfx->QPI));
    Log::WriteLog(structName + ".Accuracy=" + ToString(mfx->Accuracy));

    Log::WriteLog(structName + ".BufferSizeInKB=" + ToString(mfx->BufferSizeInKB));
    Log::WriteLog(structName + ".TargetKbps=" + ToString(mfx->TargetKbps));
    Log::WriteLog(structName + ".QPP=" + ToString(mfx->QPP));
    Log::WriteLog(structName + ".ICQQuality=" + ToString(mfx->ICQQuality));

    Log::WriteLog(structName + ".MaxKbps=" + ToString(mfx->MaxKbps));
    Log::WriteLog(structName + ".QPB=" + ToString(mfx->QPB));
    Log::WriteLog(structName + ".Convergence=" + ToString(mfx->Convergence));

    Log::WriteLog(structName + ".NumSlice=" + ToString(mfx->NumSlice));
    Log::WriteLog(structName + ".NumRefFrame=" + ToString(mfx->NumRefFrame));
    Log::WriteLog(structName + ".EncodedOrder=" + ToString(mfx->EncodedOrder));

    Log::WriteLog(structName + ".DecodedOrder=" + ToString(mfx->DecodedOrder));
    Log::WriteLog(structName + ".ExtendedPicStruct=" + ToString(mfx->ExtendedPicStruct));
    Log::WriteLog(structName + ".TimeStampCalc=" + ToString(mfx->TimeStampCalc));
    Log::WriteLog(structName + ".SliceGroupsPresent=" + ToString(mfx->SliceGroupsPresent));
    Log::WriteLog(structName + ".reserved2=" + ToString(mfx->reserved2[0]) + ", " + ToString(mfx->reserved2[1]) + ", " + ToString(mfx->reserved2[2]) + ", "
                               + ToString(mfx->reserved2[3]) + ", " + ToString(mfx->reserved2[4]) + ", " + ToString(mfx->reserved2[5]) + ", "
                               + ToString(mfx->reserved2[6]) + ", " + ToString(mfx->reserved2[7]) + ", " + ToString(mfx->reserved2[8]));

    Log::WriteLog(structName + ".JPEGChromaFormat=" + ToString(mfx->JPEGChromaFormat));
    Log::WriteLog(structName + ".Rotation=" + ToString(mfx->Rotation));
    Log::WriteLog(structName + ".JPEGColorFormat=" + ToString(mfx->JPEGColorFormat));
    Log::WriteLog(structName + ".InterleavedDec=" + ToString(mfx->InterleavedDec));
    Log::WriteLog(structName + ".reserved3=" + ToString(mfx->reserved3[0]) + ", " + ToString(mfx->reserved3[1]) + ", " + ToString(mfx->reserved3[2]) + ", "
                               + ToString(mfx->reserved3[3]) + ", " + ToString(mfx->reserved3[4]) + ", " + ToString(mfx->reserved3[5]) + ", "
                               + ToString(mfx->reserved3[6]) + ", " + ToString(mfx->reserved3[7]) + ", " + ToString(mfx->reserved3[8]));

    Log::WriteLog(structName + ".Interleaved=" + ToString(mfx->Interleaved));
    Log::WriteLog(structName + ".Quality=" + ToString(mfx->Quality));
    Log::WriteLog(structName + ".RestartInterval=" + ToString(mfx->RestartInterval));
    Log::WriteLog(structName + ".reserved5=" + ToString(mfx->reserved5[0]) + " " + ToString(mfx->reserved5[1]) + ", " + ToString(mfx->reserved5[2]) + ", "
                               + ToString(mfx->reserved5[3]) + ", " + ToString(mfx->reserved5[4]) + ", " + ToString(mfx->reserved5[5]) + ", "
                               + ToString(mfx->reserved5[6]) + ", " + ToString(mfx->reserved5[7]) + ", " + ToString(mfx->reserved5[8]) + ", "
                               + ToString(mfx->reserved5[9]) + "\n");
}

void dump_mfxVideoParam(const std::string structName, mfxVideoParam *videoParam)
{
    Log::WriteLog("mfxVideoParam " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(videoParam->reserved[0]) + ", " + ToString(videoParam->reserved[1]) + ", " + ToString(videoParam->reserved[2]));
    Log::WriteLog(structName + ".reserved3=" + ToString(videoParam->reserved3));
    Log::WriteLog(structName + ".AsyncDepth=" + ToString(videoParam->AsyncDepth));
    Log::WriteLog("\n");
    dump_mfxInfoMFX(structName + ".mfx", &videoParam->mfx);
    Log::WriteLog("\n");
    dump_mfxInfoVPP(structName + ".vpp", &videoParam->vpp);
    Log::WriteLog(structName + ".Protected=" + ToString(videoParam->Protected));
    Log::WriteLog(structName + ".IOPattern=" + ToString(videoParam->IOPattern));
    Log::WriteLog(structName + ".ExtParam=" + ToString(videoParam->ExtParam));
    Log::WriteLog(structName + ".NumExtParam=" + ToString(videoParam->NumExtParam));
    Log::WriteLog(structName + ".reserved2=" + ToString(videoParam->reserved2) + "\n");
}

void dump_mfxEncodeStat(const std::string structName, mfxEncodeStat *encodeStat)
{
    Log::WriteLog("mfxEncodeStat " + structName + ":");
    Log::WriteLog(structName + ".NumBit=" + ToString(encodeStat->NumBit));
    Log::WriteLog(structName + ".NumCachedFrame=" + ToString(encodeStat->NumCachedFrame));
    Log::WriteLog(structName + ".NumFrame=" + ToString(encodeStat->NumFrame));
    Log::WriteLog(structName + ".reserved=" + ToString(encodeStat->reserved[0]) + ", " + ToString(encodeStat->reserved[1]) + ", " + ToString(encodeStat->reserved[2]) + ", "
                              + ToString(encodeStat->reserved[3]) + ", " + ToString(encodeStat->reserved[4]) + ", " + ToString(encodeStat->reserved[5]) + ", "
                              + ToString(encodeStat->reserved[6]) + ", " + ToString(encodeStat->reserved[7]) + ", " + ToString(encodeStat->reserved[8]) + ", "
                              + ToString(encodeStat->reserved[9]) + ", " + ToString(encodeStat->reserved[10]) + ", " + ToString(encodeStat->reserved[11]) + ", "
                              + ToString(encodeStat->reserved[12]) + ", " + ToString(encodeStat->reserved[13]) + ", " + ToString(encodeStat->reserved[14]) + ", "
                              + ToString(encodeStat->reserved[15]) + "\n");
}

void dump_mfxExtBuffer(const std::string structName, mfxExtBuffer *extBuffer)
{
    Log::WriteLog("mfxExtBuffer " + structName + ":");
    Log::WriteLog(structName + ".BufferId=" + ToString(extBuffer->BufferId));
    Log::WriteLog(structName + ".BufferSz=" + ToString(extBuffer->BufferSz) + "\n");
}

void dump_mfxEncodeCtrl(const std::string structName, mfxEncodeCtrl *EncodeCtrl)
{
    Log::WriteLog("mfxEncodeCtrl " + structName + ":");

    Log::WriteLog("\n");
    dump_mfxExtBuffer(structName + ".Header=", &EncodeCtrl->Header);
    Log::WriteLog(structName + ".reserved=" + ToString(EncodeCtrl->reserved[0]) + ", " + ToString(EncodeCtrl->reserved[1]) + ", " + ToString(EncodeCtrl->reserved[2]) + ", "
                                            + ToString(EncodeCtrl->reserved[3]) + ", " + ToString(EncodeCtrl->reserved[4]));
    Log::WriteLog(structName + ".SkipFrame=" + ToString(EncodeCtrl->SkipFrame));
    Log::WriteLog(structName + ".QP=" + ToString(EncodeCtrl->QP));
    Log::WriteLog(structName + ".FrameType=" + ToString(EncodeCtrl->FrameType));
    Log::WriteLog(structName + ".NumExtParam=" + ToString(EncodeCtrl->NumExtParam));
    Log::WriteLog(structName + ".NumPayload=" + ToString(EncodeCtrl->NumPayload));
    Log::WriteLog(structName + ".reserved2=" + ToString(EncodeCtrl->reserved2));

    Log::WriteLog(structName + ".ExtParam=" + ToString(EncodeCtrl->ExtParam));
    Log::WriteLog(structName + ".Payload=" + ToString(EncodeCtrl->Payload) + "\n");
}

void dump_mfxFrameData(const std::string structName, mfxFrameData *frameData)
{
    Log::WriteLog("mfxFrameData " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(frameData->reserved[0]) + ", " + ToString(frameData->reserved[1]) + ", " + ToString(frameData->reserved[2]) + ", "
                                  + ToString(frameData->reserved[3]) + ", " + ToString(frameData->reserved[4]) + ", " + ToString(frameData->reserved[5]) + ", "
                                  + ToString(frameData->reserved[6]));


    Log::WriteLog(structName + ".reserved1=" + ToString(frameData->reserved));
    Log::WriteLog(structName + ".PitchHigh=" + ToString(frameData->PitchHigh));
    Log::WriteLog(structName + ".TimeStamp=" + ToString(frameData->TimeStamp));
    Log::WriteLog(structName + ".FrameOrder=" + ToString(frameData->FrameOrder));
    Log::WriteLog(structName + ".Locked=" + ToString(frameData->Locked));
    Log::WriteLog(structName + ".Pitch=" + ToString(frameData->Pitch));
    Log::WriteLog(structName + ".PitchLow=" + ToString(frameData->PitchLow));
    Log::WriteLog(structName + ".Y=" + ToString(frameData->Y)); // TODO: or Y or R
    Log::WriteLog(structName + ".R=" + ToString(frameData->R));
    Log::WriteLog(structName + ".UV=" + ToString(frameData->UV));
    Log::WriteLog(structName + ".VU=" + ToString(frameData->VU));
    Log::WriteLog(structName + ".CbCr=" + ToString(frameData->CbCr));
    Log::WriteLog(structName + ".CrCb=" + ToString(frameData->CrCb));
    Log::WriteLog(structName + ".Cb=" + ToString(frameData->Cb));
    Log::WriteLog(structName + ".U=" + ToString(frameData->U));
    Log::WriteLog(structName + ".G=" + ToString(frameData->G));
    Log::WriteLog(structName + ".Cr=" + ToString(frameData->Cr));
    Log::WriteLog(structName + ".V=" + ToString(frameData->V));
    Log::WriteLog(structName + ".B=" + ToString(frameData->B));
    Log::WriteLog(structName + ".A=" + ToString(frameData->A));
    Log::WriteLog(structName + ".MemId=" + ToString(frameData->MemId));
    Log::WriteLog(structName + ".Corrupted=" + ToString(frameData->Corrupted));
    Log::WriteLog(structName + ".DataFlag=" + ToString(frameData->DataFlag) + "\n");
}

void dump_mfxFrameSurface1(const std::string structName, mfxFrameSurface1 *frameSurface1)
{
    Log::WriteLog("mfxFrameSurface1 " + structName + ":");
    Log::WriteLog("\n");
    dump_mfxFrameData(structName + ".Data", &frameSurface1->Data);
    Log::WriteLog("\n");
    dump_mfxFrameInfo(structName + ".Info", frameSurface1->Info);
    Log::WriteLog(structName + ".reserved=" + ToString(frameSurface1->reserved[0]) + ", " + ToString(frameSurface1->reserved[1]) + ", "
                                            + ToString(frameSurface1->reserved[2]) + ", " + ToString(frameSurface1->reserved[3]) + "\n");
}

void dump_mfxBitstream(const std::string structName, mfxBitstream *bitstream)
{
    Log::WriteLog("mfxBitstream " + structName + ":");

    Log::WriteLog(structName + ".EncryptedData=" + ToString(bitstream->EncryptedData));

    if(bitstream->ExtParam){
         Log::WriteLog("\n");
        dump_mfxExtBuffer(structName + ".ExtParam=", (*bitstream->ExtParam));
    }
    Log::WriteLog(structName + ".NumExtParam=" + ToString(bitstream->NumExtParam));
    Log::WriteLog(structName + ".reserved=" + ToString(bitstream->reserved[0]) + ", " + ToString(bitstream->reserved[1]) + ", " + ToString(bitstream->reserved[2]) + ", "
                                            + ToString(bitstream->reserved[3]) + ", " + ToString(bitstream->reserved[4]) + ", " + ToString(bitstream->reserved[5]));
    Log::WriteLog(structName + ".DecodeTimeStamp=" + ToString(bitstream->DecodeTimeStamp));
    Log::WriteLog(structName + ".TimeStamp=" + ToString(bitstream->TimeStamp));
    Log::WriteLog(structName + ".Data=" + ToString(bitstream->Data));
    Log::WriteLog(structName + ".DataOffset=" + ToString(bitstream->DataOffset));
    Log::WriteLog(structName + ".DataLength=" + ToString(bitstream->DataLength));
    Log::WriteLog(structName + ".MaxLength=" + ToString(bitstream->MaxLength));
    Log::WriteLog(structName + ".PicStruct=" + ToString(bitstream->PicStruct));
    Log::WriteLog(structName + ".FrameType=" + ToString(bitstream->FrameType));
    Log::WriteLog(structName + ".DataFlag=" + ToString(bitstream->DataFlag));
    Log::WriteLog(structName + ".reserved2=" + ToString(bitstream->reserved2) + "\n");
}

void dump_mfxDecodeStat(const std::string structName, mfxDecodeStat *decodeStat)
{
    Log::WriteLog("mfxDecodeStat " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(decodeStat->reserved[0]) + ", " + ToString(decodeStat->reserved[1]) + ", " + ToString(decodeStat->reserved[2]) + ", "
                                            + ToString(decodeStat->reserved[3]) + ", " + ToString(decodeStat->reserved[4]) + ", " + ToString(decodeStat->reserved[5]) + ", "
                                            + ToString(decodeStat->reserved[6]) + ", " + ToString(decodeStat->reserved[7]) + ", " + ToString(decodeStat->reserved[8]) + ", "
                                            + ToString(decodeStat->reserved[9]) + ", " + ToString(decodeStat->reserved[10]) + ", " + ToString(decodeStat->reserved[11]) + ", "
                                            + ToString(decodeStat->reserved[12]) + ", " + ToString(decodeStat->reserved[13]) + ", " + ToString(decodeStat->reserved[14]) + ", "
                                            + ToString(decodeStat->reserved[15]) + ", ");
    Log::WriteLog(structName + ".NumFrame=" + ToString(decodeStat->NumFrame));
    Log::WriteLog(structName + ".NumSkippedFrame=" + ToString(decodeStat->NumSkippedFrame));
    Log::WriteLog(structName + ".NumError=" + ToString(decodeStat->NumError));
    Log::WriteLog(structName + ".NumCachedFrame=" + ToString(decodeStat->NumCachedFrame) + "\n");
}

void dump_mfxSkipMode(const std::string structName, mfxSkipMode skipMode)
{
    Log::WriteLog("mfxSkipMode " + structName + "=" + ToString(skipMode) + "\n");
}

void dump_mfxPayload(const std::string structName, mfxPayload *payload)
{
    Log::WriteLog("mfxPayload " + structName);
    Log::WriteLog(structName + ".reserved=" + ToString(payload->reserved[0]) + ", " + ToString(payload->reserved[1]) + ", "
                                            + ToString(payload->reserved[2]) + ", " + ToString(payload->reserved[3]));
    Log::WriteLog(structName + ".Data=" + ToString(payload->Data));
    Log::WriteLog(structName + ".NumBit=" + ToString(payload->NumBit));
    Log::WriteLog(structName + ".Type=" + ToString(payload->Type));
    Log::WriteLog(structName + ".BufSize=" + ToString(payload->BufSize) + "\n");
}

void dump_mfxVPPStat(const std::string structName, mfxVPPStat *vppStat)
{
    Log::WriteLog("mfxVPPStat " + structName + ":");
    Log::WriteLog(structName + ".reserved=" + ToString(vppStat->reserved[0]) + ", " + ToString(vppStat->reserved[1]) + ", " + ToString(vppStat->reserved[2]) + ", "
                                            + ToString(vppStat->reserved[3]) + ", " + ToString(vppStat->reserved[4]) + ", " + ToString(vppStat->reserved[5]) + ", "
                                            + ToString(vppStat->reserved[6]) + ", " + ToString(vppStat->reserved[7]) + ", " + ToString(vppStat->reserved[8]) + ", "
                                            + ToString(vppStat->reserved[9]) + ", " + ToString(vppStat->reserved[10]) + ", " + ToString(vppStat->reserved[11]) + ", "
                                            + ToString(vppStat->reserved[12]) + ", " + ToString(vppStat->reserved[13]) + ", " + ToString(vppStat->reserved[14]) + ", "
                                            + ToString(vppStat->reserved[15]));
    Log::WriteLog(structName + ".NumFrame=" + ToString(vppStat->NumFrame));
    Log::WriteLog(structName + ".NumCachedFrame=" + ToString(vppStat->NumCachedFrame) + "\n");
}

void dump_mfxExtVppAuxData(const std::string structName, mfxExtVppAuxData *extVppAuxData)
{
    Log::WriteLog("mfxExtVppAuxData " + structName + ":");
    Log::WriteLog("\n");
    dump_mfxExtBuffer(structName + ".Header", &extVppAuxData->Header);
    Log::WriteLog(structName + ".SpatialComplexity=" + ToString(extVppAuxData->SpatialComplexity));
    Log::WriteLog(structName + ".TemporalComplexity=" + ToString(extVppAuxData->TemporalComplexity));
    Log::WriteLog(structName + ".PicStruct=" + ToString(extVppAuxData->PicStruct));
    Log::WriteLog(structName + ".reserved=" + ToString(extVppAuxData->reserved[0]) + ToString(extVppAuxData->reserved[1]) + ToString(extVppAuxData->reserved[2]));
    Log::WriteLog(structName + ".SceneChangeRate=" + ToString(extVppAuxData->SceneChangeRate));
    Log::WriteLog(structName + ".RepeatedFrame=" + ToString(extVppAuxData->RepeatedFrame) + "\n");
}

void dump_mfxPriority(const std::string structName, mfxPriority priority)
{
    Log::WriteLog("mfxPriority " + structName + "=" + ToString(priority) + "\n");
}