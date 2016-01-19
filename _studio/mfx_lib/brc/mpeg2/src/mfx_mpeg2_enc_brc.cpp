/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.
//
//
//         MPEG-2encoder
//
*/

#include <math.h>
#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_BRC)

#include "mfx_mpeg2_enc_brc.h"
#include "mfx_enc_common.h"
#include "mfxvideo.h"


/* quant scale values table ISO/IEC 13818-2, 7.4.2.2 table 7-6 */
static mfxI32 Val_QScale[2][32] =
{
  /* linear q_scale */
  {0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
  32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62},
  /* non-linear q_scale */
  {0, 1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 18, 20, 22,
  24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96,104,112}
};

mfxI32 MFXVideoRcMpeg2::changeQuant(mfxI32 quant_value)
{
  //mfxI32 curq = m_fpar.quant_scale_value;
  mfxI32 curq = quantiser_scale_value;

  if (quant_value == quantiser_scale_value) return quantiser_scale_value;
  if ((quant_value > 7 && quant_value <= 62)) {
    q_scale_type = 0;
    quantiser_scale_code = (quant_value + 1) >> 1;
  } else { // non-linear quantizer
    q_scale_type = 1;
    if (quant_value <= 8) quantiser_scale_code = quant_value;
    else if (quant_value > 62) quantiser_scale_code = 25+((quant_value-64+4)>>3);
  }
  if (quantiser_scale_code < 1) quantiser_scale_code = 1;
  if (quantiser_scale_code > 31) quantiser_scale_code = 31;
  quantiser_scale_value = Val_QScale[q_scale_type][quantiser_scale_code];
  if(quantiser_scale_value == curq) {
    if(quant_value > curq)
      if(quantiser_scale_code == 31) return quantiser_scale_value;
      else quantiser_scale_code ++;
      if(quant_value < curq)
        if(quantiser_scale_code == 1) return quantiser_scale_value;
        else quantiser_scale_code --;
        quantiser_scale_value = Val_QScale[q_scale_type][quantiser_scale_code];
  }
/*
  if(quantiser_scale_value >= 8)
    intra_dc_precision = 0;
  else if(quantiser_scale_value >= 4)
    intra_dc_precision = 1;
  else
    intra_dc_precision = 2;
*/
  // only for High profile
//  if(encodeInfo.profile == 1 && quantiser_scale_value == 1)
//    intra_dc_precision = 3;

  //mfxI32 qq = quantiser_scale_value*quantiser_scale_value;
//  varThreshold = quantiser_scale_value;// + qq/32;
//  meanThreshold = quantiser_scale_value * 8;// + qq/32;
//  sadThreshold = (quantiser_scale_value + 0) * 8;// + qq/16;

  return quantiser_scale_value;
}


MFXVideoRcMpeg2::~MFXVideoRcMpeg2(void)
{
    Close();
}


