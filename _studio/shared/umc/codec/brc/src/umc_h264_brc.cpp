/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
//
//                     H.264 bitrate control
//
*/
#include <math.h>
#include "umc_h264_brc.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_tables.h"

namespace UMC
{

H264BRC::H264BRC()
{
  mIsInit = 0;
  mRCqap = 0;
  mRCqa = 0;
  mRCbap = 0;
  mBitsDesiredTotal = 0;
  mQuantPprev = 0;
  mRCqa0 = 0;
  mQuantBprev = 0;
  mRCfap = 0;
  mBitDepth = 0;
  mPoc = 0;
  mMaxBitrate = 0;
  mQuantIprev = 0;
  mBFsaved = 0;
  mBitsEncodedP = 0;
  mSChPoc = 0;
  mBitsEncodedTotal = 0;
  mPictureFlags = 0;
  mMaxBitsPerPic = 0;
  mRCfa_short = 0;
  mRCfa = 0;
  mQuantOffset = 0;
  mQuantMax = 0;
  mQuantPrev = 0;
  mQuantMin = 0;
  mQuantP = 0;
  mQuantB = 0;
  mQuantI = 0;
  mSceneChange = 0;
  mPictureFlagsPrev = 0;
  mBitsEncodedPrev = 0;
  mRCq = 0;
  mBitsEncoded = 0;
  mQPprev = 0;
  mRecode = 0;
  mBF = 0;
  mMaxBitsPerPicNot0 = 0;

}

H264BRC::~H264BRC()
{
  Close();
}


// Copy of mfx_h264_enc_common.cpp::LevelProfileLimitsNal
const Ipp64u LevelProfileLimits[4][H264_LIMIT_TABLE_LEVEL_MAX+1][6] = {
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
        /* 52 */ {   2073600,  36864, 70778880,  288000000, 288000000, 512},
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
        /* 52 */ {   2073600,  36864, 70778880,  360000000, 360000000, 512},
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
        /* 52 */ {   2073600,  36864, 70778880,  864000000, 864000000, 512},
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
        /* 52 */ {   2073600,  36864, 70778880,  1152000000,1152000000,512},
    },
};

static Ipp64f QP2Qstep (Ipp32s QP)
{
  return (Ipp64f)(pow(2.0, (QP - 4.0) / 6.0));
}

static Ipp32s Qstep2QP (Ipp64f Qstep)
{
  return (Ipp32s)(4.0 + 6.0 * log(Qstep) / log(2.0));
}

