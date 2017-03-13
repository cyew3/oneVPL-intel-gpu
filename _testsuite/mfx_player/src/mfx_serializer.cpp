/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2017 Intel Corporation. All Rights Reserved.

File Name: mfx_serializer.cpp

*******************************************************************************/

#include "mfx_pipeline_defs.h"
#include "mfx_serializer.h"
#include "mfx_extended_buffer.h"
#include "mfx_svc_serial.h"

using namespace Formater;

//////////////////////////////////////////////////////////////////////////
#define SERIALIZE_INT(fieldname)\
    SerializeWithInserter(VM_STRING(#fieldname), m_pStruct->fieldname)

#define SERIALIZE_DOUBLE(fieldname)\
    SERIALIZE_INT(fieldname)

#define SERIALIZE_TIMESTAMP(fieldname)\
    SerializeWithInserter(VM_STRING(#fieldname), ConvertMFXTime2mfxF64(m_pStruct->fieldname))

#define DESERIALIZE_INT(fieldname)\
    if (!DeserializeSingleElement(input_strm, m_pStruct->fieldname)){return false;}

#define SERIALIZE_STRUCT(fieldname)\
    SerializeStruct(VM_STRING(#fieldname) VM_STRING("."), m_pStruct->fieldname);

#define SERIALIZE_POD_ARRAY(fieldname, num)\
    SerializeArrayOfPODs(VM_STRING(#fieldname), m_pStruct->fieldname, num)

#define SERIALIZE_POD_ARRAY_FORMATED(fieldname, num, formater)\
    SerializeArrayOfPODs(VM_STRING(#fieldname), m_pStruct->fieldname, num, formater)

#define SERIALIZE_STRUCTS_ARRAY(fieldname, num)\
    SerializeArrayOfStructs(VM_STRING(#fieldname), m_pStruct->fieldname, num)

#define DESERIALIZE_ARRAY(fieldname, num)\
    if (!DeserializeArray(input_strm, num, m_pStruct->fieldname)){return false;}

#define DESERIALIZE_ARRAY_SIZE(array_name, num)\
    if (!DeserializeSingleElement(input_strm, num)){return false;}\
    if (num > MFX_ARRAY_SIZE(m_pStruct->array_name)){return false;}

//////////////////////////////////////////////////////////////////////////

void   MFXStructureRef <mfxMVCViewDependency>::ConstructValues () const
{
    SERIALIZE_INT(ViewId);
    SERIALIZE_POD_ARRAY(AnchorRefL0,m_pStruct->NumAnchorRefsL0);
    SERIALIZE_POD_ARRAY(AnchorRefL1,m_pStruct->NumAnchorRefsL1);
    SERIALIZE_POD_ARRAY(NonAnchorRefL0,m_pStruct->NumNonAnchorRefsL0);
    SERIALIZE_POD_ARRAY(NonAnchorRefL1,m_pStruct->NumNonAnchorRefsL1);
}

void MFXStructureRef <mfxMVCOperationPoint>::ConstructValues () const
{
    SERIALIZE_INT(TemporalId);
    SERIALIZE_INT(LevelIdc);
    SERIALIZE_INT(NumViews);
    SERIALIZE_POD_ARRAY(TargetViewId, m_pStruct->NumTargetViews);
}

void MFXStructureRef <mfxExtMVCSeqDesc>::ConstructValues () const
{
    SERIALIZE_STRUCTS_ARRAY(View, m_pStruct->NumView);
    SERIALIZE_POD_ARRAY(ViewId, m_pStruct->NumViewId);
    SERIALIZE_STRUCTS_ARRAY(OP, m_pStruct->NumOP);
    //SERIALIZE_INT(CompatibilityMode);
}

bool      MFXStructureRef <mfxMVCViewDependency>::DeSerialize(const tstring & refStr, int *nPosition)
{
    tstringstream input_strm;
    input_strm.str(refStr + VM_STRING(' '));

    //clearing attached structure firstly
    MFX_ZERO_MEM(*m_pStruct);

    input_strm>>m_pStruct->ViewId;
    if (!input_strm.good())
        return false;

    DESERIALIZE_ARRAY_SIZE(AnchorRefL0, m_pStruct->NumAnchorRefsL0);
    DESERIALIZE_ARRAY_SIZE(AnchorRefL1, m_pStruct->NumAnchorRefsL1);
    DESERIALIZE_ARRAY_SIZE(NonAnchorRefL0, m_pStruct->NumNonAnchorRefsL0);
    DESERIALIZE_ARRAY_SIZE(NonAnchorRefL1, m_pStruct->NumNonAnchorRefsL1);

    DESERIALIZE_ARRAY(AnchorRefL0, m_pStruct->NumAnchorRefsL0);
    DESERIALIZE_ARRAY(AnchorRefL1, m_pStruct->NumAnchorRefsL1);
    DESERIALIZE_ARRAY(NonAnchorRefL0, m_pStruct->NumNonAnchorRefsL0);
    DESERIALIZE_ARRAY(NonAnchorRefL1, m_pStruct->NumNonAnchorRefsL1);

    if (NULL != nPosition)
    {
        *nPosition = (int)input_strm.tellg();
    }

    return true;
}

void   MFXStructureRef <mfxVideoParam>::ConstructValues () const
{
    SERIALIZE_INT(AsyncDepth);
    if (m_flags & Formater::VPP)
    {
        SerializeStruct(VM_STRING(""), m_pStruct->vpp);
    }
    else
    {
        SerializeStruct(VM_STRING(""), m_pStruct->mfx);
    }
    SERIALIZE_INT(Protected);
    SERIALIZE_INT(IOPattern);

    //TODO: poor approach since we have magic mfxextendedbuffer helper
    for (int i = 0; i < m_pStruct->NumExtParam; i++) {
        SerializeStruct(VM_STRING(""), *m_pStruct->ExtParam[i]);
    }
}

void MFXStructureRef <mfxExtCodingOption>::ConstructValues () const
{
    SERIALIZE_INT(RateDistortionOpt);
    SERIALIZE_INT(MECostType);
    SERIALIZE_INT(MESearchType);
    SERIALIZE_INT(MVSearchWindow.x);
    SERIALIZE_INT(MVSearchWindow.y);
    SERIALIZE_INT(EndOfSequence);
    SERIALIZE_INT(FramePicture);
    SERIALIZE_INT(CAVLC);
    SERIALIZE_INT(NalHrdConformance);
    SERIALIZE_INT(SingleSeiNalUnit);
    SERIALIZE_INT(VuiVclHrdParameters);
    SERIALIZE_INT(RefPicListReordering);
    SERIALIZE_INT(ResetRefList);
    SERIALIZE_INT(RefPicMarkRep);
    SERIALIZE_INT(FieldOutput);
    SERIALIZE_INT(IntraPredBlockSize);
    SERIALIZE_INT(InterPredBlockSize);
    SERIALIZE_INT(MVPrecision);
    SERIALIZE_INT(MaxDecFrameBuffering);
    SERIALIZE_INT(AUDelimiter);
    SERIALIZE_INT(EndOfStream);
    SERIALIZE_INT(PicTimingSEI);
    SERIALIZE_INT(VuiNalHrdParameters);
}

void MFXStructureRef <mfxExtCodingOption2>::ConstructValues () const
{
    SERIALIZE_INT(IntRefType);
    SERIALIZE_INT(IntRefCycleSize);
    SERIALIZE_INT(IntRefQPDelta);
    SERIALIZE_INT(MaxFrameSize);
    SERIALIZE_INT(MaxSliceSize);
    SERIALIZE_INT(BitrateLimit);
    SERIALIZE_INT(MBBRC);
    SERIALIZE_INT(ExtBRC);
    SERIALIZE_INT(LookAheadDepth);
    SERIALIZE_INT(Trellis);
    SERIALIZE_INT(RepeatPPS);
    SERIALIZE_INT(BRefType);
    SERIALIZE_INT(AdaptiveI);
    SERIALIZE_INT(AdaptiveB);
    SERIALIZE_INT(NumMbPerSlice);
    SERIALIZE_INT(SkipFrame);
    SERIALIZE_INT(MinQPI);
    SERIALIZE_INT(MaxQPI);
    SERIALIZE_INT(MinQPP);
    SERIALIZE_INT(MaxQPP);
    SERIALIZE_INT(MinQPB);
    SERIALIZE_INT(MaxQPB);
    SERIALIZE_INT(FixedFrameRate);
    SERIALIZE_INT(DisableDeblockingIdc);
    SERIALIZE_INT(DisableVUI);
    SERIALIZE_INT(BufferingPeriodSEI);
    SERIALIZE_INT(UseRawRef);
}
void MFXStructureRef <mfxExtCodingOption3>::ConstructValues() const
{
    SERIALIZE_INT(NumSliceI);
    SERIALIZE_INT(NumSliceP);
    SERIALIZE_INT(NumSliceB);
    SERIALIZE_INT(QVBRQuality);
    SERIALIZE_INT(IntRefCycleDist);
    SERIALIZE_INT(AspectRatioInfoPresent);
    SERIALIZE_INT(OverscanInfoPresent);
    SERIALIZE_INT(TimingInfoPresent);
    SERIALIZE_INT(LowDelayHrd);
    SERIALIZE_INT(BitstreamRestriction);
    SERIALIZE_INT(WeightedPred);
    SERIALIZE_INT(WeightedBiPred);
    SERIALIZE_INT(PRefType);
    SERIALIZE_INT(FadeDetection);
    SERIALIZE_INT(GPB);
    SERIALIZE_INT(EnableQPOffset);
    SERIALIZE_INT(MaxFrameSizeI);
    SERIALIZE_INT(MaxFrameSizeP);
    SERIALIZE_POD_ARRAY(QPOffset, 8);
    SERIALIZE_POD_ARRAY(NumRefActiveP, 8);
    SERIALIZE_POD_ARRAY(NumRefActiveBL0, 8);
    SERIALIZE_POD_ARRAY(NumRefActiveBL1, 8);
    SERIALIZE_INT(ConstrainedIntraPredFlag);
    SERIALIZE_INT(TransformSkip);
    SERIALIZE_INT(AdaptiveMaxFrameSize);
    SERIALIZE_INT(TargetChromaFormatPlus1);
    SERIALIZE_INT(TargetBitDepthLuma);
    SERIALIZE_INT(TargetBitDepthChroma);
    
    if (m_pStruct->TargetChromaFormatPlus1)
        m_values_map[VM_STRING("TargetChromaFormatPlus1")] = GetMFXChromaString(m_pStruct->TargetChromaFormatPlus1 - 1) + VM_STRING(" + 1");
    SERIALIZE_INT(BRCPanicMode);
}

void MFXStructureRef <mfxExtCodingOptionDDI>::ConstructValues () const
{
    SERIALIZE_INT(IntraPredCostType);
    SERIALIZE_INT(MEInterpolationMethod);
    SERIALIZE_INT(MEFractionalSearchType);
    SERIALIZE_INT(MaxMVs);
    SERIALIZE_INT(SkipCheck);
    SERIALIZE_INT(DirectCheck);
    SERIALIZE_INT(BiDirSearch);
    SERIALIZE_INT(MBAFF);
    SERIALIZE_INT(FieldPrediction);
    SERIALIZE_INT(RefOppositeField);
    SERIALIZE_INT(ChromaInME);
    SERIALIZE_INT(WeightedPrediction);
    SERIALIZE_INT(MVPrediction);
    SerializeWithInserter(VM_STRING("IntraPredBlockSize"), m_pStruct->DDI.IntraPredBlockSize);
    SerializeWithInserter(VM_STRING("InterPredBlockSize"), m_pStruct->DDI.InterPredBlockSize);

    // MediaSDK parametrization
    SERIALIZE_INT(BRCPrecision);
    SERIALIZE_INT(RefRaw);
    SERIALIZE_INT(ConstQP);
    SERIALIZE_INT(GlobalSearch);
    SERIALIZE_INT(LocalSearch);

    // threading options
    SERIALIZE_INT(EarlySkip);
    SERIALIZE_INT(LaScaleFactor);
    SERIALIZE_INT(LookAheadDependency);
    SERIALIZE_INT(Hme);
    SERIALIZE_INT(NumActiveRefP);
    SERIALIZE_INT(NumActiveRefBL0);
    SERIALIZE_INT(NumActiveRefBL1);

    SERIALIZE_INT(DisablePSubMBPartition);
    SERIALIZE_INT(DisableBSubMBPartition);
    SERIALIZE_INT(WeightedBiPredIdc);
    SERIALIZE_INT(DirectSpatialMvPredFlag);
    SERIALIZE_INT(CabacInitIdcPlus1);

    SERIALIZE_INT(LCUSize);
    SERIALIZE_INT(WriteIVFHeaders);

}

void MFXStructureRef <mfxExtCodingOptionHEVC>::ConstructValues() const
{
    SERIALIZE_INT(Log2MaxCUSize);
    SERIALIZE_INT(MaxCUDepth);
    SERIALIZE_INT(QuadtreeTULog2MaxSize);
    SERIALIZE_INT(QuadtreeTULog2MinSize);
    SERIALIZE_INT(QuadtreeTUMaxDepthIntra);
    SERIALIZE_INT(QuadtreeTUMaxDepthInter);
    SERIALIZE_INT(QuadtreeTUMaxDepthInterRD);
    SERIALIZE_INT(AnalyzeChroma);
    SERIALIZE_INT(SignBitHiding);
    SERIALIZE_INT(RDOQuant);
    SERIALIZE_INT(SAO);
    SERIALIZE_INT(SplitThresholdStrengthCUIntra);
    SERIALIZE_INT(SplitThresholdStrengthTUIntra);
    SERIALIZE_INT(SplitThresholdStrengthCUInter);
    SERIALIZE_INT(IntraNumCand0_2);
    SERIALIZE_INT(IntraNumCand0_3);
    SERIALIZE_INT(IntraNumCand0_4);
    SERIALIZE_INT(IntraNumCand0_5);
    SERIALIZE_INT(IntraNumCand0_6);
    SERIALIZE_INT(IntraNumCand1_2);
    SERIALIZE_INT(IntraNumCand1_3);
    SERIALIZE_INT(IntraNumCand1_4);
    SERIALIZE_INT(IntraNumCand1_5);
    SERIALIZE_INT(IntraNumCand1_6);
    SERIALIZE_INT(IntraNumCand2_2);
    SERIALIZE_INT(IntraNumCand2_3);
    SERIALIZE_INT(IntraNumCand2_4);
    SERIALIZE_INT(IntraNumCand2_5);
    SERIALIZE_INT(IntraNumCand2_6);
    SERIALIZE_INT(WPP);
    SERIALIZE_INT(Log2MinCuQpDeltaSize);
    SERIALIZE_INT(PartModes);
    SERIALIZE_INT(CmIntraThreshold);
    SERIALIZE_INT(TUSplitIntra);
    SERIALIZE_INT(CUSplit);
    SERIALIZE_INT(IntraAngModes);
    SERIALIZE_INT(EnableCm);
    SERIALIZE_INT(BPyramid);
    SERIALIZE_INT(HadamardMe);
    SERIALIZE_INT(TMVP);
    SERIALIZE_INT(Deblocking);
    SERIALIZE_INT(RDOQuantChroma);
    SERIALIZE_INT(RDOQuantCGZ);
    SERIALIZE_INT(CostChroma);
    SERIALIZE_INT(IntraChromaRDO);
    SERIALIZE_INT(FastInterp);
    SERIALIZE_INT(SaoOpt);
    SERIALIZE_INT(SaoSubOpt);
    SERIALIZE_INT(PatternIntPel);
    SERIALIZE_INT(FastSkip);
    SERIALIZE_INT(PatternSubPel);
    SERIALIZE_INT(ForceNumThread);
    SERIALIZE_INT(FastCbfMode);
    SERIALIZE_INT(PuDecisionSatd);
    SERIALIZE_INT(MinCUDepthAdapt);
    SERIALIZE_INT(MaxCUDepthAdapt);
    SERIALIZE_INT(NumBiRefineIter);
    SERIALIZE_INT(CUSplitThreshold);
    SERIALIZE_INT(DeltaQpMode);
    SERIALIZE_INT(Enable10bit);
    SERIALIZE_INT(IntraAngModesP);
    SERIALIZE_INT(IntraAngModesBRef);
    SERIALIZE_INT(IntraAngModesBnonRef);
    SERIALIZE_INT(SplitThresholdTabIndex);
    SERIALIZE_INT(CpuFeature);
    SERIALIZE_INT(TryIntra);
    SERIALIZE_INT(FastAMPSkipME);
    SERIALIZE_INT(FastAMPRD);
    SERIALIZE_INT(SkipMotionPartition);
    SERIALIZE_INT(SkipCandRD);
    SERIALIZE_INT(FramesInParallel);
    SERIALIZE_INT(AdaptiveRefs);
    SERIALIZE_INT(FastCoeffCost);
    SERIALIZE_INT(NumRefFrameB);
    SERIALIZE_INT(IntraMinDepthSC);
    SERIALIZE_INT(InterMinDepthSTC);
    SERIALIZE_INT(MotionPartitionDepth);
    SERIALIZE_INT(AnalyzeCmplx);
    SERIALIZE_INT(RateControlDepth);
    SERIALIZE_INT(LowresFactor);
    SERIALIZE_INT(DeblockBorders);
    SERIALIZE_INT(SAOChroma);
    SERIALIZE_INT(RepackProb);
    SERIALIZE_INT(NumRefLayers);
    SERIALIZE_INT(ConstQpOffset);
    SERIALIZE_INT(SplitThresholdMultiplier);
    SERIALIZE_INT(EnableCmBiref);
}

void MFXStructureRef <mfxExtHEVCTiles>::ConstructValues() const
{
    SERIALIZE_INT(NumTileColumns);
    SERIALIZE_INT(NumTileRows);
}

void MFXStructureRef <mfxExtVP8CodingOption>::ConstructValues() const
{
    SERIALIZE_INT(Version);
    SERIALIZE_INT(EnableMultipleSegments);
    SERIALIZE_INT(LoopFilterType);
    SERIALIZE_INT(SharpnessLevel);
    SERIALIZE_INT(NumTokenPartitions);

    SERIALIZE_POD_ARRAY(LoopFilterLevel, 4);
    SERIALIZE_POD_ARRAY(LoopFilterRefTypeDelta, 4);
    SERIALIZE_POD_ARRAY(LoopFilterMbModeDelta, 4);
    SERIALIZE_POD_ARRAY(SegmentQPDelta, 4);
    SERIALIZE_POD_ARRAY(CoeffTypeQPDelta, 5);

    SERIALIZE_INT(WriteIVFHeaders);
    SERIALIZE_INT(NumFramesForIVFHeader);
}

// TODO: uncomment when buffer will be added to API
/*void MFXStructureRef <mfxExtVP9CodingOption>::ConstructValues() const
{
}*/

void MFXStructureRef <mfxFrameInfo>::ConstructValues () const
{
    SERIALIZE_INT(BitDepthLuma);
    SERIALIZE_INT(BitDepthChroma);
    SERIALIZE_INT(Shift);

    SERIALIZE_INT(FrameId.TemporalId);
    SERIALIZE_INT(FrameId.PriorityId);
    if (!(m_flags & Formater::SVC))
    {
        SERIALIZE_INT(FrameId.ViewId);
    }else
    {
        SERIALIZE_INT(FrameId.DependencyId);
        SERIALIZE_INT(FrameId.QualityId);
    }
    //codecID serial
    FourCC cid(m_pStruct->FourCC);
    SerializeStruct(VM_STRING("FourCC"), cid);

    SERIALIZE_INT(Width);
    SERIALIZE_INT(Height);
    SERIALIZE_INT(CropX);
    SERIALIZE_INT(CropY);
    SERIALIZE_INT(CropW);
    SERIALIZE_INT(CropH);
    SERIALIZE_INT(FrameRateExtN);
    SERIALIZE_INT(FrameRateExtD);
    SERIALIZE_INT(AspectRatioW);
    SERIALIZE_INT(AspectRatioH);
    SERIALIZE_INT(PicStruct);
    SERIALIZE_INT(ChromaFormat);

    m_values_map[VM_STRING("ChromaFormat")] = GetMFXChromaString(m_pStruct->ChromaFormat);
}

void MFXStructureRef <mfxBitstream>::ConstructValues () const
{
    SERIALIZE_INT(DataLength);
    SERIALIZE_TIMESTAMP(DecodeTimeStamp);
    SERIALIZE_TIMESTAMP(TimeStamp);

    m_values_map[VM_STRING("Data")] = GetMFXRawDataString( m_pStruct->Data + m_pStruct->DataOffset
                                                         , (std::min)((mfxU32)m_flags, m_pStruct->DataLength));
    m_values_map[VM_STRING("PicStruct")] = GetMFXPicStructString(m_pStruct->PicStruct);
    m_values_map[VM_STRING("FrameType")] = GetMFXFrameTypeString(m_pStruct->FrameType);
    SERIALIZE_INT(DataFlag);
}


void MFXStructureRef <mfxExtCodingOptionQuantMatrix>::ConstructValues () const
{
    SERIALIZE_INT(bIntraQM);
    SERIALIZE_INT(bInterQM);
    SERIALIZE_INT(bChromaIntraQM);
    SERIALIZE_INT(bChromaInterQM);
    SERIALIZE_POD_ARRAY(IntraQM,64);
    SERIALIZE_POD_ARRAY(InterQM,64);
    SERIALIZE_POD_ARRAY(ChromaIntraQM,64);
    SERIALIZE_POD_ARRAY(ChromaInterQM,64);
}

void MFXStructureRef<mfxInfoVPP>::ConstructValues () const
{
    SERIALIZE_STRUCT(In);
    SERIALIZE_STRUCT(Out);
}

void MFXStructureRef <mfxInfoMFX>::ConstructValues () const
{
    SERIALIZE_INT(LowPower);
    SERIALIZE_INT(BRCParamMultiplier);

    MFXStructureRef<mfxFrameInfo> frameinfo(m_pStruct->FrameInfo, m_flags);
    frameinfo.Serialize(KeyPrefix(VM_STRING("FrameInfo."), m_values_map));

    //codecID serial
    FourCC cid(m_pStruct->CodecId);
    MFXStructureRef<FourCC> codecId(cid, m_flags);
    codecId.Serialize(KeyPrefix(VM_STRING("CodecId"),m_values_map));

    SERIALIZE_INT(CodecProfile);
    SERIALIZE_INT(CodecLevel);
    SERIALIZE_INT(NumThread);

    if (m_flags & Formater::DECODE || m_flags & Formater::JPEG_DECODE)
    {
        if (m_flags & Formater::DECODE)
        {
            SERIALIZE_INT(DecodedOrder);
            SERIALIZE_INT(ExtendedPicStruct);
        }

        if (m_flags & Formater::JPEG_DECODE)
        {
            SERIALIZE_INT(JPEGChromaFormat);
            //mfxU8   *QuantizationTable;
            //mfxU16  *HuffmanTable[2];
            SERIALIZE_INT(Rotation);
            //mfxU16  reserved3[3];
        }
    }
    else
    {
        SERIALIZE_INT(TargetUsage);
        SERIALIZE_INT(GopPicSize);
        SERIALIZE_INT(GopRefDist);
        SERIALIZE_INT(GopOptFlag);
        SERIALIZE_INT(IdrInterval);
        SERIALIZE_INT(RateControlMethod);

        if (MFX_RATECONTROL_CQP == m_pStruct->RateControlMethod)
        {
            SERIALIZE_INT(QPI);
            SERIALIZE_INT(BufferSizeInKB);
            SERIALIZE_INT(QPP);
            SERIALIZE_INT(QPB);
        }
        else if (MFX_RATECONTROL_AVBR == m_pStruct->RateControlMethod)
        {
            SERIALIZE_INT(Accuracy);
            SERIALIZE_INT(BufferSizeInKB);
            SERIALIZE_INT(TargetKbps);
            SERIALIZE_INT(Convergence);
        }
        else if (MFX_RATECONTROL_LA == m_pStruct->RateControlMethod)
        {
            SERIALIZE_INT(TargetKbps);
        }
        else if (MFX_RATECONTROL_LA_HRD == m_pStruct->RateControlMethod)
        {
            SERIALIZE_INT(InitialDelayInKB);
            SERIALIZE_INT(BufferSizeInKB);
            SERIALIZE_INT(TargetKbps);
            SERIALIZE_INT(MaxKbps);
        }
        else if (MFX_RATECONTROL_ICQ == m_pStruct->RateControlMethod || MFX_RATECONTROL_LA_ICQ == m_pStruct->RateControlMethod)
        {
            SERIALIZE_INT(BufferSizeInKB);
            SERIALIZE_INT(ICQQuality);
        }
        else
        {
            SERIALIZE_INT(InitialDelayInKB);
            SERIALIZE_INT(BufferSizeInKB);
            SERIALIZE_INT(TargetKbps);
            SERIALIZE_INT(MaxKbps);
        }

        SERIALIZE_INT(NumSlice);
        SERIALIZE_INT(NumRefFrame);
        SERIALIZE_INT(EncodedOrder);
    }
}

void MFXStructureRef <FourCC>::ConstructValues () const
{
    vm_char sCodec[5] =
    {
        ((char*)&m_pStruct->m_id)[0],
        ((char*)&m_pStruct->m_id)[1],
        ((char*)&m_pStruct->m_id)[2],
        ((char*)&m_pStruct->m_id)[3],
        0
    };

    m_values_map[VM_STRING("")] = sCodec;
}

void MFXStructureRef<mfxVersion>::ConstructValues () const
{
    tstringstream stream;
    stream<<m_pStruct->Major<<VM_STRING(".")<<m_pStruct->Minor;

    m_values_map[VM_STRING("")] = stream.str();
}

bool MFXStructureRef<mfxVersion>::DeSerialize(const tstring & refStr, int *nPosition)
{
    tstringstream input_strm(refStr + VM_STRING(' '));

    //clearing attached structure firstly
    MFX_ZERO_MEM(*m_pStruct);
    vm_char delimiter;

    //read major and delimiter
    input_strm>>m_pStruct->Major>>delimiter;

    if (!input_strm.good() || delimiter != VM_STRING('.'))
        return false;

    //read minor
    if (!(input_strm>>m_pStruct->Minor)) {
        return false;
    }

    if (NULL != nPosition) {
        *nPosition =  (int)input_strm.tellg();
    }

    delimiter = input_strm.peek();

    if (!(input_strm.eof() || delimiter == VM_STRING(' ')))
        return false;

    return true;
}

void MFXStructureRef <MFXBufType>::ConstructValues () const
{
    tstringstream stream;

    switch (*m_pStruct)
    {
        case MFX_BUF_HW:   stream<<VM_STRING("D3D surface"); break;
        case MFX_BUF_SW:   stream<<VM_STRING("system memory"); break;
        case MFX_BUF_OPAQ: stream<<VM_STRING("OPAQ memory"); break;
        case MFX_BUF_HW_DX11: stream<<VM_STRING("D3D11 surface"); break;
        default:           stream<<VM_STRING("Unknown"); break;
    }

    m_values_map[VM_STRING("")] = stream.str();
}

void MFXStructureRef <bool>::ConstructValues () const
{
    m_values_map[VM_STRING("")] = (*m_pStruct) ? VM_STRING("yes") : VM_STRING("no");
}

void MFXStructureRef <mfxIMPL>::ConstructValues () const
{
    m_values_map[VM_STRING("")] = GetMFXImplString(*m_pStruct);
}

void MFXStructureRef <mfxExtVideoSignalInfo>::ConstructValues () const
{
    SERIALIZE_INT(VideoFormat);
    SERIALIZE_INT(VideoFullRange);
    SERIALIZE_INT(ColourDescriptionPresent);
    SERIALIZE_INT(ColourPrimaries);
    SERIALIZE_INT(TransferCharacteristics);
    SERIALIZE_INT(MatrixCoefficients);
}

void MFXStructureRef<mfxExtAVCRefListCtrl>::ConstructValues () const
{
    SERIALIZE_INT(NumRefIdxL0Active);
    SERIALIZE_INT(NumRefIdxL1Active);

    //TODO: improve: it is possible to calc length automatically
    SerializeArrayOfPODs(VM_STRING("PreferredRefList"), (RefListFormater::RefListElement*)m_pStruct->PreferredRefList, MFX_ARRAY_SIZE(m_pStruct->PreferredRefList), RefListFormater());
    SerializeArrayOfPODs(VM_STRING("RejectedRefList"), (RefListFormater::RefListElement*)m_pStruct->RejectedRefList, MFX_ARRAY_SIZE(m_pStruct->RejectedRefList), RefListFormater());
    SerializeArrayOfPODs(VM_STRING("LongTermRefList"), (LTRRefListFormater::LTRElement*)m_pStruct->LongTermRefList, MFX_ARRAY_SIZE(m_pStruct->LongTermRefList), LTRRefListFormater());
}

bool MFXStructureRef<mfxExtAVCRefListCtrl>::DeSerialize(const tstring & refStr, int *nPosition)
{
    tstringstream input_strm;
    input_strm.str(refStr + VM_STRING(' '));

    //clearing attached structure firstly
    MFXExtBufferPtr<mfxExtAVCRefListCtrl> initHeader(m_pStruct);
    initHeader.release();

    mfxU32 nPreferred, nRejected, nLongTerm;

    DESERIALIZE_INT(NumRefIdxL0Active);
    DESERIALIZE_INT(NumRefIdxL1Active);
    DESERIALIZE_INT(ApplyLongTermIdx);

    DESERIALIZE_ARRAY_SIZE(PreferredRefList, nPreferred);
    DESERIALIZE_ARRAY_SIZE(RejectedRefList, nRejected);
    DESERIALIZE_ARRAY_SIZE(LongTermRefList, nLongTerm);

    for (mfxU32 i = 0; i < MFX_ARRAY_SIZE(m_pStruct->PreferredRefList); i++)
    {
        if (i < nPreferred)
        {
            DESERIALIZE_INT(PreferredRefList[i].FrameOrder);
        }else
        {
            m_pStruct->PreferredRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        }

        m_pStruct->PreferredRefList[i].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }


    for (mfxU32 i = 0; i < MFX_ARRAY_SIZE(m_pStruct->RejectedRefList); i++)
    {
        if (i < nRejected)
        {
            DESERIALIZE_INT(RejectedRefList[i].FrameOrder);
        }else
        {
            m_pStruct->RejectedRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        }

        m_pStruct->RejectedRefList[i].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    for (mfxU32 i = 0; i < MFX_ARRAY_SIZE(m_pStruct->LongTermRefList); i++)
    {
        if (i < nLongTerm)
        {
            DESERIALIZE_INT(LongTermRefList[i].FrameOrder);
            if (1 == m_pStruct->ApplyLongTermIdx) {
                DESERIALIZE_INT(LongTermRefList[i].LongTermIdx);
            }
        }else
        {
            m_pStruct->LongTermRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        }

        m_pStruct->LongTermRefList[i].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }


    if (NULL != nPosition)
    {
        *nPosition = (int)input_strm.tellg();
    }

    return true;
}

void MFXStructureRef <mfxExtVPPFrameRateConversion>::ConstructValues () const
{
    SERIALIZE_INT(Algorithm);
}

void MFXStructureRef <mfxExtCodingOptionSPSPPS>::ConstructValues () const
{
    SERIALIZE_INT(SPSId);
    SERIALIZE_INT(PPSId);
}

bool MFXStructureRef <mfxExtAvcTemporalLayers>::DeSerialize(const tstring & refStr, int *nPosition)
{
    tstringstream input_strm;
    input_strm.str(refStr + VM_STRING(' '));

    //clearing attached structure firstly
    MFXExtBufferPtr<mfxExtAvcTemporalLayers> initHeader(m_pStruct);
    initHeader.release();

    DESERIALIZE_INT(BaseLayerPID);
    for (mfxU32 i = 0; i < MFX_ARRAY_SIZE(m_pStruct->Layer); i++)
    {
        DESERIALIZE_INT(Layer[i].Scale);
    }

    if (NULL != nPosition)
    {
        *nPosition = (int)input_strm.tellg();
    }

    return true;
}

void MFXStructureRef <mfxExtAvcTemporalLayers>::ConstructValues () const
{
    SERIALIZE_INT(BaseLayerPID);
    SerializeArrayOfPODs(VM_STRING("Layer"), (AvcTemporalLayersFormater::AvcTemporalLayerElement*)m_pStruct->Layer, MFX_ARRAY_SIZE(m_pStruct->Layer), AvcTemporalLayersFormater());
}


bool MFXStructureRef <IppiRect>::DeSerialize(const tstring & refStr, int *nPosition)
{
    tstringstream input_strm;
    input_strm.str(refStr + VM_STRING(' '));

    DESERIALIZE_INT(x);
    DESERIALIZE_INT(y);
    DESERIALIZE_INT(width);
    DESERIALIZE_INT(height);

    if (NULL != nPosition)
    {
        *nPosition = (int)input_strm.tellg();
    }

    return true;
}
void MFXStructureRef <IppiRect>::ConstructValues () const
{
    SERIALIZE_INT(x);
    SERIALIZE_INT(y);
    SERIALIZE_INT(width);
    SERIALIZE_INT(height);
}

bool MFXStructureRef <mfxExtSvcTargetLayer>::DeSerialize(const tstring & refStr, int *nPosition)
{
    tstringstream input_strm;
    input_strm.str(refStr + VM_STRING(' '));

    //clearing attached structure firstly
    MFXExtBufferPtr<mfxExtSvcTargetLayer> initHeader(m_pStruct);
    initHeader.release();

    DESERIALIZE_INT(TargetTemporalID);
    DESERIALIZE_INT(TargetDependencyID);
    DESERIALIZE_INT(TargetQualityID);

    if (NULL != nPosition)
    {
        *nPosition = (int)input_strm.tellg();
    }

    return true;
}

void MFXStructureRef <mfxExtSvcTargetLayer>::ConstructValues () const
{
    SERIALIZE_INT(TargetTemporalID);
    SERIALIZE_INT(TargetDependencyID);
    SERIALIZE_INT(TargetQualityID);
}

void MFXStructureRef <mfxExtEncoderCapability>::ConstructValues () const
{
    SERIALIZE_INT(MBPerSec);
}

void MFXStructureRef<mfxExtAVCEncodedFrameInfo>::ConstructValues () const {
    SERIALIZE_INT(FrameOrder);
    SERIALIZE_INT(PicStruct);
    SERIALIZE_INT(LongTermIdx);

    SerializeArrayOfPODs(VM_STRING("UsedRefListL0"), (RefListFormater::RefListElement*)m_pStruct->UsedRefListL0, MFX_ARRAY_SIZE(m_pStruct->UsedRefListL0), RefListFormater());
    SerializeArrayOfPODs(VM_STRING("UsedRefListL1"), (RefListFormater::RefListElement*)m_pStruct->UsedRefListL1, MFX_ARRAY_SIZE(m_pStruct->UsedRefListL1), RefListFormater());
}

void MFXStructureRef <mfxExtHEVCParam>::ConstructValues () const
{
    SERIALIZE_INT(PicWidthInLumaSamples);
    SERIALIZE_INT(PicHeightInLumaSamples);
    SERIALIZE_INT(SampleAdaptiveOffset);
}

void MFXStructureRef <mfxExtBuffer>:: ConstructValues () const {
    switch (m_pStruct->BufferId)
    {
        case MFX_EXTBUFF_CODING_OPTION :{
            SerializeStruct(VM_STRING("ExtCO."), *(mfxExtCodingOption*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION2 :{
            SerializeStruct(VM_STRING("ExtCO2."), *(mfxExtCodingOption2*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION3:{
            SerializeStruct(VM_STRING("ExtCO3."), *(mfxExtCodingOption3*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_DDI : {
            SerializeStruct(VM_STRING("DDI."), *(mfxExtCodingOptionDDI*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_QM: {
            SerializeStruct(VM_STRING("QuantMatrix."), *(mfxExtCodingOptionQuantMatrix*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_HEVCENC :{
            SerializeStruct(VM_STRING("HEVC."), *(mfxExtCodingOptionHEVC*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_HEVC_PARAM :{
            SerializeStruct(VM_STRING("HEVCPar."), *(mfxExtHEVCParam*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_VP8_CODING_OPTION :{
            SerializeStruct(VM_STRING("VP8."), *(mfxExtVP8CodingOption*)m_pStruct);
            break;
        }
        // TODO: uncomment when buffer will be added to API
        /*case MFX_EXTBUFF_CODING_OPTION_VP9 :{
            SerializeStruct(VM_STRING("VP9."), *(mfxExtVP9CodingOption*)m_pStruct);
            break;
        }*/
        case MFX_EXTBUFF_MVC_SEQ_DESC : {
            SerializeStruct(VM_STRING("MVC."), *(mfxExtMVCSeqDesc*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_SVC_SEQ_DESC : {
            SerialCollection<tstring> files;
            StructureBuilder<mfxExtSVCSeqDesc>
                (VM_STRING("SVC_SEQ"), *(mfxExtSVCSeqDesc*)m_pStruct, files).Serialize(Formaters2::MapPusherSerializer(m_values_map));
            break;
        }
        case MFX_EXTBUFF_SVC_RATE_CONTROL : {
            StructureBuilder<mfxExtSVCRateControl>
                (VM_STRING("SVC_RC"), *(mfxExtSVCRateControl*)m_pStruct).Serialize(Formaters2::MapPusherSerializer(m_values_map));
            break;
        }
        case MFX_EXTBUFF_VIDEO_SIGNAL_INFO : {
            SerializeStruct(VM_STRING("VSIG."), *(mfxExtVideoSignalInfo*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION_SPSPPS : {
            SerializeStruct(VM_STRING("ExtCOSPSPPS."), *(mfxExtCodingOptionSPSPPS*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_AVC_TEMPORAL_LAYERS : {
            SerializeStruct(VM_STRING("AVCTL."), *(mfxExtAvcTemporalLayers*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_ENCODER_CAPABILITY :{
            SerializeStruct(VM_STRING("CAP."), *(mfxExtEncoderCapability*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_ENCODED_FRAME_INFO :{
            SerializeStruct(VM_STRING("ENC_FRAME_INFO."), *(mfxExtAVCEncodedFrameInfo*)m_pStruct);
            break;
                                             }
        default :
            //unsupported buffer
        break;
    }
}
