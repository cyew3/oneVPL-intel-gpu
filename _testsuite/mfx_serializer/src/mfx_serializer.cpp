/******************************************************************************* *\

Copyright (C) 2010-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_serializer.cpp

*******************************************************************************/

#include "mfx_serializer.h"

//////////////////////////////////////////////////////////////////////////
#define SERIALIZE_INT(fieldname)\
    SerializeSingleElement(#fieldname, m_pStruct->fieldname)

#define DESERIALIZE_INT(fieldname)\
    if (!DeSerializeSingleElement(values_map[#fieldname], m_pStruct->fieldname)){return false;}

#define SERIALIZE_POD_ARRAY(fieldname, num)\
    SerializeArrayOfPODs(#fieldname, m_pStruct->fieldname, num)

#define DESERIALIZE_POD_ARRAY(fieldname, num)\
    DeSerializeArrayOfPODs(values_map[#fieldname], m_pStruct->fieldname, num)

#define SERIALIZE_STRUCT(fieldname)\
    SerializeStruct(#fieldname".", m_pStruct->fieldname);

#define DESERIALIZE_STRUCT(fieldname)\
    DeSerializeStruct(values_map, #fieldname".", m_pStruct->fieldname);

#define SERIALIZE_STRUCTS_ARRAY(fieldname, num)\
    SerializeArrayOfStructs(#fieldname, m_pStruct->fieldname, num)

#define DESERIALIZE_STRUCTS_ARRAY(fieldname, num)\
    DeSerializeArrayOfStructs(values_map, #fieldname, m_pStruct->fieldname, num)

#define SERIALIZE_TIMESTAMP(fieldname)\
    SerializeSingleElement(#fieldname, ConvertMFXTime2mfxF64(m_pStruct->fieldname))

#define DESERIALIZE_TIMESTAMP(fieldname)\
{\
    mfxF64 val;\
    if (!DeSerializeSingleElement(values_map[#fieldname], val)){return false;}\
    m_pStruct->fieldname = ConvertmfxF642MFXTime(val);\
}

#define DESERIALIZE_EXT_HEADER(BufferName, BufferID)\
    (  (!DeserializeSingleElement(values_map[BufferName"Header.BufferSz"], Header.BufferSz))\
    || (!DeserializeSingleElement(values_map[BufferName"Header.BufferId"], Header.BufferId))\
    || (Header.BufferId != BufferID) ) ? false : true

#define UNION_FOUND(fieldname)\
    values_map.contains_substr(#fieldname".")

//////////////////////////////////////////////////////////////////////////

void   MFXStructureRef <mfxMVCViewDependency>::ConstructValues() const
{
    SERIALIZE_INT(ViewId);
    SERIALIZE_INT(NumAnchorRefsL0);
    SERIALIZE_INT(NumAnchorRefsL1);
    SERIALIZE_POD_ARRAY(AnchorRefL0,m_pStruct->NumAnchorRefsL0);
    SERIALIZE_POD_ARRAY(AnchorRefL1,m_pStruct->NumAnchorRefsL1);
    SERIALIZE_INT(NumNonAnchorRefsL0);
    SERIALIZE_INT(NumNonAnchorRefsL1);
    SERIALIZE_POD_ARRAY(NonAnchorRefL0,m_pStruct->NumNonAnchorRefsL0);
    SERIALIZE_POD_ARRAY(NonAnchorRefL1,m_pStruct->NumNonAnchorRefsL1);
}

bool   MFXStructureRef <mfxMVCViewDependency>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(ViewId);
    DESERIALIZE_INT(NumAnchorRefsL0);
    DESERIALIZE_INT(NumAnchorRefsL1);
    DESERIALIZE_POD_ARRAY(AnchorRefL0,m_pStruct->NumAnchorRefsL0);
    DESERIALIZE_POD_ARRAY(AnchorRefL1,m_pStruct->NumAnchorRefsL1);
    DESERIALIZE_INT(NumNonAnchorRefsL0);
    DESERIALIZE_INT(NumNonAnchorRefsL1);
    DESERIALIZE_POD_ARRAY(NonAnchorRefL0,m_pStruct->NumNonAnchorRefsL0);
    DESERIALIZE_POD_ARRAY(NonAnchorRefL1,m_pStruct->NumNonAnchorRefsL1);

    return true;
}

void MFXStructureRef <mfxMVCOperationPoint>::ConstructValues() const
{
    SERIALIZE_INT(TemporalId);
    SERIALIZE_INT(LevelIdc);
    SERIALIZE_INT(NumViews);
    SERIALIZE_POD_ARRAY(TargetViewId, m_pStruct->NumTargetViews);
}

bool MFXStructureRef <mfxMVCOperationPoint>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(TemporalId);
    DESERIALIZE_INT(LevelIdc);
    DESERIALIZE_INT(NumViews);
    DESERIALIZE_INT(NumTargetViews);

    m_pStruct->TargetViewId = new mfxU16[m_pStruct->NumTargetViews];
    memset(m_pStruct->TargetViewId, 0, sizeof(mfxU16)*m_pStruct->NumTargetViews);
    DESERIALIZE_POD_ARRAY(TargetViewId, m_pStruct->NumTargetViews);

    return true;
}

void MFXStructureRef <mfxExtMVCSeqDesc>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(NumView);
    SERIALIZE_INT(NumViewAlloc);
    SERIALIZE_STRUCTS_ARRAY(View, m_pStruct->NumView);

    SERIALIZE_INT(NumViewId);
    SERIALIZE_INT(NumViewIdAlloc);
    SERIALIZE_POD_ARRAY(ViewId, m_pStruct->NumViewId);

    SERIALIZE_INT(NumOP);
    SERIALIZE_INT(NumOPAlloc);
    SERIALIZE_STRUCTS_ARRAY(OP, m_pStruct->NumOP);

    SERIALIZE_INT(NumRefsTotal);
}

bool MFXStructureRef <mfxExtMVCSeqDesc>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(NumView);
    DESERIALIZE_INT(NumViewAlloc);
    m_pStruct->View = new mfxMVCViewDependency[m_pStruct->NumView];
    memset(m_pStruct->View, 0, sizeof(mfxMVCViewDependency)*m_pStruct->NumView);
    DESERIALIZE_STRUCTS_ARRAY(View, m_pStruct->NumView);

    DESERIALIZE_INT(NumViewId);
    DESERIALIZE_INT(NumViewIdAlloc);
    m_pStruct->ViewId = new mfxU16[m_pStruct->NumViewId];
    memset(m_pStruct->ViewId, 0, sizeof(mfxU16)*m_pStruct->NumViewId);
    DESERIALIZE_POD_ARRAY(ViewId, m_pStruct->NumViewId);

    DESERIALIZE_INT(NumOP);
    DESERIALIZE_INT(NumOPAlloc);
    m_pStruct->OP = new mfxMVCOperationPoint[m_pStruct->NumOP];
    memset(m_pStruct->OP, 0, sizeof(mfxMVCOperationPoint)*m_pStruct->NumOP);
    DESERIALIZE_STRUCTS_ARRAY(OP, m_pStruct->NumOP);

    DESERIALIZE_INT(NumRefsTotal);

    return true;
}

void MFXStructureRef <mfxVideoParam>::ConstructValues() const
{
    SERIALIZE_INT(AsyncDepth);
    if (m_flags & SerialFlags::VPP)
    {
        SerializeStruct("vpp.", m_pStruct->vpp);
    }
    else
    {
        SerializeStruct("mfx.", m_pStruct->mfx);
    }
    SERIALIZE_INT(Protected);
    SERIALIZE_INT(IOPattern);

    SERIALIZE_INT(NumExtParam);

    for (int i = 0; i < m_pStruct->NumExtParam; i++) {
        SerializeStruct("", *m_pStruct->ExtParam[i]);
    }
}

bool MFXStructureRef <mfxVideoParam>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(AsyncDepth);
    if (UNION_FOUND(vpp))
    {
        DeSerializeStruct(values_map, "vpp.", m_pStruct->vpp);
    }
    else
    {
        DeSerializeStruct(values_map, "mfx.", m_pStruct->mfx);
    }
    DESERIALIZE_INT(Protected);
    DESERIALIZE_INT(IOPattern);

    DESERIALIZE_INT(NumExtParam);

    m_pStruct->ExtParam = new mfxExtBuffer*[m_pStruct->NumExtParam];
    memset(m_pStruct->ExtParam, 0, sizeof(mfxExtBuffer*) * m_pStruct->NumExtParam);

    hash_array<std::string, std::string>::const_iterator it;
    std::vector<std::string> ExtBufNames;
    mfxU32 BufferSz = 0;

    for(it = values_map.begin(); it != values_map.end(); it++)
    {
        if (it->first.find("Header.BufferId") == std::string::npos)
            continue;

        ExtBufNames.push_back(it->first.substr(0, it->first.find("Header.BufferId")));
    }

    if (ExtBufNames.size() != m_pStruct->NumExtParam)
        return false;

    for (int i = 0; i < m_pStruct->NumExtParam; i++) {
        DeSerializeSingleElement(values_map[ExtBufNames[i] + "Header.BufferSz"], BufferSz);

        m_pStruct->ExtParam[i] = (mfxExtBuffer*)new mfxU8[BufferSz];
        memset(m_pStruct->ExtParam[i], 0, BufferSz);

        DeSerializeStruct(values_map, ExtBufNames[i], *m_pStruct->ExtParam[i]);
    }

    return true;
}

void MFXStructureRef <mfxExtCodingOption>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

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

bool MFXStructureRef <mfxExtCodingOption>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(RateDistortionOpt);
    DESERIALIZE_INT(MECostType);
    DESERIALIZE_INT(MESearchType);
    DESERIALIZE_INT(MVSearchWindow.x);
    DESERIALIZE_INT(MVSearchWindow.y);
    DESERIALIZE_INT(EndOfSequence);
    DESERIALIZE_INT(FramePicture);
    DESERIALIZE_INT(CAVLC);
    DESERIALIZE_INT(NalHrdConformance);
    DESERIALIZE_INT(SingleSeiNalUnit);
    DESERIALIZE_INT(VuiVclHrdParameters);
    DESERIALIZE_INT(RefPicListReordering);
    DESERIALIZE_INT(ResetRefList);
    DESERIALIZE_INT(RefPicMarkRep);
    DESERIALIZE_INT(FieldOutput);
    DESERIALIZE_INT(IntraPredBlockSize);
    DESERIALIZE_INT(InterPredBlockSize);
    DESERIALIZE_INT(MVPrecision);
    DESERIALIZE_INT(MaxDecFrameBuffering);
    DESERIALIZE_INT(AUDelimiter);
    DESERIALIZE_INT(EndOfStream);
    DESERIALIZE_INT(PicTimingSEI);
    DESERIALIZE_INT(VuiNalHrdParameters);

    return true;
}

void MFXStructureRef <mfxExtCodingOption2>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

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
    SERIALIZE_INT(NumMbPerSlice);
    SERIALIZE_INT(SkipFrame);
    SERIALIZE_INT(MinQPI);
    SERIALIZE_INT(MaxQPI);
    SERIALIZE_INT(MinQPP);
    SERIALIZE_INT(MaxQPP);
    SERIALIZE_INT(MinQPB);
    SERIALIZE_INT(MaxQPB);
    SERIALIZE_INT(DisableVUI);
    SERIALIZE_INT(BufferingPeriodSEI);
    SERIALIZE_INT(UseRawRef);
}

bool MFXStructureRef <mfxExtCodingOption2>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(IntRefType);
    DESERIALIZE_INT(IntRefCycleSize);
    DESERIALIZE_INT(IntRefQPDelta);
    DESERIALIZE_INT(MaxFrameSize);
    DESERIALIZE_INT(MaxSliceSize);
    DESERIALIZE_INT(BitrateLimit);
    DESERIALIZE_INT(MBBRC);
    DESERIALIZE_INT(ExtBRC);
    DESERIALIZE_INT(LookAheadDepth);
    DESERIALIZE_INT(Trellis);
    DESERIALIZE_INT(RepeatPPS);
    DESERIALIZE_INT(BRefType);
    DESERIALIZE_INT(NumMbPerSlice);
    DESERIALIZE_INT(SkipFrame);
    DESERIALIZE_INT(MinQPI);
    DESERIALIZE_INT(MaxQPI);
    DESERIALIZE_INT(MinQPP);
    DESERIALIZE_INT(MaxQPP);
    DESERIALIZE_INT(MinQPB);
    DESERIALIZE_INT(MaxQPB);
    DESERIALIZE_INT(DisableVUI);
    DESERIALIZE_INT(BufferingPeriodSEI);
    DESERIALIZE_INT(UseRawRef);

    return true;
}

void MFXStructureRef <mfxExtCodingOption3>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(NumSliceI);
    SERIALIZE_INT(NumSliceP);
    SERIALIZE_INT(NumSliceB);
    SERIALIZE_INT(QVBRQuality);
}

bool MFXStructureRef <mfxExtCodingOption3>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(NumSliceI);
    DESERIALIZE_INT(NumSliceP);
    DESERIALIZE_INT(NumSliceB);
    DESERIALIZE_INT(QVBRQuality);

    return true;
}

void MFXStructureRef <mfxExtHEVCTiles>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(NumTileColumns);
    SERIALIZE_INT(NumTileRows);
}

bool MFXStructureRef <mfxExtHEVCTiles>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(NumTileColumns);
    DESERIALIZE_INT(NumTileRows);

    return true;
}

void MFXStructureRef <mfxExtVP8CodingOption>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

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

bool MFXStructureRef <mfxExtVP8CodingOption>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(Version);
    DESERIALIZE_INT(EnableMultipleSegments);
    DESERIALIZE_INT(LoopFilterType);
    DESERIALIZE_INT(SharpnessLevel);
    DESERIALIZE_INT(NumTokenPartitions);

    DESERIALIZE_POD_ARRAY(LoopFilterLevel, 4);
    DESERIALIZE_POD_ARRAY(LoopFilterRefTypeDelta, 4);
    DESERIALIZE_POD_ARRAY(LoopFilterMbModeDelta, 4);
    DESERIALIZE_POD_ARRAY(SegmentQPDelta, 4);
    DESERIALIZE_POD_ARRAY(CoeffTypeQPDelta, 5);

    DESERIALIZE_INT(WriteIVFHeaders);
    DESERIALIZE_INT(NumFramesForIVFHeader);

    return true;
}

void MFXStructureRef <mfxFrameInfo>::ConstructValues() const
{
    SERIALIZE_INT(FrameId.TemporalId);
    SERIALIZE_INT(FrameId.PriorityId);
    if (!(m_flags & SerialFlags::SVC))
    {
        SERIALIZE_INT(FrameId.ViewId);
    }else
    {
        SERIALIZE_INT(FrameId.DependencyId);
        SERIALIZE_INT(FrameId.QualityId);
    }

    m_values_map["FourCC"] = GetMFXFourccString(m_pStruct->FourCC);

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
    SERIALIZE_INT(BitDepthLuma);
    SERIALIZE_INT(BitDepthChroma);

    m_values_map["ChromaFormat"] = GetMFXChromaString(m_pStruct->ChromaFormat);
}

bool MFXStructureRef <mfxFrameInfo>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(FrameId.TemporalId);
    DESERIALIZE_INT(FrameId.PriorityId);
    if (UNION_FOUND(FrameId.ViewId))
    {
        DESERIALIZE_INT(FrameId.ViewId);
    }else
    {
        DESERIALIZE_INT(FrameId.DependencyId);
        DESERIALIZE_INT(FrameId.QualityId);
    }

    m_pStruct->FourCC = GetMFXFourccCode(values_map["FourCC"]);

    DESERIALIZE_INT(Width);
    DESERIALIZE_INT(Height);
    DESERIALIZE_INT(CropX);
    DESERIALIZE_INT(CropY);
    DESERIALIZE_INT(CropW);
    DESERIALIZE_INT(CropH);
    DESERIALIZE_INT(FrameRateExtN);
    DESERIALIZE_INT(FrameRateExtD);
    DESERIALIZE_INT(AspectRatioW);
    DESERIALIZE_INT(AspectRatioH);
    DESERIALIZE_INT(PicStruct);
    DESERIALIZE_INT(BitDepthLuma);
    DESERIALIZE_INT(BitDepthChroma);

    m_pStruct->ChromaFormat = (mfxU16) GetMFXChromaCode(values_map["ChromaFormat"]);

    return true;
}

void MFXStructureRef <mfxBitstream>::ConstructValues() const
{
    SERIALIZE_INT(DataLength);
    SERIALIZE_TIMESTAMP(DecodeTimeStamp);
    SERIALIZE_TIMESTAMP(TimeStamp);

    m_values_map["Data"]      = GetMFXRawDataString( m_pStruct->Data + m_pStruct->DataOffset, m_pStruct->DataLength);
    m_values_map["PicStruct"] = GetMFXPicStructString(m_pStruct->PicStruct);
    m_values_map["FrameType"] = GetMFXFrameTypeString(m_pStruct->FrameType);
    SERIALIZE_INT(DataFlag);
}

bool MFXStructureRef <mfxBitstream>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(DataLength);
    DESERIALIZE_TIMESTAMP(DecodeTimeStamp);
    DESERIALIZE_TIMESTAMP(TimeStamp);

    m_pStruct->Data = new mfxU8[m_pStruct->DataLength + 1];
    memset(m_pStruct->Data, 0, m_pStruct->DataLength + 1);

    GetMFXRawDataValues( m_pStruct->Data, values_map["Data"]);
    m_pStruct->PicStruct = (mfxU16) GetMFXPicStructCode(values_map["PicStruct"]);
    m_pStruct->FrameType = (mfxU16) GetMFXFrameTypeCode(values_map["FrameType"]);
    SERIALIZE_INT(DataFlag);

    return true;
}

void MFXStructureRef<mfxInfoVPP>::ConstructValues() const
{
    SERIALIZE_STRUCT(In);
    SERIALIZE_STRUCT(Out);
}

bool MFXStructureRef<mfxInfoVPP>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_STRUCT(In);
    DESERIALIZE_STRUCT(Out);

    return true;
}

