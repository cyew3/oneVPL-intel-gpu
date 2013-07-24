/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013 Intel Corporation. All Rights Reserved.
//
//
//                     H.265 bitrate control
//
*/


#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"


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

    if (bufSizeBits > 0 && bufSizeBits < (bitsPerFrame << 1))
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

    mHRD.underflowQuant = -1;
    mHRD.frameNum = 0;

    return MFX_ERR_NONE;
}

mfxI32 H265BRC::GetInitQP()
{
  const mfxF64 x0 = 0, y0 = 1.19, x1 = 1.75, y1 = 1.75;
  mfxI32 fs, fsLuma;

  fsLuma = mParams.width * mParams.height;
  fs = fsLuma;
  if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
    fs += fsLuma / 2;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
    fs += fsLuma;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
    fs += fsLuma * 2;
  fs = fs * BIT_DEPTH_LUMA / 8;
  mfxI32 q = (mfxI32)(1. / 1.2 * pow(10.0, (log10(fs * 2. / 3. * mFramerate / mBitrate) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
  BRC_CLIP(q, 1, mQuantMax);
  return q;
}

mfxStatus H265BRC::SetParams(mfxVideoParam *params)
{
    if (!params)
        return MFX_ERR_NULL_PTR;

    mParams.BRCMode = params->mfx.RateControlMethod;
    mParams.targetBitrate =  params->mfx.TargetKbps * 1000;
    if (!mParams.targetBitrate)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mParams.HRDBufferSizeBytes = params->mfx.BufferSizeInKB * 1000;
    mParams.HRDInitialDelayBytes = params->mfx.InitialDelayInKB * 1000;

    mParams.maxBitrate = params->mfx.MaxKbps * 1000;

    mParams.frameRateExtN = params->mfx.FrameInfo.FrameRateExtN;
    mParams.frameRateExtD = params->mfx.FrameInfo.FrameRateExtD;
    if (!mParams.frameRateExtN || !mParams.frameRateExtD)
        return MFX_ERR_INVALID_VIDEO_PARAM;


    mParams.width = params->mfx.FrameInfo.Width;
    mParams.height = params->mfx.FrameInfo.Height;
    mParams.chromaFormat = params->mfx.FrameInfo.ChromaFormat;

/*
    mFramerate = mParams.frameRateExtN / mParams.frameRateExtD;
    mBitrate = params->mfx.TargetKbps * 1000;
    if (!mBitrate)
        return MFX_ERR_INVALID_VIDEO_PARAM;
*/

    return MFX_ERR_NONE;
}

mfxStatus H265BRC::Init(mfxVideoParam *params)
{
    mfxStatus status = MFX_ERR_NONE;

    if (!params)
        return MFX_ERR_NULL_PTR;

    status = SetParams(params);
    if (status != MFX_ERR_NONE)
        return status;

    mFramerate = mParams.frameRateExtN / mParams.frameRateExtD;
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
/*
    else { // no HRD
        mHRD.bufSize = mHRD.maxFrameSize = IPP_MAX_32S;
        mHRD.bufFullness = (mfxF64)mHRD.bufSize/2; // just need buffer fullness not to trigger any hrd related actions
        mHRD.minFrameSize = 0;
    }
*/

    mQuantOffset = 6 * (BIT_DEPTH_LUMA - 8);
    mQuantMax = 51 + mQuantOffset;

    mBitsDesiredTotal = 0;
    mBitsEncodedTotal = 0;

    mQuantUpdated = 1;

    mfxI32 q = GetInitQP();
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

    mPictureFlags = mPictureFlagsPrev = MFX_PICSTRUCT_PROGRESSIVE;
    mIsInit = true;
    mRecodeInternal = 0;

    mSChPoc = 0;
    mSceneChange = 0;
    mBitsEncodedPrev = mBitsDesiredFrame;
    mPicType = MFX_FRAMETYPE_I;

    mScChFrameCnt = 0;
    mScChLength = 30;

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
}


mfxStatus H265BRC::Reset(mfxVideoParam *params)
{
    mfxStatus status;

    mfxI32 bufSize_new = (params->mfx.BufferSizeInKB * 8000 >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE);
    mfxU16 rcmode_new = params->mfx.RateControlMethod;
    bool sizeNotChanged = (mParams.width == params->mfx.FrameInfo.Width &&
                           mParams.height == params->mfx.FrameInfo.Height &&
                           mParams.chromaFormat == params->mfx.FrameInfo.ChromaFormat);

    mfxI32 bufSize, maxBitrate;
    mfxF64 bufFullness;
    mfxU16 rcmode = mRCMode;
    mfxI32 targetBitrate = mBitrate;

    if (rcmode_new != rcmode) {
        if (MFX_RATECONTROL_VBR == rcmode_new && MFX_RATECONTROL_CBR == rcmode)
            rcmode = rcmode_new;
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!(mParams.HRDBufferSizeBytes | bufSize_new)) { // no HRD
        status = SetParams(params);
        if (status != MFX_ERR_NONE)
            return status;
        mBitrate = mParams.targetBitrate;
        mFramerate = mParams.frameRateExtN / mParams.frameRateExtD;
        if (mBitrate <= 0 || mFramerate <= 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        mBitsDesiredFrame = (mfxI32)(mBitrate / mFramerate);
        RESET_BRC_CORE
        return MFX_ERR_NONE;
    } else if (mParams.HRDBufferSizeBytes == 0 && bufSize_new > 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;


    mfxI32 maxBitrate_new = (params->mfx.MaxKbps * 1000 >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);
    mfxI32 targetBitrate_new = params->mfx.TargetKbps * 1000;
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
            mfxI32 isField = (mPictureFlagsPrev & MFX_PICSTRUCT_PROGRESSIVE) ? 0 : 1;
            mfxF64 bf_delta = (maxBitrate_new - maxBitrate) / mFramerate;
            if (isField)
                bf_delta *= 0.5;
          // lower estimate for the fullness with the bitrate updated at tai;
          // for VBR the fullness encoded in buffering period SEI can be below the real buffer fullness
            bufFullness += bf_delta;
            if (bufFullness > (mfxF64)(bufSize - 1))
                bufFullness = (mfxF64)(bufSize - 1);

            mBF += (mfxI64)((maxBitrate_new >> 3) - mMaxBitrate) * (mfxI64)mParams.frameRateExtD >> isField;
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
    mScChFrameCnt = 0;

    mRecodeInternal = 0;
    return MFX_ERR_NONE;
}

mfxI32 H265BRC::GetQP(mfxU16 frameType, mfxI32 *chromaQP)
{
    mfxI32 qp = ((frameType == MFX_FRAMETYPE_I) ? mQuantI : (frameType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP) - mQuantOffset;
    if (chromaQP)
        *chromaQP = h265_QPtoChromaQP[qp];
    return qp;
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
        mHRD.underflowQuant = -1;
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

#define BRC_SCENE_CHANGE_RATIO1 10.0
#define BRC_SCENE_CHANGE_RATIO2 5.0

#define I_WEIGHT 1.2
#define P_WEIGHT 0.5
#define B_WEIGHT 0.25

mfxBRCStatus H265BRC::PostPackFrame(mfxU16 picType, mfxI32 totalFrameBits, mfxI32 overheadBits, mfxI32 repack, mfxI32 poc)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    mfxI32 bitsEncoded = totalFrameBits - overheadBits;
    mfxF64 e2bpf, e2pe;
    mfxI32 qp, qpprev;
    mfxF64 qs, qstep;
    mfxU16 prevFrameType = mPicType;

    if (mBitrate == 0)
        return Sts;

    mPoc = poc;

    if (!repack && mQuantUpdated <= 0) { // BRC reported buffer over/underflow but the application ignored it
        mQuantI = mQuantIprev;
        mQuantP = mQuantPprev;
        mQuantB = mQuantBprev;
        mPictureFlags = mPictureFlagsPrev;
        mRecodeInternal = 1; // ???
        UpdateQuant(mBitsEncoded, totalFrameBits);
    }

    mQuantIprev = mQuantI;
    mQuantPprev = mQuantP;
    mQuantBprev = mQuantB;
    mPictureFlagsPrev = mPictureFlags;

    mBitsEncoded = bitsEncoded;

    mfxI32 isField = (mPictureFlags & MFX_PICSTRUCT_PROGRESSIVE) ? 0 : 1;
    mfxU32 bitsPerPic = (mfxU32)mBitsDesiredFrame >> isField;

    mfxF64 inputBitsPerFrame = 1;

    if (mSceneChange)
        if (mQuantUpdated == 1 && poc > mSChPoc + 1)
            mSceneChange &= ~16;

    if (mParams.HRDBufferSizeBytes > 0) {
        inputBitsPerFrame = mHRD.inputBitsPerFrame;
        if (isField)
            inputBitsPerFrame *= 0.5;
        Sts = UpdateAndCheckHRD(totalFrameBits, inputBitsPerFrame, repack);
    }

    qpprev = qp = (picType == MFX_FRAMETYPE_I) ? mQuantI : (picType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;

    if (mParams.HRDBufferSizeBytes > 0) {
        e2pe = (mfxF64)bitsEncoded * qp / (mBitsEncodedPrev * mQPprev);
        e2bpf = bitsEncoded / inputBitsPerFrame;

        mfxF64 frameFactor = 1.0;
        if (picType == MFX_FRAMETYPE_I) {
            if (mPicType == MFX_FRAMETYPE_P)
                frameFactor = P_WEIGHT / I_WEIGHT;
            else if (mPicType == MFX_FRAMETYPE_B)
                frameFactor = B_WEIGHT / I_WEIGHT;
        } else if (picType == MFX_FRAMETYPE_P) {
            if (mPicType == MFX_FRAMETYPE_B)
                frameFactor = B_WEIGHT / P_WEIGHT;
        }

        e2pe *= frameFactor;
        e2bpf *= frameFactor;

        if (e2pe > BRC_SCENE_CHANGE_RATIO1 && e2bpf > BRC_SCENE_CHANGE_RATIO2) {
            mSceneChange |= 1;
            mRCfa1 = mRCfa;
            mRCqa1 = mRCqa;
            if (picType != MFX_FRAMETYPE_B) {
                mSceneChange |= 16;
                mSChPoc = poc;
            }
        } else if (!repack)
            mSceneChange &= ~15;
    }

    mPicType = picType;

    mfxF64 fa = isField ? mRCfa*0.5 : mRCfa;
    bool oldScene = false;
    if ((mSceneChange & 16) && (mPoc < mSChPoc) && (mBitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (mfxF64)mBitsEncodedP) && (mfxF64)mBitsEncoded < 1.5*fa)
        oldScene = true;

    if (Sts != MFX_BRC_OK) {
        Sts = UpdateQuantHRD(totalFrameBits, Sts, overheadBits);
        mQuantUpdated = 0;
        mPicType = prevFrameType;
    } else {

        if (mParams.HRDBufferSizeBytes > 0) {
            mfxF64 frSizeTresh_sq = (mHRD.bufSize + 2*mHRD.bufFullness) * (fa + bitsPerPic) * 2.;
            if ((mfxF64)bitsEncoded * bitsEncoded > frSizeTresh_sq) {

                qstep = QP2Qstep(qp);
                qs = (mfxF64)bitsEncoded * bitsEncoded / frSizeTresh_sq;
                qstep = qstep * qs;
                qp = Qstep2QP(qstep) + 1;
                BRC_CLIP(qp, 1, mQuantMax);

                if (qp != qpprev) {
                    Sts = MFX_BRC_ERR_BIG_FRAME;
                    switch (mPicType) {
                    case (MFX_FRAMETYPE_I):
                        mQuantI = qp;
                        break;
                    case (MFX_FRAMETYPE_B):
                        mQuantB = qp;
                        break;
                    case (MFX_FRAMETYPE_P):
                    default:
                        mQuantP = qp;
                    }
                    mQuantUpdated = 0;


                    mRCq = mQuantI = mQuantP = qp;
                    if (mPicType == MFX_FRAMETYPE_B)
                        mQuantB = qp;
                    else {
                        mQuantB = ((qp + qp) * 563 >> 10) + 1;
                        BRC_CLIP(mQuantB, 1, mQuantMax);
                    }

                    mPicType = prevFrameType;

                    return Sts;
                }
            }
        }

        if (mScChFrameCnt <= 0) {
            if (mQuantUpdated == 0 && 1./qp < mRCqa)
                mRCqa += (1. / qp - mRCqa) / 4;
            else if (mQuantUpdated == 0)
                mRCqa += (1. / qp - mRCqa) / (mRCqap > 25 ? 25 : mRCqap);
            else
                mRCqa += (1. / qp - mRCqa) / mRCqap;
        }
        BRC_CLIP(mRCqa, 1./mQuantMax , 1./1.);

        if (mSceneChange & 15)
            mScChFrameCnt = mScChLength;

        if (repack != MFX_BRC_RECODE_PANIC && repack != MFX_BRC_RECODE_EXT_PANIC && !oldScene) {

            mQPprev = qp;
            mBitsEncodedPrev = mBitsEncoded;

            if (mScChFrameCnt > 0) {
                Sts = UpdateQuant_ScCh(bitsEncoded, totalFrameBits);
                mScChFrameCnt--;
            } else {
                Sts = UpdateQuant(bitsEncoded, totalFrameBits);
            }

            if (mPicType != MFX_FRAMETYPE_B) {
                mQuantPrev = mQuantP;
                mBitsEncodedP = mBitsEncoded;
            }

            mQuantP = mQuantI = mRCq;
        }
        mQuantUpdated = 1;
        //    mMaxBitsPerPic = mMaxBitsPerPicNot0;

        if (mParams.HRDBufferSizeBytes > 0) {
            mHRD.underflowQuant = -1;
            mBF += (mfxI64)mMaxBitrate * (mfxI64)mParams.frameRateExtD >> isField;
            mBF -= ((mfxI64)totalFrameBits >> 3) * mParams.frameRateExtN;
            if ((MFX_RATECONTROL_VBR == mRCMode) && (mBF > (mfxI64)mParams.HRDBufferSizeBytes * mParams.frameRateExtN))
                mBF = (mfxI64)mParams.HRDBufferSizeBytes * mParams.frameRateExtN;
        }
    }

    return Sts;
};

mfxBRCStatus H265BRC::UpdateQuant(mfxI32 bEncoded, mfxI32 totalPicBits)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    mfxF64  bo, qs, dq;
    mfxI32  quant;
    mfxI32 isfield = (mPictureFlags & MFX_PICSTRUCT_PROGRESSIVE) ?  0 : 1;
    mfxU32 bitsPerPic = (mfxU32)mBitsDesiredFrame >> isfield;
    mfxI64 totalBitsDeviation;

    if (isfield)
        mRCfa *= 0.5;

    quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : (mPicType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;

    if (mRecodeInternal) {
        mRCfa = bitsPerPic;
        mRCqa = mRCqa0;
        mRecodeInternal = 0;
    }

    mBitsEncodedTotal += totalPicBits;
    mBitsDesiredTotal += bitsPerPic;
    totalBitsDeviation = mBitsEncodedTotal - mBitsDesiredTotal;

    if (mPicType != MFX_FRAMETYPE_I || mRCMode == MFX_RATECONTROL_CBR || mQuantUpdated == 0)
        mRCfa += (bEncoded - mRCfa) / mRCfap;
    SetQuantB();
    if (mQuantUpdated == 0)
        if (mQuantB < quant)
            mQuantB = quant;
    qs = pow(bitsPerPic / mRCfa, 2.0);
    dq = mRCqa * qs;

    bo = (mfxF64)totalBitsDeviation / mRCbap / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);

    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./1.);
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
    if (isfield)
        mRCfa *= 2;

    return Sts;
}

mfxBRCStatus H265BRC::UpdateQuant_ScCh(mfxI32 bEncoded, mfxI32 totalPicBits)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    mfxF64  bo, qs, dq;
    mfxI32  quant, quant1;
    mfxI32 isfield = (mPictureFlags & MFX_PICSTRUCT_PROGRESSIVE) ?  0 : 1;
    mfxU32 bitsPerPic = (mfxU32)mBitsDesiredFrame >> isfield;
    mfxI64 totalBitsDeviation;

    if (isfield) {
        mRCfa *= 0.5;
        mRCfa1 *= 0.5;
    }

    quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : (mPicType == MFX_FRAMETYPE_B) ? mQuantB : mQuantP;

    mBitsEncodedTotal += totalPicBits;
    mBitsDesiredTotal += bitsPerPic;
    totalBitsDeviation = mBitsEncodedTotal - mBitsDesiredTotal;

    mRCfa += (bEncoded - mRCfa) / mRCfap;
    mRCfa1 += (bEncoded - mRCfa1) / mRCfap1;

    SetQuantB();

    qs = pow(bitsPerPic / mRCfa, 2.0);
    dq = mRCqa * qs;
    bo = (mfxF64)totalBitsDeviation / mRCbap / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);
    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./1.);
    quant = (mfxI32) (1. / dq + 0.5);

    qs = pow(bitsPerPic / mRCfa1, 2.0);
    dq = mRCqa1 * qs;
    bo = (mfxF64)totalBitsDeviation / mRCbap1 / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);
    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./1.);
    quant1 = (mfxI32) (1. / dq + 0.5);

    quant = (mfxI32)((quant1 * mScChFrameCnt + quant * (mScChLength - mScChFrameCnt) + 1) / (mfxF64)mScChLength);
    BRC_CLIP(quant, 1, mQuantMax);
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

    if (mQuantB < quant)
        mQuantB = quant;

    if (isfield) {
        mRCfa *= 2;
        mRCfa1 *= 2;
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

    qstep = QP2Qstep(quant);
    qstep_new = qstep * (mfxF64)bEncoded / wantedBits;

    quant = Qstep2QP(qstep_new);
    if (quant == quant_prev)
        quant += (sts == MFX_BRC_ERR_BIG_FRAME ? 1 : -1);

    BRC_CLIP(quant, 1, mQuantMax);

    if (quant < quant_prev) {
        while (quant <= mHRD.underflowQuant)
            quant++;
    }

    if (quant == quant_prev)
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    switch (mPicType) {
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

#endif