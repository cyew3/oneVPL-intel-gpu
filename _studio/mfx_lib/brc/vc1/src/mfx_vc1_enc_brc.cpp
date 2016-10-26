//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VC1_VIDEO_BRC)
#include "mfxvideo.h"
#include "mfx_enc_common.h"
#include "mfx_vc1_enc_brc.h"
//#include "mfx_vc1_enc_ex_param_buf.h"

#if 0
static mfxU32 bMax_LevelLimits[3][5] =
{
  {   /* Simple Profile */
    20,     /* Low    Level */
    77,     /* Medium Level */
    0,      /* High Level */
    0,
    0
  },
  {   /* Main Profile */
    306,        /* Low    Level */
    611,        /* Medium Level */
    2442,       /* High Level */
    0,
    0
  },
  {   /* Advanced Profile */
    250,        /*L0 level*/
    1250,       /*L1 level*/
    2500,       /*L2 level*/
    5500,       /*L3 level*/
    16500       /*L4 level*/
  }
};


static mfxU32 rMax_LevelLimits[3][5] =
{
  {   /* Simple Profile */
    96000,      /* Low    Level */
    384000,     /* Medium Level */
    0,          /* High Level */
    0,
    0
  },
  {   /* Main Profile */
    2000000,        /* Low    Level */
    10000000,       /* Medium Level */
    20000000,       /* High Level */
    0,
    0
  },
  {   /* Advanced Profile */
    2000000,        /*L0 level*/
    10000000,       /*L1 level*/
    20000000,       /*L2 level*/
    45000000,       /*L3 level*/
    135000000       /*L4 level*/
  }
};
#endif

MFXVideoRcVc1::~MFXVideoRcVc1(void)
{
    Close();
}


mfxStatus MFXVideoRcVc1::Query(mfxVideoParam *in, mfxVideoParam *out)
{
  MFX_CHECK_NULL_PTR1(out)
  if (in == 0) {
    memset(&out->mfx, 0, sizeof(mfxInfoMFX));
    out->mfx.FrameInfo.FourCC = 1;
    out->mfx.FrameInfo.Width = 1;
    out->mfx.FrameInfo.Height = 1;
    out->mfx.FrameInfo.CropW = 1;
    out->mfx.FrameInfo.CropH = 1;
    out->mfx.FrameInfo.PicStruct = 1;

    out->mfx.GopOptFlag = 1;
    out->mfx.GopPicSize = 1;
    out->mfx.GopRefDist = 1;
    out->mfx.RateControlMethod = 1;
    out->mfx.InitialDelayInKB = 1;
    out->mfx.BufferSizeInKB = 1;
    out->mfx.TargetKbps = 1;
    out->mfx.MaxKbps = 1;
    out->mfx.FrameInfo.ChromaFormat = 1;

//    out->mfx.NumSlice = 1; // ???
//    out->mfx.NumThread = 1; ???
  } else {
    *out = *in;
    out->mfx.FrameInfo.FourCC = MFX_CODEC_VC1;

    out->mfx.GopOptFlag &= (MFX_GOP_CLOSED | MFX_GOP_STRICT);


    if (out->mfx.GopPicSize == 0)
      out->mfx.GopPicSize = CalculateUMCGOPLength(out->mfx.GopPicSize,out->mfx.TargetUsage& 0x0f);

    if (out->mfx.GopRefDist == 0)
      out->mfx.GopRefDist = 1;

    if (out->mfx.RateControlMethod != MFX_RATECONTROL_VBR)
      out->mfx.RateControlMethod = MFX_RATECONTROL_CBR;

    if (out->mfx.RateControlMethod == MFX_RATECONTROL_VBR && out->mfx.MaxKbps < out->mfx.TargetKbps)
      out->mfx.MaxKbps = out->mfx.TargetKbps;

    if (out->mfx.TargetKbps) {
      mFramerate = CalculateUMCFramerate(out->mfx.FrameInfo.FrameRateExtN, out->mfx.FrameInfo.FrameRateExtD);

      if (out->mfx.BufferSizeInKB && out->mfx.BufferSizeInKB < (mfxU32)(out->mfx.TargetKbps * 8 / mFramerate) << 2)
        out->mfx.BufferSizeInKB = (mfxU32)(out->mfx.TargetKbps * 8 / mFramerate) << 2;

      if (out->mfx.BufferSizeInKB && out->mfx.InitialDelayInKB < (mfxU32)(out->mfx.TargetKbps * 8 / mFramerate) << 1)
        out->mfx.InitialDelayInKB = (mfxU32)(out->mfx.TargetKbps * 8 / mFramerate) << 1;

      if (out->mfx.InitialDelayInKB > out->mfx.BufferSizeInKB)
        out->mfx.InitialDelayInKB = out->mfx.BufferSizeInKB;
    }


    if ((out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 + 1) && (out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 + 1) &&
      (out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444 + 1))
      out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420 + 1;

    out->mfx.FrameInfo.PicStruct &= MFX_PICSTRUCT_PROGRESSIVE |
      MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF;
    if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
      out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
      out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
      out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  }

  return MFX_ERR_NONE;

}