void MFXStructureRef <mfxInfoMFX>::ConstructValues() const
{
    SERIALIZE_INT(LowPower);
    SERIALIZE_INT(BRCParamMultiplier);

    SERIALIZE_STRUCT(FrameInfo);

    m_values_map["CodecId"] = GetMFXCodecString(m_pStruct->CodecId);

    SERIALIZE_INT(CodecProfile);
    SERIALIZE_INT(CodecLevel);
    SERIALIZE_INT(NumThread);

    if (m_flags & SerialFlags::DECODE)
    {
        SERIALIZE_INT(DecodedOrder);
        SERIALIZE_INT(ExtendedPicStruct);
        SERIALIZE_INT(TimeStampCalc);
        SERIALIZE_INT(SliceGroupsPresent);
    }
    else if (m_flags & SerialFlags::JPEG_DECODE)
    {
        SERIALIZE_INT(JPEGChromaFormat);
        SERIALIZE_INT(Rotation);
        SERIALIZE_INT(JPEGColorFormat);
        SERIALIZE_INT(InterleavedDec);
    }
    else if (m_flags & SerialFlags::JPEG_ENCODE)
    {
        SERIALIZE_INT(Interleaved);
        SERIALIZE_INT(Quality);
        SERIALIZE_INT(RestartInterval);
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

bool MFXStructureRef <mfxInfoMFX>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(LowPower);
    DESERIALIZE_INT(BRCParamMultiplier);

    DESERIALIZE_STRUCT(FrameInfo);

    m_pStruct->CodecId = GetMFXCodecCode(values_map["CodecId"]);

    DESERIALIZE_INT(CodecProfile);
    DESERIALIZE_INT(CodecLevel);
    DESERIALIZE_INT(NumThread);

    if (UNION_FOUND(DecodedOrder))
    {
        SERIALIZE_INT(DecodedOrder);
        SERIALIZE_INT(ExtendedPicStruct);
        SERIALIZE_INT(TimeStampCalc);
        SERIALIZE_INT(SliceGroupsPresent);
    }
    else if (UNION_FOUND(JPEGChromaFormat))
    {
        SERIALIZE_INT(JPEGChromaFormat);
        SERIALIZE_INT(Rotation);
        SERIALIZE_INT(JPEGColorFormat);
        SERIALIZE_INT(InterleavedDec);
    }
    else if (UNION_FOUND(Interleaved))
    {
        SERIALIZE_INT(Interleaved);
        SERIALIZE_INT(Quality);
        SERIALIZE_INT(RestartInterval);
    }
    else
    {
        DESERIALIZE_INT(TargetUsage);
        DESERIALIZE_INT(GopPicSize);
        DESERIALIZE_INT(GopRefDist);
        DESERIALIZE_INT(GopOptFlag);
        DESERIALIZE_INT(IdrInterval);
        DESERIALIZE_INT(RateControlMethod);

        if (MFX_RATECONTROL_CQP == m_pStruct->RateControlMethod)
        {
            DESERIALIZE_INT(QPI);
            DESERIALIZE_INT(BufferSizeInKB);
            DESERIALIZE_INT(QPP);
            DESERIALIZE_INT(QPB);
        }
        else if (MFX_RATECONTROL_AVBR == m_pStruct->RateControlMethod)
        {
            DESERIALIZE_INT(Accuracy);
            DESERIALIZE_INT(BufferSizeInKB);
            DESERIALIZE_INT(TargetKbps);
            DESERIALIZE_INT(Convergence);
        }
        else if (MFX_RATECONTROL_LA == m_pStruct->RateControlMethod)
        {
            DESERIALIZE_INT(TargetKbps);
        }
        else if (MFX_RATECONTROL_LA_HRD == m_pStruct->RateControlMethod)
        {
            DESERIALIZE_INT(InitialDelayInKB);
            DESERIALIZE_INT(BufferSizeInKB);
            DESERIALIZE_INT(TargetKbps);
            DESERIALIZE_INT(MaxKbps);
        }
        else if (MFX_RATECONTROL_ICQ == m_pStruct->RateControlMethod || MFX_RATECONTROL_LA_ICQ == m_pStruct->RateControlMethod)
        {
            DESERIALIZE_INT(BufferSizeInKB);
            DESERIALIZE_INT(ICQQuality);
        }
        else
        {
            DESERIALIZE_INT(InitialDelayInKB);
            DESERIALIZE_INT(BufferSizeInKB);
            DESERIALIZE_INT(TargetKbps);
            DESERIALIZE_INT(MaxKbps);
        }

        DESERIALIZE_INT(NumSlice);
        DESERIALIZE_INT(NumRefFrame);
        DESERIALIZE_INT(EncodedOrder);
    }

    return true;
}

void MFXStructureRef<mfxVersion>::ConstructValues() const
{
    std::stringstream stream;
    stream<<m_pStruct->Major<<"."<<m_pStruct->Minor;

    m_values_map[""] = stream.str();
}

bool MFXStructureRef<mfxVersion>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    std::stringstream stream(values_map[""] + ' ');
    char delimiter;
    stream>>m_pStruct->Major>>delimiter>>m_pStruct->Minor;

    if (!stream.good() || delimiter != '.')
        return false;

    return true;
}

