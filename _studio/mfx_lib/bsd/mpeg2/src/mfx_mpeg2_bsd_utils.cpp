/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_bsd_utils.cpp

\* ****************************************************************************** */

#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_BSD)

#include "mfx_mpeg2_dec_common.h"
#include "mfx_mpeg2_bsd_utils.h"

namespace MfxMpeg2Bsd
{

void ClearMfxStruct(mfxVideoParam *par)
{
    memset(par, 0, sizeof(mfxVideoParam));
}

void ClearMfxStruct(mfxFrameParam *par)
{
    memset(par, 0, sizeof(mfxFrameParam));
}

void ClearMfxStruct(mfxSliceParam *par)
{
    memset(par, 0, sizeof(mfxSliceParam));
}

mfxU8 GetMfxSliceTypeType(mfxU8 frameType)
{
    if (frameType == UMC::MPEG2_I_PICTURE)
        return MFX_SLICETYPE_I;
    else if (frameType == UMC::MPEG2_P_PICTURE)
        return MFX_SLICETYPE_P;
    else // UMC::MPEG2_B_PICTURE
        return MFX_SLICETYPE_B;
}

mfxU8 GetMfxFieldMbFlag(mfxU8 picStruct, mfxU8 motionType)
{
    return (picStruct == FRAME_PICTURE && motionType == IPPVC_MC_FRAME) ? 0 : 1;
}

mfxU16 GetMfxMbDataOffset32b(mfxHDL ptr, mfxHDL base)
{
    return (mfxU16)(((mfxU32)((mfxU8 *)ptr - (mfxU8 *)base) + MFX_MB_DATA_OFFSET_ALIGNMENT - 1) /
        MFX_MB_DATA_OFFSET_ALIGNMENT);
}

mfxU16 Expand4BitTo16Bit(mfxU32 val4bit)
{
    mfxU16 res = 0;
    for (mfxU32 i = 0; i < 4; i++)
        res |= (((val4bit >> i) & 1) ? 0xf : 0x0) << (4 * i);
    return res;
}

mfxU16 GetMfxCodedPattern4x4Y(mfxU32 umcCodedPattern, mfxU8 ChromaFormatIdc)
{
    if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV420)
        umcCodedPattern = (umcCodedPattern >> 2) & 0xf;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV422)
        umcCodedPattern = (umcCodedPattern >> 4) & 0xf;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV444)
        umcCodedPattern = (umcCodedPattern >> 8) & 0xf;
    return Expand4BitTo16Bit(umcCodedPattern);
}

mfxU16 GetMfxCodedPattern4x4U(mfxU32 umcCodedPattern, mfxU8 ChromaFormatIdc)
{
    if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV420)
        umcCodedPattern = (umcCodedPattern >> 1) & 0x1;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV422)
        umcCodedPattern = (umcCodedPattern >> 2) & 0x3;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV444)
        umcCodedPattern = (umcCodedPattern >> 4) & 0xf;
    return Expand4BitTo16Bit(umcCodedPattern);
}

mfxU16 GetMfxCodedPattern4x4V(mfxU32 umcCodedPattern, mfxU8 ChromaFormatIdc)
{
    if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV420)
        umcCodedPattern = umcCodedPattern & 0x1;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV422)
        umcCodedPattern = umcCodedPattern & 0x3;
    else if (ChromaFormatIdc == MFX_CHROMAFORMAT_YUV444)
        umcCodedPattern = umcCodedPattern & 0xf;
    return Expand4BitTo16Bit(umcCodedPattern);
}

static const mfxU8 MV_COUNT[2][4] = {
    { 0, 2, 1, 1 }, // frame motion type
    { 0, 1, 2, 1 }  // field motion type
};

mfxU8 GetMfxMvQuantity(mfxU8 picStruct, mfxU8 mbType, mfxU8 motionType)
{
    VM_ASSERT(motionType < 4);
    mfxU8 mvQuantity = 0;
    mvQuantity += (mbType & IPPVC_MB_FORWARD) ? 1 : 0;
    mvQuantity += (mbType & IPPVC_MB_BACKWARD) ? 1 : 0;
    return mvQuantity * MV_COUNT[picStruct == FRAME_PICTURE ? 0 : 1][motionType];
}

mfxU16 GetMfxMbType5bits(mfxU8 picStruct, Ipp8u mbType, Ipp8u motionType)
{
    if (mbType & IPPVC_MB_INTRA)
    {
        return MPEG2_Intra;
    }
    else if (motionType == IPPVC_MC_DP)
    {
        return P_DualPrime;
    }
    else if ((mbType & IPPVC_MB_FORWARD) && (mbType & IPPVC_MB_BACKWARD))
    {
        if (picStruct != FRAME_PICTURE)
            return (mfxU16)(motionType == IPPVC_MC_16X8 ? B_16x8_Field_L2L2 : B_16x16_Bi);
        else
            return (mfxU16)(motionType == IPPVC_MC_FIELD ? B_16x8_Field_L2L2 : B_16x16_Bi);
    }
    else if (mbType & IPPVC_MB_BACKWARD)
    {
        if (picStruct != FRAME_PICTURE)
            return (mfxU16)(motionType == IPPVC_MC_16X8 ? B_16x8_Field_L1L1 : B_16x16_L1);
        else
            return (mfxU16)(motionType == IPPVC_MC_FIELD ? B_16x8_Field_L1L1 : B_16x16_L1);
    }
    else //if (mbType & IPPVC_MB_FORWARD)
    {
        if (picStruct != FRAME_PICTURE)
            return (mfxU16)(motionType == IPPVC_MC_16X8 ? PB_16x8_Field_L0L0 : PB_16x16_L0);
        else
            return (mfxU16)(motionType == IPPVC_MC_FIELD ? PB_16x8_Field_L0L0 : PB_16x16_L0);
    }
}

}

#endif