Status H264BRC::InitHRD()
{
  Ipp32s profile_ind, level_ind;
  Ipp64u bufSizeBits = mParams.HRDBufferSizeBytes << 3;
  Ipp64u maxBitrate = mParams.maxBitrate;
  Ipp32s bitsPerFrame;

  if (mFramerate <= 0)
    return UMC_ERR_INVALID_PARAMS;

  bitsPerFrame = (Ipp32s)(mBitrate / mFramerate);

  if (BRC_CBR == mRCMode)
    maxBitrate = mParams.maxBitrate = mParams.targetBitrate;
  if (maxBitrate < (Ipp64u)mParams.targetBitrate)
    maxBitrate = mParams.maxBitrate = 0;

  if (bufSizeBits > 0 && bufSizeBits < static_cast<Ipp64u>(bitsPerFrame << 1))
    bufSizeBits = (bitsPerFrame << 1);

  profile_ind = ConvertProfileToTable(mParams.profile);
  level_ind = ConvertLevelToTable(mParams.level);

  if (mParams.targetBitrate > (Ipp32s)LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR])
    mParams.targetBitrate = (Ipp32s)LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR];
  if (static_cast<Ipp64u>(mParams.maxBitrate) > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR])
    maxBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR];
  if (bufSizeBits > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_CPB])
    bufSizeBits = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_CPB];

  if (mParams.maxBitrate <= 0 && mParams.HRDBufferSizeBytes <= 0) {
    if (profile_ind < 0) {
      profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
      level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
    } else if (level_ind < 0)
      level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
    maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
    bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
  } else if (mParams.HRDBufferSizeBytes <= 0) {
    if (profile_ind < 0)
      bufSizeBits = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_CPB];
    else if (level_ind < 0) {
      for (; profile_ind < H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++)
        if (maxBitrate <= LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR])
          break;
      bufSizeBits = LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_CPB];
    } else {
      for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
        for (; level_ind <= H264_LIMIT_TABLE_LEVEL_MAX; level_ind++) {
          if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
            break;
        }
        if (level_ind <= H264_LIMIT_TABLE_LEVEL_MAX)
          break;
        level_ind = 0;
      }
      if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
        profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
        level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
      }
      bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
    }
  } else if (mParams.maxBitrate <= 0) {
    if (profile_ind < 0)
      maxBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR];
    else if (level_ind < 0) {
      for (; profile_ind < H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++)
        if (bufSizeBits <= LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_CPB])
          break;
      maxBitrate = LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR];
    } else {
      for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
        for (; level_ind <= H264_LIMIT_TABLE_LEVEL_MAX; level_ind++) {
          if (bufSizeBits <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
            break;
        }
        if (level_ind <= H264_LIMIT_TABLE_LEVEL_MAX)
          break;
        level_ind = 0;
      }
      if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
        profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
        level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
      }
      maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
    }
  }

  if (maxBitrate < (Ipp64u)mParams.targetBitrate) {
    maxBitrate = (Ipp64u)mParams.targetBitrate;
    for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
      for (; level_ind <= H264_LIMIT_TABLE_LEVEL_MAX; level_ind++) {
        if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
          break;
      }
      if (level_ind <= H264_LIMIT_TABLE_LEVEL_MAX)
        break;
      level_ind = 0;
    }
    if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
      profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
      level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
    }
    bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
  }
  mParams.HRDBufferSizeBytes = (Ipp32s)(bufSizeBits >> 3);
  mParams.maxBitrate = (Ipp32s)((maxBitrate >> 6) << 6);  // In H.264 HRD params bitrate is coded as value*2^(6+scale), we assume scale=0
  mHRD.maxBitrate = mParams.maxBitrate;
  mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame = mHRD.maxBitrate / mFramerate;

  mParams.HRDBufferSizeBytes = (mParams.HRDBufferSizeBytes >> 4) << 4; // coded in bits as value*2^(4+scale), assume scale<=3
  if (mParams.HRDInitialDelayBytes <= 0)
    mParams.HRDInitialDelayBytes = (BRC_CBR == mRCMode ? mParams.HRDBufferSizeBytes/2 : mParams.HRDBufferSizeBytes);
  else if (mParams.HRDInitialDelayBytes * 8 < bitsPerFrame)
    mParams.HRDInitialDelayBytes = bitsPerFrame / 8;
  if (mParams.HRDInitialDelayBytes > mParams.HRDBufferSizeBytes)
    mParams.HRDInitialDelayBytes = mParams.HRDBufferSizeBytes;
  mHRD.bufSize = mParams.HRDBufferSizeBytes * 8;
  mHRD.bufFullness = mParams.HRDInitialDelayBytes * 8;
  mHRD.underflowQuant = -1;
  mHRD.frameNum = 0;
  mHRD.roundError = 1;

  return UMC_OK;
}