void MFXStructureRef <bool>::ConstructValues() const
{
    switch (*m_pStruct)
    {
    case 1:
        m_values_map[""] = "yes";
        break;
    default:
        m_values_map[""] = "no";
        break;
    }
}

bool MFXStructureRef <bool>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    std::string stream = values_map[""];

    if (stream.compare("yes") == 0)
        *m_pStruct = 1;
    else
        *m_pStruct = 0;

    return true;
}

void MFXStructureRef <mfxIMPL>::ConstructValues() const
{
    m_values_map[""] = GetMFXImplString(*m_pStruct);
}

bool MFXStructureRef <mfxIMPL>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    *m_pStruct = GetMFXImplCode(values_map[""]);

    return true;
}

void MFXStructureRef <mfxExtVideoSignalInfo>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(VideoFormat);
    SERIALIZE_INT(VideoFullRange);
    SERIALIZE_INT(ColourDescriptionPresent);
    SERIALIZE_INT(ColourPrimaries);
    SERIALIZE_INT(TransferCharacteristics);
    SERIALIZE_INT(MatrixCoefficients);
}

bool MFXStructureRef <mfxExtVideoSignalInfo>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(VideoFormat);
    DESERIALIZE_INT(VideoFullRange);
    DESERIALIZE_INT(ColourDescriptionPresent);
    DESERIALIZE_INT(ColourPrimaries);
    DESERIALIZE_INT(TransferCharacteristics);
    DESERIALIZE_INT(MatrixCoefficients);

    return true;
}