mfxStatus MFXVideoRcMpeg2::Query(mfxVideoParam *in, mfxVideoParam *out)
{
  MFX_CHECK_NULL_PTR1(out)
  if (in == 0) {
    memset(&out->mfx, 0, sizeof(mfxInfoMFX));
    out->mfx.FrameInfo.FourCC = 1;
    out->mfx.FrameInfo.Width = 1;
    out->mfx.FrameInfo.Height = 1;
//    out->mfx.FrameInfo.CropW = 1; // Not used in MPEG-2
//    out->mfx.FrameInfo.CropH = 1; // Not used in MPEG-2
//    out->mfx.FrameInfo.PicStruct = 1; ???
//    out->mfx.FrameInfo.Step = 1;
//    out->mfx.CodecProfile = 1;
//    out->mfx.CodecLevel = 1;
//    out->mfx.ClosedGopOnly = 1;
    out->mfx.GopPicSize = 1;
    out->mfx.GopRefDist = 1;
    out->mfx.RateControlMethod = 1;
    out->mfx.InitialDelayInKB = 1;
    out->mfx.BufferSizeInKB = 1;
    out->mfx.TargetKbps = 1;
    out->mfx.MaxKbps = 1;
    out->mfx.FrameInfo.PicStruct = 1;

//    out->mfx.NumSlice = 1; // ???
//    out->mfx.NumThread = 1; ???

    out->mfx.FrameInfo.ChromaFormat = 1;
    // MPEG-2 specific
//    out->mfx.FramePicture= 1;
    out->mfx.GopOptFlag = 1;
  } else {
    *out = *in;
    out->mfx.FrameInfo.FourCC = MFX_CODEC_MPEG2;

    out->mfx.GopOptFlag &= (MFX_GOP_CLOSED | MFX_GOP_STRICT);

    if (out->mfx.GopPicSize == 0) {
      if (out->mfx.GopOptFlag & MFX_GOP_STRICT)
        out->mfx.GopPicSize = 132; // max refresh rate
      else
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
      if (mFramerate == 0)
          mFramerate = 30;

      if (out->mfx.BufferSizeInKB && out->mfx.BufferSizeInKB < (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 2)
        out->mfx.BufferSizeInKB = (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 2;

      if (out->mfx.InitialDelayInKB && out->mfx.InitialDelayInKB < (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 1)
        out->mfx.InitialDelayInKB = (mfxU16)(out->mfx.TargetKbps * 8 / mFramerate) << 1;
      // if (out->mfx.InitialDelayInKB == 0)
      //    InitialDelayInKB is calculated internally
    }

    if ((out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420) && (out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422) &&
      (out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444))
      out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;


//    out->mfx.FrameInfo.Step = 1;

    out->mfx.FrameInfo.PicStruct &= MFX_PICSTRUCT_PROGRESSIVE |
      MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF;
    if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
      out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
      out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
      out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  }
  return MFX_ERR_NONE;
}


mfxStatus MFXVideoRcMpeg2::Init(mfxVideoParam *par)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxF64 ppb; //pixels per bit (~ density)
    mfxI32 i;
    mfxF64 nr;
    mfxF64 gopw;
    mfxF64 u_len, ip_weight;

    if(!m_pMFXCore)
       return MFX_ERR_MEMORY_ALLOC;

    mBitrate = CalculateUMCBitrate(par->mfx.TargetKbps);
    if (par->mfx.FrameInfo.Width == 0 || par->mfx.FrameInfo.Height == 0)
      mBitrate = 0;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    FieldPicture = 0;
    if (par->ExtParam) {
      for (i = 0; i < par->NumExtParam; i++) {
        if (par->ExtParam[i] && (par->ExtParam[i]->BufferId == MFX_EXTBUFF_CODING_OPTION)) {
          FieldPicture = ((mfxExtCodingOption *)par->ExtParam[i])->FramePicture== MFX_CODINGOPTION_OFF ? 1 : 0;
          break;
        }
      }
    }
    mQuantUpdated = 1;

    quantiser_scale_value = -1;
    //    intra_dc_precision = 0; // 8 bit
    prqscale[0] = prqscale[1] = prqscale[2] = 0;
    prsize[0]   = prsize[1]   = prsize[2]   = 0;

    m_FirstFrame = 1;

    mRCMode = par->mfx.RateControlMethod;

    if (mRCMode != MFX_RATECONTROL_VBR) // tmp ???
       mRCMode = MFX_RATECONTROL_CBR;

    if (mRCMode == MFX_RATECONTROL_VBR) {
      mHRD.maxBitrate = CalculateUMCBitrate(par->mfx.MaxKbps);
      if (mHRD.maxBitrate < mBitrate)
        mHRD.maxBitrate = mBitrate;
    } else // CBR
      mHRD.maxBitrate = mBitrate;

    mFramerate = CalculateUMCFramerate(par->mfx.FrameInfo.FrameRateExtN, par->mfx.FrameInfo.FrameRateExtD);
    if (mFramerate == 0)
      mFramerate = 30;

    InitHRD(par);

    if (mRCMode != MFX_RATECONTROL_CBR) {
      quant_vbr[0] = qscale[0] = 4;
      quant_vbr[1] = qscale[1] = qscale[0] + 1;
      quant_vbr[2] = qscale[2] = qscale[1] + 1;
    }

    rc_ave_frame_bits = mHRD.inputBitsPerFrame;
//    rc_ip_delay = rc_ave_frame_bits;

    rc_ip_delay = mHRD.maxInputBitsPerFrame;

    VBV_BufferSize = mHRD.bufSize  / 16384;
    if (VBV_BufferSize > 0x3fffe)
      VBV_BufferSize = 0x3fffe;

    rc_vbv_fullness = mHRD.bufFullness;

    switch (par->mfx.FrameInfo.ChromaFormat - 1) {
    case MFX_CHROMAFORMAT_YUV420:
      block_count = 6;
      break;
    case MFX_CHROMAFORMAT_YUV422:
      block_count = 8;
      break;
    case MFX_CHROMAFORMAT_YUV444:
      block_count = 12;
      break;
    default:
      block_count = 6;
    }

    // one can vary weights, can be added to API
    {
      rc_weight[0] = 120;
      rc_weight[1] = 50;
      rc_weight[2] = 25;
      ip_weight = rc_weight[0];

//      if(encodeInfo.FieldPicture)
//        ip_weight = (rc_weight[0] + rc_weight[1]) * .5; // every second I-field became P-field

      gopSize = par->mfx.GopPicSize;
      if(par->mfx.GopPicSize == 0) {
        if(par->mfx.GopOptFlag & MFX_GOP_STRICT) {
          gopSize = 132; // max refresh rate
        } else {
          gopSize = 15;
        }
      }
      IPDistance = par->mfx.GopRefDist;
      if (IPDistance <= 0)
        IPDistance = 1;
      nr = gopSize/IPDistance;

      gopw = (IPDistance-1)*rc_weight[2]*nr + rc_weight[1]*(nr-1) + ip_weight;
      u_len = rc_ave_frame_bits * gopSize / gopw;
      rc_tagsize[0] = u_len * rc_weight[0];
      rc_tagsize[1] = u_len * rc_weight[1];
      rc_tagsize[2] = u_len * rc_weight[2];

      rc_dev = 0; // deviation from ideal bitrate (should be Ipp32f or renewed)

      mfxF64 rrel = gopw / (rc_weight[0] * gopSize);
      ppb = par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height * mFramerate/mBitrate * (block_count-2) / (6-2);

      qscale[0] = (mfxI32)(6.0 * rrel * ppb); // numbers are empiric
      qscale[1] = (mfxI32)(9.0 * rrel * ppb);
      qscale[2] = (mfxI32)(12.0 * rrel * ppb);
    }

    if (!rc_vbv_fullness) {
      if(mRCMode == MFX_RATECONTROL_CBR) {
        rc_vbv_fullness =
        mHRD.bufSize / 2 +                          // half of vbv_buffer
        u_len * ip_weight / 2 +                               // top to center length of I frame
        (IPDistance-1) * (rc_ave_frame_bits - rc_tagsize[2]); // first gop has no M-1 B frames: add'em
        if (rc_vbv_fullness > mHRD.bufSize)
          rc_vbv_fullness = mHRD.bufSize;
      } else {
        rc_vbv_fullness = mHRD.bufSize; // full buffer
      }
    }
    mHRD.bufFullness = (mfxI32)rc_vbv_fullness;

    for(i=0; i<3; i++) {
      if     (qscale[i]< 1) qscale[i]= 1;
      else if(qscale[i]>63) qscale[i]=63; // can be even more
      if(prqscale[i] == 0) prqscale[i] = qscale[i];
      if(prsize[i]   == 0) prsize[i] = (mfxI32)rc_tagsize[i]; // for first iteration
    }
    return MFXSts;
}



mfxStatus MFXVideoRcMpeg2::InitHRD(mfxVideoParam *par)
{
  mfxStatus MFXSts = MFX_ERR_NONE;

  mHRD.bufSize = par->mfx.BufferSizeInKB * MFX_BIT_IN_KB;
//  if (mHRD.bufSize == 0)
//    return MFX_ERR_UNSUPPORTED;
  mHRD.bufFullness = par->mfx.InitialDelayInKB * MFX_BIT_IN_KB;
  mHRD.inputBitsPerFrame = (mfxI32)(mBitrate / mFramerate);
  mHRD.maxInputBitsPerFrame = (mfxI32)(mHRD.maxBitrate / mFramerate);
  mHRD.frameCnt = (mfxU16)-1;
  mHRD.prevFrameCnt = (mfxU16)-2;

//  if (mHRD.bufSize && (mHRD.bufSize < mHRD.inputBitsPerFrame << 2))
  if (mHRD.bufSize < mHRD.inputBitsPerFrame << 2)
    mHRD.bufSize = mHRD.inputBitsPerFrame << 2;
//  if (mHRD.bufSize && (mHRD.bufFullness < mHRD.inputBitsPerFrame << 1))
  if (mHRD.bufFullness && (mHRD.bufFullness < mHRD.inputBitsPerFrame << 1))
    mHRD.bufFullness = mHRD.inputBitsPerFrame << 1;

  if (mHRD.bufFullness > mHRD.bufSize)
    mHRD.bufFullness = mHRD.bufSize;
  return MFXSts;
}

mfxStatus  MFXVideoRcMpeg2::Close(void)
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

mfxStatus  MFXVideoRcMpeg2::Reset(mfxVideoParam *par)
{
  mfxStatus MFXSts;
  Close();
  MFXSts = Init(par);

  return MFXSts;
}


mfxStatus MFXVideoRcMpeg2::FrameENCUpdate(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxI32 q0, q2, sz1;
    mfxI32 indx = (cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I) ? 0 :
      ((cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_P) ? 1 : 2);

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    mPrevDataLength = cuc->Bitstream->DataLength;

    if (mHRD.frameCnt != cuc->FrameCnt) {
      if (!mQuantUpdated) { // BRC reported buffer over/underflow but the application ignored it
        quantiser_scale_code = mPrevQScaleCode;
        quantiser_scale_value = mPrevQScaleValue;
        q_scale_type = mPrevQScaleType;
        UpdateQuant(mBitsEncoded);
      }

      // refresh rate deviation with every new I frame // ???
      if((cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I) &&
        m_FirstFrame == 0 && mRCMode == MFX_RATECONTROL_CBR) {
          Ipp64f ip_tagsize = rc_tagsize[0];
          if (FieldPicture)
            ip_tagsize = (rc_tagsize[0] + rc_tagsize[1]) * .5; // every second I-field became P-field
          rc_dev =
            VBV_BufferSize * 16384 / 2            // half of vbv_buffer
            + ip_tagsize / 2                      // top to center length of I (or IP) frame
            - rc_vbv_fullness;
      }


      // vbv computations for len range
  /*
      if (mRCMode != MFX_RATECONTROL_CBR) {
        q2 = quant_vbr[indx];
      } else
  */
      {
        q0 = qscale[indx];   // proposed from post picture
        q2 = prqscale[indx]; // last used scale
        sz1 = prsize[indx];  // last coded size

        // compute newscale again
        // adaptation to current rate deviation
        mfxF64 target_size = rc_tagsize[indx];
        mfxI32 wanted_size = (mfxI32)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);

        if      (sz1 > 2*wanted_size)   q2 = q2 * 3 / 2 + 1;
        else if (sz1 > wanted_size+100) q2 ++;
        else if (2*sz1 < wanted_size)   q2 = q2 * 3 / 4;
        else if (sz1 < wanted_size-100 && q2>2) q2 --;

        if (rc_dev > 0) {
          q2 = IPP_MAX(q0,q2);
        } else {
          q2 = IPP_MIN(q0,q2);
        }
      }
      // this call is used to accept small changes in value, which are mapped to the same code
      // changeQuant bothers about to change scale code if value changes
      q2 = changeQuant(q2);
      qscale[indx] = q2;
    }

//    printf("quant %d %d %d \n", quantiser_scale_value, q_scale_type, quantiser_scale_code);

    SetUniformQPs(cuc, q_scale_type, quantiser_scale_code);
    cuc->FrameParam->MPEG2.QuantScaleType = q_scale_type;

//    cuc->FrameParam->IntraDCprecision = intra_dc_precision;
    m_FirstFrame = 0;

    return MFXSts;
}

mfxStatus MFXVideoRcMpeg2::FramePAKRefine(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    //mPrevDataLength = cuc->Bitstream->DataLength;

    if (mHRD.frameCnt != cuc->FrameCnt) {
      if (!mQuantUpdated) { // BRC reported buffer over/underflow but the application ignored it
        quantiser_scale_code = mPrevQScaleCode;
        quantiser_scale_value = mPrevQScaleValue;
        q_scale_type = mPrevQScaleType;
        UpdateQuant(mBitsEncoded);
      }
    }

    SetUniformQPs(cuc, q_scale_type, quantiser_scale_code);
    cuc->FrameParam->MPEG2.QuantScaleType = q_scale_type;
//    MFXSts = FrameEncUpdate(cuc);

    return MFXSts;
}


mfxI32 MFXVideoRcMpeg2::UpdateAndCheckHRD(mfxI32 frameBits)
{
  mfxI32 ret = MPEG2_BRC_OK;
  mfxI32 bufFullness;

  if (mHRD.frameCnt != mHRD.prevFrameCnt) {
    mHRD.prevBufFullness = mHRD.bufFullness;
    mHRD.prevFrameCnt = mHRD.frameCnt;
  } else { // frame is being recoded - restore buffer state
    mHRD.bufFullness = mHRD.prevBufFullness;
  }

  mHRD.maxFrameSize = mHRD.bufFullness;
  mHRD.minFrameSize = (mRCMode == MFX_RATECONTROL_VBR ? 0 : mHRD.bufFullness + (mfxI32)rc_delay - mHRD.bufSize);
  if (mHRD.minFrameSize < 0)
    mHRD.minFrameSize = 0;

  bufFullness = mHRD.bufFullness - frameBits;

  if (bufFullness < 0) {
    bufFullness = 0;
    ret = MPEG2_BRC_ERR_BIG_FRAME;
    bufFullness += (mfxI32)rc_delay; // maxInputBitsPerFrame = inputBitsPerFrame in case of CBR
    if (bufFullness > mHRD.bufSize) // possible in VBR mode if at all (???)
      bufFullness = mHRD.bufSize;
  } else {
    bufFullness += (mfxI32)rc_delay;
    if (bufFullness > mHRD.bufSize) {
      bufFullness =  mHRD.bufSize;
      if (mRCMode != MFX_RATECONTROL_VBR)
        ret = MPEG2_BRC_ERR_SMALL_FRAME;
    }
  }
  mHRD.bufFullness = bufFullness;

  return ret;
}

void MFXVideoRcMpeg2::UpdateQuantHRD(mfxI32 bEncoded, mfxI32 sts)
{
  mfxI32 quant;
  mfxF64 qs;
  mfxI32 wantedBits = (sts == MPEG2_BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize : mHRD.minFrameSize);
  
  if (wantedBits == 0) // KW-inspired safety check
  {
      wantedBits = 1;
  }

  quant = quantiser_scale_value;

  qs = pow((mfxF64)bEncoded/wantedBits, 2.0);
  quant = (mfxI32)(quant * qs + 0.5);

  if (quant == quantiser_scale_value)
    quant += (sts == MPEG2_BRC_ERR_BIG_FRAME ? 1 : -1);
  else if (quant >= quantiser_scale_value + 5)
    quant = quantiser_scale_value + 3;
  else if (quant >= quantiser_scale_value + 3)
    quant = quantiser_scale_value + 2;
  else if (quant > quantiser_scale_value + 1)
    quant = quantiser_scale_value + 1;
  else if (quant <= quantiser_scale_value - 5)
    quant = quantiser_scale_value - 3;
  else if (quant <= quantiser_scale_value - 3)
    quant = quantiser_scale_value - 2;
  else if (quant < quantiser_scale_value - 1)
    quant = quantiser_scale_value - 1;

  changeQuant(quant);
}

void MFXVideoRcMpeg2::UpdateQuant(mfxI32 bits_encoded)
{
  mfxI32 indx = mIndx;
  mfxI32 isfield = mIsField;
  m_totalBitsEncoded += bits_encoded;

  prsize[indx] = (mfxI32)(bits_encoded << isfield);
  prqscale[indx] = qscale[indx];

//  if (mRCMode != MFX_RATECONTROL_CBR)
//    return;

  mfxI32 cur_qscale = qscale[indx];
  Ipp64f target_size = rc_tagsize[indx];
  rc_dev += bits_encoded - (isfield ? target_size/2 : target_size);
  mfxI32 wanted_size = (mfxI32)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);
  mfxI32 newscale;
  mfxI32 del_sc;

  newscale = cur_qscale;
  wanted_size >>= isfield;

  if (bits_encoded > wanted_size) newscale ++;
  if (bits_encoded > 2*wanted_size) newscale = cur_qscale * 3 / 2 + 1;
  if (bits_encoded < wanted_size && cur_qscale>2) newscale --;
  if (2*bits_encoded < wanted_size) newscale = cur_qscale * 3 / 4;
  // this call is used to accept small changes in value, which are mapped to the same code
  // changeQuant bothers about to change scale code if value changes
  newscale = changeQuant(newscale);

  if (indx == 0) { // (picture_coding_type == MPEG2_I_PICTURE)
    if (newscale+1 > qscale[1]) qscale[1] = newscale+1;
    if (newscale+2 > qscale[2]) qscale[2] = newscale+2;
  } else if (indx == 1){ // (picture_coding_type == MPEG2_P_PICTURE)
    if (newscale < qscale[0]) {
      del_sc = qscale[0] - newscale;
      qscale[0] -= del_sc/2;
      newscale = qscale[0];
      newscale = changeQuant(newscale);
    }
    if (newscale+1 > qscale[2]) qscale[2] = newscale+1;
  } else {
    if (newscale < qscale[1]) {
      del_sc = qscale[1] - newscale;
      qscale[1] -= del_sc/2;
      newscale = qscale[1];
      newscale = changeQuant(newscale);
      if (qscale[1] < qscale[0]) qscale[0] = qscale[1];
    }
  }
  qscale[indx] = newscale;
  mQuantUpdated = 1;
}