mfxStatus MFXVideoRcVc1::Init(mfxVideoParam *par)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxU32          memSize = 0;
//    mfxI32          profile; // = UMC_VC1_ENCODER::VC1_ENC_PROFILE_A;
//    mfxI32          level; //   = UMC_VC1_ENCODER::VC1_ENC_LEVEL_4;
    mfxU32          yuvFSize;
    mfxU32          AverageCompression;
    mfxI32          w, h;

    mBitrate = CalculateUMCBitrate(par->mfx.TargetKbps);
    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED; // ??? need invalid param status ?

    mFrameCompleted = 1;

    mIPsize = VC1_P_SIZE;
    mIBsize = VC1_B_SIZE;
    mFailPQuant = 0;
    mFailBQuant = 0;
    mAverageIQuant = 0;

    mPQIndex = 0;
    mHalfPQ = 1;

    mIQuant = 1;
    mPQuant = 1;
    mBQuant = 1;
    mLimIQuant = 1;
    mLimPQuant = 1;
    mLimBQuant = 1;
    mIHalf = 0;
    mPHalf = 0;
    mBHalf = 0;
    mSizeAberration = 0;

    mRCMode = par->mfx.RateControlMethod;

    if(!m_pMFXCore)
       return MFX_ERR_MEMORY_ALLOC;

    mGOPLength = CalculateUMCGOPLength(par->mfx.GopPicSize,par->mfx.TargetUsage & 0x0F);
    mBFrLength = CalculateMAXBFrames(par->mfx.GopRefDist);

    w = par->mfx.FrameInfo.CropW ? par->mfx.FrameInfo.CropW : par->mfx.FrameInfo.Width;
    h = par->mfx.FrameInfo.CropH ? par->mfx.FrameInfo.CropH : par->mfx.FrameInfo.Height;
    yuvFSize = (w * h * 3)/2 * 8;
    if (yuvFSize <= 0) {
      mBitrate = 0; // BRC funcs will do nothing
      return MFX_ERR_UNSUPPORTED; // ??? need invalid param status ?
    }

//    profile = par->mfx.CodecProfile;
//    level = par->mfx.CodecLevel;

/*
    mProfile = mLevel = 0;
    if (profile == MFX_PROFILE_VC1_ADVANCED) {
      mProfile = 2;
      if (level == MFX_LEVEL_VC1_4) mLevel = 4;
      else if (level == MFX_LEVEL_VC1_3) mLevel = 3;
      else if (level == MFX_LEVEL_VC1_2) mLevel = 2;
      else if (level == MFX_LEVEL_VC1_1) mLevel = 1;
    } else {
      if (profile == MFX_PROFILE_VC1_MAIN)
        mProfile = 1;
      if (level == MFX_LEVEL_VC1_HIGH)      mLevel = 2;
      else if (level == MFX_LEVEL_VC1_MEDIAN) mLevel = 1;
    }
*/
    //Init BRC
    mFramerate = CalculateUMCFramerate(par->mfx.FrameInfo.FrameRateExtN, par->mfx.FrameInfo.FrameRateExtD);

//    if (mBitrate > rMax_LevelLimits[mProfile][mLevel]) ???
//      mBitrate = rMax_LevelLimits[mProfile][mLevel];


//    if (mBitrate == 0)
//      mBitrate = 2*1024000;
    if (mFramerate == 0)
      mFramerate = 30;

    if (mRCMode == MFX_RATECONTROL_VBR) {
      mHRD.maxBitrate = CalculateUMCBitrate(par->mfx.MaxKbps);
      if (mHRD.maxBitrate < mBitrate)
        mHRD.maxBitrate = mBitrate;
    } else // CBR
      mHRD.maxBitrate = mBitrate;

    // check max values fro profile/level - TODO ???


    mIdealFrameSize = (mfxU32)(mBitrate/mFramerate);

    SetGOPParams();
//    m_encMode   = mode;
//    m_QuantMode = QuantMode;

    AverageCompression = ((yuvFSize)/mNeedISize)/3;
    vc1_Clip(AverageCompression, VC1_MIN_QUANT, VC1_MAX_QUANT);
    mPQIndex = mIQuant = (mfxU8)AverageCompression;
    AverageCompression = ((yuvFSize)/mNeedPSize)/5;
    vc1_Clip(AverageCompression, VC1_MIN_QUANT, VC1_MAX_QUANT);
    mPQuant =  (mfxU8)AverageCompression;
    if(mPQuant > 2)
      mPQuant -= 2;

    AverageCompression = ((yuvFSize)/mNeedBSize)/6;
    vc1_Clip(AverageCompression, VC1_MIN_QUANT, VC1_MAX_QUANT);

    mBQuant = (mfxU8)AverageCompression;

    if(mBQuant > 2)
      mBQuant -= 2;


    InitHRD(par);