Ipp32s H264BRC::GetInitQP()
{
  const Ipp64f x0 = 0, y0 = 1.19, x1 = 1.75, y1 = 1.75;
  Ipp32s fs, fsLuma;

  fsLuma = mParams.info.clip_info.width * mParams.info.clip_info.height;
  fs = fsLuma;
  if (mParams.info.color_format == YUV420)
    fs += fsLuma / 2;
  else if (mParams.info.color_format == YUV422)
    fs += fsLuma;
  else if (mParams.info.color_format == YUV444)
    fs += fsLuma * 2;
  fs = fs * mBitDepth / 8;
  Ipp32s q = (Ipp32s)(1. / 1.2 * pow(10.0, (log10(fs * 2. / 3. * mParams.info.framerate / mBitrate) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
  BRC_CLIP(q, 1, mQuantMax);
  return q;
}


Status H264BRC::Init(BaseCodecParams *params, Ipp32s enableRecode)
{
  Status status = UMC_OK;
  Ipp32s level_ind;
  Ipp32s maxMBPS;
  Ipp32s numMBPerFrame;
  Ipp64f bitsPerMB, tmpf;

  status = CommonBRC::Init(params);
  if (status != UMC_OK)
    return status;

  mRecode = enableRecode ? 1 : 0;

  if (mParams.frameRateExtN_1 > 0) {
      if (mParams.frameRateExtN == mParams.frameRateExtN_1 * 2) {
          mParams.frameRateExtN = mParams.frameRateExtN_1;
          mParams.frameRateExtN_1 = 0;
          mFramerate *= 0.5;
          mBitsDesiredFrame *= 2;
      } else {
          mBitsDesiredFrame = mBitrate / ((mParams.frameRateExtN - mParams.frameRateExtN_1) / mParams.frameRateExtD);
      }
  }

  if (mParams.HRDBufferSizeBytes != 0) {
    status = InitHRD();
    mMaxBitrate = mParams.maxBitrate >> 3;
    mBF = (Ipp64s)mParams.HRDInitialDelayBytes * mParams.frameRateExtN;
    mBFsaved = mBF;
  } else { // no HRD
    mHRD.bufSize = mHRD.maxFrameSize = IPP_MAX_32S;
    mHRD.bufFullness = (Ipp64f)mHRD.bufSize/2; // just need buffer fullness not to trigger any hrd related actions
    mHRD.minFrameSize = 0;
  }

 if (status != UMC_OK)
    return status;

  if (mBitrate <= 0 || mFramerate <= 0)
    return UMC_ERR_INVALID_PARAMS;

  level_ind = ConvertLevelToTable(mParams.level);
  if (level_ind < 0)
    return UMC_ERR_INVALID_PARAMS;

  if (level_ind >= H264_LIMIT_TABLE_LEVEL_31 && level_ind <= H264_LIMIT_TABLE_LEVEL_42)
    bitsPerMB = 96.; // 384 / minCR; minCR = 4
  else
    bitsPerMB = 192.; // minCR = 2

  maxMBPS = (Ipp32s)LevelProfileLimits[0][level_ind][H264_LIMIT_TABLE_MAX_MBPS];
  numMBPerFrame = ((mParams.info.clip_info.width + 15) >> 4) * ((mParams.info.clip_info.height + 15) >> 4);

  tmpf = (Ipp64f)numMBPerFrame;
  if (tmpf < maxMBPS / 172.)
    tmpf = maxMBPS / 172.;

  mMaxBitsPerPic = (Ipp64u)(tmpf * bitsPerMB) * 8;
  mMaxBitsPerPicNot0 = (Ipp64u)((Ipp64f)maxMBPS / mFramerate * bitsPerMB) * 8;

  mBitDepth = 8; // hard coded for now
  mQuantOffset = 6 * (mBitDepth - 8);
  mQuantMax = 51 + mQuantOffset;
  mQuantMin = 1;

  mBitsDesiredTotal = 0;
  mBitsEncodedTotal = 0;
  mBitsDesiredFrame = (Ipp32s)((Ipp64f)mBitrate / mFramerate);
  if (mBitsDesiredFrame < 10)
    return UMC_ERR_INVALID_PARAMS;

  mQuantUpdated = 1;

  Ipp32s q = GetInitQP();

  if (!mRecode) {
      if (q - 6 > 10)
        mQuantMin = IPP_MAX(10, q - 24);
      else
        mQuantMin = IPP_MAX(q - 6, 2);

      if (q < mQuantMin)
        q = mQuantMin;
  }

  mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = q;
  mRCqap = 100;
  mRCfap = 100;
  mRCbap = 100;

  mRCq = q;
  mRCqa = mRCqa0 = 1. / (Ipp64f)mRCq;
  mRCfa = mBitsDesiredFrame;
  mRCfa_short = mBitsDesiredFrame;

  mPictureFlags = mPictureFlagsPrev = BRC_FRAME;
  mIsInit = true;

  mSChPoc = 0;
  mSceneChange = 0;
  mBitsEncodedPrev = mBitsDesiredFrame; //???
  mFrameType = I_PICTURE;

  return status;
}

Status H264BRC::Reset(BaseCodecParams *params, Ipp32s enableRecode)
{
  Status status = UMC_OK;
  VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
  VideoBrcParams tmpParams;

  if (NULL == brcParams)
    return UMC_ERR_NULL_PTR;

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

  mRecode = enableRecode ? 1 : 0;

  if (BRC_AVBR != rcmode_new && BRC_AVBR == rcmode)
    return UMC_ERR_INVALID_PARAMS;
  if (BRC_CBR == rcmode_new && BRC_VBR == rcmode)
    return UMC_ERR_INVALID_PARAMS;
  rcmode = rcmode_new;

  if (BRC_AVBR == rcmode) {
      targetBitrate_new_r = targetBitrate_new;
      maxBitrate_new = maxBitrate = 0;
      bufSize_new  = bufSize = 0;
  }

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
    } else if (BRC_CBR == rcmode) {
      return UMC_ERR_INVALID_PARAMS; // tmp ???
    }
  }

  if (BRC_AVBR != rcmode && bufSize_new > 0 && bufSize_new < bufSize) {
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

  if (targetBitrate > maxBitrate && BRC_AVBR != rcmode)
    targetBitrate = maxBitrate;

  if (brcParams->profile > 0)
    profile = brcParams->profile;
  if (brcParams->level > 0)
    level = brcParams->level;

  profile_ind = ConvertProfileToTable(profile);
  level_ind = ConvertLevelToTable(level);

  if (profile_ind < 0 || level_ind < 0)
    return UMC_ERR_INVALID_PARAMS;

  if (static_cast<Ipp64u>(bufSize) > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
    return UMC_ERR_INVALID_PARAMS;
  if (static_cast<Ipp64u>(maxBitrate) > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
    return UMC_ERR_INVALID_PARAMS;

  bool sizeNotChanged = (brcParams->info.clip_info.height == mParams.info.clip_info.height && brcParams->info.clip_info.width == mParams.info.clip_info.width);

  tmpParams = *brcParams;
  tmpParams.BRCMode = rcmode;
  tmpParams.HRDBufferSizeBytes = bufSize >> 3;
  tmpParams.maxBitrate = maxBitrate;
  tmpParams.HRDInitialDelayBytes = (Ipp32s)(bufFullness / 8);
  tmpParams.targetBitrate = targetBitrate;
  tmpParams.profile = profile;
  tmpParams.level = level;

  if (BRC_AVBR == rcmode) // just in case
      tmpParams.HRDBufferSizeBytes = 0;

  Ipp32s bitsDesiredFrameOld = mBitsDesiredFrame;

  status = CommonBRC::Init(&tmpParams);
  if (status != UMC_OK)
    return status;

  if (mParams.HRDBufferSizeBytes > 0) {
    mHRD.bufSize = bufSize;
    mHRD.maxBitrate = (Ipp64f)maxBitrate;
    mHRD.bufFullness = bufFullness;
    mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame = mHRD.maxBitrate / mFramerate;
    mMaxBitrate = (Ipp32u)(maxBitrate >> 3);
  } else {
    mHRD.bufSize = mHRD.maxFrameSize = IPP_MAX_32S;
    mHRD.bufFullness = (Ipp64f)mHRD.bufSize/2; // just need buffer fullness not to trigger any hrd related actions
    mHRD.minFrameSize = 0;
  }

  Ipp64f bitsPerMB;
  Ipp32s maxMBPS;
  if (level_ind >= H264_LIMIT_TABLE_LEVEL_31 && level_ind <= H264_LIMIT_TABLE_LEVEL_42)
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
    if (!mRecode) {
        if (mRCq - 6 > 10)
            mQuantMin = IPP_MAX(10, mRCq - 24);
        else
            mQuantMin = IPP_MAX(mRCq - 6, 2);
        if (mRCq < mQuantMin)
            mRCq = mQuantMin;
    }
  }

  mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = mRCq;
  mRCqa = mRCqa0 = 1./mRCq;
  mRCfa = mBitsDesiredFrame;
  mRCfa_short = mBitsDesiredFrame;
  Ipp64f ratio = (Ipp64f)mBitsDesiredFrame / bitsDesiredFrameOld;
  mBitsEncodedTotal = (Ipp32s)(mBitsEncodedTotal * ratio + 0.5);
  mBitsDesiredTotal = (Ipp32s)(mBitsDesiredTotal * ratio + 0.5);

  mSceneChange = 0;
  mBitsEncodedPrev = mBitsDesiredFrame; //???

  return status;
}


Status H264BRC::Close()
{
  Status status = UMC_OK;
  if (!mIsInit)
    return UMC_ERR_NOT_INITIALIZED;
  mIsInit = false;
  return status;
}


Status H264BRC::SetParams(BaseCodecParams* params, Ipp32s)
{
  return Init(params);
}

Status H264BRC::SetPictureFlags(FrameType, Ipp32s picture_structure, Ipp32s, Ipp32s, Ipp32s)
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

Ipp32s H264BRC::GetQP(FrameType frameType, Ipp32s)
{
  return ((frameType == I_PICTURE) ? mQuantI : (frameType == B_PICTURE) ? mQuantB : mQuantP) - mQuantOffset;
}

Status H264BRC::SetQP(Ipp32s qp, FrameType frameType, Ipp32s)
{
  if (B_PICTURE == frameType) {
    mQuantB = qp + mQuantOffset;
    BRC_CLIP(mQuantB, 1, mQuantMax);
  } else {
    mRCq = qp + mQuantOffset;
    BRC_CLIP(mRCq, 1, mQuantMax);
    mQuantI = mQuantP = mRCq;
  }
  return UMC_OK;
}

#define BRC_SCENE_CHANGE_RATIO1 20.0
#define BRC_SCENE_CHANGE_RATIO2 10.0

#define BRC_RCFAP_SHORT 5

BRCStatus H264BRC::PostPackFrame(FrameType picType, Ipp32s totalFrameBits, Ipp32s payloadBits, Ipp32s repack, Ipp32s poc)
{
    BRCStatus Sts = BRC_OK;
    Ipp32s bitsEncoded = totalFrameBits - payloadBits;
    Ipp64f e2pe;
    Ipp32s qp, qpprev;
    Ipp64f qs, qstep;
    FrameType prevFrameType = mFrameType;

    if (mBitrate == 0)
        return Sts;

    mPoc = poc;

    if (!repack && mQuantUpdated <= 0) { // BRC reported buffer over/underflow but the application ignored it
        mQuantI = mQuantIprev;
        mQuantP = mQuantPprev;
        mQuantB = mQuantBprev;
        mPictureFlags = mPictureFlagsPrev;
        mRecode |= 2; // ???
        UpdateQuant(mBitsEncoded, totalFrameBits);
    }

    mQuantIprev = mQuantI;
    mQuantPprev = mQuantP;
    mQuantBprev = mQuantB;
    mPictureFlagsPrev = mPictureFlags;

    mBitsEncoded = bitsEncoded;

    Ipp32s isField = ((mPictureFlags & BRC_FRAME) == BRC_FRAME) ? 0 : 1;

    if (mSceneChange)
        if (mQuantUpdated == 1 && poc > mSChPoc + 1)
            mSceneChange &= ~16;

    qpprev = qp = (picType == I_PICTURE) ? mQuantI : (picType == B_PICTURE) ? mQuantB : mQuantP;

    Ipp64f buffullness = repack ? mHRD.prevBufFullness : mHRD.bufFullness;
    if (mParams.HRDBufferSizeBytes > 0) {
        mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame;
        if (isField)
            mHRD.inputBitsPerFrame *= 0.5;
        Sts = UpdateAndCheckHRD(totalFrameBits, repack);
    }

    Ipp64f fa_short0 = mRCfa_short;
    mRCfa_short += (bitsEncoded - mRCfa_short) / BRC_RCFAP_SHORT;

    Ipp64f frameFactor = 1.0;
    Ipp64f targetFrameSize = IPP_MAX((Ipp64f)mBitsDesiredFrame, mRCfa);
    if (isField) targetFrameSize *= 0.5; 
    
    qstep = QP2Qstep(qp);
    Ipp64f qstep_prev = QP2Qstep(mQPprev);
    e2pe = bitsEncoded * sqrt(qstep) / (mBitsEncodedPrev * sqrt(qstep_prev));
    e2pe *= frameFactor;
    Ipp64f maxFrameSize;

    maxFrameSize = 2.5/9. * buffullness + 5./9. * targetFrameSize;
    BRC_CLIP(maxFrameSize, targetFrameSize, BRC_SCENE_CHANGE_RATIO2 * targetFrameSize);

    Ipp64f famax = 1./9. * buffullness + 8./9. * mRCfa  ;

    if (bitsEncoded >  maxFrameSize && qp < mQuantMax) {

        Ipp64f targetSizeScaled = maxFrameSize * 0.8;
        Ipp64f qstepnew = qstep * bitsEncoded / targetSizeScaled;
        Ipp32s qpnew = Qstep2QP(qstepnew);
        if (qpnew == qp)
          qpnew++;
        BRC_CLIP(qpnew, 1, mQuantMax);
        mRCq = mQuantI = mQuantP = qpnew; 
        if (picType == B_PICTURE)
            mQuantB = qpnew;
        else
            SetQuantB();

        mRCfa_short = fa_short0;

        if (e2pe > BRC_SCENE_CHANGE_RATIO1) { // scene change, resetting BRC statistics
          mRCfa = mBitsDesiredFrame;
          mRCqa = 1./qpnew;
          mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;
          mSceneChange |= 1;
          if (picType != B_PICTURE) {
              mSceneChange |= 16;
              mSChPoc = poc;
          }
          mRCfa_short = mBitsDesiredFrame;
        }
        if (mRecode) {
            mQuantUpdated = 0;
            mHRD.frameNum--;
            return BRC_ERR_BIG_FRAME;
        }
    }

    if (mRCfa_short > famax && (!repack) && qp < mQuantMax) {

        Ipp64f qstepnew = qstep * mRCfa_short / (famax * 0.8);
        Ipp32s qpnew = Qstep2QP(qstepnew);
        if (qpnew == qp)
            qpnew++;
        BRC_CLIP(qpnew, 1, mQuantMax);

        mRCfa = mBitsDesiredFrame;
        mRCqa = 1./qpnew;
        mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;

        mRCfa_short = mBitsDesiredFrame;

        if (mRecode) {
            mQuantUpdated = 0;
            mHRD.frameNum--;
            return BRC_ERR_BIG_FRAME;
        }
    }

    mFrameType = picType;

    Ipp64f fa = isField ? mRCfa*0.5 : mRCfa;
    bool oldScene = false;
    if ((mSceneChange & 16) && (mPoc < mSChPoc) && (mBitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (Ipp64f)mBitsEncodedP) && (Ipp64f)mBitsEncoded < 1.5*fa)
        oldScene = true;
    if (Sts != BRC_OK && mRecode) {
        Sts = UpdateQuantHRD(totalFrameBits, Sts, payloadBits);
        mQuantUpdated = 0;
        mFrameType = prevFrameType;
        mRCfa_short = fa_short0;
    } else {
        Ipp64u maxBitsPerFrame = IPP_MIN(mMaxBitsPerPic >> isField, mHRD.bufSize);

        if (static_cast<Ipp64u>(totalFrameBits) > maxBitsPerFrame) {
            if (maxBitsPerFrame > static_cast<Ipp64u>(mHRD.minFrameSize)) { // otherwise we ignore minCR requirement
                qstep = QP2Qstep(qp);
                qs = (Ipp64f)totalFrameBits/maxBitsPerFrame;
                qstep *= qs;
                qp = Qstep2QP(qstep);
                if (qp == qpprev)
                    qp++;

                if (qp > mQuantMax)
                    qp = mQuantMax;
                if (qpprev < mQuantMax) { // compression below minCR is hardly achievable at mQuantMax, but anyways
                    mQuantUpdated = -1;

                    mRCq = mQuantI = mQuantP = qp; // ???
                    if (mFrameType == B_PICTURE)
                        mQuantB = qp;
                    else
                        SetQuantB();

                    Sts = BRC_ERR_BIG_FRAME;
                    mFrameType = prevFrameType;
                    mRCfa_short = fa_short0;
                    mHRD.frameNum--;
                    return Sts;
                }
            }
        }

        if (mQuantUpdated == 0 && 1./qp < mRCqa)
            mRCqa += (1. / qp - mRCqa) / 16;
        else if (mQuantUpdated == 0)
            mRCqa += (1. / qp - mRCqa) / (mRCqap > 25 ? 25 : mRCqap);
        else
            mRCqa += (1. / qp - mRCqa) / mRCqap;

        if (mRecode) {
            BRC_CLIP(mRCqa, 1./mQuantMax , 1./1.);
        } else {
            BRC_CLIP(mRCqa, 1./mQuantMax , 1./mQuantMin);
        }

        if (repack != BRC_RECODE_PANIC && repack != BRC_RECODE_EXT_PANIC && !oldScene) {

            mQPprev = qp;
            mBitsEncodedPrev = mBitsEncoded;

            Sts = UpdateQuant(bitsEncoded, totalFrameBits);

            if (!mRecode) {
                if (mRCq < mQuantMin) 
                    mRCq = mQuantMin;
            }
            if (mFrameType != B_PICTURE) {
                mQuantPrev = mQuantP;
                mBitsEncodedP = mBitsEncoded;
            }
            mQuantP = mQuantI = mRCq;
        }
        mHRD.underflowQuant = -1;
        mQuantUpdated = 1;
        mMaxBitsPerPic = mMaxBitsPerPicNot0;
        mBF += (Ipp64s)mMaxBitrate * (Ipp64s)mParams.frameRateExtD >> isField;
        mBF -= ((Ipp64s)totalFrameBits >> 3) * mParams.frameRateExtN;

        if ((BRC_VBR == mRCMode) && (mBF > (Ipp64s)mParams.HRDBufferSizeBytes * mParams.frameRateExtN))
            mBF = (Ipp64s)mParams.HRDBufferSizeBytes * mParams.frameRateExtN;
    }
    return Sts;
};

BRCStatus H264BRC::UpdateQuant(Ipp32s bEncoded, Ipp32s totalPicBits)
{
  BRCStatus Sts = BRC_OK;
  Ipp64f  bo, qs, dq;
  Ipp32s  quant;
  Ipp32s isfield = ((mPictureFlags & BRC_FRAME) != BRC_FRAME) ? 1 : 0;
  Ipp32u bitsPerPic = (Ipp32u)mBitsDesiredFrame >> isfield;
  Ipp64s totalBitsDeviation;
//  Ipp32s fap = mRCfap, qap = mRCqap;

  if (isfield)
    mRCfa *= 0.5;

  quant = (mFrameType == I_PICTURE) ? mQuantI : (mFrameType == B_PICTURE) ? mQuantB : mQuantP;

  if (mRecode & 2) {
    mRCfa = bitsPerPic;
    mRCqa = mRCqa0;
    mRecode &= ~2;
  }

  mBitsEncodedTotal += totalPicBits;
  mBitsDesiredTotal += bitsPerPic;

  Ipp64s targetFullness = mParams.HRDInitialDelayBytes << 3;
  Ipp64s minTargetFullness = IPP_MIN(mHRD.bufSize / 2, mBitrate * 2); // half bufsize or 2 sec
  if (targetFullness < minTargetFullness)
      targetFullness = minTargetFullness;
  totalBitsDeviation = targetFullness - (Ipp64s)mHRD.bufFullness;
  if (totalBitsDeviation < mBitsEncodedTotal - mBitsDesiredTotal) 
      totalBitsDeviation = mBitsEncodedTotal - mBitsDesiredTotal;

  if (mFrameType != I_PICTURE || mRCMode == BRC_CBR || mQuantUpdated == 0)
    mRCfa += (bEncoded - mRCfa) / mRCfap;
  SetQuantB();
  if (mQuantUpdated == 0)
    if (mQuantB < quant)
      mQuantB = quant;
  qs = pow(bitsPerPic / mRCfa, 2.0);
  dq = mRCqa * qs;

  Ipp32s bap = mRCbap;
  Ipp64f bfRatio = mHRD.bufFullness / mBitsDesiredFrame;

  if (totalBitsDeviation > 0) {
      bap = (Ipp32s)bfRatio*3;
      bap = IPP_MAX(bap, 10);
      BRC_CLIP(bap, mRCbap/10, mRCbap);
  }
  bo = (Ipp64f)totalBitsDeviation / bap / mBitsDesiredFrame; // ??? bitsPerPic ?

  BRC_CLIP(bo, -1.0, 1.0);

  dq = dq + (1./mQuantMax - dq) * bo;
  BRC_CLIP(dq, 1./mQuantMax, 1./1.);
  quant = (int) (1. / dq + 0.5);

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

  mRCq = quant;

  Ipp64f qstep = QP2Qstep(quant);
  Ipp64f fullnessThreshold = IPP_MIN(bitsPerPic * 12, mHRD.bufSize*3/16);
  qs = 1.0;
  if (bEncoded > mHRD.bufFullness && mFrameType != I_PICTURE) {
    qs = (Ipp64f)bEncoded / (mHRD.bufFullness);
  }

  if (mHRD.bufFullness < fullnessThreshold && ((Ipp32u)totalPicBits > bitsPerPic || quant < mQuantPrev))
   qs *= sqrt((Ipp64f)fullnessThreshold * 1.3 / mHRD.bufFullness); // ??? is often useless (quant == quant_old)

  if (qs > 1.0) {
    qstep *= qs;
    quant = Qstep2QP(qstep);
    if (mRCq == quant)
      quant++;

    BRC_CLIP(quant, 1, mQuantMax);

    mQuantB = ((quant + quant) * 563 >> 10) + 1;
    BRC_CLIP(mQuantB, 1, mQuantMax);
    mRCq = quant;
  }

  if (isfield)
    mRCfa *= 2;

  return Sts;
}

BRCStatus H264BRC::UpdateQuantHRD(Ipp32s totalFrameBits, BRCStatus sts, Ipp32s payloadBits)
{
  Ipp32s quant, quant_prev;
  Ipp64f qs;
  Ipp32s wantedBits = (sts == BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize : mHRD.minFrameSize);
  Ipp32s bEncoded = totalFrameBits - payloadBits;

  wantedBits -= payloadBits;
  if (wantedBits <= 0) // possible only if BRC_ERR_BIG_FRAME
    return (sts | BRC_NOT_ENOUGH_BUFFER);

  quant_prev = quant = (mFrameType == I_PICTURE) ? mQuantI : (mFrameType == B_PICTURE) ? mQuantB : mQuantP;
  if (sts & BRC_ERR_BIG_FRAME)
    mHRD.underflowQuant = quant;

  qs = pow((Ipp64f)bEncoded/wantedBits, 2.0);
  quant = (Ipp32s)(quant * qs + 0.5);

  if (quant == quant_prev)
    quant += (sts == BRC_ERR_BIG_FRAME ? 1 : -1);

  BRC_CLIP(quant, 1, mQuantMax);

  if (quant < quant_prev) {
    while (quant <= mHRD.underflowQuant)
      quant++;
  }

  if (quant == quant_prev)
    return (sts | BRC_NOT_ENOUGH_BUFFER);

  // need this ??? loosen the limits ???
  if (quant >= quant_prev + 5)
    quant = quant_prev + 3;
  else if (quant >= quant_prev + 3)
    quant = quant_prev + 2;
  else if (quant > quant_prev + 1)
    quant = quant_prev + 1;
  else if (quant <= quant_prev - 5)
    quant = quant_prev - 3;
  else if (quant <= quant_prev - 3)
    quant = quant_prev - 2;
  else if (quant < quant_prev - 1)
    quant = quant_prev - 1;

  switch (mFrameType) {
  case (I_PICTURE):
    mQuantI = quant;
    break;
  case (B_PICTURE):
    mQuantB = quant;
    break;
  case (P_PICTURE):
  default:
    mQuantP = quant;
  }
  return sts;
}

Status H264BRC::GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode)
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
}