mfxStatus MFXVideoRcMpeg2::FramePAKRecord(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxI32 bits_encoded = (cuc->Bitstream->DataLength  - mPrevDataLength) << 3;
    mfxI32 Sts = MPEG2_BRC_OK;

    mfxI32 isfield = cuc->FrameParam->MPEG2.FieldPicFlag;
    mfxI32 indx = (cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I) ? 0 :
      ((cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_P) ? 1 : 2);
    mfxF64 ip_delay;

    if (mBitrate == 0)
      return MFX_ERR_UNSUPPORTED;

    if (mHRD.frameCnt != cuc->FrameCnt) {
      if (!mQuantUpdated) { // BRC reported buffer over/underflow but the application ignored it
        quantiser_scale_code = mPrevQScaleCode;
        quantiser_scale_value = mPrevQScaleValue;
        q_scale_type = mPrevQScaleType;
        UpdateQuant(bits_encoded);
      }

      mHRD.frameCnt = cuc->FrameCnt;

      mPrevQScaleCode = quantiser_scale_code;
      mPrevQScaleValue = quantiser_scale_value;
      mPrevQScaleType = q_scale_type;
      mBitsEncoded = bits_encoded;
      mIsField = (mfxU8)isfield;
      mIndx = (mfxU8)indx;

      rc_delay = mHRD.maxInputBitsPerFrame;
      if (cuc->FrameSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) {
        if (cuc->FrameParam->MPEG2.RepeatFirstField) rc_delay += mHRD.maxInputBitsPerFrame;
        if (cuc->FrameParam->MPEG2.TopFieldFirst) rc_delay += mHRD.maxInputBitsPerFrame;
      } else {
        if (!isfield) rc_delay += mHRD.maxInputBitsPerFrame;
        if (cuc->FrameParam->MPEG2.RepeatFirstField) rc_delay += mHRD.maxInputBitsPerFrame;
        rc_delay = rc_delay / 2;
      }
      if (0 == (cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_B)) {
        ip_delay = rc_delay;
        if(!isfield) {
          rc_delay = rc_ip_delay;
          rc_ip_delay = ip_delay;
        } else if (cuc->FrameParam->MPEG2.SecondFieldFlag != 0) {
          rc_delay = rc_ip_delay - rc_delay;
          rc_ip_delay = 2*ip_delay;
        }
      }
    }

    mHRD.bufFullness = (mfxI32)rc_vbv_fullness;
    Sts = UpdateAndCheckHRD(bits_encoded);
    rc_vbv_fullness = mHRD.bufFullness;
    cuc->FrameParam->MPEG2.MaxFrameSize = mHRD.maxFrameSize >> 3;
    cuc->FrameParam->MPEG2.MinFrameSize = (mHRD.minFrameSize + 7) >> 3;

    if (Sts != MPEG2_BRC_OK) {
      UpdateQuantHRD(bits_encoded, Sts);
      mQuantUpdated = 0;
    } else
      UpdateQuant(bits_encoded);

    //FILE* fl = _wfopen(VM_STRING("brc_out.txt"), VM_STRING("a"));
    //fprintf(fl, "%3d, %3d, %5d;\n", cuc->FrameCnt, quantiser_scale_value, bits_encoded);
    //fclose(fl);

    return MFXSts;
}

