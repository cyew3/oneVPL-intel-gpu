/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_dec_common.cpp

\* ****************************************************************************** */

#include "mfx_common.h"
#if defined MFX_ENABLE_MPEG2_VIDEO_BSD || defined MFX_ENABLE_MPEG2_VIDEO_DEC || defined MFX_ENABLE_MPEG2_VIDEO_DECODE

#include "mfx_mpeg2_dec_common.h"
#include "mfx_enc_common.h"

void ConvertBitStreamMfx2Umc(const mfxBitstream& mfxBs, UMC::Mpeg2Bitstream& umcBs)
{
    umcBs.bs_curr_ptr = mfxBs.Data + mfxBs.DataOffset;
    umcBs.bs_bit_offset = 0;
    umcBs.bs_start_ptr = mfxBs.Data;
    umcBs.bs_end_ptr = mfxBs.Data + mfxBs.DataOffset + mfxBs.DataLength;
}

void ConvertBitStreamUmc2Mfx(const UMC::Mpeg2Bitstream& umcBs, mfxBitstream& mfxBs)
{
    mfxBs.DataOffset = (mfxU32)(umcBs.bs_curr_ptr - umcBs.bs_start_ptr);
    mfxBs.DataLength =  (mfxU32)(umcBs.bs_end_ptr - umcBs.bs_curr_ptr);
}

mfxU8 GetMfxSecondFieldFlag(
    mfxU32 curPicStruct,
    mfxU32 prevFieldPicFlag,
    mfxU32 prevBottomFieldFlag,
    mfxU32 prevSecondFieldFlag)
{
    mfxU32 curFieldPicFlag = curPicStruct != FRAME_PICTURE;
    mfxU32 curBottomFieldFlag = curPicStruct == BOTTOM_FIELD;
    if (curFieldPicFlag && prevFieldPicFlag)
    {
        if (prevBottomFieldFlag == curBottomFieldFlag)
            return 0; // both are fields but of the same parity
        else
            return !prevSecondFieldFlag;
    }
    else
    {
        return 0; // one of them is a frame
    }
}

void AddPredAndResidual(
    mfxU8* dst,
    mfxU32 dstPitch,
    mfxU8* srcPred,
    mfxU32 srcPredPitch,
    mfxI16* srcRes,
    mfxU32 srcResPitch,
    mfxU32 roiWidth,
    mfxU32 roiHeight)
{
    for (mfxU32 y = 0; y < roiHeight; y++, dst += dstPitch, srcPred += srcPredPitch, srcRes += srcResPitch)
    {
        for (mfxU32 x = 0; x < roiWidth; x++)
        {
            mfxI16 sum = (mfxU16)(srcPred[x] + srcRes[x]);
            dst[x] = (mfxU8)(sum > 255 ? 255 : sum < 0 ? 0 : sum);
        }
    }
}

mfxMbCode* GetFirstMfxMbCode(const mfxFrameCUC &cuc)
{
    mfxU32 mbCnt = (cuc.FrameParam->MPEG2.FrameWinMbMinus1 + 1) *
        cuc.SliceParam[cuc.SliceId].MPEG2.FirstMbY +
        cuc.SliceParam[cuc.SliceId].MPEG2.FirstMbX;
    VM_ASSERT(mbCnt < cuc.MbParam->NumMbs);
    return &cuc.MbParam->Mb[mbCnt];
}

const mfxU32 numBlk[5] = { 4, 6, 8, 12 };
mfxI16* GetDctCoefPtr(mfxHDL pExtBufResCoef, mfxU32 mbX, mfxU32 mbY, const mfxFrameParam& par)
{
    mfxI16* res = (mfxI16 *)pExtBufResCoef;
    return &res[(mbY * (par.MPEG2.FrameWinMbMinus1 + 1) + mbX) * 64 * numBlk[par.MPEG2.ChromaFormatIdc]];
}

mfxU8 GetMfxFrameType(Ipp8u picType, Ipp32u /*picStruct*/)
{
    if (picType == 1 || picType == 4)
        return MFX_FRAMETYPE_I;
    else if (picType == 2)
        return MFX_FRAMETYPE_P;
    else // UMC::MPEG2_B_PICTURE
        return MFX_FRAMETYPE_B;
}

mfxU8 GetMfxNumRefFrame(Ipp8u frameType)
{
    return frameType == UMC::I_PICTURE ? 0 : frameType == UMC::P_PICTURE ? 1 : 2;
}

mfxU16 GetMfxBitStreamFcodes(Ipp8u* fCode)
{
    return (fCode[UMC::FCodeFwdX] << 12) | (fCode[UMC::FCodeFwdY] <<  8) |
           (fCode[UMC::FCodeBwdX] <<  4) | (fCode[UMC::FCodeBwdY]);
}