//    MFXSts = GetUMCProfile(par->mfx.CodecProfile, &profile);
//    MFX_CHECK_STS(MFXSts);
//    MFXSts = GetUMCLevel(par->mfx.CodecProfile, par->mfx.CodecLevel, &level);
//    MFX_CHECK_STS(MFXSts);
    //Init HRD
//    err = m_pBitRateControl->InitBuffer(profile, level,
//        CalculateUMC_HRD_BufferSize(par->mfx.BufferSizeInKB),
//        CalculateUMC_HRD_InitDelay(par->mfx.InitialDelayInKB));
//    UMC_MFX_CHECK_STS(err);

    return MFXSts;
}

mfxStatus MFXVideoRcVc1::InitHRD(mfxVideoParam *par)
{
  mfxStatus MFXSts = MFX_ERR_NONE;
//  mfxI32 profile, level, i = 0, j = 0;

  mHRD.bufSize = par->mfx.BufferSizeInKB * MFX_BIT_IN_KB;
  if (mHRD.bufSize == 0)
    return MFX_ERR_UNSUPPORTED;
  mHRD.bufFullness = par->mfx.InitialDelayInKB * MFX_BIT_IN_KB;
  mHRD.inputBitsPerFrame = mBitrate / mFramerate;
  mHRD.maxInputBitsPerFrame = mHRD.maxBitrate / mFramerate;
  mHRD.frameCnt = 1;
  mHRD.prevFrameCnt = 0;

  if (mHRD.bufSize && (mHRD.bufSize < mHRD.inputBitsPerFrame << 2))
    mHRD.bufSize = mHRD.inputBitsPerFrame << 2;
  if (mHRD.bufSize && (mHRD.bufFullness < mHRD.inputBitsPerFrame << 1))
    mHRD.bufFullness = mHRD.inputBitsPerFrame << 1;

  if (mHRD.bufFullness > mHRD.bufSize)
    mHRD.bufFullness = mHRD.bufSize;


/*
  profile = par->mfx.CodecProfile;
  level = par->mfx.CodecLevel;

  if (profile == MFX_PROFILE_VC1_ADVANCED) {
    j = 2;
    if (level == MFX_LEVEL_VC1_4) i = 4;
    else if (level == MFX_LEVEL_VC1_3) i = 3;
    else if (level == MFX_LEVEL_VC1_2) i = 2;
    else if (level == MFX_LEVEL_VC1_1) i = 1;
  } else {
    if (profile == MFX_PROFILE_VC1_MAIN)
      j = 1;
    if (level == MFX_LEVEL_VC1_HIGH)      i = 2;
    else if (level == MFX_LEVEL_VC1_MEDIAN) i = 1;
  }
*/

  // check max values for profile/level - TODO ???
  return MFXSts;
}


mfxI32 MFXVideoRcVc1::UpdateAndCheckHRD(mfxI32 frameBits)
{
  mfxI32 ret = VC1_BRC_OK;
  mfxI32 bufFullness;

  if (mHRD.frameCnt != mHRD.prevFrameCnt) {
    mHRD.prevBufFullness = mHRD.bufFullness;
    mHRD.prevFrameCnt = mHRD.frameCnt;
  } else { // frame is being recoded - restore buffer state
    mHRD.bufFullness = mHRD.prevBufFullness;
  }

  mHRD.maxFrameSize = mHRD.bufFullness;
  mHRD.minFrameSize = (mRCMode == MFX_RATECONTROL_VBR ? 0 : mHRD.bufFullness + mHRD.inputBitsPerFrame - mHRD.bufSize);
  if (mHRD.minFrameSize < 0)
    mHRD.minFrameSize = 0;

  bufFullness = mHRD.bufFullness - frameBits;

  if (bufFullness < 0) {
    bufFullness = 0;
    ret = VC1_BRC_ERR_BIG_FRAME;
    bufFullness += mHRD.maxInputBitsPerFrame; // maxInputBitsPerFrame = inputBitsPerFrame in case of CBR
    if (bufFullness > mHRD.bufSize) // possible in VBR mode if at all (???)
      bufFullness = mHRD.bufSize;
  } else {
    bufFullness += mHRD.maxInputBitsPerFrame;
    if (bufFullness > mHRD.bufSize) {
      bufFullness =  mHRD.bufSize;
      if (mRCMode != MFX_RATECONTROL_VBR)
        ret = VC1_BRC_ERR_SMALL_FRAME;
    }
  }
  mHRD.bufFullness = bufFullness;

  return ret;
}