/*
mfxStatus MFXVideoRcMpeg2::SliceEncUpdate(mfxFrameCUC *cuc){
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcMpeg2::SlicePakRefine(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcMpeg2::SlicePakRecord(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}
*/

/*
mfxStatus MFXVideoRcMpeg2::MbEncUpdate(mfxMbParam *par, mfxI16 *qp, mfxI16 *st)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcMpeg2::MbPakRefine(mfxMbParam *par, mfxI16 *qp, mfxI16 *st)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}

mfxStatus MFXVideoRcMpeg2::MbPakRecord(mfxU32 bits)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFXSts;
}
*/


mfxStatus MFXVideoRcMpeg2::SetUniformQPs(mfxFrameCUC *cuc, mfxI32 q_scale_type, mfxI32 quantiser_scale_code)
{
  mfxStatus       MFXSts = MFX_ERR_NONE;
  mfxU32 i;
//  MFX_CHECK_COND(cuc->FrameParam->NumMb == cuc->NumMb);
//  MFX_CHECK_COND(cuc->MbParam->NumMb == cuc->FrameParam->NumMb);

  for (i = 0; i < cuc->MbParam->NumMb; i++) {
    cuc->MbParam->Mb[i].MPEG2.QpScaleType = (mfxU8)q_scale_type;
    cuc->MbParam->Mb[i].MPEG2.QpScaleCode = (mfxU8)quantiser_scale_code;
  }
  return MFXSts;
}

#endif //MFX_ENABLE_H264_VIDEO_BRC
