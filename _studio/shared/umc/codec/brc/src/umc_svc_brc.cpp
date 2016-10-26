//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include <math.h>
#include "umc_svc_brc.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_tables.h"

namespace UMC
{

SVCBRC::SVCBRC()
{
  mIsInit = 0;

  mNumTemporalLayers = 0;
  mRCfa = 0;
  mQuant = 0;
  mQuantP = 0;
  mQuantI = 0;
  mQuantB = 0;
  mPictureFlagsPrev = 0;
  mQuantUnderflow = 0;
  mQuantOverflow = 0;
  mMaxFrameSize = 0;
  mOversize = 0;
  mUndersize = 0;
  mQuantMax = 0;
  mPictureFlags = 0;
  mRCfap = 0;
  mRCMode = 0;
  mFrameType = NONE_PICTURE;
  mRCqa = 0;
  mRCq = 0;
  mRCqa0 = 0;
  mMinFrameSize = 0;
  mTid = 0;
  mQuantUpdated = 0;
  mRCqap = 0;
  mBitsEncoded = 0;
  mRCbap = 0;
  mRecodeInternal = 0;
}

SVCBRC::~SVCBRC()
{
  Close();
}

// Copy of mfx_h264_enc_common.cpp::LevelProfileLimitsNal
const Ipp64u LevelProfileLimits[4][16][6] = {
    {
        // BASE_PROFILE, MAIN_PROFILE, EXTENDED_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    76800,     210000,    64},
        /* 1b */ {    1485,    99,    152064,    153600,    420000,    64},
        /* 11 */ {    3000,    396,   345600,    230400,    600000,    128},
        /* 12 */ {    6000,    396,   912384,    460800,    1200000,   128},
        /* 13 */ {    11880,   396,   912384,    921600,    2400000,   128},
        /* 2  */ {    11880,   396,   912384,    2400000,   2400000,   128},
        /* 21 */ {    19800,   792,   1824768,   4800000,   4800000,   256},
        /* 22 */ {    20250,   1620,  3110400,   4800000,   4800000,   256},
        /* 3  */ {    40500,   1620,  3110400,   12000000,  12000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   16800000,  16800000,  512},
        /* 32 */ {    216000,  5120,  7864320,   24000000,  24000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  24000000,  30000000,  512},
        /* 41 */ {    245760,  8192,  12582912,  60000000,  75000000,  512},
        /* 42 */ {    522240,  8704,  13369344,  60000000,  75000000,  512},
        /* 5  */ {    589824,  22080, 42393600,  162000000, 162000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  288000000, 288000000, 512},
    },
    {
        // HIGH_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    96000,     262500,    64},
        /* 1b */ {    1485,    99,    152064,    192000,    525000,    64},
        /* 11 */ {    3000,    396,   345600,    288000,    750000,    128},
        /* 12 */ {    6000,    396,   912384,    576000,    1500000,   128},
        /* 13 */ {    11880,   396,   912384,    1152000,   3000000,   128},
        /* 2  */ {    11880,   396,   912384,    3000000,   3000000,   128},
        /* 21 */ {    19800,   792,   1824768,   6000000,   6000000,   256},
        /* 22 */ {    20250,   1620,  3110400,   6000000,   6000000,   256},
        /* 3  */ {    40500,   1620,  3110400,   15000000,  15000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   21000000,  21000000,  512},
        /* 32 */ {    216000,  5120,  7864320,   30000000,  30000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  30000000,  37500000,  512},
        /* 41 */ {    245760,  8192,  12582912,  75000000,  93750000,  512},
        /* 42 */ {    522240,  8704,  13369344,  75000000,  93750000,  512},
        /* 5  */ {    589824,  22080, 42393600,  202500000, 202500000, 512},
        /* 51 */ {    983040,  36864, 70778880,  360000000, 360000000, 512},
    },
    {
        // HIGH10_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    230400,    630000,    64},
        /* 1b */ {    1485,    99,    152064,    460800,    1260000,   64},
        /* 11 */ {    3000,    396,   345600,    691200,    1800000,   128},
        /* 12 */ {    6000,    396,   912384,    1382400,   3600000,   128},
        /* 13 */ {    11880,   396,   912384,    2764800,   7200000,   128},
        /* 2  */ {    11880,   396,   912384,    7200000,   7200000,   128},
        /* 21 */ {    19800,   792,   1824768,   14400000,  14400000,  256},
        /* 22 */ {    20250,   1620,  3110400,   14400000,  14400000,  256},
        /* 3  */ {    40500,   1620,  3110400,   36000000,  36000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   50400000,  50400000,  512},
        /* 32 */ {    216000,  5120,  7864320,   72000000,  72000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  72000000,  90000000,  512},
        /* 41 */ {    245760,  8192,  12582912,  180000000, 225000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  180000000, 225000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  486000000, 486000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  864000000, 864000000, 512},
    },
    {
        // HIGH422_PROFILE, HIGH444_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    307200,    840000,    64},
        /* 1b */ {    1485,    99,    152064,    614400,    1680000,   64},
        /* 11 */ {    3000,    396,   345600,    921600,    2400000,   128},
        /* 12 */ {    6000,    396,   912384,    1843200,   4800000,   128},
        /* 13 */ {    11880,   396,   912384,    3686400,   9600000,   128},
        /* 2  */ {    11880,   396,   912384,    9600000,   9600000,   128},
        /* 21 */ {    19800,   792,   1824768,   19200000,  19200000,  256},
        /* 22 */ {    20250,   1620,  3110400,   19200000,  19200000,  256},
        /* 3  */ {    40500,   1620,  3110400,   48000000,  48000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   67200000,  67200000,  512},
        /* 32 */ {    216000,  5120,  7864320,   96000000,  96000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  96000000,  120000000, 512},
        /* 41 */ {    245760,  8192,  12582912,  240000000, 300000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  240000000, 300000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  648000000, 648000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  1152000000,1152000000,512},
    },
};

/*
static Ipp64f QP2Qstep (Ipp32s QP)
{
  return (Ipp64f)(0.85 * pow(2.0, (QP - 12.0) / 6.0));
}

static Ipp32s Qstep2QP (Ipp64f Qstep)
{
  return (Ipp32s)(12.0 + 6.0 * log(Qstep/0.85) / log(2.0));
}
*/

Status SVCBRC::InitHRDLayer(Ipp32s tid)
{
  VideoBrcParams *pParams = &mParams[tid];
  Ipp32s profile_ind, level_ind;
  Ipp64u bufSizeBits = pParams->HRDBufferSizeBytes << 3;
  Ipp64u maxBitrate = pParams->maxBitrate;
  Ipp32s bitsPerFrame;

  bitsPerFrame = (Ipp32s)(pParams->targetBitrate / pParams->info.framerate);

  if (BRC_CBR == pParams->BRCMode)
    maxBitrate = pParams->maxBitrate = pParams->targetBitrate;
  if (maxBitrate < (Ipp64u)pParams->targetBitrate)
    maxBitrate = pParams->maxBitrate = 0;

  if (bufSizeBits > 0 && bufSizeBits < static_cast<Ipp64u>(bitsPerFrame << 1))
    bufSizeBits = (bitsPerFrame << 1);

  profile_ind = ConvertProfileToTable(pParams->profile);
  level_ind = ConvertLevelToTable(pParams->level);

  if (level_ind > H264_LIMIT_TABLE_LEVEL_51) // just in case svc brc is called with mvc level
      level_ind = H264_LIMIT_TABLE_LEVEL_51;

  if (pParams->targetBitrate > (Ipp32s)LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR])
    pParams->targetBitrate = (Ipp32s)LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
  if (static_cast<Ipp64u>(pParams->maxBitrate) > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR])
    maxBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
  if (bufSizeBits > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB])
    bufSizeBits = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB];

  if (pParams->maxBitrate <= 0 && pParams->HRDBufferSizeBytes <= 0) {
    if (profile_ind < 0) {
      profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
      level_ind = H264_LIMIT_TABLE_LEVEL_51;
    } else if (level_ind < 0)
      level_ind = H264_LIMIT_TABLE_LEVEL_51;
    maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
    bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
  } else if (pParams->HRDBufferSizeBytes <= 0) {
    if (profile_ind < 0)
      bufSizeBits = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB];
    else if (level_ind < 0) {
      for (; profile_ind < H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++)
        if (maxBitrate <= LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR])
          break;
      bufSizeBits = LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB];
    } else {
      for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
        for (; level_ind <= H264_LIMIT_TABLE_LEVEL_51; level_ind++) {
          if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
            break;
        }
        if (level_ind <= H264_LIMIT_TABLE_LEVEL_51)
          break;
        level_ind = 0;
      }
      if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
        profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
        level_ind = H264_LIMIT_TABLE_LEVEL_51;
      }
      bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
    }
  } else if (pParams->maxBitrate <= 0) {
    if (profile_ind < 0)
      maxBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
    else if (level_ind < 0) {
      for (; profile_ind < H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++)
        if (bufSizeBits <= LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB])
          break;
      maxBitrate = LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
    } else {
      for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
        for (; level_ind <= H264_LIMIT_TABLE_LEVEL_51; level_ind++) {
          if (bufSizeBits <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
            break;
        }
        if (level_ind <= H264_LIMIT_TABLE_LEVEL_51)
          break;
        level_ind = 0;
      }
      if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
        profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
        level_ind = H264_LIMIT_TABLE_LEVEL_51;
      }
      maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
    }
  }

  if (maxBitrate < (Ipp64u)pParams->targetBitrate) {
    maxBitrate = (Ipp64u)pParams->targetBitrate;
    for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
      for (; level_ind <= H264_LIMIT_TABLE_LEVEL_51; level_ind++) {
        if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
          break;
      }
      if (level_ind <= H264_LIMIT_TABLE_LEVEL_51)
        break;
      level_ind = 0;
    }
    if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
      profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
      level_ind = H264_LIMIT_TABLE_LEVEL_51;
    }
    bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
  }
  pParams->HRDBufferSizeBytes = (Ipp32s)(bufSizeBits >> 3);
  pParams->maxBitrate = (Ipp32s)((maxBitrate >> 6) << 6);  // In H.264 HRD params bitrate is coded as value*2^(6+scale), we assume scale=0
  mHRD[tid].maxBitrate = pParams->maxBitrate;
  mHRD[tid].inputBitsPerFrame = mHRD[tid].maxInputBitsPerFrame = mHRD[tid].maxBitrate / pParams->info.framerate;

  pParams->HRDBufferSizeBytes = (pParams->HRDBufferSizeBytes >> 4) << 4; // coded in bits as value*2^(4+scale), assume scale<=3
  if (pParams->HRDInitialDelayBytes <= 0)
    pParams->HRDInitialDelayBytes = (BRC_CBR == pParams->BRCMode ? pParams->HRDBufferSizeBytes/2 : pParams->HRDBufferSizeBytes);
  else if (pParams->HRDInitialDelayBytes * 8 < bitsPerFrame)
    pParams->HRDInitialDelayBytes = bitsPerFrame / 8;
  if (pParams->HRDInitialDelayBytes > pParams->HRDBufferSizeBytes)
    pParams->HRDInitialDelayBytes = pParams->HRDBufferSizeBytes;
  mHRD[tid].bufSize = pParams->HRDBufferSizeBytes * 8;
  mHRD[tid].bufFullness = pParams->HRDInitialDelayBytes * 8;
  mHRD[tid].frameNum = 0;

  mHRD[tid].maxFrameSize = (Ipp32s)(mHRD[tid].bufFullness - 1);
  mHRD[tid].minFrameSize = (mParams[tid].BRCMode == BRC_VBR ? 0 : (Ipp32s)(mHRD[tid].bufFullness + 1 + 1 + mHRD[tid].inputBitsPerFrame - mHRD[tid].bufSize));

  mHRD[tid].minBufFullness = 0;
  mHRD[tid].maxBufFullness = mHRD[tid].bufSize - mHRD[tid].inputBitsPerFrame;

  return UMC_OK;
}

