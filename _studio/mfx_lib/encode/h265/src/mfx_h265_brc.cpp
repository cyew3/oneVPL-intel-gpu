/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013 - 2014 Intel Corporation. All Rights Reserved.
//
//
//                     H.265 bitrate control
//
*/


#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <math.h>
#include "mfx_h265_brc.h"
#include "mfx_h265_tables.h"

namespace H265Enc {

mfxStatus H265BRC::Close()
{
    mfxStatus status = MFX_ERR_NONE;
    if (!mIsInit)
        return MFX_ERR_NOT_INITIALIZED;
    mIsInit = false;
    return status;
}


static mfxF64 QP2Qstep (mfxI32 QP)
{
    return (mfxF64)(pow(2.0, (QP - 4.0) / 6.0));
}

static mfxI32 Qstep2QP (mfxF64 Qstep)
{
    return (mfxI32)(4.0 + 6.0 * log(Qstep) / log(2.0));
}


mfxStatus H265BRC::InitHRD()
{
    mfxU64 bufSizeBits = mParams.HRDBufferSizeBytes << 3;
    mfxI32 bitsPerFrame = mBitsDesiredFrame;
    mfxU64 maxBitrate = mParams.maxBitrate;

    if (MFX_RATECONTROL_CBR == mRCMode)
        maxBitrate = mBitrate;

    if (maxBitrate < (mfxU64)mBitrate)
        maxBitrate = 0;

    mParams.maxBitrate = (mfxI32)((maxBitrate >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE));
    mHRD.maxBitrate = mParams.maxBitrate;
    mHRD.inputBitsPerFrame = mHRD.maxBitrate / mFramerate;

    if (bufSizeBits > 0 && bufSizeBits < (mfxU64)(bitsPerFrame << 1))
        bufSizeBits = (bitsPerFrame << 1);

    mHRD.bufSize = (mfxU32)((bufSizeBits >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE));
    mParams.HRDBufferSizeBytes = (mfxI32)(mHRD.bufSize >> 3);

    if (mParams.HRDInitialDelayBytes <= 0)
        mParams.HRDInitialDelayBytes = (MFX_RATECONTROL_CBR == mRCMode ? mParams.HRDBufferSizeBytes/2 : mParams.HRDBufferSizeBytes);
    else if (mParams.HRDInitialDelayBytes < bitsPerFrame >> 3)
        mParams.HRDInitialDelayBytes = bitsPerFrame >> 3;
    if (mParams.HRDInitialDelayBytes > mParams.HRDBufferSizeBytes)
        mParams.HRDInitialDelayBytes = mParams.HRDBufferSizeBytes;
    mHRD.bufFullness = mParams.HRDInitialDelayBytes << 3;

    mHRD.underflowQuant = 0;
    mHRD.overflowQuant = 999;
    mHRD.frameNum = 0;

    return MFX_ERR_NONE;
}

mfxI32 H265BRC::GetInitQP()
{
//  const mfxF64 x0 = 0, y0 = 1.19, x1 = 1.75, y1 = 1.75;
  mfxI32 fs, fsLuma;

  fsLuma = mParams.width * mParams.height;
  fs = fsLuma;
  if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
    fs += fsLuma / 2;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
    fs += fsLuma;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
    fs += fsLuma * 2;
  fs = fs * mParams.bitDepthLuma / 8;
  //mfxI32 q = (mfxI32)(1. / 1.2 * pow(10.0, (log10(fs * 2. / 3. * mFramerate / mBitrate) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
  
  mfxF64 qstep = pow(1.5 * fs * mFramerate / mBitrate, 0.8);
  mfxI32 q = Qstep2QP(qstep);

  BRC_CLIP(q, 1, mQuantMax);
  return q;
}

mfxStatus H265BRC::SetParams(const mfxVideoParam *params, H265VideoParam &video)
{
    if (!params)
        return MFX_ERR_NULL_PTR;

    mfxU16 brcParamMultiplier = params->mfx.BRCParamMultiplier ? params->mfx.BRCParamMultiplier : 1;
    mParams.BRCMode = params->mfx.RateControlMethod;
    mParams.targetBitrate =  params->mfx.TargetKbps * brcParamMultiplier * 1000;
    if (!mParams.targetBitrate)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (mParams.BRCMode != MFX_RATECONTROL_AVBR) {
        mParams.HRDBufferSizeBytes = params->mfx.BufferSizeInKB * brcParamMultiplier * 1000;
        mParams.HRDInitialDelayBytes = params->mfx.InitialDelayInKB * brcParamMultiplier * 1000;
    } else {
        mParams.HRDBufferSizeBytes = mParams.HRDInitialDelayBytes = 0;
    }

    mParams.maxBitrate = params->mfx.MaxKbps * brcParamMultiplier * 1000;

    mParams.frameRateExtN = params->mfx.FrameInfo.FrameRateExtN;
    mParams.frameRateExtD = params->mfx.FrameInfo.FrameRateExtD;
    if (!mParams.frameRateExtN || !mParams.frameRateExtD)
        return MFX_ERR_INVALID_VIDEO_PARAM;


    mParams.width = params->mfx.FrameInfo.Width;
    mParams.height = params->mfx.FrameInfo.Height;
    mParams.chromaFormat = params->mfx.FrameInfo.ChromaFormat;
    mParams.bitDepthLuma =video.bitDepthLuma;

    return MFX_ERR_NONE;
}

mfxStatus H265BRC::Init(const mfxVideoParam *params,  H265VideoParam &video, mfxI32 enableRecode)
{
    mfxStatus status = MFX_ERR_NONE;

    if (!params)
        return MFX_ERR_NULL_PTR;

    status = SetParams(params, video);
    if (status != MFX_ERR_NONE)
        return status;

    mRecode = (enableRecode && (mParams.BRCMode != MFX_RATECONTROL_AVBR)) ? 1 : 0;

    mFramerate = (mfxF64)mParams.frameRateExtN / mParams.frameRateExtD;
    mBitrate = mParams.targetBitrate;
    mRCMode = (mfxU16)mParams.BRCMode;

    if (mBitrate <= 0 || mFramerate <= 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mBitsDesiredFrame = (mfxI32)(mBitrate / mFramerate);
    if (mBitsDesiredFrame < 10)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (mParams.HRDBufferSizeBytes > 0) {
        status = InitHRD();
        if (status != MFX_ERR_NONE)
            return status;
        mMaxBitrate = mParams.maxBitrate >> 3;
        mBF = (mfxI64)mParams.HRDInitialDelayBytes * mParams.frameRateExtN;
        mBFsaved = mBF;
    }

    mQuantOffset = 6 * (mParams.bitDepthLuma - 8);
    mQuantMax = 51 + mQuantOffset;
    mQuantMin = 1;

    mBitsDesiredTotal = 0;
    mBitsEncodedTotal = 0;

    mQuantUpdated = 1;

    mfxI32 q = GetInitQP();
    if (!mRecode) {
        if (q - 6 > 10)
            mQuantMin = IPP_MAX(10, q - 10);
        else
            mQuantMin = IPP_MAX(q - 6, 2);

        if (q < mQuantMin)
            q = mQuantMin;
    }

#if defined(NEW_BRC)
    if (video.BiPyramidLayers > 1) {
        mBpyramidLayersLen = video.GopRefDist;
        //small_memcpy(mBpyramidLayers, video.m_BpyramidRefLayers, mBpyramidLayersLen);
        mNumLayers = 0;
        for (int j = mBpyramidLayersLen, i = 0; j > 0; j >>= 1, i++) {
            mDeltaQp[i] = i;
            mNumLayers++;
        }
        for (int i = 0; i < mNumLayers; i++)
            mQp[i] = q + mDeltaQp[i];
    }
    mFrameSizeHist = new mfxI32[video.GopRefDist];
    for (int i = 0; i < video.GopRefDist; i++)
        mFrameSizeHist[i] = mBitsDesiredFrame;
#endif
   
    mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = q;

    mRCbap = 100;
    mRCqap = 100;
    mRCfap = 100;

    // post scene change values
    mRCbap1 = 30;
    mRCqap1 = 30;
    mRCfap1 = 30;

    mRCq = q;
    mRCqa = mRCqa0 = 1. / (mfxF64)mRCq;
    mRCfa = mBitsDesiredFrame;
    mRCfa_short = mBitsDesiredFrame;

    mPictureFlags = mPictureFlagsPrev = MFX_PICSTRUCT_PROGRESSIVE;
    mIsInit = true;

    mSChPoc = 0;
    mSceneChange = 0;
    mBitsEncodedPrev = mBitsDesiredFrame;
    mPicType = MFX_FRAMETYPE_I;

    mMaxQp = 999;
    mMinQp = -1;

    return status;
}


#define RESET_BRC_CORE \
{ \
    if (sizeNotChanged) { \
        mRCq = (mfxI32)(1./mRCqa * pow(mRCfa/mBitsDesiredFrame, 0.32) + 0.5); \
        BRC_CLIP(mRCq, 1, mQuantMax); \
    } else \
        mRCq = GetInitQP(); \
    mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = mRCq; \
    mRCqa = mRCqa0 = 1./mRCq; \
    mRCfa = mBitsDesiredFrame; \
    mRCfa_short = mBitsDesiredFrame; \
}


mfxStatus H265BRC::Reset(mfxVideoParam *params, H265VideoParam &video, mfxI32 enableRecode)
{
    mfxStatus status;

    mfxU16 brcParamMultiplier = params->mfx.BRCParamMultiplier ? params->mfx.BRCParamMultiplier : 1;
    mfxU16 rcmode_new = params->mfx.RateControlMethod;
    mfxI32 bufSize_new = rcmode_new == MFX_RATECONTROL_AVBR ? 0 : (params->mfx.BufferSizeInKB * brcParamMultiplier * 8000 >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE);
    bool sizeNotChanged = (mParams.width == params->mfx.FrameInfo.Width &&
                           mParams.height == params->mfx.FrameInfo.Height &&
                           mParams.chromaFormat == params->mfx.FrameInfo.ChromaFormat);

    mfxI32 bufSize, maxBitrate;
    mfxF64 bufFullness;
    mfxU16 rcmode = mRCMode;
    mfxI32 targetBitrate = mBitrate;

    if (rcmode_new != rcmode) {
        if (MFX_RATECONTROL_CBR != rcmode_new && MFX_RATECONTROL_AVBR != rcmode)
            rcmode = rcmode_new;
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mRecode = enableRecode ? 1 : 0;

    if (!(mParams.HRDBufferSizeBytes | bufSize_new)) { // no HRD
        status = SetParams(params, video);
        if (status != MFX_ERR_NONE)
            return status;
        mBitrate = mParams.targetBitrate;
        mFramerate = (mfxF64)mParams.frameRateExtN / mParams.frameRateExtD;
        if (mBitrate <= 0 || mFramerate <= 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        mBitsDesiredFrame = (mfxI32)(mBitrate / mFramerate);
        RESET_BRC_CORE
        return MFX_ERR_NONE;
    } else if (mParams.HRDBufferSizeBytes == 0 && bufSize_new > 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxI32 maxBitrate_new = (params->mfx.MaxKbps * brcParamMultiplier * 1000 >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);
    mfxI32 targetBitrate_new = params->mfx.TargetKbps * brcParamMultiplier * 1000;
    mfxI32 targetBitrate_new_r = (targetBitrate_new >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);

    // framerate change not allowed in case of HRD
    if (mParams.frameRateExtN * params->mfx.FrameInfo.FrameRateExtD != mParams.frameRateExtD * params->mfx.FrameInfo.FrameRateExtN)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    bufSize = mHRD.bufSize;
    maxBitrate = (mfxI32)mHRD.maxBitrate;
    bufFullness = mHRD.bufFullness;

    if (targetBitrate_new_r > maxBitrate_new && maxBitrate_new > 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if (MFX_RATECONTROL_CBR == rcmode && targetBitrate_new > 0 && maxBitrate_new > 0 && maxBitrate_new != targetBitrate_new_r)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (MFX_RATECONTROL_CBR == rcmode) {
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
        mBF = (mfxI64)((mfxU64)mBF * (maxBitrate_new >> 6) / (mMaxBitrate >> 3));
        maxBitrate = maxBitrate_new;
    } else if (maxBitrate_new > maxBitrate) {
        if (MFX_RATECONTROL_VBR == rcmode) {
            mfxF64 bf_delta = (maxBitrate_new - maxBitrate) / mFramerate;
          // lower estimate for the fullness with the bitrate updated at tai;
          // for VBR the fullness encoded in buffering period SEI can be below the real buffer fullness
            bufFullness += bf_delta;
            if (bufFullness > (mfxF64)(bufSize - 1))
                bufFullness = (mfxF64)(bufSize - 1);

            mBF += (mfxI64)((maxBitrate_new >> 3) - mMaxBitrate) * (mfxI64)mParams.frameRateExtD;
            if (mBF > (mfxI64)(bufSize >> 3) * mParams.frameRateExtN)
                mBF = (mfxI64)(bufSize >> 3) * mParams.frameRateExtN;

            maxBitrate = maxBitrate_new;
        } else // HRD overflow is possible for CBR
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (bufSize_new > 0 && bufSize_new < bufSize) {
        bufSize = bufSize_new;
        if (bufFullness > (mfxF64)(bufSize - 1)) {
            if (MFX_RATECONTROL_VBR == rcmode)
                bufFullness = (mfxF64)(bufSize - 1);
            else
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if (targetBitrate > maxBitrate)
        targetBitrate = maxBitrate;

    mHRD.bufSize = bufSize;
    mHRD.maxBitrate = (mfxF64)maxBitrate;
    mHRD.bufFullness = bufFullness;
    mHRD.inputBitsPerFrame = mHRD.maxBitrate / mFramerate;

    mParams.BRCMode = rcmode;
    mParams.HRDBufferSizeBytes = bufSize >> 3;
    mParams.maxBitrate = maxBitrate;
    mParams.targetBitrate = targetBitrate;
    if (!sizeNotChanged) {
        mParams.width = params->mfx.FrameInfo.Width;
        mParams.height = params->mfx.FrameInfo.Height;
        mParams.chromaFormat = params->mfx.FrameInfo.ChromaFormat;
    }
    mMaxBitrate = (mfxU32)(maxBitrate >> 3);

    RESET_BRC_CORE

    mSceneChange = 0;
    mBitsEncodedPrev = mBitsDesiredFrame;

    mMaxQp = 999;
    mMinQp = -1;

    return MFX_ERR_NONE;
}

static const Ipp8u h265_QPtoChromaQP_1[3][58]=
{
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 30, 31, 32,
        33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44,
        45, 46, 47, 48, 49, 50, 51
    },
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
    },
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
     }
};

mfxI32 H265BRC::GetQP(H265VideoParam &video, H265Frame *pFrame, mfxI32 *chromaQP) //mfxU16 frameType, mfxU16 chromaFormatIdc, mfxI32 *chromaQP)
{
#if defined(NEW_BRC)
    mfxI32 qp;

    if (pFrame) {

        mfxU32 frameType = pFrame->m_picCodeType;

        if (frameType & MFX_FRAMETYPE_I) 
            qp =  mRCq;
        else if (frameType & MFX_FRAMETYPE_P) 
            qp = mRCq;
        else {
            mfxI32 layer;
            if (video.BiPyramidLayers > 1)
                layer = pFrame->m_pyramidLayer;
            else
                layer = 1;
            qp = mRCq + mDeltaQp[layer];
            BRC_CLIP(qp, mQuantMin, mQuantMax);
        }
 //printf("Get %d %d %d %d %d \n", qp, mRCq, pFrame->m_picCodeType, pFrame->getPicOrderCnt(), (pFrame->m_picCodeType & MFX_FRAMETYPE_B) ? video.m_BpyramidRefLayers[pFrame->getPicOrderCnt() % video.GopRefDist] : 0);
   } else
        qp = mRCq;
 

    if (chromaQP)
        *chromaQP = h265_QPtoChromaQP_1[video.chromaFormatIdc-1][qp];
    return qp;
#else
    mfxI32 qp = mQuantB;

    if (pFrame) {
        mfxU16 frameType = (mfxU16)pFrame->m_picCodeType | (pFrame->IsReference() ?  MFX_FRAMETYPE_REF : 0);
        //mfxU32 frameType = pFrame->m_picCodeType;

        if (frameType & MFX_FRAMETYPE_I) 
            qp =  mQuantI;
        else if (frameType & MFX_FRAMETYPE_P) 
            qp = mQuantP;
        else if (frameType & MFX_FRAMETYPE_REF)
            qp = mQuantP;
        else
            qp = mQuantB;
        /*if (chromaQP)
        *chromaQP = h265_QPtoChromaQP[chromaFormatIdc-1][qp];*/
    }

    return qp;

#endif

}

mfxStatus H265BRC::SetQP(mfxI32 qp, mfxU16 frameType)
{
    if (MFX_FRAMETYPE_B == frameType) {
        mQuantB = qp + mQuantOffset;
        BRC_CLIP(mQuantB, 1, mQuantMax);
    } else {
        mRCq = qp + mQuantOffset;
        BRC_CLIP(mRCq, 1, mQuantMax);
        mQuantI = mQuantP = mRCq;
    }
    return MFX_ERR_NONE;
}


mfxBRCStatus H265BRC::UpdateAndCheckHRD(mfxI32 frameBits, mfxF64 inputBitsPerFrame, mfxI32 recode)
{
    mfxBRCStatus ret = MFX_ERR_NONE;
    mfxF64 bufFullness;

    if (!(recode & (MFX_BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
        mHRD.prevBufFullness = mHRD.bufFullness;
        mHRD.underflowQuant = 0;
        mHRD.overflowQuant = 999;
    } else { // frame is being recoded - restore buffer state
        mHRD.bufFullness = mHRD.prevBufFullness;
    }

    mHRD.maxFrameSize = (mfxI32)(mHRD.bufFullness - 1);
    mHRD.minFrameSize = (mRCMode == MFX_RATECONTROL_VBR ? 0 : (mfxI32)(mHRD.bufFullness + 1 + 1 + inputBitsPerFrame - mHRD.bufSize));
    if (mHRD.minFrameSize < 0)
        mHRD.minFrameSize = 0;

    bufFullness = mHRD.bufFullness - frameBits;

    if (bufFullness < 2) {
        bufFullness = inputBitsPerFrame;
        ret = MFX_BRC_ERR_BIG_FRAME;
        if (bufFullness > (mfxF64)mHRD.bufSize)
            bufFullness = (mfxF64)mHRD.bufSize;
    } else {
        bufFullness += inputBitsPerFrame;
        if (bufFullness > (mfxF64)mHRD.bufSize - 1) {
            bufFullness = (mfxF64)mHRD.bufSize - 1;
            if (mRCMode != MFX_RATECONTROL_VBR)
                ret = MFX_BRC_ERR_SMALL_FRAME;
        }
    }
    if (MFX_ERR_NONE == ret)
        mHRD.frameNum++;
    else if ((recode & MFX_BRC_EXT_FRAMESKIP) || MFX_BRC_RECODE_EXT_PANIC == recode || MFX_BRC_RECODE_PANIC == recode) // no use in changing QP
        ret |= MFX_BRC_NOT_ENOUGH_BUFFER;

    mHRD.bufFullness = bufFullness;

    return ret;
}


#define BRC_SCENE_CHANGE_RATIO1 20.0
#define BRC_SCENE_CHANGE_RATIO2 10.0
#define BRC_RCFAP_SHORT 5

//#define BRC_SCENE_CHANGE_RATIO1 10.0
//#define BRC_SCENE_CHANGE_RATIO2 5.0

#define I_WEIGHT 1.2
#define P_WEIGHT 0.25
#define B_WEIGHT 0.2

//mfxBRCStatus H265BRC::PostPackFrame(mfxU16 picType, mfxI32 totalFrameBits, mfxI32 overheadBits, mfxI32 repack, mfxI32 poc)
mfxBRCStatus H265BRC::PostPackFrame(H265VideoParam &video, Ipp8s sliceQpY, H265Frame *pFrame, mfxI32 totalFrameBits, mfxI32 overheadBits, mfxI32 repack)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    mfxI32 bitsEncoded = totalFrameBits - overheadBits;
    mfxF64 e2pe;
    mfxI32 qp, qpprev;
    mfxF64 qstep;
    mfxU32 prevFrameType = mPicType;
    mfxU32 picType = pFrame->m_picCodeType;

    if (mBitrate == 0)
        return Sts;

    mPoc = pFrame->m_poc;

    if (!repack && mQuantUpdated <= 0) { // BRC reported buffer over/underflow but the application ignored it
        mQuantI = mQuantIprev;
        mQuantP = mQuantPprev;
        mQuantB = mQuantBprev;
        mPictureFlags = mPictureFlagsPrev;
        mRecode |= 2;
        UpdateQuant(mBitsEncoded, totalFrameBits);
    }

    mQuantIprev = mQuantI;
    mQuantPprev = mQuantP;
    mQuantBprev = mQuantB;
    mPictureFlagsPrev = mPictureFlags;

    mBitsEncoded = bitsEncoded;

    mfxF64 inputBitsPerFrame = 1;

    if (mSceneChange)
        if (mQuantUpdated == 1 && mPoc > mSChPoc + 1)
            mSceneChange &= ~16;

    mfxF64 buffullness = 1.e12; // a big number
    if (mParams.HRDBufferSizeBytes > 0) {
        inputBitsPerFrame = mHRD.inputBitsPerFrame;
        buffullness = repack ? mHRD.prevBufFullness : mHRD.bufFullness;
        Sts = UpdateAndCheckHRD(totalFrameBits, inputBitsPerFrame, repack);
    }

    qpprev = qp = (picType == MFX_FRAMETYPE_I) ? mQuantI : (picType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;

    //printf("%d %d %d %d %f \n", poc, totalFrameBits, qp, mHRD.bufSize, mHRD.prevBufFullness);

    


    mfxF64 fa_short0 = mRCfa_short;
    mRCfa_short += (bitsEncoded - mRCfa_short) / BRC_RCFAP_SHORT;

//    if (mParams.HRDBufferSizeBytes > 0)  ???
    {
        qstep = QP2Qstep(qp);
        mfxF64 qstep_prev = QP2Qstep(mQPprev);
        mfxF64 frameFactor = 1.0;
        mfxF64 targetFrameSize = IPP_MAX((mfxF64)mBitsDesiredFrame, mRCfa);
        if (picType & MFX_FRAMETYPE_I)
            frameFactor = 1.5;


        e2pe = bitsEncoded * sqrt(qstep) / (mBitsEncodedPrev * sqrt(qstep_prev));

        // mPicType to be set to P in Init() for the right factor for the 1st frame ???
        //if (picType == MFX_FRAMETYPE_I) {
        //    if (mPicType == MFX_FRAMETYPE_P)
        //        frameFactor = P_WEIGHT / I_WEIGHT;
        //    else if (mPicType == MFX_FRAMETYPE_B)
        //        frameFactor = B_WEIGHT / I_WEIGHT;
        //} else if (picType == MFX_FRAMETYPE_P) {
        //    if (mPicType == MFX_FRAMETYPE_B)
        //        frameFactor = B_WEIGHT / P_WEIGHT;
        //}

        mfxF64 maxFrameSize;
        maxFrameSize = 2.5/9. * buffullness + 5./9. * targetFrameSize;
        BRC_CLIP(maxFrameSize, targetFrameSize, BRC_SCENE_CHANGE_RATIO2 * targetFrameSize * frameFactor);

        mfxF64 famax = 1./9. * buffullness + 8./9. * mRCfa;

        mfxI32 maxqp = mQuantMax;
        if (mParams.HRDBufferSizeBytes > 0) {
            maxqp = IPP_MIN(maxqp, mHRD.overflowQuant - 1);
        }

        if (bitsEncoded >  maxFrameSize && qp < maxqp) {
            mfxF64 targetSizeScaled = maxFrameSize * 0.8;
            mfxF64 qstepnew = qstep * pow(bitsEncoded / targetSizeScaled, 0.9);
            mfxI32 qpnew = Qstep2QP(qstepnew);
            if (qpnew == qp)
              qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            if (qpnew > qp) {
                mRCq = mQuantI = mQuantP = qpnew;
                if (picType & MFX_FRAMETYPE_B)
                    mQuantB = qpnew;
                else {
                    SetQuantB();
                }

                mRCfa_short = fa_short0;

                if (e2pe > BRC_SCENE_CHANGE_RATIO1) { // scene change, resetting BRC statistics
                  mRCfa = mBitsDesiredFrame;
                  mRCqa = 1./qpnew;
                  mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;
                  mSceneChange |= 1;
                  if (picType != MFX_FRAMETYPE_B) {
                      mSceneChange |= 16;
                      mSChPoc = mPoc;
                  }
                  mRCfa_short = mBitsDesiredFrame;
                }
                if (mRecode) {
                    mQuantUpdated = 0;
                    mHRD.frameNum--;
                    mMinQp = qp;
                    return MFX_BRC_ERR_BIG_FRAME;
                }
            }
        }

        if (mRCfa_short > famax && (!repack) && qp < maxqp) {

            mfxF64 qstepnew = qstep * mRCfa_short / (famax * 0.8);
            mfxI32 qpnew = Qstep2QP(qstepnew);
            if (qpnew == qp)
                qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            mRCfa = mBitsDesiredFrame;
            mRCqa = 1./qpnew;
            mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;

            mRCfa_short = mBitsDesiredFrame;

            if (mRecode) {
                mQuantUpdated = 0;
                mHRD.frameNum--;
                mMinQp = qpnew;
                return MFX_BRC_ERR_BIG_FRAME;
            }
        }
    }

    mPicType = picType;

    mfxF64 fa = mRCfa;
    bool oldScene = false;
    if ((mSceneChange & 16) && (mPoc < mSChPoc) && (mBitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (mfxF64)mBitsEncodedP) && (mfxF64)mBitsEncoded < 1.5*fa)
        oldScene = true;

    if (Sts != MFX_BRC_OK && mRecode) {
        Sts = UpdateQuantHRD(totalFrameBits, Sts, overheadBits);
        mQuantUpdated = 0;
        mPicType = prevFrameType;
        mRCfa_short = fa_short0;
    } else {
        // check minCR ??? { ... }

        if (mQuantUpdated == 0 && 1./qp < mRCqa)
            mRCqa += (1. / qp - mRCqa) / 16;
        else if (mQuantUpdated == 0)
            mRCqa += (1. / qp - mRCqa) / (mRCqap > 25 ? 25 : mRCqap);
        else
            mRCqa += (1. / qp - mRCqa) / mRCqap;

        BRC_CLIP(mRCqa, 1./mQuantMax , 1./1.);

        if (repack != MFX_BRC_RECODE_PANIC && repack != MFX_BRC_RECODE_EXT_PANIC && !oldScene) {
            mQPprev = qp;
            mBitsEncodedPrev = mBitsEncoded;

            Sts = UpdateQuant(bitsEncoded, totalFrameBits);

            if (mPicType != MFX_FRAMETYPE_B) {
                mQuantPrev = mQuantP;
                mBitsEncodedP = mBitsEncoded;
            }

            mQuantP = mQuantI = mRCq;
        }
        mQuantUpdated = 1;
        //    mMaxBitsPerPic = mMaxBitsPerPicNot0;

        if (mParams.HRDBufferSizeBytes > 0) {
            mHRD.underflowQuant = 0;
            mHRD.overflowQuant = 999;
            mBF += (mfxI64)mMaxBitrate * (mfxI64)mParams.frameRateExtD;
            mBF -= ((mfxI64)totalFrameBits >> 3) * mParams.frameRateExtN;
            if ((MFX_RATECONTROL_VBR == mRCMode) && (mBF > (mfxI64)mParams.HRDBufferSizeBytes * mParams.frameRateExtN))
                mBF = (mfxI64)mParams.HRDBufferSizeBytes * mParams.frameRateExtN;
        }
        mMinQp = -1;
        mMaxQp = 999;
    }

    return Sts;
};

mfxBRCStatus H265BRC::UpdateQuant(mfxI32 bEncoded, mfxI32 totalPicBits)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    mfxF64  bo, qs, dq;
    mfxI32  quant;
    mfxU32 bitsPerPic = (mfxU32)mBitsDesiredFrame;
    mfxI64 totalBitsDeviation;

    quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : (mPicType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;

    if (mRecode & 2) {
        mRCfa = bitsPerPic;
        mRCqa = mRCqa0;
        mRecode &= ~2;
    }

    mBitsEncodedTotal += totalPicBits;
    mBitsDesiredTotal += bitsPerPic;
    totalBitsDeviation = mBitsEncodedTotal - mBitsDesiredTotal;

    if (mParams.HRDBufferSizeBytes > 0) {
        mfxI64 targetFullness = mParams.HRDInitialDelayBytes << 3;
        mfxI64 minTargetFullness = IPP_MIN(mHRD.bufSize / 2, mBitrate * 2); // half bufsize or 2 sec
        if (targetFullness < minTargetFullness)
            targetFullness = minTargetFullness;
        mfxI64 bufferDeviation = targetFullness - (Ipp64s)mHRD.bufFullness;
        if (bufferDeviation > totalBitsDeviation)
            totalBitsDeviation = bufferDeviation;
    }

    if (mPicType != MFX_FRAMETYPE_I || mRCMode == MFX_RATECONTROL_CBR || mQuantUpdated == 0)
        mRCfa += (bEncoded - mRCfa) / mRCfap;
    SetQuantB();
    if (mQuantUpdated == 0)
        if (mQuantB < quant)
            mQuantB = quant;
    qs = pow(bitsPerPic / mRCfa, 2.0);
    dq = mRCqa * qs;

    mfxI32 bap = mRCbap;
    if (mParams.HRDBufferSizeBytes > 0) {
        mfxF64 bfRatio = mHRD.bufFullness / mBitsDesiredFrame;
        if (totalBitsDeviation > 0) {
            bap = (mfxI32)bfRatio*3;
            bap = IPP_MAX(bap, 10);
            BRC_CLIP(bap, mRCbap/10, mRCbap);
        }
    }
    bo = (mfxF64)totalBitsDeviation / bap / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);

    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./mQuantMin);
    quant = (mfxI32) (1. / dq + 0.5);

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

    if (mParams.HRDBufferSizeBytes > 0) {
        mfxF64 qstep = QP2Qstep(quant);
        mfxF64 fullnessThreshold = MIN(bitsPerPic * 12, mHRD.bufSize*3/16);
        qs = 1.0;
        if (bEncoded > mHRD.bufFullness && mPicType != MFX_FRAMETYPE_I)
            qs = (mfxF64)bEncoded / (mHRD.bufFullness);

        if (mHRD.bufFullness < fullnessThreshold && (mfxU32)totalPicBits > bitsPerPic)
            qs *= sqrt((mfxF64)fullnessThreshold * 1.3 / mHRD.bufFullness); // ??? is often useless (quant == quant_old)

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
    }

    return Sts;
}

mfxBRCStatus H265BRC::UpdateQuantHRD(mfxI32 totalFrameBits, mfxBRCStatus sts, mfxI32 overheadBits)
{
    mfxI32 quant, quant_prev;
    mfxI32 wantedBits = (sts == MFX_BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize * 3 / 4 : mHRD.minFrameSize * 5 / 4);
    mfxI32 bEncoded = totalFrameBits - overheadBits;
    mfxF64 qstep, qstep_new;

    wantedBits -= overheadBits;
    if (wantedBits <= 0) // possible only if BRC_ERR_BIG_FRAME
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    quant_prev = quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : (mPicType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;
    if (sts & MFX_BRC_ERR_BIG_FRAME)
        mHRD.underflowQuant = quant;
    else if (sts & MFX_BRC_ERR_SMALL_FRAME)
        mHRD.overflowQuant = quant;

    qstep = QP2Qstep(quant);
    qstep_new = qstep * sqrt((mfxF64)bEncoded / wantedBits);
    quant = Qstep2QP(qstep_new);

    if (sts & MFX_BRC_ERR_SMALL_FRAME) // overflow
    {
        mfxI32 qpMin = IPP_MAX(mHRD.underflowQuant, mMinQp);
        if (qpMin > 0) {
            if (quant < (qpMin + quant_prev + 1) >> 1)
                quant = (qpMin + quant_prev + 1) >> 1;
        }
        if (quant > quant_prev - 1)
            quant = quant_prev - 1;
        if (quant < mHRD.underflowQuant + 1)
            quant = mHRD.underflowQuant + 1;
        if (quant < mMinQp + 1 && quant_prev > mMinQp + 1)
            quant = mMinQp + 1;
    } 
    else // underflow
    {
        if (mHRD.overflowQuant <= mQuantMax) {
            if (quant > (quant_prev + mHRD.overflowQuant + 1) >> 1)
                quant = (quant_prev + mHRD.overflowQuant + 1) >> 1;
        }
        if (quant < quant_prev + 1)
            quant = quant_prev + 1;
        if (quant > mHRD.overflowQuant - 1)
            quant = mHRD.overflowQuant - 1;
    }

    if (quant == quant_prev)
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    /*switch (mPicType) {
    case (MFX_FRAMETYPE_I):
        mQuantI = quant;
    break;
    case (MFX_FRAMETYPE_B):
        mQuantB = quant;
    break;
    case (MFX_FRAMETYPE_P):
    default:
        mQuantP = quant;
    }*/

    if ( mPicType & MFX_FRAMETYPE_I )
        mQuantI = quant;
    else if (mPicType & MFX_FRAMETYPE_B)
        mQuantB = quant;
    else 
        mQuantP = quant;

    return sts;
}

mfxStatus H265BRC::GetInitialCPBRemovalDelay(mfxU32 *initial_cpb_removal_delay, mfxI32 recode)
{
    mfxU32 cpb_rem_del_u32;
    mfxU64 cpb_rem_del_u64, temp1_u64, temp2_u64;

    if (MFX_RATECONTROL_VBR == mRCMode) {
        if (recode)
            mBF = mBFsaved;
        else
            mBFsaved = mBF;
    }

    temp1_u64 = (mfxU64)mBF * 90000;
    temp2_u64 = (mfxU64)mMaxBitrate * mParams.frameRateExtN;
    cpb_rem_del_u64 = temp1_u64 / temp2_u64;
    cpb_rem_del_u32 = (mfxU32)cpb_rem_del_u64;

    if (MFX_RATECONTROL_VBR == mRCMode) {
        mBF = temp2_u64 * cpb_rem_del_u32 / 90000;
        temp1_u64 = (mfxU64)cpb_rem_del_u32 * mMaxBitrate;
        mfxU32 dec_buf_ful = (mfxU32)(temp1_u64 / (90000/8));
        if (recode)
            mHRD.prevBufFullness = (mfxF64)dec_buf_ful;
        else
            mHRD.bufFullness = (mfxF64)dec_buf_ful;
    }

    *initial_cpb_removal_delay = cpb_rem_del_u32;
    return MFX_ERR_NONE;
}



void  H265BRC::GetMinMaxFrameSize(mfxI32 *minFrameSizeInBits, mfxI32 *maxFrameSizeInBits)
{
    if (minFrameSizeInBits)
      *minFrameSizeInBits = mHRD.minFrameSize;
    if (maxFrameSizeInBits)
      *maxFrameSizeInBits = mHRD.maxFrameSize;
}

} // namespace

#endif