//mfxU16 GetMfxFrameRateCode(mfxU8 umcFrameRateCode)
//{
//    switch (umcFrameRateCode)
//    {
//    case 1: return MFX_FRAMERATE_239;
//    case 2: return MFX_FRAMERATE_24;
//    case 3: return MFX_FRAMERATE_25;
//    case 4: return MFX_FRAMERATE_299;
//    case 5: return MFX_FRAMERATE_30;
//    case 6: return MFX_FRAMERATE_50;
//    case 7: return MFX_FRAMERATE_599;
//    case 8: return MFX_FRAMERATE_60;
//    default: return MFX_FRAMERATE_UNKNOWN;
//    }
//}

void GetMfxFrameRate(mfxU8 umcFrameRateCode, mfxU32 *frameRateNom, mfxU32 *frameRateDenom)
{
    switch (umcFrameRateCode)
    {
        case 0: *frameRateNom = 30; *frameRateDenom = 1; break;
        case 1: *frameRateNom = 24000; *frameRateDenom = 1001; break;
        case 2: *frameRateNom = 24; *frameRateDenom = 1; break;
        case 3: *frameRateNom = 25; *frameRateDenom = 1; break;
        case 4: *frameRateNom = 30000; *frameRateDenom = 1001; break;
        case 5: *frameRateNom = 30; *frameRateDenom = 1; break;
        case 6: *frameRateNom = 50; *frameRateDenom = 1; break;
        case 7: *frameRateNom = 60000; *frameRateDenom = 1001; break;
        case 8: *frameRateNom = 60; *frameRateDenom = 1; break;
        default: *frameRateNom = 30; *frameRateDenom = 1;
    }
    return;
}

void Mpeg2SetConfigurableCommon(mfxVideoParam &par)
{
    memset(&par, 0, sizeof(par));
    par.mfx.CodecId = MFX_CODEC_MPEG2;
    par.mfx.FrameInfo.FourCC = 1;
    par.mfx.FrameInfo.Width = 1;
    par.mfx.FrameInfo.Height = 1;
    par.mfx.FrameInfo.CropX = 1;
    par.mfx.FrameInfo.CropY = 1;
    par.mfx.FrameInfo.CropW = 1;
    par.mfx.FrameInfo.CropH = 1;
    //par.mfx.FrameInfo.ScaleCWidth = 1;
   // par.mfx.FrameInfo.ScaleCHeight = 1;
    //par.mfx.FrameInfo.ByteOrderFlag = 0; only 8bit yuv for mpeg2
    //par.mfx.FrameInfo.DeltaCBitDepth = 0; only 8bit yuv for mpeg2
   // par.mfx.FrameInfo.DeltaCStep = 1;
    par.mfx.FrameInfo.AspectRatioH = 1;
    par.mfx.FrameInfo.AspectRatioW = 1;

    par.mfx.FrameInfo.FrameRateExtN = 1;
    par.mfx.FrameInfo.FrameRateExtD = 1;
    //par.mfx.FrameInfo.BitDepthLuma = 0; // only 8 bits for mpeg2
    //par.mfx.FrameInfo.BitDepthChroma = 0; // only 8 bits for mpeg2
    par.mfx.FrameInfo.PicStruct = 1;
  //  par.mfx.FrameInfo.Step = 1;
    par.mfx.FrameInfo.ChromaFormat = 1;
    //par.mfx.FrameInfo.NumFrameData = 0; // ?
    //par.mfx.CodecId = 0;
    par.mfx.CodecProfile = 1;
    par.mfx.CodecLevel = 1;
    par.IOPattern |= MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    //par.mfx.Log2MaxFrameOrder = 0; // H264 only
    //par.mfx.CodingLimits = 0; // encoders only
    //par.mfx.NumFrameSkipped = 0; // output parameter
    //par.mfx.ConstructNFrames = 0; // DECODE only
    //par.mfx.SkipOption = 1;
    //par.mfx.DecodedOrder = 0; // DECODE only
    //par.mfx.InitialDelayInKB = 0; // encoders only
    //par.mfx.BufferSizeInKB = 0; // encoders only
    //par.mfx.TargetKbps = 0; // encoders only
    //par.mfx.MaxKbps = 0; // encoders only
    //par.mfx.NumSlice = 0; // encoders only
    //par.mfx.NumThread; // DECODE only
}