void MFXStructureRef<mfxExtAVCRefListCtrl>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(NumRefIdxL0Active);
    SERIALIZE_INT(NumRefIdxL1Active);
}

bool MFXStructureRef<mfxExtAVCRefListCtrl>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(NumRefIdxL0Active);
    DESERIALIZE_INT(NumRefIdxL1Active);

    return true;
}

void MFXStructureRef <mfxExtVPPFrameRateConversion>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(Algorithm);
}

bool MFXStructureRef <mfxExtVPPFrameRateConversion>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(Algorithm);

    return true;
}

void MFXStructureRef <mfxExtCodingOptionSPSPPS>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(SPSId);
    SERIALIZE_INT(PPSId);
}

bool MFXStructureRef <mfxExtCodingOptionSPSPPS>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(SPSId);
    DESERIALIZE_INT(PPSId);

    return true;
}

void MFXStructureRef <mfxExtAvcTemporalLayers>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(BaseLayerPID);
}

bool MFXStructureRef <mfxExtAvcTemporalLayers>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(BaseLayerPID);

    return true;
}

void MFXStructureRef <mfxExtEncoderCapability>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(MBPerSec);
}

bool MFXStructureRef <mfxExtEncoderCapability>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(MBPerSec);

    return true;
}

void MFXStructureRef<mfxExtAVCEncodedFrameInfo>::ConstructValues() const 
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(FrameOrder);
    SERIALIZE_INT(PicStruct);
    SERIALIZE_INT(LongTermIdx);
}