mfxStatus MFXVideoRcVc1::Reset(mfxVideoParam *par)
{
  mfxStatus MFXSts;
  Close();
  MFXSts = Init(par);
  return MFXSts;
}


mfxStatus  MFXVideoRcVc1::Close(void)
 {
    mfxStatus       MFXSts = MFX_ERR_NONE;

    if(m_pBRCBuffer)
    {
        m_pMFXCore->UnlockBuffer(m_BRCID);
        m_pBRCBuffer = NULL;
    }
    m_BRCID = 0;

    return MFXSts;
 }


mfxStatus MFXVideoRcVc1::FrameEncUpdate(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    mPrevDataLength = cuc->Bitstream->DataLength;

    if (mHRD.frameCnt != cuc->FrameCnt && !mFrameCompleted) // BRC reported buffer over/underflow but the application ignored it
      CompleteFrame(mPicType, mFrameSize); // use prev frame type here

    mPicType = GetPicType(cuc->FrameParam);

    if (mPicType & MFX_FRAMETYPE_B) {
      mPQIndex = mBQuant  + mFailBQuant;
      mHalfPQ  = mBHalf;
      if(mPoorRefFrame) { //correct quant for next B frame
        if(mPQIndex > 9) {
          mPQIndex--;
          mHalfPQ = 1;
          mBQuant--;
          mBHalf = 0;
        } else {
          mHalfPQ = 0;
          mBHalf = 0;
        }
      }
      vc1_Clip(mPQIndex, VC1_MIN_QUANT, VC1_MAX_QUANT);
      vc1_ClipL(mPQIndex, mLimPQuant);
    } else if (mPicType & MFX_FRAMETYPE_P) {
      mPQIndex = mPQuant + mFailPQuant;
      mHalfPQ  = mPHalf;

      if (mPoorRefFrame & 0x1)
       {//correct quant for next B frame
        if (mPQIndex > 9) {
          mPQIndex--;
          mHalfPQ = 1;
          mPQuant--;
          mPHalf = 0;
        } else {
          mHalfPQ = 0;
          mPHalf = 0;
        }
      }
      vc1_Clip(mPQIndex, VC1_MIN_QUANT, VC1_MAX_QUANT);
      vc1_ClipL(mPQIndex, mLimPQuant);
    } else { // I frame or 2 I fields // if MFX_FRAMETYPE_S, quant doesn't matter (???)
      mPQIndex = mIQuant;
      mHalfPQ  = mIHalf;
      vc1_Clip(mPQIndex, VC1_MIN_QUANT, VC1_MAX_QUANT);
    }

    if(mPQIndex > 8)
      mHalfPQ  = 0;

//mPQIndex = 10;
//mHalfPQ = 0;

    cuc->FrameParam->VC1.PQuant = mPQIndex;
    cuc->FrameParam->VC1.HalfQP = mHalfPQ;

    SetUniformQPs(cuc, 2*mPQIndex + mHalfPQ, cuc->FrameParam->VC1.PQuantUniform); // ??? PQuantUniform tmp

    return MFXSts;
}

mfxStatus MFXVideoRcVc1::FramePakRefine(mfxFrameCUC *cuc)
{
  return FrameEncUpdate(cuc);
}



mfxStatus MFXVideoRcVc1::FramePakRecord(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxI32 sts;
    mfxU32 frameSizeBytes;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    if (mHRD.frameCnt != cuc->FrameCnt && !mFrameCompleted) // BRC reported buffer over/underflow but the application ignored it
      CompleteFrame(mPicType, mFrameSize);

    mHRD.frameCnt = cuc->FrameCnt;
    mPicType = GetPicType(cuc->FrameParam);
    frameSizeBytes = cuc->Bitstream->DataLength - mPrevDataLength;
    mFrameSize = frameSizeBytes << 3;

    sts = CheckFrameCompression(mPicType, mFrameSize);
    if (mHRD.bufSize) {
      cuc->FrameParam->VC1.MaxFrameSize = mHRD.maxFrameSize >> 3;
      cuc->FrameParam->VC1.MinFrameSize = (mHRD.minFrameSize + 7) >> 3;
    } else {
      cuc->FrameParam->VC1.MaxFrameSize = 0xFFFFFFFF;
      cuc->FrameParam->VC1.MinFrameSize = 0;
    }

    if (frameSizeBytes > cuc->FrameParam->VC1.MaxFrameSize || frameSizeBytes < cuc->FrameParam->VC1.MinFrameSize)
      mFrameCompleted = 0;
    else
      CompleteFrame(mPicType, mFrameSize);

    return MFXSts;
}

