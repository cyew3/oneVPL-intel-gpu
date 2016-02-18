/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.
//
//
//          H.264 (AVC) encoder
//
*/

#include <math.h>
#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_BRC)

#include "mfx_h264_enc_brc.h"
#include "mfx_enc_common.h"
#include "mfxvideo.h"


MFXVideoRcH264::~MFXVideoRcH264(void)
{
    Close();
}

mfxStatus MFXVideoRcH264::Query(mfxVideoParam *in, mfxVideoParam *out)
{
  MFX_CHECK_NULL_PTR1(out)
  if (in == 0) {
    memset(&out->mfx, 0, sizeof(mfxInfoMFX));
    out->mfx.FrameInfo.FourCC = 1;
    out->mfx.FrameInfo.Width = 1;
    out->mfx.FrameInfo.Height = 1;
//    out->mfx.FrameInfo.CropW = 1; // Not used in H.264
//    out->mfx.FrameInfo.CropH = 1; // Not used in H.264
//    out->mfx.FrameInfo.Step = 1;
//    out->mfx.CodecProfile = 1;
//    out->mfx.CodecLevel = 1;
//    out->mfx.ClosedGopOnly = 1;

    out->mfx.GopPicSize = 1; // currently not used in H.264 BRC
    out->mfx.GopRefDist = 1; // currently not used in H.264 BRC
    out->mfx.RateControlMethod = 1;
    out->mfx.InitialDelayInKB = 1;
    out->mfx.BufferSizeInKB = 1;
    out->mfx.TargetKbps = 1;
    out->mfx.MaxKbps = 1;
//    out->mfx.NumSlice = 1; // ???
//    out->mfx.NumThread = 1; ???
    out->mfx.FrameInfo.PicStruct = 1;
    out->mfx.FrameInfo.ChromaFormat = 1;
    out->mfx.GopOptFlag = 1;
  } else {
    *out = *in;
    out->mfx.FrameInfo.FourCC = MFX_CODEC_AVC;

    out->mfx.GopOptFlag &= (MFX_GOP_CLOSED | MFX_GOP_STRICT);

    if(out->mfx.GopPicSize == 0) {
        out->mfx.GopPicSize = 15;
    }
    if (out->mfx.GopRefDist == 0)
      out->mfx.GopRefDist = 1;

    if (out->mfx.RateControlMethod != MFX_RATECONTROL_VBR)
      out->mfx.RateControlMethod = MFX_RATECONTROL_CBR;

    if (out->mfx.RateControlMethod == MFX_RATECONTROL_VBR && out->mfx.MaxKbps < out->mfx.TargetKbps)
      out->mfx.MaxKbps = out->mfx.TargetKbps;

    if (out->mfx.TargetKbps) {
      mfxF64 mFramerate = CalculateUMCFramerate(out->mfx.FrameInfo.FrameRateExtD, out->mfx.FrameInfo.FrameRateExtN);

      if(mFramerate > 0) {
        if (out->mfx.BufferSizeInKB && out->mfx.BufferSizeInKB < (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 2) {
            out->mfx.BufferSizeInKB = (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 2;
        }
        if (out->mfx.InitialDelayInKB < (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 1) {
            out->mfx.InitialDelayInKB = (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 1;
        }
      }
    }

    if (out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 && out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
      out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
      out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    out->mfx.FrameInfo.PicStruct &= MFX_PICSTRUCT_PROGRESSIVE |
      MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF;
    if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
      out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
      out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
      out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  }
  return MFX_ERR_NONE;
}

mfxI32 MFXVideoRcH264::GetInitQP(mfxI32 fPixels, mfxU8 chromaFormat, mfxI32 alpha)
{
  const Ipp64f x0 = 0, y0 = 1.19, x1 = 1.75, y1 = 1.75;
  mfxI32 fs, fsLuma;

  fsLuma = fPixels;
  fs = fsLuma;
  if (alpha)
    fs += fsLuma;
  if (chromaFormat == MFX_CHROMAFORMAT_YUV420)
    fs += fsLuma / 2;
  else if (chromaFormat == MFX_CHROMAFORMAT_YUV422)
    fs += fsLuma;
  else if (chromaFormat == MFX_CHROMAFORMAT_YUV444)
    fs += fsLuma * 2;
  fs = fs * mBitDepth / 8;
  mfxI32 q = (mfxI32)(1. / 1.2 * pow(10.0, (log10(fs * 2. / 3. * mFramerate / mBitrate) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
  h264_Clip(q, 1, mQuantMax);
  return q;
}


mfxStatus MFXVideoRcH264::Init(mfxVideoParam *par)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxI32 qas, fas, bas;
    mfxI32 alpha = 0;

    if(!m_pMFXCore)
       return MFX_ERR_MEMORY_ALLOC;

    mBitrate = CalculateUMCBitrate(par->mfx.TargetKbps);
    if (par->mfx.FrameInfo.Width == 0 || par->mfx.FrameInfo.Height == 0)
      mBitrate = 0;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    mQuantUpdated = 1;

    mRCMode = par->mfx.RateControlMethod;
    if (mRCMode != MFX_RATECONTROL_VBR)
      mRCMode = MFX_RATECONTROL_CBR;

    mFramerate = CalculateUMCFramerate(par->mfx.FrameInfo.FrameRateExtN, par->mfx.FrameInfo.FrameRateExtD);
    if (mFramerate == 0)
      mFramerate = 30;

    if (mRCMode == MFX_RATECONTROL_VBR) {
      mHRD.maxBitrate = CalculateUMCBitrate(par->mfx.MaxKbps);
      if (mHRD.maxBitrate < mBitrate)
        mHRD.maxBitrate = mBitrate;
    } else // CBR
      mHRD.maxBitrate = mBitrate;

    mBitDepth = 8;//par->mfx.FrameInfo.BitDepthLuma;

    InitHRD(par);

    if (mRCMode == MFX_RATECONTROL_VBR) {
      if (par->mfx.GopPicSize < 30) {
        qas = 30;
        fas = 20;
        bas = 30;
      } else {
        qas = 100;
        fas = 30;
        bas = 100;
      }
    } else {
      qas = 4;
      fas = 8;
      bas = 4;
    }

    {
      mQuantOffset = 6 * (mBitDepth - 8);
      mQuantMax = 51 + mQuantOffset;
      mBitsDesiredTotal = 0;
      mBitsDesiredFrame = (mfxI32)((mfxF64)mBitrate / mFramerate);
      mfxI32 q = GetInitQP(par->mfx.FrameInfo.Width*par->mfx.FrameInfo.Height, (mfxU8)par->mfx.FrameInfo.ChromaFormat, alpha);
      mQuantPrev = mQuantI = mQuantP = q;
      SetQuantB();
      mRCfap = fas;
      mRCqap = qas;
      mRCbap = bas;
      mRCq = q;
      mRCqa = 1. / (Ipp64f)mRCq;
      mRCfa = mBitsDesiredFrame;
      mBitsEncodedTotal = 0;
    }

    return MFXSts;
}


mfxStatus MFXVideoRcH264::InitHRD(mfxVideoParam *par)
{
  mfxStatus MFXSts = MFX_ERR_NONE;

  mHRD.frameCnt = (mfxU16)-1;
  mHRD.prevFrameCnt = (mfxU16)-2;
  mHRD.bufSize = par->mfx.BufferSizeInKB * MFX_BIT_IN_KB;
  if (mHRD.bufSize == 0)
    return MFX_ERR_UNSUPPORTED;
  mHRD.bufFullness = par->mfx.InitialDelayInKB * MFX_BIT_IN_KB;
  mHRD.inputBitsPerFrame = (mfxI32)(mBitrate / mFramerate);
  mHRD.maxInputBitsPerFrame = (mfxI32)(mHRD.maxBitrate / mFramerate);

  if (mHRD.bufSize && (mHRD.bufSize < mHRD.inputBitsPerFrame << 2))
    mHRD.bufSize = mHRD.inputBitsPerFrame << 2;
  if (mHRD.bufSize && (mHRD.bufFullness < mHRD.inputBitsPerFrame << 1))
    mHRD.bufFullness = mHRD.inputBitsPerFrame << 1;

  if (mHRD.bufFullness > mHRD.bufSize)
    mHRD.bufFullness = mHRD.bufSize;
  return MFXSts;
}


void MFXVideoRcH264::UpdateQuantHRD(mfxI32 bEncoded, mfxI32 sts)
{
  mfxI32 quant;
  mfxF64 qs;
  mfxI32 wantedBits = (sts == H264_BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize : mHRD.minFrameSize);

  quant = (mFrameType == MFX_FRAMETYPE_I) ? mQuantI : (mFrameType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;
  mQPprev = quant;

  qs = wantedBits == 0 ? 0 : pow((mfxF64)bEncoded/wantedBits, 2.0);
  quant = (mfxI32)(quant * qs + 0.5);

  if (quant == mQPprev)
    quant += (sts == H264_BRC_ERR_BIG_FRAME ? 1 : -1);

  h264_Clip(quant, 1, mQuantMax);

  if (quant >= mQPprev + 5)
    quant = mQPprev + 3;
  else if (quant >= mQPprev + 3)
    quant = mQPprev + 2;
  else if (quant > mQPprev + 1)
    quant = mQPprev + 1;
  else if (quant <= mQPprev - 5)
    quant = mQPprev - 3;
  else if (quant <= mQPprev - 3)
    quant = mQPprev - 2;
  else if (quant < mQPprev - 1)
    quant = mQPprev - 1;

  mQP = quant;
  switch (mFrameType) {
  case (MFX_FRAMETYPE_I):
    mQuantI = quant;
    break;
  case (MFX_FRAMETYPE_B):
    mQuantB = quant;
    break;
  case (MFX_FRAMETYPE_P):
  default:
    mQuantP = quant;
  }
}

void MFXVideoRcH264::UpdateQuant(mfxI32 bEncoded)
{
  mfxF64  bo, qs, dq;
  mfxI32  quant;

  mBitsEncodedTotal += bEncoded;
  mBitsDesiredTotal += mBitsDesiredFrame;

  quant = (mFrameType == MFX_FRAMETYPE_I) ? mQuantI : (mFrameType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;
  mRCqa += (1. / quant - mRCqa) / mRCqap;
  h264_Clip(mRCqa, 1./mQuantMax , 1./1.);
  if (mFrameType != MFX_FRAMETYPE_I || mRCfap < 30)
    mRCfa += (bEncoded - mRCfa) / mRCfap;
  SetQuantB();
  //if (frameType == MFX_FRAMETYPE_B) return;
  qs = pow(mBitsDesiredFrame / mRCfa, 2.0);
  dq = mRCqa * qs;
  bo = (Ipp64f)((Ipp64s)mBitsEncodedTotal - (Ipp64s)mBitsDesiredTotal) / mRCbap / mBitsDesiredFrame;
  h264_Clip(bo, -1.0, 1.0);
  //dq = dq * (1.0 - bo);
  dq = dq + (1./mQuantMax - dq) * bo;
  h264_Clip(dq, 1./mQuantMax, 1./1.);
  quant = (int) (1. / dq + 0.5);
  //h264_Clip(quant, 1, mQuantMax);
  if (quant >= mRCq + 5)
    quant = mRCq + 3;
  else if (quant >= mRCq + 3)
    quant = mRCq + 2;
  else if (quant > mRCq + 1)
    quant = mRCq + 1;
  else if (quant <= mRCq - 5)
    quant = mRCq - 3;
  else if (quant <= mRCq - 3)
    quant = mRCq - 2;
  else if (quant < mRCq - 1)
    quant = mRCq - 1;
  if (mFrameType != MFX_FRAMETYPE_B) {
    mQuantPrev = mQuantP;
    mQP = mQuantB;
  } else
    mQP = quant;
  mRCq = mQuantI = mQuantP = quant;
  mQuantUpdated = 1;
}


mfxI32 MFXVideoRcH264::UpdateAndCheckHRD(mfxI32 frameBits)
{
  mfxI32 ret = H264_BRC_OK;
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
    ret = H264_BRC_ERR_BIG_FRAME;
    bufFullness += mHRD.maxInputBitsPerFrame; // maxInputBitsPerFrame = inputBitsPerFrame in case of CBR
    if (bufFullness > mHRD.bufSize) // possible in VBR mode if at all (???)
      bufFullness = mHRD.bufSize;
  } else {
    bufFullness += mHRD.maxInputBitsPerFrame;
    if (bufFullness > mHRD.bufSize) {
      bufFullness =  mHRD.bufSize;
      if (mRCMode != MFX_RATECONTROL_VBR)
        ret = H264_BRC_ERR_SMALL_FRAME;
    }
  }
  mHRD.bufFullness = bufFullness;

  return ret;
}


mfxStatus  MFXVideoRcH264::Close(void)
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


mfxStatus MFXVideoRcH264::Reset(mfxVideoParam *par)
{
    mfxStatus MFXSts;
    Close();
    MFXSts = Init(par);
    return MFXSts;
}


mfxStatus MFXVideoRcH264::FrameENCUpdate(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxI32 qp;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    mPrevDataLength = cuc->Bitstream->DataLength;

    if (mHRD.frameCnt != cuc->FrameCnt) {
      if (!mQuantUpdated) { // BRC reported buffer over/underflow but the application ignored it
        mQuantI = mQuantIprev;
        mQuantP = mQuantPprev;
        mQuantB = mQuantBprev;
        UpdateQuant(mBitsEncoded);
      }
    }

    mFrameType = cuc->FrameParam->AVC.FrameType & 0xF;

    switch (mFrameType) {
    case (MFX_FRAMETYPE_I):
      qp = mQuantI;
      break;
    case (MFX_FRAMETYPE_B):
      qp = mQuantB;
      break;
    case (MFX_FRAMETYPE_P):
    default:
      qp = mQuantP;
    }

//    qp -= mQuantOffset;

    SetUniformQPs(cuc, (mfxU8)qp);

    return MFXSts;
}

mfxStatus MFXVideoRcH264::FramePAKRefine(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    if (m_curPicInitQpMinus26 == NO_INIT_QP)
    {
        m_curPicInitQpMinus26 = cuc->FrameParam->AVC.PicInitQpMinus26;
    }
    else if (m_curPicInitQpMinus26 != cuc->FrameParam->AVC.PicInitQpMinus26)
    {
        mfxI32 diff = cuc->FrameParam->AVC.PicInitQpMinus26 - m_curPicInitQpMinus26;
        cuc->FrameParam->AVC.PicInitQpMinus26 = m_curPicInitQpMinus26;
        for (mfxU16 i = 0; i < cuc->NumSlice; i++)
            cuc->SliceParam[i].AVC.SliceQpDelta = (mfxI8)(cuc->SliceParam[i].AVC.SliceQpDelta + diff);
    }

    return MFXSts;
}

mfxStatus MFXVideoRcH264::FramePAKRecord(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxI32 encodedBits = (cuc->Bitstream->DataLength - mPrevDataLength) << 3;
    mfxI32  Sts = H264_BRC_OK;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    if (mHRD.frameCnt != cuc->FrameCnt && !mQuantUpdated) { // BRC reported buffer over/underflow but the application ignored it
      mQuantI = mQuantIprev;
      mQuantP = mQuantPprev;
      mQuantB = mQuantBprev;
      UpdateQuant(mBitsEncoded);
    }

    mHRD.frameCnt = cuc->FrameCnt;

    mQuantIprev = mQuantI;
    mQuantPprev = mQuantP;
    mQuantBprev = mQuantB;

    mBitsEncoded = encodedBits;

    mFrameType = cuc->FrameParam->AVC.FrameType & 0xF;

    if (mHRD.bufSize) {
      Sts = UpdateAndCheckHRD(encodedBits);
      cuc->FrameParam->AVC.MaxFrameSize = mHRD.maxFrameSize >> 3;
      cuc->FrameParam->AVC.MinFrameSize = (mHRD.minFrameSize + 7) >> 3;
    } else {
      cuc->FrameParam->AVC.MaxFrameSize = 0xFFFFFFFF;
      cuc->FrameParam->AVC.MinFrameSize = 0;
    }

    if (Sts != H264_BRC_OK) {
      UpdateQuantHRD(encodedBits, Sts);
      mQuantUpdated = 0;
    } else
      UpdateQuant(encodedBits);

    return MFXSts;
}

/*
mfxStatus MFXVideoRcH264::SliceEncUpdate(mfxFrameCUC *cuc){
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcH264::SlicePakRefine(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcH264::SlicePakRecord(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}
*/

/*
mfxStatus MFXVideoRcH264::MbEncUpdate(mfxMbParam *par, mfxI16 *qp, mfxI16 *st)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcH264::MbPakRefine(mfxMbParam *par, mfxI16 *qp, mfxI16 *st)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcH264::MbPakRecord(mfxU32 bits)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}
*/


mfxStatus MFXVideoRcH264::SetUniformQPs(mfxFrameCUC *cuc, mfxU8 qp)
{
  mfxStatus       MFXSts = MFX_ERR_NONE;
  mfxU32 i;
  MFX_CHECK_COND(cuc->FrameParam->AVC.NumMb == cuc->NumMb);
  MFX_CHECK_COND(cuc->MbParam->NumMb == cuc->FrameParam->AVC.NumMb);

  cuc->FrameParam->AVC.PicInitQpMinus26 = (mfxI8)(qp - mQuantOffset - 26);
  for (i = 0; i < cuc->NumSlice; i++)
      cuc->SliceParam[i].AVC.SliceQpDelta = 0;

//  for (i = 0; i < cuc->MbParam->NumMb; i++) {
//    cuc->MbParam->Mb[i].QpPrimeY = qp; // ??? what is/will be used in PAK?
//  }


  return MFXSts;
}

#endif //MFX_ENABLE_H264_VIDEO_BRC