bool MFXStructureRef<mfxExtAVCEncodedFrameInfo>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(FrameOrder);
    DESERIALIZE_INT(PicStruct);
    DESERIALIZE_INT(LongTermIdx);

    return true;
}

void MFXStructureRef <mfxExtHEVCParam>::ConstructValues() const
{
    SERIALIZE_INT(Header.BufferSz);
    SERIALIZE_INT(Header.BufferId);

    SERIALIZE_INT(PicWidthInLumaSamples);
    SERIALIZE_INT(PicHeightInLumaSamples);
}

bool MFXStructureRef <mfxExtHEVCParam>::DeSerialize(hash_array<std::string, std::string> values_map)
{
    DESERIALIZE_INT(Header.BufferSz);
    DESERIALIZE_INT(Header.BufferId);

    DESERIALIZE_INT(PicWidthInLumaSamples);
    DESERIALIZE_INT(PicHeightInLumaSamples);

    return true;
}

void MFXStructureRef <mfxExtBuffer>::ConstructValues() const {
    switch (m_pStruct->BufferId)
    {
        case MFX_EXTBUFF_CODING_OPTION :{
            SerializeStruct("ExtCO.", *(mfxExtCodingOption*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION2 :{
            SerializeStruct("ExtCO2.", *(mfxExtCodingOption2*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION3:{
            SerializeStruct("ExtCO3.", *(mfxExtCodingOption3*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_HEVC_PARAM :{
            SerializeStruct("HEVCPar.", *(mfxExtHEVCParam*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_HEVC_TILES :{
            SerializeStruct("HEVCTil.", *(mfxExtHEVCTiles*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_VP8_CODING_OPTION :{
            SerializeStruct("VP8.", *(mfxExtVP8CodingOption*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_MVC_SEQ_DESC : {
            SerializeStruct("MVC.", *(mfxExtMVCSeqDesc*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_VIDEO_SIGNAL_INFO : {
            SerializeStruct("VSIG.", *(mfxExtVideoSignalInfo*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION_SPSPPS : {
            SerializeStruct("ExtCOSPSPPS.", *(mfxExtCodingOptionSPSPPS*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_AVC_TEMPORAL_LAYERS : {
            SerializeStruct("AVCTL.", *(mfxExtAvcTemporalLayers*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_ENCODER_CAPABILITY :{
            SerializeStruct("CAP.", *(mfxExtEncoderCapability*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_ENCODED_FRAME_INFO :{
            SerializeStruct("ENC_FRAME_INFO.", *(mfxExtAVCEncodedFrameInfo*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION :{
            SerializeStruct("VPP_FRC.", *(mfxExtVPPFrameRateConversion*)m_pStruct);
            break;
        }
        default :
            //unsupported buffer
        break;
    }
}

bool MFXStructureRef <mfxExtBuffer>::DeSerialize(hash_array<std::string, std::string> values_map) {
    mfxU32 BufferId;

    DeSerializeSingleElement(values_map["Header.BufferId"], BufferId);

    switch (BufferId)
    {
        case MFX_EXTBUFF_CODING_OPTION :{
            DeSerializeStruct(values_map, "", *(mfxExtCodingOption*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION2 :{
            DeSerializeStruct(values_map, "", *(mfxExtCodingOption2*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION3:{
            DeSerializeStruct(values_map, "", *(mfxExtCodingOption3*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_HEVC_PARAM :{
            DeSerializeStruct(values_map, "", *(mfxExtHEVCParam*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_HEVC_TILES :{
            DeSerializeStruct(values_map, "", *(mfxExtHEVCTiles*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_VP8_CODING_OPTION :{
            DeSerializeStruct(values_map, "", *(mfxExtVP8CodingOption*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_MVC_SEQ_DESC : {
            DeSerializeStruct(values_map, "", *(mfxExtMVCSeqDesc*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_VIDEO_SIGNAL_INFO : {
            DeSerializeStruct(values_map, "", *(mfxExtVideoSignalInfo*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_CODING_OPTION_SPSPPS : {
            DeSerializeStruct(values_map, "", *(mfxExtCodingOptionSPSPPS*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_AVC_TEMPORAL_LAYERS : {
            DeSerializeStruct(values_map, "", *(mfxExtAvcTemporalLayers*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_ENCODER_CAPABILITY :{
            DeSerializeStruct(values_map, "", *(mfxExtEncoderCapability*)m_pStruct);
            break;
        }
            case MFX_EXTBUFF_ENCODED_FRAME_INFO :{
            DeSerializeStruct(values_map, "", *(mfxExtAVCEncodedFrameInfo*)m_pStruct);
            break;
        }
        case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION :{
            DeSerializeStruct(values_map, "", *(mfxExtVPPFrameRateConversion*)m_pStruct);
            break;
        }
        default :
            return false;
    }
    return true;
}