#define SVCBRC_BUFSIZE2BPF_RATIO 30
#define SVCBRC_BPF2BUFSIZE_RATIO (1./SVCBRC_BUFSIZE2BPF_RATIO)

Ipp32s SVCBRC::GetInitQP()
{
    const Ipp64f x0 = 0, y0 = 1.19, x1 = 1.75, y1 = 1.75;
    Ipp32s fsLuma;
    Ipp32s i;
    Ipp32s bpfSum, bpfAv, bpfMin, bpfTarget;
    Ipp32s bpfMin_ind;
    Ipp64f bs2bpfMin, bf2bpfMin;

    fsLuma = mParams[0].info.clip_info.width * mParams[0].info.clip_info.height;

    bpfMin = bpfSum = mBRC[mNumTemporalLayers-1].mBitsDesiredFrame;
    bpfMin_ind  = mNumTemporalLayers-1;

    bs2bpfMin = bf2bpfMin = (Ipp64f)IPP_MAX_32S;

    for (i = mNumTemporalLayers - 1; i >= 0; i--) {
        if (mHRD[i].bufSize > 0) {
            Ipp64f bs2bpf = (Ipp64f)mHRD[i].bufSize / mBRC[i].mBitsDesiredFrame;
            if (bs2bpf < bs2bpfMin) {
                bs2bpfMin = bs2bpf;
            }
        }
    }

    for (i = mNumTemporalLayers - 2; i >= 0; i--) {
        bpfSum += mBRC[i].mBitsDesiredFrame;
        if (mBRC[i].mBitsDesiredFrame < bpfMin) {
            bpfMin = mBRC[i].mBitsDesiredFrame;
            bpfMin_ind = i;
        }
        mBRC[i].mQuantI = mBRC[i].mQuantP = (Ipp32s)(1. / 1.2 * pow(10.0, (log10((Ipp64f)fsLuma / mBRC[i].mBitsDesiredFrame) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
    }
    bpfAv = bpfSum / mNumTemporalLayers;

    if (bpfMin_ind == mNumTemporalLayers - 1) {
        bpfTarget = bpfMin;
        for (i = mNumTemporalLayers - 2; i >= 0; i--) {
            if (mHRD[i].bufSize > 0) {
                if (bpfTarget < mHRD[i].minFrameSize)
                    bpfTarget = mHRD[i].minFrameSize;
            }
        }
    } else {
        Ipp64f bpfTarget0, bpfTarget1, weight;
        bpfTarget0 = (3*mBRC[mNumTemporalLayers - 1].mBitsDesiredFrame + bpfAv) >> 2;
        if (bs2bpfMin > SVCBRC_BUFSIZE2BPF_RATIO) {
            bpfTarget = (Ipp32s)bpfTarget0;
        } else {
            if (mHRD[mNumTemporalLayers - 1].bufSize > 0) {
                Ipp64f bs2bpfN_1 = mHRD[mNumTemporalLayers - 1].bufSize / mBRC[mNumTemporalLayers - 1].mBitsDesiredFrame;
                weight = bs2bpfMin / bs2bpfN_1;
                bpfTarget1 = weight * mBRC[mNumTemporalLayers - 1].mBitsDesiredFrame + (1 - weight) * mBRC[bpfMin_ind].mBitsDesiredFrame;
            } else {
                bpfTarget1 = mBRC[bpfMin_ind].mBitsDesiredFrame;
            }
            weight = bs2bpfMin / SVCBRC_BUFSIZE2BPF_RATIO;
            bpfTarget =  (Ipp32s)(weight * bpfTarget0  + (1 - weight) * bpfTarget1);
        }
    }


    for (i = mNumTemporalLayers - 1; i >= 0; i--) {
        if (mHRD[i].bufSize > 0) {
            if (bpfTarget > mHRD[i].maxFrameSize)
                bpfTarget = mHRD[i].maxFrameSize;
            if (bpfTarget < mHRD[i].minFrameSize)
                bpfTarget = mHRD[i].minFrameSize;
        }
    }

    mRCfa = bpfTarget;
  
    Ipp32s q = (Ipp32s)(1. / 1.2 * pow(10.0, (log10((Ipp64f)fsLuma / bpfTarget) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
    BRC_CLIP(q, 1, mQuantMax);

    for (i = mNumTemporalLayers - 1; i >= 0; i--) {
        mBRC[i].mRCq = mBRC[i].mQuant = mBRC[i].mQuantI = mBRC[i].mQuantP = mBRC[i].mQuantB = mBRC[i].mQuantPrev = q;
        mBRC[i].mRCqa = mBRC[i].mRCqa0 = 1. / (Ipp64f)mBRC[i].mRCq;
        mBRC[i].mRCfa = mBRC[i].mBitsDesiredFrame; //??? bpfTarget ?
    }

    return q;
}


Status SVCBRC::InitLayer(VideoBrcParams *brcParams, Ipp32s tid)
{
    Status status = UMC_OK;
    if (!brcParams)
        return UMC_ERR_NULL_PTR;
    if (brcParams->targetBitrate <= 0 || brcParams->info.framerate <= 0)
        return UMC_ERR_INVALID_PARAMS;

    VideoBrcParams  *pmParams = &mParams[tid];
    *pmParams = *brcParams;

    if (pmParams->frameRateExtN && pmParams->frameRateExtD)
        pmParams->info.framerate = (Ipp64f)pmParams->frameRateExtN /  pmParams->frameRateExtD;

    mIsInit = true;
    return status;
}


Status SVCBRC::Init(BaseCodecParams *params, Ipp32s numTemporalLayers)
{
  VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
  Status status = UMC_OK;
  Ipp32s i;
  if (!brcParams)
      return UMC_ERR_INVALID_PARAMS;
  
  if (numTemporalLayers > MAX_TEMP_LEVELS)
      return UMC_ERR_INVALID_PARAMS;

  mNumTemporalLayers = numTemporalLayers;

  for (i = 0; i < numTemporalLayers; i++) {
      status = InitLayer(brcParams + i, i);
      if (status != UMC_OK)
          return status;
  }

  for (i = 0; i < numTemporalLayers; i++) {
      if (mParams[i].HRDBufferSizeBytes != 0) {
          status = InitHRDLayer(i);
          if (status != UMC_OK)
              return status;
//          mHRD[i].mBF = (Ipp64s)mParams[i].HRDInitialDelayBytes * mParams[i].frameRateExtN;
//          mHRD[i].mBFsaved = mHRD[i].mBF;
      } else {
          mHRD[i].bufSize = 0;
      }

      mBRC[i].mBitsDesiredTotal = mBRC[i].mBitsEncodedTotal = 0;
      mBRC[i].mQuantUpdated = 1;
      mBRC[i].mBitsDesiredFrame = (Ipp32s)((Ipp64f)mParams[i].targetBitrate / mParams[i].info.framerate);

/*
      level_ind = ConvertLevelToTable(mParams[i].level);
      if (level_ind < 0)
          return UMC_ERR_INVALID_PARAMS;

      if (level_ind >= H264_LIMIT_TABLE_LEVEL_31 && level_ind <= H264_LIMIT_TABLE_LEVEL_4)
          bitsPerMB = 96.; // 384 / minCR; minCR = 4
      else
          bitsPerMB = 192.; // minCR = 2

      maxMBPS = (Ipp32s)LevelProfileLimits[0][level_ind][H264_LIMIT_TABLE_MAX_MBPS];
      numMBPerFrame = ((mParams[i].info.clip_info.width + 15) >> 4) * ((mParams.info.clip_info.height + 15) >> 4);
      // numMBPerFrame should include ref layers as well - see G10.2.1 !!! ???

      tmpf = (Ipp64f)numMBPerFrame;
      if (tmpf < maxMBPS / 172.)
          tmpf = maxMBPS / 172.;

      mMaxBitsPerPic = (Ipp64u)(tmpf * bitsPerMB) * 8;
      mMaxBitsPerPicNot0 = (Ipp64u)((Ipp64f)maxMBPS / mFramerate * bitsPerMB) * 8;
*/

  }

  mQuantUpdated = 1;
  mQuantMax = 51;
  mQuantUnderflow = -1;
  mQuantOverflow = mQuantMax + 1;

  Ipp32s q = GetInitQP();

  mRCqap = 100;
  mRCfap = 100;
  mRCbap = 100;
  mQuant = mRCq = q;
  mRCqa = mRCqa0 = 1. / (Ipp64f)mRCq;

  for (i = 0; i < mNumTemporalLayers; i++) {
      mBRC[i].mRCqap = mRCqap;
      mBRC[i].mRCfap = mRCfap;
      mBRC[i].mRCbap = mRCbap;
  }


  mPictureFlags = mPictureFlagsPrev = BRC_FRAME;
  mIsInit = true;
  mRecodeInternal = 0;

  return status;
}

Status SVCBRC::Reset(BaseCodecParams *params,  Ipp32s numTemporalLayers)
{
  VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
//  VideoBrcParams tmpParams;

  if (NULL == brcParams)
    return UMC_ERR_NULL_PTR;
  else // tmp !!!
    return Init(params, numTemporalLayers);

#if 0
  Ipp32s bufSize = mHRD.bufSize, maxBitrate = mParams.maxBitrate;
  Ipp64f bufFullness = mHRD.bufFullness;
  Ipp32s bufSize_new = (brcParams->HRDBufferSizeBytes >> 4) << 7;  // coded in bits as value*2^(4+scale), assume scale<=3
  Ipp32s maxBitrate_new = (brcParams->maxBitrate >> 6) << 6;   // In H.264 HRD params bitrate is coded as value*2^(6+scale), we assume scale=0
  Ipp32s targetBitrate_new = brcParams->targetBitrate;
  Ipp32s targetBitrate_new_r = (targetBitrate_new >> 6) << 6;
  Ipp32s rcmode_new = brcParams->BRCMode, rcmode = mRCMode;
  Ipp32s targetBitrate = mBitrate;
  Ipp32s profile_ind, level_ind;
  Ipp32s profile = mParams.profile, level = mParams.level;

  if (BRC_CBR == rcmode_new && BRC_VBR == rcmode)
    return UMC_ERR_INVALID_PARAMS;
  else if (BRC_VBR == rcmode_new && BRC_CBR == rcmode)
    rcmode = rcmode_new;

  if (targetBitrate_new_r > maxBitrate_new && maxBitrate_new > 0)
    return UMC_ERR_INVALID_PARAMS;
  else if (BRC_CBR == rcmode && targetBitrate_new > 0 && maxBitrate_new > 0 && maxBitrate_new != targetBitrate_new_r)
    return UMC_ERR_INVALID_PARAMS;

  if (BRC_CBR == rcmode) {
    if (targetBitrate_new > 0 && maxBitrate_new <= 0)
      maxBitrate_new = targetBitrate_new_r;
    else if (targetBitrate_new <= 0 && maxBitrate_new > 0)
      targetBitrate_new = maxBitrate_new;
  }

  if (targetBitrate_new > 0)
    targetBitrate = targetBitrate_new;

  if (bufSize_new > bufSize)
    bufSize = bufSize_new;

  if (maxBitrate_new > 0 && maxBitrate_new < maxBitrate) {
    bufFullness = mHRD.bufFullness * maxBitrate_new / maxBitrate;
    if ((Ipp64u)mBF < (Ipp64u)IPP_MAX_64S / (Ipp64u)(maxBitrate_new >> 7))
      mBF = (Ipp64s)((Ipp64u)mBF * (maxBitrate_new >> 6) / (mMaxBitrate >> 3));
    else
      mBF = (Ipp64s)((Ipp64u)mBF / (mMaxBitrate >> 3) * (maxBitrate_new >> 6));
    maxBitrate = maxBitrate_new;
  } else if (maxBitrate_new > maxBitrate) {
    if (BRC_VBR == rcmode) {
      Ipp32s isField = ((mPictureFlagsPrev & BRC_FRAME) == BRC_FRAME) ? 0 : 1;
      Ipp64f bf_delta = (maxBitrate_new - maxBitrate) / mFramerate;
      if (isField)
        bf_delta *= 0.5;
      // lower estimate for the fullness with the bitrate updated at tai;
      // for VBR the fullness encoded in buffering period SEI can be below the real buffer fullness
      bufFullness += bf_delta;
      if (bufFullness > (Ipp64f)bufSize - mHRD.roundError)
        bufFullness = (Ipp64f)bufSize - mHRD.roundError;

      mBF += (Ipp64s)((maxBitrate_new >> 3) - mMaxBitrate) * (Ipp64s)mParams.frameRateExtD >> isField;
      if (mBF > (Ipp64s)(bufSize >> 3) * mParams.frameRateExtN)
        mBF = (Ipp64s)(bufSize >> 3) * mParams.frameRateExtN;

      maxBitrate = maxBitrate_new;
    } else { // CBR
      return UMC_ERR_INVALID_PARAMS; // tmp ???
    }
  }

  if (bufSize_new > 0 && bufSize_new < bufSize) {
    bufSize = bufSize_new;
    if (bufFullness > (Ipp64f)bufSize - mHRD.roundError) {
      if (BRC_VBR == rcmode)
        bufFullness = (Ipp64f)bufSize - mHRD.roundError;
      else
        return UMC_ERR_INVALID_PARAMS;
    }
  }

/*
  // ignore new fullness
  if (brcParams->HRDInitialDelayBytes > 0 && (brcParams->HRDInitialDelayBytes << 3) < bufFullness && BRC_VBR == mRCMode) {
    bufFullness = brcParams->HRDInitialDelayBytes << 3;
  }
*/

  if (targetBitrate > maxBitrate)
    targetBitrate = maxBitrate;

  if (brcParams->profile > 0)
    profile = brcParams->profile;
  if (brcParams->level > 0)
    level = brcParams->level;

  profile_ind = ConvertProfileToTable(profile);
  level_ind = ConvertLevelToTable(level);

  if (profile_ind < 0 || level_ind < 0)
    return UMC_ERR_INVALID_PARAMS;

  if (bufSize > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
    return UMC_ERR_INVALID_PARAMS;
  if (maxBitrate > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
    return UMC_ERR_INVALID_PARAMS;

  bool sizeNotChanged = (brcParams->info.clip_info.height == mParams.info.clip_info.height && brcParams->info.clip_info.width == mParams.info.clip_info.width);
  bool gopChanged = (brcParams->GOPPicSize != mParams.GOPPicSize || brcParams->GOPRefDist != mParams.GOPRefDist);

  tmpParams = *brcParams;
  tmpParams.BRCMode = rcmode;
  tmpParams.HRDBufferSizeBytes = bufSize >> 3;
  tmpParams.maxBitrate = maxBitrate;
  tmpParams.HRDInitialDelayBytes = (Ipp32s)(bufFullness / 8);
  tmpParams.targetBitrate = targetBitrate;
  tmpParams.profile = profile;
  tmpParams.level = level;

  Ipp32s bitsDesiredFrameOld = mBitsDesiredFrame;

  status = CommonBRC::Init(&tmpParams);
  if (status != UMC_OK)
    return status;

  mHRD.bufSize = bufSize;
  mHRD.maxBitrate = (Ipp64f)maxBitrate;
  mHRD.bufFullness = bufFullness;
  mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame = mHRD.maxBitrate / mFramerate;
  mMaxBitrate = (Ipp32u)(maxBitrate >> 3);

  Ipp64f bitsPerMB;
  Ipp32s maxMBPS;
  if (level_ind >= H264_LIMIT_TABLE_LEVEL_31 && level_ind <= H264_LIMIT_TABLE_LEVEL_4)
    bitsPerMB = 96.; // 384 / minCR; minCR = 4
  else
    bitsPerMB = 192.; // minCR = 2
  maxMBPS = (Ipp32s)LevelProfileLimits[0][level_ind][H264_LIMIT_TABLE_MAX_MBPS];
  mMaxBitsPerPic = mMaxBitsPerPicNot0 = (Ipp64u)((Ipp64f)maxMBPS / mFramerate * bitsPerMB) * 8;

  if (sizeNotChanged) {
    mRCq = (Ipp32s)(1./mRCqa * pow(mRCfa/mBitsDesiredFrame, 0.32) + 0.5);
    BRC_CLIP(mRCq, 1, mQuantMax);
  } else {
    mRCq = GetInitQP();
  }

  mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = mRCq;
  mRCqa = mRCqa0 = 1./mRCq;
  mRCfa = mBitsDesiredFrame;
  Ipp64f ratio = (Ipp64f)mBitsDesiredFrame / bitsDesiredFrameOld;
  mBitsEncodedTotal = (Ipp32s)(mBitsEncodedTotal * ratio + 0.5);
  mBitsDesiredTotal = (Ipp32s)(mBitsDesiredTotal * ratio + 0.5);

  mSceneChange = 0;
  mBitsEncodedPrev = mBitsDesiredFrame; //???
  mScChFrameCnt = 0;

  if (gopChanged) {
    Ipp32s i;
    SetGOP();

    /* calculating instant bitrate thresholds */
    for (i = 0; i < N_INST_RATE_THRESHLDS; i++)
      instant_rate_thresholds[i]=((Ipp64f)mBitrate*inst_rate_thresh[i]);

    /* calculating deviation from ideal fullness/bitrate thresholds */
    for (i = 0; i < N_DEV_THRESHLDS; i++)
      deviation_thresholds[i]=((Ipp64f)mHRD.bufSize*dev_thresh[i]);

    Ipp64f rc_weight[3] = {I_WEIGHT, P_WEIGHT, B_WEIGHT};
    Ipp64f gopw = N_B_frame * rc_weight[2] + rc_weight[1] * N_P_frame + rc_weight[0];
    Ipp64f gopw_field = (2*N_B_frame * rc_weight[2] + rc_weight[1] * (2*N_P_frame + 1) + rc_weight[0]) * 0.5;

    for (i = 0; i < 3; i++) {
      sRatio[i] = sizeRatio[i] = rc_weight[i] * mGOPPicSize / gopw;
      sizeRatio_field[i] = rc_weight[i] * mGOPPicSize / gopw_field;
    }
  }

  mRecodeInternal = 0;
  return status;

#endif
}


Status SVCBRC::Close()
{
  Status status = UMC_OK;
  if (!mIsInit)
    return UMC_ERR_NOT_INITIALIZED;
  mIsInit = false;
  return status;
}


Status SVCBRC::SetParams(BaseCodecParams* params, Ipp32s tid)
{
    return Init(params, tid);
}

Status SVCBRC::GetParams(BaseCodecParams* params, Ipp32s tid)
{
    VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
    VideoEncoderParams *videoParams = DynamicCast<VideoEncoderParams>(params);


    // ??? 
    if (tid < 0 || tid >= mNumTemporalLayers) {
        for (tid = 0; tid < mNumTemporalLayers; tid++) {
            if (NULL != brcParams) {
                brcParams[tid] = mParams[tid];
            } else if (NULL != videoParams) {
                params[tid] = *(VideoEncoderParams*)&(mParams[tid]);
            } else {
                params[tid] = *(BaseCodecParams*)&(mParams[tid]);
            }
        }
        return UMC_OK;
    }

    if (NULL != brcParams) {
        *brcParams = mParams[tid];
    } else if (NULL != videoParams) {
        *params = *(VideoEncoderParams*)&(mParams[tid]);
    } else {
        *params = *(BaseCodecParams*)&(mParams[tid]);
    }
    return UMC_OK;
};


/*

Status SVCBRC::SetParams(BaseCodecParams* params)
{
  return Init(params);
}
*/

Status SVCBRC::SetPictureFlags(FrameType, Ipp32s picture_structure, Ipp32s, Ipp32s, Ipp32s)
{
  switch (picture_structure & PS_FRAME) {
  case (PS_TOP_FIELD):
    mPictureFlags = BRC_TOP_FIELD;
    break;
  case (PS_BOTTOM_FIELD):
    mPictureFlags = BRC_BOTTOM_FIELD;
    break;
  case (PS_FRAME):
  default:
    mPictureFlags = BRC_FRAME;
  }
  return UMC_OK;
}

Ipp32s SVCBRC::GetQP(FrameType frameType, Ipp32s tid)
{
    if (tid < 0) {
        return mQuant;
    } else if (tid < mNumTemporalLayers)
        return ((frameType == I_PICTURE) ? mBRC[tid].mQuantI : (frameType == B_PICTURE) ? mBRC[tid].mQuantB : mBRC[tid].mQuantP);
    else
        return -1;
}

Status SVCBRC::SetQP(Ipp32s qp, FrameType frameType, Ipp32s tid)
{
    if (tid < 0)
        mQuant = qp;
    else if (tid < mNumTemporalLayers) {
        mBRC[tid].mQuant = qp;

        if (B_PICTURE == frameType) {
            mBRC[tid].mQuantB = qp;
            BRC_CLIP(mBRC[tid].mQuantB, 1, mQuantMax);
        } else {
            mBRC[tid].mRCq = qp;
            BRC_CLIP(mBRC[tid].mRCq, 1, mQuantMax);
            mBRC[tid].mQuantI = mBRC[tid].mQuantP = mRCq;
        }
    } else
        return UMC_ERR_INVALID_PARAMS;

    return UMC_OK;
}

#define BRC_SCENE_CHANGE_RATIO1 10.0
#define BRC_SCENE_CHANGE_RATIO2 5.0


#define BRC_QP_DELTA_WMAX 5
#define  BRC_QP_DELTA_WMIN 5

BRCStatus SVCBRC::PreEncFrame(FrameType picType, Ipp32s, Ipp32s tid)
{ 
    Ipp32s maxfs, minfs, maxfsi, minfsi;
    Ipp32s maxfsind, minfsind;
    Ipp32s i;
    Ipp32s qp, qpmax, qpmin, qpav, qpi, qpminind, qpmaxind, qptop;
    Ipp64f fqpav;
    BRCStatus Sts = BRC_OK;
    Ipp32s cnt;

    mTid = tid;
    if (mHRD[tid].bufSize > 0) {
        maxfs = (Ipp32s)(mHRD[tid].bufFullness - mHRD[tid].minBufFullness);
        minfs = (Ipp32s)(mHRD[tid].inputBitsPerFrame - mHRD[tid].maxBufFullness + mHRD[tid].bufFullness);
        maxfsind = minfsind = tid;
        qpmaxind = qpminind = tid;
        qpav = qpmax = qpmin = (picType == I_PICTURE) ? mBRC[tid].mQuantI : (picType == B_PICTURE) ? mBRC[tid].mQuantB : mBRC[tid].mQuantP;
        cnt = 1;
    } else {
        maxfs = IPP_MAX_32S;
        minfs = 0;
        maxfsind = minfsind = mNumTemporalLayers-1;
        cnt = 0;
        qpav = (picType == I_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantI : (picType == B_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantB : mBRC[mNumTemporalLayers-1].mQuantP;
        qpmax = 101;
        qpmin = -1;
        qpmaxind = qpminind = tid;
    }

    for (i = tid + 1; i < mNumTemporalLayers; i++) {

        if (mHRD[i].bufSize > 0) {

            maxfsi = (Ipp32s)(mHRD[i].bufFullness - mHRD[i].minBufFullness);
            minfsi = (Ipp32s)(mHRD[i].inputBitsPerFrame - mHRD[i].maxBufFullness + mHRD[i].bufFullness);
            qpi = (picType == I_PICTURE) ? mBRC[i].mQuantI : (picType == B_PICTURE) ? mBRC[i].mQuantB : mBRC[i].mQuantP;

            if (maxfsi <= maxfs) {
                maxfs = maxfsi;
                maxfsind = i;
            }
            if (minfsi >= minfs) {
                minfs = minfsi;
                minfsind = i;
            }
            if (qpi >= qpmax) {
                qpmax = qpi;
                qpmaxind = i;
            }
            if (qpi <= qpmin) {
                qpmin = qpi;
                qpminind = i;
            }
            qpav += qpi;
            cnt++;
        }
    }

    if (cnt == 0) {
        mQuant = (picType == I_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantI : (picType == B_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantB : mBRC[mNumTemporalLayers-1].mQuantP;
        mMaxFrameSize = IPP_MAX_32S;
        mMinFrameSize = 0;
        return Sts;
    }

    fqpav = (Ipp64f)qpav / cnt;
    qptop = picType == I_PICTURE ? mBRC[mNumTemporalLayers-1].mQuantI : (picType == B_PICTURE ? mBRC[mNumTemporalLayers-1].mQuantB : mBRC[mNumTemporalLayers-1].mQuantP);
    qp = (qptop * 3 + (Ipp32s)fqpav + 3) >> 2; // ???

    for (i = mNumTemporalLayers - 2; i >= tid; i--) {
        if (mHRD[i].bufSize > 0) {
            maxfsi = (Ipp32s)(mHRD[i].bufFullness - mHRD[i].minBufFullness);
            minfsi = (Ipp32s)(mHRD[i].inputBitsPerFrame - mHRD[i].maxBufFullness + mHRD[i].bufFullness);
            qpi = (picType == I_PICTURE) ? mBRC[i].mQuantI : (picType == B_PICTURE) ? mBRC[i].mQuantB : mBRC[i].mQuantP;

            if (qpi > qp) {
                if (maxfsi < mHRD[i].inputBitsPerFrame)
                    qp = qpi;
                else if (maxfsi < 2*mHRD[i].inputBitsPerFrame)
                    qp = (qp + qpi + 1) >> 1;

            }
            if (qpi < qp) {
                if (minfsi > (Ipp64f)(mHRD[i].inputBitsPerFrame * 0.5))
                    qp = qpi;
                else if (minfsi > (Ipp64f)(mHRD[i].inputBitsPerFrame * 0.25))
                    qp = (qp + qpi) >> 1;
            }
        }
    }

    Ipp32s qpt = (picType == I_PICTURE) ? mBRC[maxfsind].mQuantI : (picType == B_PICTURE) ? mBRC[maxfsind].mQuantB : mBRC[maxfsind].mQuantP;
    if (qp < qpt) {
        if (maxfs < 4*mHRD[maxfsind].inputBitsPerFrame)
            qp = (qp + qpt + 1) >> 1;
    }
    qpt = (picType == I_PICTURE) ? mBRC[minfsind].mQuantI : (picType == B_PICTURE) ? mBRC[minfsind].mQuantB : mBRC[minfsind].mQuantP;
    if (qp > qpt) {
        if (minfs > 0) // ???
            qp = (qp + qpt) >> 1;
    }
    
    if (qp < qpmax - BRC_QP_DELTA_WMAX) {
        if ((Ipp32s)(mHRD[qpmaxind].bufFullness - mHRD[qpmaxind].minBufFullness) < 4*mHRD[qpmaxind].inputBitsPerFrame)
            qp = (qp + qpmax + 1) >> 1;
    }

    if (qp > qpmin + BRC_QP_DELTA_WMIN) {
        if ((mHRD[qpminind].maxBufFullness  - (Ipp32s)mHRD[qpminind].bufFullness) < 2*mHRD[qpminind].inputBitsPerFrame)
            qp = (qp + qpmin + 1) >> 1;
    }

    mQuant = qp;
    mMaxFrameSize = maxfs;
    mMinFrameSize = minfs;

    return Sts;
}

BRCStatus SVCBRC::UpdateAndCheckHRD(Ipp32s tid, Ipp32s frameBits, Ipp32s payloadbits, Ipp32s recode)
{
    BRCStatus ret = BRC_OK;
    BRCSVC_HRDState *pHRD = &mHRD[tid];

    if (!(recode & (BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
        pHRD->prevBufFullness = pHRD->bufFullness;
    } else { // frame is being recoded - restore buffer state
        pHRD->bufFullness = pHRD->prevBufFullness;
    }

    pHRD->maxFrameSize = (Ipp32s)(pHRD->bufFullness - 1);
    pHRD->minFrameSize = (mParams[tid].BRCMode == BRC_VBR ? 0 : (Ipp32s)(pHRD->bufFullness + 2 + pHRD->inputBitsPerFrame - pHRD->bufSize));
    if (pHRD->minFrameSize < 0)
        pHRD->minFrameSize = 0;

    if (frameBits > pHRD->maxFrameSize) {
        ret = BRC_ERR_BIG_FRAME;
    } else if (frameBits < pHRD->minFrameSize) {
        ret = BRC_ERR_SMALL_FRAME;
    } else {
         pHRD->bufFullness += pHRD->inputBitsPerFrame - frameBits;
    }

    if (BRC_OK != ret) {
        if ((recode & BRC_EXT_FRAMESKIP) || BRC_RECODE_EXT_PANIC == recode || BRC_RECODE_PANIC == recode) // no use in changing QP
            ret |= BRC_NOT_ENOUGH_BUFFER;
        else {
            ret = UpdateQuantHRD(tid, frameBits, ret, payloadbits);
        }
        mBRC[tid].mQuantUpdated = 0;
    }

    return ret;
}

Status SVCBRC::GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits) {
    Ipp32s i, minfs = 0, maxfs = IPP_MAX_32S;
    for (i = mTid; i < mNumTemporalLayers; i++) {
        if (mHRD[i].bufSize > 0) {
            if (mHRD[i].minFrameSize > minfs)
                minfs = mHRD[i].minFrameSize;
            if (mHRD[i].maxFrameSize < maxfs)
                maxfs = mHRD[i].maxFrameSize;
        }
    }
    if (minFrameSizeInBits)
        *minFrameSizeInBits = minfs;
    if (maxFrameSizeInBits)
        *maxFrameSizeInBits = maxfs;
    return UMC_OK;
};


BRCStatus SVCBRC::PostPackFrame(FrameType picType, Ipp32s totalFrameBits, Ipp32s payloadBits, Ipp32s repack, Ipp32s /* poc */)
{
  BRCStatus Sts = BRC_OK;
  Ipp32s bitsEncoded = totalFrameBits - payloadBits;

  mBitsEncoded = bitsEncoded;

  mOversize = bitsEncoded - mMaxFrameSize;
  mUndersize = mMinFrameSize - bitsEncoded;

  if (mOversize > 0)
      Sts |= BRC_BIG_FRAME;
  if (mUndersize > 0)
      Sts |= BRC_SMALL_FRAME;

  if (Sts & BRC_BIG_FRAME) {
      if (mQuant < mQuantUnderflow - 1) {
          mQuant++;
          mQuantUpdated = 0;
          return BRC_ERR_BIG_FRAME; // ???
      }
  } else if (Sts & BRC_SMALL_FRAME) {
      if (mQuant > mQuantOverflow + 1) {
          mQuant--;
          mQuantUpdated = 0;
          return BRC_ERR_SMALL_FRAME;
      }
  }

  Sts = BRC_OK;
  Ipp32s quant = mQuant;
  Ipp32s i;
  Ipp32s tid = mTid;

  for (i = tid; i < mNumTemporalLayers; i++) {
      if (mHRD[i].bufSize > 0) {
          BRCStatus sts = UpdateAndCheckHRD(i, totalFrameBits, payloadBits, repack);
          Sts |= sts;
          if ((sts & BRC_ERR_BIG_FRAME) && (mBRC[i].mQuant > quant))
                quant = mBRC[i].mQuant;
          else if ((sts & BRC_ERR_SMALL_FRAME) && (mBRC[i].mQuant < quant))
                quant = mBRC[i].mQuant;
      }
  }

  if (Sts != BRC_OK) {
      if (((Sts & BRC_ERR_BIG_FRAME) && (Sts & BRC_ERR_SMALL_FRAME)) || (Sts & BRC_NOT_ENOUGH_BUFFER)) {
          return BRC_NOT_ENOUGH_BUFFER;
      }
      if (Sts & BRC_ERR_BIG_FRAME)
          mQuantUnderflow = mQuant;
      if (Sts & BRC_ERR_SMALL_FRAME)
          mQuantOverflow = mQuant;

      if (quant != mQuant)
          mQuant = quant;
      else
          mQuant = quant + ((Sts & BRC_ERR_BIG_FRAME) ? 1 : -1);

      mQuantUpdated = 0;
      return Sts;
  }


  Ipp32s j;
  if (mHRD[tid].bufSize > 0) {
      mHRD[tid].minBufFullness = 0;
      mHRD[tid].maxBufFullness = mHRD[tid].bufSize - mHRD[tid].inputBitsPerFrame;
  }

  for (i = tid; i < mNumTemporalLayers; i++) {
      if (mHRD[i].bufSize > 0) {
          mHRD[i].minFrameSize = (mParams[i].BRCMode == BRC_VBR ? 0 : (Ipp32s)(mHRD[i].bufFullness + 1 + 1 + mHRD[i].inputBitsPerFrame - mHRD[i].bufSize));

          for (j = i + 1; j < mNumTemporalLayers; j++) {
              if (mHRD[j].bufSize > 0) {
                  Ipp64f minbf, maxbf;
                  minbf = mHRD[i].minFrameSize - mHRD[j].inputBitsPerFrame;
                  maxbf = mHRD[j].bufSize - mHRD[j].inputBitsPerFrame + mHRD[i].bufFullness - mHRD[j].inputBitsPerFrame;

                  if (minbf > mHRD[j].minBufFullness)
                      mHRD[j].minBufFullness = minbf;

                  if (maxbf < mHRD[j].maxBufFullness)
                      mHRD[j].maxBufFullness = maxbf;
              }
          }
      }
  }

  for (i = tid; i < mNumTemporalLayers; i++) {
      if (mHRD[i].bufSize > 0) {
          if (mHRD[i].bufFullness - mHRD[i].inputBitsPerFrame < mHRD[i].minBufFullness)
              Sts |= BRC_BIG_FRAME;
          if (mHRD[i].bufFullness - mHRD[i].inputBitsPerFrame > mHRD[i].maxBufFullness)
              Sts |= BRC_SMALL_FRAME;
      }
  }

  if ((Sts & BRC_BIG_FRAME) && (Sts & BRC_SMALL_FRAME))
      Sts = BRC_OK; // don't do anything, everything is already bad

  if (Sts & BRC_BIG_FRAME) {
      if (mQuant < mQuantUnderflow - 1) {
          mQuant++;
          mQuantUpdated = 0;
          return BRC_ERR_BIG_FRAME; // ???
      }
  } else if (Sts & BRC_SMALL_FRAME) {
      if (mQuant > mQuantOverflow + 1) {
          mQuant--;
          mQuantUpdated = 0;
          return BRC_ERR_SMALL_FRAME;
      }
  }
  Sts = BRC_OK;

  mQuantOverflow = mQuantMax + 1;
  mQuantUnderflow = -1;

  mFrameType = picType;

  {
    if (repack != BRC_RECODE_PANIC && repack != BRC_RECODE_EXT_PANIC) {

      for (i = mNumTemporalLayers - 1; i >= tid; i--) {
          UpdateQuant(i, bitsEncoded, totalFrameBits);
      }

      mQuantP = mQuantI = mRCq = mQuant; // ???
    }
    mQuantUpdated = 1;
  }

  return Sts;
};

BRCStatus SVCBRC::UpdateQuant(Ipp32s tid, Ipp32s bEncoded, Ipp32s totalPicBits)
{
  BRCStatus Sts = BRC_OK;
  Ipp64f  bo, qs, dq;
  Ipp32s  quant;
  Ipp32s isfield = ((mPictureFlags & BRC_FRAME) != BRC_FRAME) ? 1 : 0;
  Ipp64s totalBitsDeviation;
  BRC_SVCLayer_State *pBRC = &mBRC[tid];
  Ipp32u bitsPerPic = (Ipp32u)pBRC->mBitsDesiredFrame >> isfield;

  if (isfield)
    pBRC->mRCfa *= 0.5;

  quant = mQuant;
  if (mQuantUpdated)
      pBRC->mRCqa += (1. / quant - pBRC->mRCqa) / mRCqap;
  else {
      if (pBRC->mQuantUpdated)
          pBRC->mRCqa += (1. / quant - pBRC->mRCqa) / (mRCqap > 25 ? 25 : mRCqap);
      else
          pBRC->mRCqa += (1. / quant - pBRC->mRCqa) / (mRCqap > 10 ? 10 : mRCqap);
  }

  if (mRecodeInternal) {
    pBRC->mRCfa = bitsPerPic;
    pBRC->mRCqa = mRCqa0;
  }
  mRecodeInternal = 0;

  pBRC->mBitsEncodedTotal += totalPicBits;
  pBRC->mBitsDesiredTotal += bitsPerPic;
  totalBitsDeviation = pBRC->mBitsEncodedTotal - pBRC->mBitsDesiredTotal;
  
  if (mFrameType != I_PICTURE || mParams[tid].BRCMode == BRC_CBR || mQuantUpdated == 0)
    pBRC->mRCfa += (bEncoded - pBRC->mRCfa) / pBRC->mRCfap;

  pBRC->mQuantB = ((pBRC->mQuantP + pBRC->mQuantPrev) * 563 >> 10) + 1; \
  BRC_CLIP(pBRC->mQuantB, 1, mQuantMax);

  if (pBRC->mQuantUpdated == 0)
    if (pBRC->mQuantB < quant)
      pBRC->mQuantB = quant;
  qs = pow(bitsPerPic / pBRC->mRCfa, 2.0);
  dq = pBRC->mRCqa * qs;

  bo = (Ipp64f)totalBitsDeviation / pBRC->mRCbap / pBRC->mBitsDesiredFrame; // ??? bitsPerPic ?
  BRC_CLIP(bo, -1.0, 1.0);

  dq = dq + (1./mQuantMax - dq) * bo;
  BRC_CLIP(dq, 1./mQuantMax, 1./1.);
  quant = (int) (1. / dq + 0.5);


  if (quant >= pBRC->mRCq + 5)
    quant = pBRC->mRCq + 3;
  else if (quant >= pBRC->mRCq + 3)
    quant = pBRC->mRCq + 2;
  else if (quant > pBRC->mRCq + 1)
    quant = pBRC->mRCq + 1;
  else if (quant <= pBRC->mRCq - 5)
    quant = pBRC->mRCq - 3;
  else if (quant <= pBRC->mRCq - 3)
    quant = pBRC->mRCq - 2;
  else if (quant < pBRC->mRCq - 1)
    quant = pBRC->mRCq - 1;

  pBRC->mRCq = quant;

  if (isfield)
    pBRC->mRCfa *= 2;


  if (mFrameType != B_PICTURE) {
      pBRC->mQuantPrev = pBRC->mQuantP;
      pBRC->mBitsEncodedP = mBitsEncoded;
  }

  pBRC->mQuant = pBRC->mQuantP = pBRC->mQuantI =  pBRC->mRCq;
  pBRC->mQuantUpdated = 1;

  return Sts;
}


BRCStatus SVCBRC::UpdateQuantHRD(Ipp32s tid, Ipp32s totalFrameBits, BRCStatus sts, Ipp32s payloadBits)
{
  Ipp32s quant, quant_prev;
  Ipp64f qs;
  BRCSVC_HRDState *pHRD = &mHRD[tid];
  Ipp32s wantedBits = (sts == BRC_ERR_BIG_FRAME ? pHRD->maxFrameSize : pHRD->minFrameSize);
  Ipp32s bEncoded = totalFrameBits - payloadBits;

  wantedBits -= payloadBits;
  if (wantedBits <= 0) // possible only if BRC_ERR_BIG_FRAME
    return (sts | BRC_NOT_ENOUGH_BUFFER);

  quant_prev = quant = mQuant;
  if (sts & BRC_ERR_BIG_FRAME)
      mQuantUnderflow = quant;
  else if (sts & BRC_ERR_SMALL_FRAME)
      mQuantOverflow = quant;

  qs = pow((Ipp64f)bEncoded/wantedBits, 1.5);
  quant = (Ipp32s)(quant * qs + 0.5);

  if (quant == quant_prev)
    quant += (sts == BRC_ERR_BIG_FRAME ? 1 : -1);

  BRC_CLIP(quant, 1, mQuantMax);

  if (quant < quant_prev) {
      while (quant <= mQuantUnderflow)
          quant++;
  } else {
      while (quant >= mQuantOverflow)
          quant--;
  }

  if (quant == quant_prev)
    return (sts | BRC_NOT_ENOUGH_BUFFER);

  mBRC[tid].mQuant = quant;

  switch (mFrameType) {
  case (I_PICTURE):
    mBRC[tid].mQuantI = quant;
    break;
  case (B_PICTURE):
    mBRC[tid].mQuantB = quant;
    break;
  case (P_PICTURE):
  default:
    mBRC[tid].mQuantP = quant;
  }
  return sts;
}

Status SVCBRC::GetHRDBufferFullness(Ipp64f *hrdBufFullness, Ipp32s recode, Ipp32s tid)
{
    *hrdBufFullness = (recode & (BRC_EXT_FRAMESKIP - 1)) ? mHRD[tid].prevBufFullness : mHRD[tid].bufFullness;

    return UMC_OK;
}

/*
Status SVCBRC::GetInitialCPBRemovalDelay(Ipp32s tid, Ipp32u *initial_cpb_removal_delay, Ipp32s recode)
{
  Ipp32u cpb_rem_del_u32;
  Ipp64u cpb_rem_del_u64, temp1_u64, temp2_u64;

  if (BRC_VBR == mRCMode) {
    if (recode)
      mBF = mBFsaved;
    else
      mBFsaved = mBF;
  }

  temp1_u64 = (Ipp64u)mBF * 90000;
  temp2_u64 = (Ipp64u)mMaxBitrate * mParams.frameRateExtN;
  cpb_rem_del_u64 = temp1_u64 / temp2_u64;
  cpb_rem_del_u32 = (Ipp32u)cpb_rem_del_u64;

  if (BRC_VBR == mRCMode) {
    mBF = temp2_u64 * cpb_rem_del_u32 / 90000;
    temp1_u64 = (Ipp64u)cpb_rem_del_u32 * mMaxBitrate;
    Ipp32u dec_buf_ful = (Ipp32u)(temp1_u64 / (90000/8));
    if (recode)
      mHRD.prevBufFullness = (Ipp64f)dec_buf_ful;
    else
      mHRD.bufFullness = (Ipp64f)dec_buf_ful;
  }

  *initial_cpb_removal_delay = cpb_rem_del_u32;
  return UMC_OK;
}
*/
}