mfxStatus MFXVideoRcVc1::SliceEncUpdate(mfxFrameCUC *cuc){
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcVc1::SlicePakRefine(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcVc1::SlicePakRecord(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}


mfxU16 MFXVideoRcVc1::GetPicType(mfxFrameParam *frameParam)
{
  mfxU16 picType = frameParam->VC1.FrameType;
  if (frameParam->VC1.FieldPicFlag)
     picType |= frameParam->VC1.FrameType2nd;
  return picType;
}

mfxStatus MFXVideoRcVc1::SetUniformQPs(mfxFrameCUC *cuc, mfxU8 qp2, mfxU8 uniformQP)
{
  mfxStatus       MFXSts = MFX_ERR_NONE;
  int i;
  //  MFX_CHECK_COND(cuc->FrameParam->NumMb == cuc->NumMb);
  //  MFX_CHECK_COND(cuc->MbParam->NumMb == cuc->FrameParam->NumMb);

  for (i = 0; i < cuc->MbParam->NumMb; i++) {
    cuc->MbParam->Mb[i].VC1.QpScaleType = uniformQP;
    cuc->MbParam->Mb[i].VC1.QpScaleCode = qp2;
  }
  return MFXSts;
}


mfxI32 MFXVideoRcVc1::CheckFrameCompression(mfxU16 picType, mfxI32 currSize)
{
  mfxI32 curQuant;
  mfxI32 Sts = VC1_BRC_OK;
  mfxU8 HalfQP = 0;
  mfxI32 requiredSize;
  mfxF32 ratio = 0;

  if (mHRD.bufSize)
    Sts = UpdateAndCheckHRD(currSize);

  if (picType == MFX_FRAMETYPE_S) { // ???
    return Sts;
  }

  if (picType & MFX_FRAMETYPE_B) {
    requiredSize = mNeedBSize;
  } else if (picType & MFX_FRAMETYPE_P) {
    requiredSize = mNeedPSize;
  } else {
    requiredSize = mNeedISize;
  }

  if (Sts == VC1_BRC_ERR_BIG_FRAME && (requiredSize > mHRD.maxFrameSize))
    requiredSize = mHRD.maxFrameSize;
  else if (Sts == VC1_BRC_ERR_SMALL_FRAME && (requiredSize < mHRD.minFrameSize))
    requiredSize = mHRD.minFrameSize;

  ratio = currSize / (mfxF32)requiredSize;

  if ((picType & MFX_FRAMETYPE_B) == 0) {
    if (ratio < VC1_POOR_REF_FRAME)
      mPoorRefFrame = ((mPoorRefFrame << 1) + 1) & 0x3;
    else
      mPoorRefFrame = (mPoorRefFrame << 1) & 0x3;
  }

//   if(m_failGOP)
//    HRDSts = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

//  CheckFrame_QualityMode(picType, ratio, HRDSts);
//  m_recoding++;


  //new quant calculation
  curQuant = (mfxI32)(ratio * mPQIndex + 0.5);
  vc1_Clip(curQuant, VC1_MIN_QUANT, VC1_MAX_QUANT);

  //check compression
  if ((ratio <= VC1_BUFFER_OVERFLOW) && (ratio >= VC1_GOOD_COMPRESSION))
    Sts |= VC1_BRC_OK;       //good frame
  else if (ratio > VC1_BUFFER_OVERFLOW)
    Sts |= VC1_BRC_NOT_ENOUGH_BUFFER; //could be critical situation, if HRD buffer small
  else if (ratio <= VC1_RATIO_MIN) {
    //very small frame
    Sts |= VC1_BRC_ERR_SMALL_FRAME;
    if (mPQIndex > VC1_MIN_QUANT)
      HalfQP = 1;
  } else if (ratio >= VC1_MIN_RATIO_HALF) {
    if ((mPQIndex > VC1_MIN_QUANT) && (curQuant > mPQIndex))
      HalfQP = 1;
    Sts |= VC1_BRC_SMALL_FRAME;
  } else
    Sts |= VC1_BRC_SMALL_FRAME;

  //check HRD status
/*
  if (HRDSts ==  UMC::UMC_ERR_NOT_ENOUGH_BUFFER)
  {
    Sts |= VC1_BRC_NOT_ENOUGH_BUFFER;
    HalfQP = 0;
  }
*/

  if(Sts == VC1_BRC_OK) {
    return Sts;
  }

  if (picType & MFX_FRAMETYPE_B) {
    if (Sts & (VC1_BRC_NOT_ENOUGH_BUFFER | VC1_BRC_ERR_BIG_FRAME)) {
      if (ratio > VC1_RATIO_ILIM || (Sts & VC1_BRC_ERR_BIG_FRAME)) { // ??? VC1_RATIO_BLIM ?
        mFailBQuant   = ((mBQuant + curQuant + 1) >> 1) - mBQuant;
        mBQuant++;
        mLimBQuant = mPQIndex + 1;
      } else {
        mBQuant = (mBQuant + curQuant + 1) >> 1;

        if(mBQuant <= mPQIndex) {
          if (mPQIndex > 9) {
            if(mBQuant <= mPQIndex)
              mBQuant++;
          } else {
            if (!mHalfPQ)
              HalfQP = 1;
            else {
              HalfQP = 0;
              if(mBQuant <= mPQIndex)
                mBQuant++;
            }
          }
        }
      }
    } else if (Sts & VC1_BRC_ERR_SMALL_FRAME) { // ???
      mBQuant = (mBQuant + curQuant + 1) >> 1;
      mLimBQuant--;

      if (mPQIndex > 9) {
        if(mBQuant >= mPQIndex)
          mBQuant--;
      } else {
        if (mHalfPQ)
          HalfQP = 0;
        else {
          if (mBQuant >= mPQIndex)
            mBQuant--;
        }
      }
    } else if (Sts & VC1_BRC_SMALL_FRAME) {
      mBQuant = (mBQuant + curQuant + 1) >> 1;
      mLimBQuant--;

      if (mHalfPQ)
        HalfQP = 0;
      else {
        if (mBQuant >= mPQIndex) {
          mBQuant--;
          if ((mPQIndex > VC1_MIN_QUANT) && (ratio > VC1_MIN_RATIO_HALF))
            HalfQP = 1;
        }
      }
    }
    vc1_Clip(mLimBQuant, VC1_MIN_QUANT, VC1_MAX_QUANT);
    vc1_ClipL(mBQuant, mLimBQuant);
    mBHalf = HalfQP;

  } else if (picType & MFX_FRAMETYPE_P) {

    if (Sts & (VC1_BRC_NOT_ENOUGH_BUFFER | VC1_BRC_ERR_BIG_FRAME)) {
      if (ratio > VC1_RATIO_ILIM || (Sts & VC1_BRC_ERR_BIG_FRAME)) { // ??? VC1_RATIO_PLIM ?
        mFailPQuant   = ((mPQuant + curQuant + 1) >> 1) - mPQuant;
        mPQuant++;
        mLimPQuant = mPQIndex + 1;
      } else {
        mPQuant = (mPQuant + curQuant + 1) >> 1;

        if (mPQuant <= mPQIndex) {
          if (mPQIndex > 9) {
            if (mPQuant <= mPQIndex)
              mPQuant++;
          } else {
            if (!mHalfPQ)
              HalfQP = 1;
            else {
              HalfQP = 0;
              if (mPQuant <= mPQIndex)
                mPQuant++;
            }
          }
        }
      }
    } else if (Sts & VC1_BRC_ERR_SMALL_FRAME) { // ???
      mPQuant = (mPQuant + curQuant + 1) >> 1;
      mLimPQuant--;

      if (mPQIndex > 9) {
        if (mPQuant >= mPQIndex)
          mPQuant--;
      } else {
        if (mHalfPQ)
          HalfQP = 0;
        else {
          if(mPQuant >= mPQIndex)
            mPQuant--;
        }
      }
    } else if (Sts & VC1_BRC_SMALL_FRAME) {
      mPQuant = (mPQuant + curQuant + 1) >> 1;
      mLimPQuant--;

      if(mHalfPQ)
        HalfQP = 0;
      else {
        if (mPQuant >= mPQIndex) {
          mPQuant--;
          if ((mPQIndex > VC1_MIN_QUANT) && (ratio > VC1_MIN_RATIO_HALF))
            HalfQP = 1;
        }
      }
    }
    vc1_Clip(mLimPQuant, VC1_MIN_QUANT, VC1_MAX_QUANT);
    vc1_ClipL(mPQuant, mLimPQuant);
    mPHalf = HalfQP;

  } else { // if (picType & MFX_FRAMETYPE_I) {
    mIQuant = (mIQuant + curQuant) >> 1;

    if (Sts & (VC1_BRC_NOT_ENOUGH_BUFFER | VC1_BRC_ERR_BIG_FRAME)) {
      if (ratio > VC1_RATIO_ILIM || (Sts & VC1_BRC_ERR_BIG_FRAME)) {
        mIQuant++;
        mLimIQuant = mPQIndex + 2;
      } else {
        if (mPQIndex > 9)
          mLimIQuant = mPQIndex + 1;
        else if (mIQuant <= mPQIndex)
          HalfQP = 1;
      }
    } else if (Sts & VC1_BRC_ERR_SMALL_FRAME) { // ???
      mLimIQuant--;

      if (mIQuant >= mPQIndex)
        mIQuant--;

      if ((ratio > VC1_MIN_RATIO_HALF) &&  (mPQIndex > 1))
        HalfQP = 1;
    } else if(Sts & VC1_BRC_SMALL_FRAME) {
      mLimIQuant--;

      if ((mIQuant < mPQIndex) && mIQuant && (mPQIndex > VC1_MIN_QUANT))
        HalfQP = 1;

    }

    vc1_Clip(mLimIQuant, VC1_MIN_QUANT, VC1_MAX_QUANT);
    vc1_ClipL(mIQuant, mLimIQuant);
    mIHalf = HalfQP;
  }

  return Sts;
}

void MFXVideoRcVc1::CompleteFrame(mfxU16 picType, mfxU32 currSize)
{
  mfxF32 ratioI = 0;
  mfxF32 ratioP = 0;
  mfxF32 ratioB = 0;
  mfxI32 prefINeedSize = mNeedISize;
  mfxI32 prefPNeedSize = mNeedPSize;
  mfxI32 prefBNeedSize = mNeedBSize;

//  if(m_encMode == VC1_BRC_CONST_QUANT_MODE)
//    return;

  mCurrFrameInGOP++;
//  m_frameCount++;
  mCurrGOPSize += currSize;

  if (picType == MFX_FRAMETYPE_S) {
    if (mCurrINum) {
      if(mCurrFrameInGOP % (mBFrLength + 1))
        mPNum--;
      else
        mBNum--;
    }
  } else if (picType & MFX_FRAMETYPE_B) {
    mBGOPSize += currSize;
    mCurrBNum++;
    mAverageBQuant +=  (mPQIndex*2 + mHalfPQ);
  } else if (picType & MFX_FRAMETYPE_P) {
    mPGOPSize += currSize;
    mCurrPNum++;
    mAveragePQuant +=  (mPQIndex*2 + mHalfPQ);
  } else {
    mIGOPSize += currSize;
    mCurrINum++;
    mAverageIQuant = mPQIndex;
  }


  if (mCurrGOPLength > 1) {
    if ((mGOPHalfFlag && ((mCurrGOPSize > mGOPSize/2) || mCurrFrameInGOP == mCurrGOPLength/2))
      || (mCurrFrameInGOP == 1) || mFailPQuant || mFailBQuant || picType == MFX_FRAMETYPE_S)
    {

      mfxI32 PBSize = mNextGOPSize - mCurrGOPSize;

      if (mCurrFrameInGOP != 1)
        mGOPHalfFlag = 0;

      if ((PBSize > mIdealFrameSize) && (mCurrFrameInGOP != mCurrGOPLength)) {
        mNeedPSize = (mfxI32)(PBSize/((mPNum - mCurrPNum) + (mIBsize/mIPsize)*(mBNum - mCurrBNum)));
        mNeedBSize = (mfxI32)(mNeedPSize*(mIBsize/mIPsize));
      } else {
        mFailGOP = 1;
        if(mNeedPSize > mIdealFrameSize/2)
          mNeedPSize = (mfxI32)(mNeedPSize*0.75);
        if(mNeedBSize > mIdealFrameSize/2)
          mNeedBSize = (mfxI32)(mNeedBSize*0.75);
      }

      ratioP = ((mfxF32)prefPNeedSize/(mfxF32)mNeedPSize);
      ratioB = ((mfxF32)prefBNeedSize/(mfxF32)mNeedBSize);

      if(mNeedPSize < (mIdealFrameSize >> 2))
        ratioP = 1;
      if(mNeedBSize < (mIdealFrameSize >> 2))
        ratioB = 1;

      if(mPNum!=mCurrPNum)
        CorrectGOPQuant(MFX_FRAMETYPE_P, ratioP);
      CorrectGOPQuant(MFX_FRAMETYPE_B, ratioB);
    }
  }
  if (mCurrFrameInGOP == mCurrGOPLength) // && (mCurrPNum || mCurrBNum))
  {
    mSizeAberration += mGOPSize - mCurrGOPSize;

    //check overflow mSizeAberration
    if (mSizeAberration > 4*mGOPSize)
      mSizeAberration = 4*mGOPSize;

    if (mSizeAberration < (- 4*mGOPSize))
      mSizeAberration = -(4*mGOPSize);

    ///---------------------------
    if (mCurrPNum) {
      mAveragePQuant /= (mCurrPNum*2);
      mAveragePQuant++;
    }

    if (mCurrBNum) {
      mAverageBQuant /= (mCurrBNum*2);
      mAverageBQuant++;
    }

    //correction of I/P/B ratio
    if (mIGOPSize) {
      mfxF32 P_average_size, PRatio, B_average_size, BIRatio, BPRatio;
      if (mPGOPSize) {
        P_average_size =  (mfxF32)(mPGOPSize/mPNum);
        PRatio = (P_average_size*mAveragePQuant)/(mIGOPSize*mAverageIQuant);
        mIPsize = (mIPsize + PRatio)/2;

        if (mIPsize > 1.0)
          mIPsize = 1.0;
      }

      if (mBGOPSize) {
        B_average_size =  (mfxF32)(mBGOPSize/mBNum);
        BIRatio = (B_average_size*mAverageBQuant)/(mIGOPSize*mAverageIQuant);
        if (mPGOPSize) {
          BPRatio = (B_average_size*mAverageBQuant)/(mPGOPSize*mAveragePQuant);
          mIBsize = (mIBsize + BIRatio + BPRatio)/3;
        } else
          mIBsize = (mIBsize + BIRatio)/2;

        if (mIBsize > 1.0)
          mIBsize = 1.0;
      }
    }

    SetGOPParams();
    mNextGOPSize = mGOPSize + mSizeAberration;

    //check m_needISize
    if (mNextGOPSize > mGOPSize + (mGOPSize >> 1))
      mNextGOPSize = mGOPSize + (mGOPSize >> 1);

    if (mNextGOPSize < (mGOPSize >> 1))
      mNextGOPSize = mGOPSize >> 1;

    mNeedISize = (mfxI32)(mNextGOPSize/(1 + mIPsize*mPNum + mIBsize*mBNum));

    ratioI = ((mfxF32)prefINeedSize/(mfxF32)mNeedISize);

    CorrectGOPQuant(MFX_FRAMETYPE_I, ratioI);
  }

//  m_recoding = 0;
  mFailPQuant = 0;
  mFailBQuant = 0;
  mFrameCompleted = 1;
};


void MFXVideoRcVc1::CorrectGOPQuant(mfxU16 picType, mfxF32 ratio)
{
  mfxI32 curQuant = 0;

  if(ratio == 1 || picType == MFX_FRAMETYPE_S)
    return;

  if (picType & MFX_FRAMETYPE_B) {
    curQuant = (mfxI32)(ratio * mBQuant - 0.5);
    vc1_Clip(curQuant, VC1_MIN_QUANT, VC1_MAX_QUANT);

    if(curQuant != mBQuant)
      mBHalf = 0;

    mBQuant = (mBQuant + curQuant + 1) >> 1;
    mLimBQuant = (mLimBQuant + mBQuant) >> 1;

    vc1_Clip(mLimBQuant, VC1_MIN_QUANT, VC1_MAX_QUANT); // ??? redundant
    vc1_ClipL(mBQuant, mLimBQuant);
  } else if (picType & MFX_FRAMETYPE_P) {

    curQuant = (mfxI32)(ratio * mPQuant - 0.5);
    vc1_Clip(curQuant, VC1_MIN_QUANT, VC1_MAX_QUANT);

    if(curQuant != mPQuant)
      mPHalf = 0;

    mPQuant = (mPQuant + curQuant + 1) >> 1;
    mLimPQuant = (mLimPQuant + mPQuant) >> 1;

    vc1_Clip(mLimPQuant, VC1_MIN_QUANT, VC1_MAX_QUANT); // ??? redundant
    vc1_ClipL(mPQuant, mLimPQuant);
  } else {
    curQuant = (mfxI32)(ratio * mIQuant + 0.5);
    vc1_Clip(curQuant, VC1_MIN_QUANT, VC1_MAX_QUANT);

    if(curQuant != mIQuant)
      mIHalf = 0;

    mIQuant = (mIQuant + curQuant + 1) >> 1;
    // m_Quant.LimIQuant = (m_Quant.LimIQuant + m_Quant.IQuant)/2;

    //VC1_QUANT_CLIP(m_Quant.LimIQuant,0);
    vc1_ClipL(mIQuant, mLimIQuant);
  }
}

void MFXVideoRcVc1::SetGOPParams()
{

//  m_frameCount = 0;


  mPNum = (mGOPLength - 1)/(mBFrLength + 1);
  if (mPQIndex)
    mBNum = mGOPLength - 1 - mPNum;
  else //  first frame (first GOP is different)
    mBNum = mPNum*mBFrLength;

  mCurrGOPLength = 1 + mPNum + mBNum;

  mGOPSize     = mIdealFrameSize * mCurrGOPLength;
  mNextGOPSize = mGOPSize;

  mNeedISize = (mfxI32)(mGOPSize/(1 + mIPsize*mPNum + mIBsize*mBNum));
  mNeedPSize = (mfxI32)(mIPsize*mNeedISize);
  mNeedBSize = (mfxI32)(mIBsize*mNeedISize);

/*
  mCurrISize = mNeedISize;
  mCurrPSize = mNeedPSize;
  mCurrBSize = mNeedBSize;
*/

  mCurrFrameInGOP = 0;
  mPoorRefFrame = 0;
  mCurrGOPSize = 0;
  mGOPHalfFlag = 1;

  mIGOPSize = 0;
  mPGOPSize = 0;
  mBGOPSize = 0;

  mCurrINum = 0;
  mCurrPNum = 0;
  mCurrBNum = 0;
  mFailGOP = 0;

  mAveragePQuant = 0;
  mAverageBQuant = 0;
}

#endif //MFX_ENABLE_VC1_VIDEO_BRC
