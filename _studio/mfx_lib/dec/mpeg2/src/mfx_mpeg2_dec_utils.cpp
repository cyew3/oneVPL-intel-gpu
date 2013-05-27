/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_dec_utils.cpp

\* ****************************************************************************** */
#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_DEC)

#include "mfx_mpeg2_dec_common.h"
#include "mfx_mpeg2_dec_utils.h"

namespace MfxMpeg2Dec
{

// can be shared between all
void ClearMfxStruct(mfxVideoParam *par)
{
    memset(par, 0, sizeof(mfxVideoParam));
}

// can be shared between all
void ClearMfxStruct(mfxFrameParam *par)
{
    memset(par, 0, sizeof(mfxFrameParam));
}

// can be shared between all
void ClearMfxStruct(mfxSliceParam *par)
{
    memset(par, 0, sizeof(mfxSliceParam));
}

// can be shared between Mpeg2DEC and Mpeg2BSD
mfxU8 GetUmcPicStruct(mfxU8 FieldPicFlag, mfxU8 BottomFieldFlag)
{
    return FieldPicFlag ? (BottomFieldFlag ? BOTTOM_FIELD : TOP_FIELD) : FRAME_PICTURE;
}

// can be shared between Mpeg2DEC and Mpeg2BSD
UMC::MPEG2FrameType GetUmcPicCodingType(mfxU8 FrameType, mfxU8 /*BottomFieldFlag*/)
{
    if ((FrameType & 0xf) == MFX_FRAMETYPE_I)
        return UMC::MPEG2_I_PICTURE;
    else if ((FrameType & 0xf) == MFX_FRAMETYPE_P)
        return UMC::MPEG2_P_PICTURE;
    else //(fieldType == MFX_FRAMETYPE_B)
        return UMC::MPEG2_B_PICTURE;
}

// can be shared between Mpeg2DEC and Mpeg2BSD
mfxU8 GetUmcTopFieldFirst(mfxU8 FieldPicFlag, mfxU8 BottomFieldFlag, mfxU8 SecondFieldFlag)
{
    return FieldPicFlag ? (BottomFieldFlag == SecondFieldFlag ? 1 : 0) : 0;
}

// can be shared between Mpeg2DEC and Mpeg2BSD
mfxU8 GetUmcDctType(mfxU8 FieldPicFlag, mfxU8 TransformFlag)
{
    return FieldPicFlag ? 2 : TransformFlag;
}

inline mfxU32 Pack4BitsTo1Bit(mfxU32 pattern, mfxU8 bit)
{
    pattern >>= 4 * bit;
    return (pattern & 0x0f) ? (1 << bit) : 0;
}

// can be shared between Mpeg2DEC and Mpeg2BSD
mfxU16 GetUmcCodedBlockPattern(mfxU8 ChromaFormatIdc, mfxU16 cp4x4Y, mfxU16 cp4x4U, mfxU16 cp4x4V)
{
    mfxU32 chromaBlk = 0;
    if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV420)
        chromaBlk = 1;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV422)
        chromaBlk = 2;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV444)
        chromaBlk = 4;

    mfxU8 i;
    mfxU16 res = 0;
    for (i = 0; i < 4; i++)
        res |= Pack4BitsTo1Bit(cp4x4Y, i);

    res <<= chromaBlk;
    for (i = 0; i < chromaBlk; i++)
        res |= Pack4BitsTo1Bit(cp4x4U, i);

    res <<= chromaBlk;
    for (i = 0; i < chromaBlk; i++)
        res |= Pack4BitsTo1Bit(cp4x4V, i);
    return res;
}

// can be shared between Mpeg2DEC and Mpeg2BSD
mfxU8 GetUmcMbType(mfxU8 MbType5Bits, mfxU8 MbSkipFlag)
{
    if (!MbSkipFlag && (MbType5Bits == MPEG2_Intra || MbType5Bits == MPEG2_Intra_Field))
        return IPPVC_MB_INTRA;
    else if (MbType5Bits == B_16x16_Bi || MbType5Bits == B_16x8_Field_L2L2)
        return IPPVC_MB_FORWARD | IPPVC_MB_BACKWARD;
    else if (MbType5Bits == B_16x16_L1 || MbType5Bits == B_16x8_Field_L1L1)
        return IPPVC_MB_BACKWARD;
    else if (MbType5Bits == PB_16x16_L0 || MbType5Bits == PB_16x8_Field_L0L0)
        return IPPVC_MB_FORWARD;
    return 0;
}

// can be shared between Mpeg2DEC and Mpeg2BSD
mfxU8 GetUmcMotionType(mfxU8 MbType5Bits, mfxU8 FieldPicFlag)
{
    if (MbType5Bits == MPEG2_Intra || MbType5Bits == MPEG2_Intra_Field)
        return 0;
    else if (MbType5Bits == P_DualPrime)
        return IPPVC_MC_DP;
    else if (MbType5Bits == B_16x16_Bi || MbType5Bits == B_16x16_L1 || MbType5Bits == PB_16x16_L0)
        return FieldPicFlag ? IPPVC_MC_FIELD : IPPVC_MC_FRAME;
    else if (MbType5Bits == B_16x8_Field_L2L2 || MbType5Bits == B_16x8_Field_L1L1 || MbType5Bits == PB_16x8_Field_L0L0)
        return FieldPicFlag ? IPPVC_MC_16X8 : IPPVC_MC_FIELD;
    return 0;
}

void InitFirstRefFrame(const Ipp8u* ref[3], const mfxFrameCUC& cuc)
{
    ref[0] = ref[1] = ref[2] = 0;
    if (cuc.FrameParam->MPEG2.RefFrameListP[0] != 0xFF)
    {
        ref[0] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.RefFrameListP[0], 0);
        ref[1] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.RefFrameListP[0], 1);
        ref[2] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.RefFrameListP[0], 2);
    }
}

void InitSecondRefFrame(const Ipp8u* ref[3], const mfxFrameCUC& cuc)
{
    ref[0] = ref[1] = ref[2] = 0;
    if (!cuc.FrameParam->MPEG2.BackwardPredFlag)
    {
        ref[0] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.CurrFrameLabel, 0);
        ref[1] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.CurrFrameLabel, 1);
        ref[2] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.CurrFrameLabel, 2);
    }
    else if (cuc.FrameParam->MPEG2.RefFrameListB[1][0] != 0xFF)
    {
        ref[0] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.RefFrameListB[1][0], 0);
        ref[1] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.RefFrameListB[1][0], 1);
        ref[2] = GetPlane(*cuc.FrameSurface, cuc.FrameParam->MPEG2.RefFrameListB[1][0], 2);
    }
}

}

#endif