void Mpeg2CheckConfigurableCommon(mfxVideoParam &par)
{
    par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    par.mfx.FrameInfo.Width = AlignValueSat(par.mfx.FrameInfo.Width, MFX_MB_WIDTH);
    par.mfx.FrameInfo.Height = AlignValueSat(par.mfx.FrameInfo.Height,
        par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ?
            MFX_MB_WIDTH :
            MFX_MB_WIDTH * 2);

    par.mfx.FrameInfo.Width =  IPP_MIN(par.mfx.FrameInfo.Width, MFX_MAX_WIDTH_HEIGHT);
    par.mfx.FrameInfo.Height =  IPP_MIN(par.mfx.FrameInfo.Height, MFX_MAX_WIDTH_HEIGHT);

    par.mfx.FrameInfo.CropW = IPP_MIN(par.mfx.FrameInfo.CropW, par.mfx.FrameInfo.Width);
    par.mfx.FrameInfo.CropW = IPP_MIN(par.mfx.FrameInfo.CropW, MFX_MPEG2_MAX_WIDTH);
    if (par.mfx.FrameInfo.CropW > 0 && IsAligned(par.mfx.FrameInfo.CropW, 0x1000))
        par.mfx.FrameInfo.CropW--;

    par.mfx.FrameInfo.CropH = IPP_MIN(par.mfx.FrameInfo.CropH, par.mfx.FrameInfo.Height);
    par.mfx.FrameInfo.CropH = IPP_MIN(par.mfx.FrameInfo.CropH, MFX_MPEG2_MAX_HEIGHT);
    if (par.mfx.FrameInfo.CropH > 0 && IsAligned(par.mfx.FrameInfo.CropH, 0x1000))
        par.mfx.FrameInfo.CropH--;

    par.mfx.FrameInfo.CropX = IPP_MIN(par.mfx.FrameInfo.CropX, par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropW);
    par.mfx.FrameInfo.CropY = IPP_MIN(par.mfx.FrameInfo.CropY, par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropH);

   // par.mfx.FrameInfo.ScaleCWidth = 1;
   // par.mfx.FrameInfo.ScaleCHeight = 1;
    //par.mfx.FrameInfo.DeltaCStep = 0;
    //par.mfx.FrameInfo.BitDepthLuma = 8;
    //par.mfx.FrameInfo.BitDepthChroma = 8;
    //par.mfx.FrameInfo.Step = 1;

    mfxU32 code, nom, den;
    code = TranslateMfxFRCodeMPEG2(&par.mfx.FrameInfo, &nom, &den);
    if (code == 0) { // error
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
    }

    //if (par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_239 &&
    //    par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_24 &&
    //    par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_25 &&
    //    par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_299 &&
    //    par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_30 &&
    //    par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_50 &&
    //    par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_599 &&
    //    par.mfx.FrameInfo.FrameRateCode != MFX_FRAMERATE_60)
    //    par.mfx.FrameInfo.FrameRateCode = MFX_FRAMERATE_UNKNOWN;

    //if (par.mfx.FrameInfo.FrameRateExtN > MFX_MPEG2_MAX_FR_EXT_N)
    //    par.mfx.FrameInfo.FrameRateExtN = 0;

    //if (par.mfx.FrameInfo.FrameRateExtD > MFX_MPEG2_MAX_FR_EXT_D)
    //    par.mfx.FrameInfo.FrameRateExtD = 0;

    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
        par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;

    if (par.mfx.CodecLevel != MFX_LEVEL_MPEG2_LOW &&
        par.mfx.CodecLevel != MFX_LEVEL_MPEG2_MAIN &&
        par.mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH1440 &&
        par.mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH)
        par.mfx.CodecLevel = MFX_LEVEL_UNKNOWN;

    if (par.mfx.CodecProfile != MFX_PROFILE_MPEG2_SIMPLE &&
        par.mfx.CodecProfile != MFX_PROFILE_MPEG2_MAIN &&
      /*par.mfx.CodecProfile != MFX_PROFILE_MPEG2_SNR &&
        par.mfx.CodecProfile != MFX_PROFILE_MPEG2_SPATIAL &&*/
#if !defined(MFX_VA_LINUX)
        par.mfx.CodecProfile != MFX_PROFILE_MPEG2_HIGH &&
#endif
        par.mfx.CodecProfile != MFX_PROFILE_MPEG1)
        par.mfx.CodecProfile = MFX_PROFILE_UNKNOWN;

   // par.mfx.SkipOption = MFX_SKIPMODE_NONE;
}

mfxU8* GetPlane(const mfxFrameSurface& fs, mfxU32 data, mfxU32 plane)
{
    const mfxFrameInfo& fi = fs.Info;
    const mfxFrameData& fd = *fs.Data[data];
    Ipp32s    ScaleCHeight = (fi.ChromaFormat == MFX_CHROMAFORMAT_YUV420+1)?1:0;
    Ipp32s    ScaleCWidth = (fi.ChromaFormat != MFX_CHROMAFORMAT_YUV444+1)?1:0;

    switch (plane)
    {
    case 0:
        return fd.Y + fd.Pitch * fi.CropY + fi.CropX;
    case 1:
        return fd.U + (fd.Pitch >> ScaleCWidth) * (fi.CropY >> ScaleCHeight) + (fi.CropX >> ScaleCWidth);
    case 2:
        return fd.V + (fd.Pitch >> ScaleCWidth) * (fi.CropY >> ScaleCHeight) + (fi.CropX >> ScaleCWidth);
    default:
        return 0;
    }
}

mfxU16 GetPitch(const mfxFrameSurface& fs, mfxU32 data, mfxU32 plane)
{
    Ipp16u    ScaleCPitch = (fs.Info.ChromaFormat != MFX_CHROMAFORMAT_YUV444+1)?1:0;
    switch (plane)
    {
    case 0:
        return fs.Data[data]->Pitch;
    case 1:
    case 2:
        return (fs.Data[data]->Pitch >> ScaleCPitch);
    default:
        return 0;
    }
}

#endif
