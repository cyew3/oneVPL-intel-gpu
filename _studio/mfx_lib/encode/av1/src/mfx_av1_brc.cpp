// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include <math.h>
#include <algorithm>
#include "mfx_av1_brc.h"

namespace {

    double const QSTEP[88] = {
         0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
         2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
        12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
        57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070,
        256.000, 287.350, 322.540, 362.039, 406.375, 456.140, 512.000, 574.701, 645.080, 724.077, 812.749, 912.280,
        1024.000, 1149.401, 1290.159, 1448.155, 1625.499, 1824.561, 2048.000, 2298.802, 2580.318, 2896.309, 3250.997, 3649.121,
        4096.000, 4597.605, 5160.637, 5792.619, 6501.995, 7298.242, 8192.000, 9195.209, 10321.273, 11585.238, 13003.989, 14596.485
    };

    uint8_t QStep2QpFloor(double qstep, uint8_t qpoffset = 0) // QSTEP[qp] <= qstep, return 0<=qp<=51+mQuantOffset
    {
        uint8_t qp = uint8_t(std::upper_bound(QSTEP, QSTEP + 52 + qpoffset, qstep) - QSTEP);
        return qp > 0 ? qp - 1 : 0;
    }

    uint8_t Qstep2QP(double qstep, uint8_t qpoffset = 0) // return 0<=qp<=51+mQuantOffset
    {
        uint8_t qp = QStep2QpFloor(qstep, qpoffset);
        return (qp == 51 + qpoffset || qstep < (QSTEP[qp] + QSTEP[qp + 1]) / 2) ? qp : qp + 1;
    }

    double QP2Qstep(uint32_t qp, uint32_t qpoffset = 0)
    {
        return QSTEP[std::min(51 + qpoffset, qp)];
    }
}

namespace AV1Enc {

//#define PRINT_BRC_STATS
#ifdef PRINT_BRC_STATS

static int brc_fprintf(char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   int r = vfprintf(stderr, fmt, args);
   va_end(args);
   return r;
}

#else
#define brc_fprintf
#endif

#define BRC_QSTEP_SCALE_EXPONENT 0.7
#define BRC_RATE_CMPLX_EXPONENT 0.5

mfxStatus AV1BRC::Close()
{
    mfxStatus status = MFX_ERR_NONE;
    if (!mIsInit)
        return MFX_ERR_NOT_INITIALIZED;
    mIsInit = false;
    return status;
}

mfxStatus AV1BRC::InitHRD()
{
    mfxU64 bufSizeBits = mParams.HRDBufferSizeBytes << 3;
    mfxU64 maxBitrate = mParams.maxBitrate;

    if (MFX_RATECONTROL_VBR != mRCMode)
        maxBitrate = mBitrate;

    if (maxBitrate < (mfxU64)mBitrate)
        maxBitrate = mBitrate;

    mParams.maxBitrate = (int32_t)((maxBitrate >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE));
    mHRD.maxBitrate = mParams.maxBitrate;
    mHRD.inputBitsPerFrame = mHRD.maxBitrate / mFramerate;

    if (mHRD.maxBitrate < mBitrate)
        mBitrate = mHRD.maxBitrate;

    int32_t bitsPerFrame = (int32_t)(mBitrate / mFramerate);

    if (bufSizeBits > 0 && bufSizeBits < (mfxU64)(bitsPerFrame << 1))
        bufSizeBits = (bitsPerFrame << 1);

    mHRD.bufSize = (uint32_t)((bufSizeBits >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE));
    mParams.HRDBufferSizeBytes = (int32_t)(mHRD.bufSize >> 3);

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

int32_t AV1BRC::GetInitQP()
{
  int32_t fs, fsLuma;

  fsLuma = mParams.width * mParams.height;
  fs = fsLuma;
  if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
    fs += fsLuma / 2;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
    fs += fsLuma;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
    fs += fsLuma * 2;
  fs = fs * mParams.bitDepthLuma / 8;

  double qstep = pow(1.5 * fs * mFramerate / mBitrate, 0.8);
  int32_t q = Qstep2QP(qstep, mQuantOffset) + mQuantOffset;

  BRC_CLIP(q, 1, mQuantMax);

  return q;
}

mfxStatus AV1BRC::SetParams(const mfxVideoParam *params, AV1VideoParam &video)
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
        if (mParams.HRDInitialDelayBytes > mParams.HRDBufferSizeBytes)
            mParams.HRDInitialDelayBytes = mParams.HRDBufferSizeBytes;
    } else {
        mParams.HRDBufferSizeBytes = mParams.HRDInitialDelayBytes = 0;
    }

    mParams.maxBitrate = params->mfx.MaxKbps * brcParamMultiplier * 1000;

    mParams.frameRateExtN = video.FrameRateExtN;
    mParams.frameRateExtD = video.FrameRateExtD;
    if (!mParams.frameRateExtN || !mParams.frameRateExtD)
        return MFX_ERR_INVALID_VIDEO_PARAM;


    mParams.width = params->mfx.FrameInfo.Width;
    mParams.height = params->mfx.FrameInfo.Height;
    if (params->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        mParams.height >>= 1;
    mParams.chromaFormat = params->mfx.FrameInfo.ChromaFormat;
    mParams.bitDepthLuma =video.bitDepthLuma;

    return MFX_ERR_NONE;
}

mfxStatus AV1BRC::Init(const mfxVideoParam *params,  AV1VideoParam &video, int32_t enableRecode)
{
    mfxStatus status = MFX_ERR_NONE;

    if (!params)
        return MFX_ERR_NULL_PTR;

    status = SetParams(params, video);
    if (status != MFX_ERR_NONE)
        return status;

    mLowres = video.LowresFactor ? 1 : 0;
    mRecode = (enableRecode && (mParams.BRCMode != MFX_RATECONTROL_AVBR)) ? 1 : 0;

    mFramerate = (double)mParams.frameRateExtN / mParams.frameRateExtD;
    mBitrate = mParams.targetBitrate;
    mRCMode = (mfxU16)mParams.BRCMode;

    if (mBitrate <= 0 || mFramerate <= 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        status = InitHRD();
        if (status != MFX_ERR_NONE)
            return status;
        mMaxBitrate = mParams.maxBitrate >> 3;
        mBF = (mfxI64)mParams.HRDInitialDelayBytes * mParams.frameRateExtN;
        mBFsaved = mBF;

        mPredBufFulness = mHRD.bufFullness;
        mRealPredFullness = mHRD.bufFullness;
    }

    mBitsDesiredFrame = (int32_t)(mBitrate / mFramerate);
    if (mBitsDesiredFrame < 10)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mQuantOffset = 6 * (mParams.bitDepthLuma - 8);

    mQuantMax = 51 + mQuantOffset;
    mQuantMin = 1;
    mMinQp = mQuantMin;


    mBitsDesiredTotal = 0;
    mBitsEncodedTotal = 0;

    mQuantUpdated = 1;
    mRecodedFrame_encOrder = -1;

    if (video.AnalyzeCmplx) {

        mTotalDeviation = 0;

        mNumLayers = video.PGopPicSize > 1 ?  2 : video.BiPyramidLayers + 1;
        if (video.GopRefDist > 1 && video.BiPyramidLayers == 1)
            mNumLayers = 3; // in case of BiPyramid is off there are 3 layers for BRC: I, P, and other Bs

        if (mRCMode == MFX_RATECONTROL_AVBR) {
            Zero(mEstCoeff);
            Zero(mEstCnt);
        }
        mTotAvComplx = 0;
        mTotComplxCnt = 0;
        mTotPrevCmplx = 0;
        mComplxSum = 0;
        mTotMeanComplx = 0;
        mTotTarget = mBitsDesiredFrame;
        mLoanLength = 0;
        mLoanBitsPerFrame = 0;

        //mLoanLengthP = 0;
        //mLoanBitsPerFrameP = 0;

        mQstepBase = -1;

        if (mRCMode != MFX_RATECONTROL_AVBR) {
            Zero(mPrevCmplxLayer);
            Zero(mPrevQstepLayer);
            Zero(mPrevBitsLayer);
        }
        Zero(mPrevQpLayer);

        mPrevBitsIP = -1;
        mPrevCmplxIP = -1;
        mPrevQstepIP = -1;
        mPrevQpIP = -1;

        mEncOrderCoded = -1;

        int32_t fs;
        fs = mParams.width * mParams.height;
        if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
            fs += fs / 2;
        else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
            fs += fs;
        else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
            fs += fs * 2;

        mCmplxRate = 20.*pow((double)fs, 0.8); // adjustment for subpel
        if (mQuantOffset)
            mCmplxRate *= (1 << mQuantOffset / 6) * 0.9;
        mQstepScale0 = mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);// * (1 << mQuantOffset / 6);

        double sum = 1;
        double layerRatio = 0.3; // tmp ???
        double layerShare = layerRatio;
        for (int32_t i = 2; i < mNumLayers; i++) {
            sum += layerShare;
            layerShare *= 2*layerRatio;
        }
        Zero(mLayerTarget);
        double targetMiniGop = mBitsDesiredFrame * video.GopRefDist;
        mLayerTarget[1] = targetMiniGop / sum;
        mLayerTarget[0] = mLayerTarget[1] * 3;

        brc_fprintf("layer Targets: %d %d ", (int)mLayerTarget[0], (int)mLayerTarget[1]);
        for (int32_t i = 2; i < mNumLayers; i++) {
            mLayerTarget[i] = mLayerTarget[i-1]*layerRatio;
            brc_fprintf(" %d", (int)mLayerTarget[i]);
        }
        brc_fprintf("\n");

        if (mRCMode == MFX_RATECONTROL_AVBR) {
            for (int32_t i = 0; i < mNumLayers; i++) {
                mEstCoeff[i] = mLayerTarget[i] * 10.;
                mEstCnt[i] = 1.;
            }
        }

    } else {

        int32_t q = GetInitQP();

        if (!mRecode) {
            if (q - 6 > 10)
                mQuantMin = std::max(10, q - 10);
            else
                mQuantMin = std::max(q - 6, 2);

            if (q < mQuantMin)
                q = mQuantMin;
        }

        mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = q;

        mRCbap = 100;
        mRCqap = 100;
        mRCfap = 100;

        mRCq = q;
        mRCqa = mRCqa0 = 1. / (double)mRCq;
        mRCfa = mBitsDesiredFrame;
        mRCfa_short = mBitsDesiredFrame;

        mSChPoc = 0;
        mSceneChange = 0;
        mBitsEncodedPrev = mBitsDesiredFrame;

        mMaxQp = 999;
        mMinQp = -1;
    }

    mPicType = MFX_FRAMETYPE_I;
    mSceneNum = 0;
    mIsInit = true;

    return status;
}


#define RESET_BRC_CORE \
{ \
    if (sizeNotChanged) { \
        mRCq = (int32_t)(1./mRCqa * pow(mRCfa/mBitsDesiredFrame, 0.32) + 0.5); \
        BRC_CLIP(mRCq, 1, mQuantMax); \
    } else \
        mRCq = GetInitQP(); \
    mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = mRCq; \
    mRCqa = mRCqa0 = 1./mRCq; \
    mRCfa = mBitsDesiredFrame; \
    mRCfa_short = mBitsDesiredFrame; \
}


mfxStatus AV1BRC::Reset(mfxVideoParam *params, AV1VideoParam &video, int32_t enableRecode)
{
    mfxStatus status;

    mfxU16 brcParamMultiplier = params->mfx.BRCParamMultiplier ? params->mfx.BRCParamMultiplier : 1;
    mfxU16 rcmode_new = params->mfx.RateControlMethod;
    int32_t bufSize_new = rcmode_new == MFX_RATECONTROL_AVBR ? 0 : (params->mfx.BufferSizeInKB * brcParamMultiplier * 8000 >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE);
    bool sizeNotChanged = (mParams.width == params->mfx.FrameInfo.Width &&
                           mParams.height == params->mfx.FrameInfo.Height &&
                           mParams.chromaFormat == params->mfx.FrameInfo.ChromaFormat);

    int32_t bufSize, maxBitrate;
    double bufFullness;
    mfxU16 rcmode = mRCMode;
    int32_t targetBitrate = mBitrate;

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
        mFramerate = (double)mParams.frameRateExtN / mParams.frameRateExtD;
        if (mBitrate <= 0 || mFramerate <= 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        mBitsDesiredFrame = (int32_t)(mBitrate / mFramerate);
        RESET_BRC_CORE
        return MFX_ERR_NONE;
    } else if (mParams.HRDBufferSizeBytes == 0 && bufSize_new > 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    int32_t maxBitrate_new = (params->mfx.MaxKbps * brcParamMultiplier * 1000 >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);
    int32_t targetBitrate_new = params->mfx.TargetKbps * brcParamMultiplier * 1000;
    int32_t targetBitrate_new_r = (targetBitrate_new >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);

    // framerate change not allowed in case of HRD
    if (mParams.frameRateExtN * video.FrameRateExtD != mParams.frameRateExtD * video.FrameRateExtN)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    bufSize = mHRD.bufSize;
    maxBitrate = (int32_t)mHRD.maxBitrate;
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
            double bf_delta = (maxBitrate_new - maxBitrate) / mFramerate;
          // lower estimate for the fullness with the bitrate updated at tai;
          // for VBR the fullness encoded in buffering period SEI can be below the real buffer fullness
            bufFullness += bf_delta;
            if (bufFullness > (double)(bufSize - 1))
                bufFullness = (double)(bufSize - 1);

            mBF += (mfxI64)((maxBitrate_new >> 3) - mMaxBitrate) * (mfxI64)mParams.frameRateExtD;
            if (mBF > (mfxI64)(bufSize >> 3) * mParams.frameRateExtN)
                mBF = (mfxI64)(bufSize >> 3) * mParams.frameRateExtN;

            maxBitrate = maxBitrate_new;
        } else // HRD overflow is possible for CBR
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (bufSize_new > 0 && bufSize_new < bufSize) {
        bufSize = bufSize_new;
        if (bufFullness > (double)(bufSize - 1)) {
            if (MFX_RATECONTROL_VBR == rcmode)
                bufFullness = (double)(bufSize - 1);
            else
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if (targetBitrate > maxBitrate)
        targetBitrate = maxBitrate;

    mHRD.bufSize = bufSize;
    mHRD.maxBitrate = (double)maxBitrate;
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
    mMaxBitrate = (uint32_t)(maxBitrate >> 3);

    RESET_BRC_CORE

    mSceneChange = 0;
    mBitsEncodedPrev = mBitsDesiredFrame;

    mMaxQp = 999;
    mMinQp = -1;

    return MFX_ERR_NONE;
}

#define BRC_QSTEP_COMPL_EXPONENT 0.4
#define BRC_QSTEP_COMPL_EXPONENT_AVBR 0.2
#define BRC_CMPLX_DECAY 0.1
#define BRC_CMPLX_DECAY_AVBR 0.2
#define BRC_MIN_CMPLX 0.01
#define BRC_MIN_CMPLX_LAYER 0.05
#define BRC_MIN_CMPLX_LAYER_I 0.01

static const double brc_qstep_factor[8] = {pow(2., -1./6.), 1., pow(2., 1./6.),   pow(2., 2./6.), pow(2., 3./6.), pow(2., 4./6.), pow(2., 5./6.), 2.};
static const double minQstep = QSTEP[1];
static const double maxQstepChange = pow(2, 0.5);
static const double predCoef = 1000000. / (1920 * 1080);

int32_t AV1BRC::PredictFrameSize(AV1VideoParam *video, Frame *frames[], int32_t qp, mfxBRC_FrameData *refFrData)
{
    int32_t i;
    double cmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;
    int32_t layer = (frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[0]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + std::max(1, frames[0]->m_pyramidLayer)));
    int32_t predBitsLayer = -1, predBits = -1;
    double q = QP2Qstep(qp, mQuantOffset);
    int32_t refQp = -1, refEncOrder = -1, refDispOrder = -1;
    double refQstep = 0;

    if (frames[0]->m_picCodeType == MFX_FRAMETYPE_I)
    {
        double w;
        double predBits0 = cmplx * predCoef * mParams.width * mParams.height / q;

        if (mQuantOffset)
            predBits0 *= (1 << mQuantOffset / 6) * 0.9;

        if (mPrevCmplxLayer[0] > 0) {
            w = fabs(cmplx / mPrevCmplxLayer[0] - 1) * 0.1;
            if (w > 1)
                w = 1;
            if (mPrevCmplxLayer[0] > BRC_MIN_CMPLX_LAYER_I)
                predBits = cmplx / mPrevCmplxLayer[0] * mPrevQstepLayer[0] / q * mPrevBitsLayer[0];
            else
                predBits = cmplx / BRC_MIN_CMPLX_LAYER_I * mPrevQstepLayer[0] / q * mPrevBitsLayer[0];

            double predBits1 = predBits;
            predBits = (int32_t)(w*predBits0  + (1 - w)*predBits);
            if (mPrevCmplxLayer[0] > 0.1 && cmplx > mPrevCmplxLayer[0]) {
                predBits  = std::max(predBits1, (double)predBits);
            }

        } else // can never happen
            predBits = predBits0;

        brc_fprintf("%f %f %f %f %d | %d \n", cmplx , mPrevCmplxLayer[0] , mPrevQstepLayer[0] , q , mPrevBitsLayer[0], predBits);

        refFrData->encOrder = -1;
        refFrData->qp = -1; // ???
        refFrData->cmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;

    }
    else if (frames[0]->m_picCodeType == MFX_FRAMETYPE_P)
    {
        Frame *refFrame = frames[0]->m_refPicList[0].m_refFrames[0];
        refEncOrder = refFrame->m_encOrder;
        refDispOrder = refFrame->m_frameOrder;

        int32_t size = mMiniGopFrames.size();
        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].encOrder == refEncOrder) {
                refQstep = mMiniGopFrames[i].qstep;
                refQp = mMiniGopFrames[i].qp;
                break;
            }
        }

        if (refQp < 0) { // ref frame is not in the buffer (rare case)
            for (i = 0; i < size; i++) {
                if (mMiniGopFrames[i].layer <= 1 && mMiniGopFrames[i].encOrder < frames[0]->m_encOrder) {
                    refDispOrder = mMiniGopFrames[i].dispOrder;
                    refEncOrder = mMiniGopFrames[i].encOrder;
                    refQstep = mMiniGopFrames[i].qstep;
                    refQp = mMiniGopFrames[i].qp;
                }
            }
        }

        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].dispOrder < frames[0]->m_frameOrder && mMiniGopFrames[i].dispOrder > refDispOrder) {
                if (mMiniGopFrames[i].cmplx > cmplx)
                    cmplx = mMiniGopFrames[i].cmplx;
            }
        }

        refFrData->encOrder = refEncOrder;
        refFrData->qp = refQp;
        refFrData->cmplx = cmplx;

        if (mPrevCmplxLayer[layer] > BRC_MIN_CMPLX_LAYER)
            predBitsLayer = cmplx / mPrevCmplxLayer[layer] * mPrevQstepLayer[layer] / q * mPrevBitsLayer[layer];

        if (mPrevCmplxLayer[layer - 1] > BRC_MIN_CMPLX)
            predBits = cmplx / mPrevCmplxLayer[layer-1] * mPrevQstepLayer[layer-1] / q * mPrevBitsLayer[layer-1];

        brc_fprintf(" -- %d %d -- ", qp, refQp);

        if (qp < refQp) {
            double intraCmplx = frames[0]->m_stats[mLowres]->m_avgIntraSatd;

            if (video->GopRefDist <= 2) {
                double w = ((double)refQp - qp) / 12;
                if (w > 0.5)
                    w = 0.5;
                intraCmplx = w*intraCmplx + (1 - w)*cmplx;
            }

            if (mPrevCmplxIP >= 0 && frames[0]->m_sceneOrder == mSceneNum) {
                if (mPrevCmplxIP > BRC_MIN_CMPLX) {
                    predBits = intraCmplx / mPrevCmplxIP * mPrevQstepIP / q * mPrevBitsIP;
                    if (predBits > predBitsLayer)
                        predBitsLayer = predBits;
                }
            } else if (mPrevCmplxLayer[0] > BRC_MIN_CMPLX) {
                predBits = intraCmplx / mPrevCmplxLayer[0] * mPrevQstepLayer[0] / q * mPrevBitsLayer[0];
                if (predBits > predBitsLayer)
                    predBitsLayer = predBits;
            }
        }
    }
    else
    {

        Frame *refFrame0 = frames[0]->m_refPicList[0].m_refFrames[0];
        Frame *refFrame1 = frames[0]->m_refPicList[1].m_refFrames[0];

        refEncOrder = refFrame0->m_encOrder;
        refDispOrder = refFrame0->m_frameOrder;


        int32_t size = mMiniGopFrames.size();
        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].encOrder == refEncOrder) {
                refQstep = mMiniGopFrames[i].qstep;
                refQp = mMiniGopFrames[i].qp;
                break;
            }
        }

        if (refQp < 0) { // ref frame is not in the buffer
            for (i = 0; i < size; i++) {
                if (mMiniGopFrames[i].layer < layer && mMiniGopFrames[i].dispOrder < frames[0]->m_frameOrder) {
                    refDispOrder = mMiniGopFrames[i].dispOrder;
                    refEncOrder = mMiniGopFrames[i].encOrder;
                    refQstep = mMiniGopFrames[i].qstep;
                    refQp = mMiniGopFrames[i].qp;
                }
            }
        }

        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].dispOrder < frames[0]->m_frameOrder && mMiniGopFrames[i].dispOrder > refDispOrder) {
                if (mMiniGopFrames[i].cmplx > cmplx)
                    cmplx = mMiniGopFrames[i].cmplx;
            }
        }

        double pastCmpx = cmplx;
        double pastQstep = refQstep;
        int32_t pastQp = refQp;
        int32_t pastEncOrder = refEncOrder;

        refQp = -1;
        refEncOrder = refFrame1->m_encOrder;
        refDispOrder = refFrame1->m_frameOrder;


        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].encOrder == refEncOrder) {
                refQstep = mMiniGopFrames[i].qstep;
                refQp = mMiniGopFrames[i].qp;
                break;
            }
        }

        if (refQp < 0) { // ref frame is not in the buffer
            for (i = size - 1; i >= 0; i--) {
                if (mMiniGopFrames[i].layer < layer && mMiniGopFrames[i].dispOrder > frames[0]->m_frameOrder) {
                    refDispOrder = mMiniGopFrames[i].dispOrder;
                    refEncOrder = mMiniGopFrames[i].encOrder;
                    refQstep = mMiniGopFrames[i].qstep;
                    refQp = mMiniGopFrames[i].qp;
                }
            }
        }

        double futCmpx = -1;
        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].dispOrder > frames[0]->m_frameOrder && mMiniGopFrames[i].dispOrder <= refDispOrder) {
                if (mMiniGopFrames[i].cmplxInter > futCmpx)
                    futCmpx = mMiniGopFrames[i].cmplxInter;
            }
        }

        if (futCmpx < 0)
            futCmpx = 1e5;
        double futQstep = refQstep;
        int32_t futQp = refQp;
        int32_t futEncOrder = refEncOrder;

        // need ot take into account intraSatd or at least intraShare (?)
        if (qp >= pastQp && qp >= futQp) {
            if (futCmpx < pastCmpx) {
                cmplx = futCmpx;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;

            } else {
                cmplx = pastCmpx;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            }
        } else if (qp >= pastQp && qp < futQp) {
            if (pastCmpx < futCmpx * futQstep / q)  {
                cmplx = pastCmpx;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            } else {
                cmplx = futCmpx * futQstep / q;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;
            }
        } else if (qp < pastQp && qp >= futQp) {
            if (futCmpx < pastCmpx * pastQstep / q)  {
                cmplx = futCmpx;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;
            } else {
                cmplx = pastCmpx * pastQstep / q;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            }
        } else { // qp < pastQp && qp < futQp
            if (pastCmpx <= futCmpx * sqrt(futQstep / pastQstep)) {
                cmplx = pastCmpx * pastQstep / q;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            } else {
                cmplx = futCmpx * futQstep / q;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;
            }
        }

        refFrData->cmplx = cmplx;

        if (mPrevCmplxLayer[layer] > BRC_MIN_CMPLX_LAYER)
            predBitsLayer = cmplx / mPrevCmplxLayer[layer] * mPrevQstepLayer[layer] / q * mPrevBitsLayer[layer];

        if (mPrevCmplxLayer[layer - 1] > BRC_MIN_CMPLX)
            predBits = cmplx / mPrevCmplxLayer[layer-1] * mPrevQstepLayer[layer-1] / q * mPrevBitsLayer[layer-1];

    }

    if (predBits < 0 && predBitsLayer < 0) {
        predBitsLayer = cmplx / BRC_MIN_CMPLX_LAYER * mPrevQstepLayer[layer] / q * mPrevBitsLayer[layer];
        if (layer > 0)
            predBits = cmplx / BRC_MIN_CMPLX * mPrevQstepLayer[layer-1] / q * mPrevBitsLayer[layer-1];
    }

    if (predBitsLayer >= 0 && predBits >= 0) {

        double ratL, rat;
        if (mPrevCmplxLayer[layer] > 0) {
            ratL = cmplx / mPrevCmplxLayer[layer];
            if (ratL > 1)
                ratL = 1/ratL;
        } else
            ratL = 0;
        if (mPrevCmplxLayer[layer-1] > 0) {
            rat = cmplx / mPrevCmplxLayer[layer-1];
            if (rat > 1)
                rat = 1/rat;
        } else
            rat = 0;

        if (ratL > 0.8 * rat)
            predBits = predBitsLayer;

    } else if (predBitsLayer >= 0)
        predBits = predBitsLayer;

    return predBits;
}

void AV1BRC::SetMiniGopData(Frame *frames[], int32_t numFrames)
{
    int32_t frcnt = 0, i;
    int32_t size;
    int32_t start = 0;
    int32_t toerase = 0;

    if (numFrames <= 0)
        return;

    if (mMiniGopFrames.size() > 0) {
        if (mMiniGopFrames.back().encOrder <= mEncOrderCoded)
            mMiniGopFrames.clear();
        else {
            for (i = (int32_t)mMiniGopFrames.size() - 1; i > 0; i--) {
                if (mMiniGopFrames[i].layer <= 1) {
                    if (mMiniGopFrames[i - 1].encOrder <= mEncOrderCoded) {
                        toerase = i;
                        break;
                    }
                }
            }
            if (toerase > 0) {
                for (i = toerase - 1; i >= 0; i--) {
                    if (mMiniGopFrames[i].layer <= 1) { // need to keep prev P for not yet coded B's
                        if (i > 0)
                            mMiniGopFrames.erase(mMiniGopFrames.begin(), mMiniGopFrames.begin() + i);
                        break;
                    } else
                        mMiniGopFrames.erase(mMiniGopFrames.begin() + i);
                }
            }
        }

        for (i = (int32_t)mMiniGopFrames.size() - 1; i >= 0; i--) {
            if (frames[0]->m_encOrder >  mMiniGopFrames[i].encOrder) {
                start = i + 1;
                break;
            }
        }
    }

    size = numFrames + start;
    mMiniGopFrames.resize(size);

    for (i = 0; i < numFrames; i++) {
        mMiniGopFrames[i + start].cmplx = frames[i]->m_stats[mLowres]->m_avgBestSatd;
        mMiniGopFrames[i + start].cmplxInter = frames[i]->m_stats[mLowres]->m_avgInterSatd;
        mMiniGopFrames[i + start].encOrder = frames[i]->m_encOrder;
        mMiniGopFrames[i + start].dispOrder = frames[i]->m_frameOrder;
        mMiniGopFrames[i + start].layer = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + std::max(1, frames[i]->m_pyramidLayer)));
    }
}

void AV1BRC::UpdateMiniGopData(Frame *frame, int8_t qp)
{
    uint32_t i;
    for (i = 0; i < mMiniGopFrames.size(); i++) {
        if (mMiniGopFrames[i].encOrder == frame->m_encOrder) {
            mMiniGopFrames[i].qp = qp;
            mMiniGopFrames[i].qstep = QP2Qstep(qp, mQuantOffset);
            break;
        }
    }
}

int32_t AV1BRC::GetQP(AV1VideoParam *video, Frame *frames[], int32_t numFrames)
{
    Frame *frame;
    int32_t i, j;
    int32_t qp;

    if (video->AnalyzeCmplx) {

        if (!frames || numFrames <= 0) {
            if (mQstepBase > 0)
                return Qstep2QP(mQstepBase, mQuantOffset) - mQuantOffset;
            else
                return GetInitQP() - mQuantOffset;
        }

        // limit ourselves to miniGOP+1 for the time being
        int32_t lalenlim = (video->picStruct == MFX_PICSTRUCT_PROGRESSIVE ? video->GopRefDist + 1 : video->GopRefDist*2 /* + 2*/);
        if (numFrames > lalenlim)
            numFrames = lalenlim;

        int32_t len = numFrames;
        //for (i = 1; i < numFrames; i++) {
        //    if (!frames[i] || (frames[i]->m_stats[mLowres]->m_sceneCut)) {
        //        len = i;
        //        break;
        //    }
        //}
#ifdef PRINT_BRC_STATS
        if (frames[0]->m_stats[mLowres]->m_sceneCut > 0)
            brc_fprintf("SCENE CUT %d \n", frames[0]->m_encOrder);
#endif

        double scCmplx = -1;
        int32_t rfo = -1, reo = -1, ri = -1, rso;
        int32_t eso = -1, ei = -1;
        for (i = 0; i < len; i++) {
            if (frames[i] && (frames[i]->m_stats[mLowres]->m_sceneCut > 0)) { // enc order scene cut frame
                eso = frames[i]->m_sceneOrder;
                ei = i;
            }
            if (frames[i] && (frames[i]->m_stats[mLowres]->m_sceneCut < 0)) { // real (frame order) scene cut frame
                if (frames[i]->m_sceneOrder == eso) { // enc order scene cut must have been met, we need the sc frame nearest in frame order to the I (in case we have several scene cuts within frames[])
                    rfo = frames[i]->m_frameOrder;
                    reo = frames[i]->m_encOrder;
                    ri = i;
                }
            }
        }

        int32_t mindist = 999;
        if (ri >= 0) {
            for (i = 0; i < len; i++) {
                if (frames[i] && (frames[i]->m_sceneOrder == eso) && (i != ri)) { // real (frame order) scene cut frame
                    int32_t dist = frames[i]->m_frameOrder - rfo; // can not be <= 0
                    if (dist < mindist) {
                        scCmplx = frames[i]->m_stats[mLowres]->m_avgInterSatd;
                        mindist = dist;
                    }
                }
            }

            if (scCmplx < frames[ri]->m_stats[mLowres]->m_avgBestSatd)
                frames[ri]->m_stats[mLowres]->m_avgBestSatd = scCmplx;

            frames[ri]->m_stats[mLowres]->m_sceneCut = 0; // we estimate the cmplx the first time we see the scene cut frame, and don't update it later - shuold be good enough for our needs
        }

        double avCmplx = 0;

        if (frames[0]->m_stats[mLowres]->m_sceneCut > 0) {
            int32_t cnt = 1;
            avCmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;
            for (i = 0; i < len; i++) {
                if (frames[i]->m_sceneOrder == eso) {
                    avCmplx += frames[i]->m_stats[mLowres]->m_avgBestSatd;
                    cnt++;
                }
            }
            avCmplx /= cnt;
        } else {
            for (i = 0; i < len; i++)
                avCmplx += frames[i]->m_stats[mLowres]->m_avgBestSatd;
            avCmplx /= len;
        }
        frames[0]->m_avCmplx = avCmplx;

        frames[0]->m_totAvComplx = mTotAvComplx;
        frames[0]->m_totComplxCnt = mTotComplxCnt;
        frames[0]->m_complxSum = mComplxSum;

        double w = 0, qScale, r, prCmplx = mTotPrevCmplx;
        double decay = mRCMode != MFX_RATECONTROL_AVBR ? BRC_CMPLX_DECAY : BRC_CMPLX_DECAY_AVBR;

        mTotAvComplx *= decay;
        mTotComplxCnt *= decay;

        if (avCmplx < BRC_MIN_CMPLX)
            avCmplx = BRC_MIN_CMPLX;

        mTotAvComplx += avCmplx;
        mTotComplxCnt += 1;
        double complx = mTotAvComplx / mTotComplxCnt;

        mComplxSum += avCmplx;
        mTotMeanComplx = mComplxSum / (frames[0]->m_encOrder + 1);

        double expnt = mRCMode != MFX_RATECONTROL_AVBR ? BRC_QSTEP_COMPL_EXPONENT : BRC_QSTEP_COMPL_EXPONENT_AVBR;
        frames[0]->m_CmplxQstep = pow(complx, expnt);

        int32_t layer = (frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[0]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + std::max(1, frames[0]->m_pyramidLayer)));

        //commented out since it affected certain clips (eg. bbc)
        //if (frames[0]->m_encOrder == 0) {
        //    if (avCmplx > 6)
        //        mQstepScale0 = mQstepScale = mQstepScale0 * avCmplx / 6;
        //}

        if (prCmplx) {
            r = fabs(avCmplx / prCmplx - 1);
            {
                w = r * 0.1;
                if (w > 1) w = 1;

                qScale = w * mQstepScale0 + (1 - w) * mQstepScale;

                if (mRCMode != MFX_RATECONTROL_AVBR || ((qScale - mQstepScale) * (avCmplx - prCmplx) > 0))
                {
                    mCmplxRate *= qScale / mQstepScale;
                    brc_fprintf("prCmplx %d %.3f %.3f %.1f %.3f %.3f \n", frames[0]->m_encOrder, avCmplx , prCmplx,  mCmplxRate , mQstepScale, qScale);
                    mQstepScale = qScale;
                }
            }
        }

        double q = frames[0]->m_CmplxQstep * mQstepScale;


        brc_fprintf("%d %.3f %.3f %.4f %.3f %.3f \n", frames[0]->m_encOrder, q, complx, frames[0]->m_CmplxQstep , mQstepScale, w);

        double targetBits = 0;
        for (i = 0; i < len; i++) {
            int32_t lr = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + std::max(1, frames[i]->m_pyramidLayer)));
            targetBits += mLayerTarget[lr];
        }

        if (mRCMode != MFX_RATECONTROL_AVBR) // cbr/vbr
        {
            int32_t predbits;
            mfxBRC_FrameData refFrameData;
            int32_t qpbase = Qstep2QP(q, mQuantOffset);
            int32_t qpmin = 0, qp_, qp0;

            if (frames[0]->m_picCodeType == MFX_FRAMETYPE_I || frames[0]->m_picCodeType == MFX_FRAMETYPE_P)
                SetMiniGopData(frames, len);

            qp0 = qp = std::max(qpbase + layer - 1, 1);
            qp_ = std::max(qp - 3, 1);

            frames[0]->m_qpBase = qp;

            brc_fprintf("%d: l-%d %d qp %d %.3f %.3f %.3f \n", frames[0]->m_encOrder, layer, len, qp, complx,  frames[0]->m_stats[mLowres]->m_avgBestSatd, frames[0]->m_stats[mLowres]->m_avgIntraSatd);

            double maxbits;

            for (;;) {

                if (mPrevQstepLayer[layer] <= 0  && frames[0]->m_encOrder == 0) {
                    double qi = QP2Qstep(qp, mQuantOffset);
                    predbits = frames[0]->m_stats[mLowres]->m_avgBestSatd * predCoef * mParams.width * mParams.height / qi;
                    if (mQuantOffset)
                        predbits =  (int32_t)((predbits << mQuantOffset/6) * 0.9);
                    refFrameData.qp = qp;
                    refFrameData.encOrder = -1;
                    refFrameData.cmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;

                } else {
                    predbits = PredictFrameSize(video, frames, qp, &refFrameData);
                }


                if (mPrevQpLayer[layer] <= 0) {
                    mPrevCmplxLayer[layer] = refFrameData.cmplx;
                    mPrevQpLayer[layer] = qp;
                    mPrevQstepLayer[layer] = QP2Qstep(qp, mQuantOffset);
                    mPrevBitsLayer[layer] = predbits;
                }
                brc_fprintf(" %d %d \n", qp, predbits);

                maxbits = mPredBufFulness >> 1;

                if ((layer > 1) || qp >= refFrameData.qp)
                    if (maxbits > mHRD.bufSize >> 2)
                        maxbits = mHRD.bufSize >> 2;

                if (predbits > maxbits && qp < mQuantMax) { // frame is predicted to exceed half buf fullness
                    qp++;
                    qpmin = qp;
                } else if ((int32_t)(mPredBufFulness + mBitsDesiredFrame) - predbits > (int32_t)mHRD.bufSize) {
                    if (qp > qpmin + 1 && qp > qp_)
                        qp--;
                    else
                        break;
                } else
                    break;
            }

            if (video->m_framesInParallel <= video->GopRefDist / 2) {
                if (refFrameData.encOrder > mEncOrderCoded) {
                    if (qp < refFrameData.qp) {
                        qp = refFrameData.qp;
                    }
                }
            }

            UpdateMiniGopData(frames[0], qp);

            int32_t predbufFul;
            qpbase = qp + 1 - layer;
            int32_t refqp = -1;

            for (;;) {
                int32_t qpp = qpbase;
                q = QP2Qstep(qpbase, mQuantOffset);

                // trying to forecast the buffer state for the LA frames
                int32_t forecastBits = predbits;
                //int32_t maxRefQp = -1, minRefQp = 99;
                for (i = 1; i < len; i++) {
                    int32_t lri = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + std::max(1, frames[i]->m_pyramidLayer)));
                    int32_t qpi = qpbase + lri - 1;
                    UpdateMiniGopData(frames[i], qpi);
                    int32_t bits = PredictFrameSize(video, &frames[i], qpi, &refFrameData);
                    if (frames[i]->m_picCodeType == MFX_FRAMETYPE_P) {
                        refqp = refFrameData.qp;  // currently there can be only one p-frame (2 p-fields with the same refqp) in frames[]
                        frames[i]->m_refQp = refqp;
                    }

                    brc_fprintf("( %d %d ) ", frames[i]->m_encOrder, bits);
                    if (mPrevQpLayer[lri] <= 0) {
                        //mPrevCmplxLayer[lri] = frames[i]->m_stats[mLowres]->m_avgBestSatd;
                        mPrevCmplxLayer[lri] = refFrameData.cmplx; // ???
                        mPrevQpLayer[lri] = qpi;
                        mPrevQstepLayer[lri] = QP2Qstep(qpi, mQuantOffset);
                        mPrevBitsLayer[lri] = bits;
                    }

                    forecastBits += bits;
                }
                brc_fprintf("\n");

                predbufFul = mPredBufFulness;
                if (mRCMode == MFX_RATECONTROL_VBR && mParams.maxBitrate > (int32_t)mBitrate && mPredBufFulness > mRealPredFullness)
                    mPredBufFulness = mRealPredFullness;

                int32_t targetfullness = std::max(mParams.HRDInitialDelayBytes << 3, (int32_t)mHRD.bufSize >> 1);
                int32_t curdev = targetfullness - mPredBufFulness;
                int64_t estdev = curdev + forecastBits - targetBits;

    //            int32_t maxdevSoft = std::min((int32_t)(mHRD.bufSize >> 3), mPredBufFulness >> 1);
                int32_t maxdevSoft = std::max((int32_t)(curdev >> 1), mPredBufFulness >> 1);
                maxdevSoft = std::min((int32_t)(mHRD.bufSize >> 3), maxdevSoft);
                int32_t mindevSoft = std::min((int32_t)(mHRD.bufSize >> 3), ((int32_t)mHRD.bufSize - mPredBufFulness) >> 1);

                double qf = 1;
                int32_t targ;

                brc_fprintf("dev %d %d %d %d \n", (int)estdev, forecastBits, (int)targetBits, len*(int)mHRD.inputBitsPerFrame);

                double qf1 = 0;
                int32_t laLim = std::min(mPredBufFulness * 3 >> 2, (int32_t)mHRD.bufSize * 3 >> 3);
                if (forecastBits - (len - 1)*(int32_t)mHRD.inputBitsPerFrame > laLim) {
                    targ = (len - 1)*(int32_t)mHRD.inputBitsPerFrame + laLim;
                    if (targ < forecastBits >> 1)
                        targ = forecastBits >> 1;
                    qf1 = (double)forecastBits / targ;
                }
                if (estdev > maxdevSoft) {
                    targ = targetBits + maxdevSoft - curdev;
                    if (targ < (forecastBits >> 1) + 1)
                        targ = (forecastBits >> 1) + 1;
                    if (targ < forecastBits)
                        qf = sqrt((double)forecastBits / targ);
                } else if (estdev < -mindevSoft) { // && mRCMode != MFX_RATECONTROL_VBR) {
                    targ = targetBits - mindevSoft - curdev;
                    brc_fprintf("%d %d %d \n", mindevSoft, curdev, targ);
                    if (targ > (forecastBits * 3 >> 1) + 1)
                        targ = (forecastBits * 3 >> 1) + 1;
                    if (targ > forecastBits)
                        qf = sqrt((double)forecastBits / targ);
                }

                qf = std::max(qf, qf1);

                q *= qf;
                mQstepBase = q;
                qpbase = Qstep2QP(q, mQuantOffset);

                if (qpbase > qpp) {
                    if (qpbase > refqp && qpp < refqp) {
                        qpbase = refqp;
                        continue;
                    }
                }

                int32_t qpl = qpbase + layer - 1;
                int32_t qpn = qp;

                if (qpl > qp) // 07/04/16
                    qpn = qpl;
                else if (qf < 1 && qp <= qp0 && qpl < qp) {
                    qpn = std::max(qpl, qpmin + 1);
                }
                BRC_CLIP(qpn, mQuantMin, mQuantMax); // 08/04
                //BRC_CLIP(qp, qp0 - 1, qp0 + 3);

                if (qpn < qp && qpmin == 0) {
                    qp = qpn;

                    for (;;) { // check framesinParallel frames as above ???

                        predbits = PredictFrameSize(video, frames, qp, &refFrameData);
                        brc_fprintf(" %d %d \n", qp, predbits);

                        maxbits = mPredBufFulness >> 1;

                        if ((layer > 1) || qp >= refFrameData.qp)
                            if (maxbits > mHRD.bufSize >> 2)
                                maxbits = mHRD.bufSize >> 2;

                        if (predbits > maxbits && qp < mQuantMax) { // frame is predicted to exceed half buf fullness
                            qp++;
                            qpmin = qp;
                        } else if ((int32_t)(mPredBufFulness + mBitsDesiredFrame) - predbits > (int32_t)mHRD.bufSize) {
                            if (qp > qpmin + 1 && qp > qp_)
                                qp--;
                            else
                                break;
                        } else
                            break;
                    }
                } else
                    qp = qpn;

                brc_fprintf("\n %d %d %.3f %.3f ", forecastBits, qp, mQstepBase, qf);
                break;
            }


            if (layer > 1) {
                int32_t qpLayer_1 = mPrevQpLayer[layer - 1];
                if (qp < qpLayer_1)
                    qp  = qpLayer_1;
            } else
                BRC_CLIP(qp, qp0 - 2, qp);

            BRC_CLIP(qp, mMinQp, mQuantMax);
            mMinQp = mQuantMin;

            UpdateMiniGopData(frames[0], qp);

            int32_t predbits_new = PredictFrameSize(video, frames, qp, &refFrameData);

            frames[0]->m_cmplx = refFrameData.cmplx;

            mPredBufFulness = predbufFul;

            mRealPredFullness -= predbits_new - mHRD.inputBitsPerFrame;
            mPredBufFulness -= predbits_new - mBitsDesiredFrame;
            frames[0]->m_predBits = predbits_new;

            mPrevQpLayer[layer] = qp;

            brc_fprintf("\n # %d %d %d %d %f %d f %d\n", frames[0]->m_encOrder, predbits, predbits_new, mPredBufFulness, frames[0]->m_cmplx, qp, frames[0]->m_secondFieldFlag ? 1 : 0);

        }
        else // AVBR
        {

            int32_t bufsize = mParams.width * mParams.height * mParams.bitDepthLuma;
            //double maxQstep = QP2Qstep(51 + mQuantOffset, mQuantOffset);
            double maxQstep = QSTEP[51 + mQuantOffset];
            if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
                bufsize += bufsize >> 1;
            else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
                bufsize += bufsize;
            else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
                bufsize += bufsize << 1;

            int64_t totdev;
            double estBits = 0;
            double cmpl = pow(avCmplx, BRC_RATE_CMPLX_EXPONENT); // use frames[0]->m_CmplxQstep (^0.4) ???

            for (i = 0; i < len; i++) {
                int32_t layer = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + std::max(1, frames[i]->m_pyramidLayer)));
                double qstep = q * brc_qstep_factor[layer];
                BRC_CLIP(qstep, minQstep, maxQstep);
                if (layer == 0)
                    layer = 1;
                estBits += mEstCoeff[layer] * cmpl / (mEstCnt[layer] * qstep);
            }

            brc_fprintf("%d: l-%d %d q %.3f %.3f %.3f %.3f \n", frames[0]->m_encOrder, layer, len, q, cmpl, frames[0]->m_stats[mLowres]->m_avgBestSatd, frames[0]->m_stats[mLowres]->m_avgIntraSatd);

            if (frames[0]->m_encOrder <= video->GopRefDist)
                totdev = mTotalDeviation + 0.1*(estBits - targetBits);
            else
                totdev = mTotalDeviation + 0.5*(estBits - targetBits);

            //double devtopLim = bufsize*0.1;
            //if (devtopLim < mTotalDeviation - targetBits*0.5)
            //    devtopLim = mTotalDeviation - targetBits*0.5;
            //double devbotLim = -bufsize*0.1;
            //if (devbotLim > mTotalDeviation + targetBits*0.5)
            //    devbotLim = mTotalDeviation + targetBits*0.5;

            //brc_fprintf("dev %d %d %Id %Id %d %d \n", frames[0]->m_encOrder, bufsize, mTotalDeviation, totdev, (int)devtopLim, (int)devbotLim);

            double qf = 1;
            //if (totdev > devtopLim)
            //    qf = totdev / devtopLim;
            //else if (totdev < devbotLim)
            //    qf = devbotLim / totdev;

            //if (frames[0]->m_encOrder > video->GopRefDist) {
            //    if (mTotalDeviation > bufsize) {
            //        qf1 = mTotalDeviation / (double)bufsize;
            //        if (qf > 1) {
            //            if (qf1 > qf)
            //                qf = qf1;
            //        } else {
            //            qf *= qf1;
            //            if (qf < 1)
            //                qf = 1;
            //        }
            //    } else if (mTotalDeviation < -bufsize) {
            //        qf1 = (double)(-bufsize) / mTotalDeviation;
            //        if (qf < 1) {
            //            if (qf1 < qf)
            //                qf = qf1;
            //        } else {
            //            qf *= qf1;
            //            if (qf > 1)
            //                qf = 1;
            //        }
            //    }
            //}

            //brc_fprintf("%d %.3f %.3f %.3f %.3f \n", frames[0]->m_encOrder, qf, avCmplx, mTotMeanComplx, q);

            //if (qf < 1) {
            //    double w = 0.5 * avCmplx / mTotMeanComplx;
            //    if (w > 1.)
            //        w = 1.;
            //    qf = w*qf + (1 - w);
            //}

            //q *= qf;


            //qp = Qstep2QP(q, mQuantOffset);

            if (layer <= 1 && frames[0]->m_stats[mLowres]->m_sceneCut <= 0) {
                if (totdev > 10*mBitsDesiredFrame)
                    qf *= 1 + ((double)totdev - 10*mBitsDesiredFrame) / (20*mBitsDesiredFrame);
                else if (totdev < -10*mBitsDesiredFrame)
                    qf *= 1 + ((double)totdev - 10*mBitsDesiredFrame) / (20*mBitsDesiredFrame);
                BRC_CLIP(qf, 1/maxQstepChange, maxQstepChange);


                double qc = mQstepScale0 * frames[0]->m_CmplxQstep;

                brc_fprintf("%d %.3f ", frames[0]->m_encOrder, qf);

                if (qf > 1 && q > qc) {
                    double w = (4 * qc - q) / (4 * qc);
                    if (w > 0)
                        qf = qf * w + 1 - w;
                } else if (qf < 1 && q < qc) {
                    double w = (q - qc * 0.25) / q;
                    if (w > 0)
                        qf = qf * w + 1 - w;
                }
                q *= qf;
                brc_fprintf("%.3f %.3f %.3f %.3f \n", q, qc, qf, mQstepBase);
            }


            if (layer <= 1) {
                if (mQstepBase > 0)
                    BRC_CLIP(q, mQstepBase/maxQstepChange, q);
                    //BRC_CLIP(q, mQstepBase/maxQstepChange, mQstepBase*maxQstepChange);
                mQstepBase = q;
            }
            //else
            //    q = mQstepBase;

            int32_t l = (frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[0]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + std::max(1, frames[0]->m_pyramidLayer)));
            q *= brc_qstep_factor[l];

            qp = Qstep2QP(q, mQuantOffset);

            if (l > 1) {
                int32_t qpLayer_1 = mPrevQpLayer[l - 1];
                if (qp < qpLayer_1) {
                    qp  = qpLayer_1;
                    //mQstepBase = QP2Qstep(qp, mQuantOffset);
                }
            }
            brc_fprintf("%d %.3f %.3f %d \n", frames[0]->m_encOrder, q, qf, qp);

            BRC_CLIP(qp, mQuantMin, mQuantMax);

            if (mPrevQpLayer[l] <= 0)
                mPrevQpLayer[l] = qp;

        }

        return qp - mQuantOffset;

    } else {

        int32_t qp = mQuantB;
        Frame* pFrame = NULL;

        if (frames && numFrames > 0)
            pFrame = frames[0];

        if (pFrame) {

            if (pFrame->m_encOrder == mRecodedFrame_encOrder)
                qp = mQuantRecoded;
            else {

                if (pFrame->m_picCodeType == MFX_FRAMETYPE_I)
                    qp = mQuantI;
                else if (pFrame->m_picCodeType == MFX_FRAMETYPE_P)
                    qp = mQuantP;
                else {
                    if (video->BiPyramidLayers > 1) {
                        qp = mQuantB + pFrame->m_pyramidLayer - 1;
                        BRC_CLIP(qp, 1, mQuantMax);
                    } else
                        qp = mQuantB;
                }
            }
        }

        return qp - mQuantOffset;
    }

}

mfxStatus AV1BRC::SetQP(int32_t qp, mfxU16 frameType)
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


mfxBRCStatus AV1BRC::UpdateAndCheckHRD(int32_t frameBits, double inputBitsPerFrame, int32_t recode)
{
    mfxBRCStatus ret = MFX_ERR_NONE;
    double bufFullness;

    if (!(recode & (MFX_BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
        mHRD.prevBufFullness = mHRD.bufFullness;
        mHRD.underflowQuant = 0;
        mHRD.overflowQuant = 999;
    } else { // frame is being recoded - restore buffer state
        mHRD.bufFullness = mHRD.prevBufFullness;
    }

    mHRD.maxFrameSize = (int32_t)(mHRD.bufFullness - 1);
    mHRD.minFrameSize = (mRCMode != MFX_RATECONTROL_CBR ? 0 : (int32_t)(mHRD.bufFullness + 1 + 1 + inputBitsPerFrame - mHRD.bufSize));
    if (mHRD.minFrameSize < 0)
        mHRD.minFrameSize = 0;

    bufFullness = mHRD.bufFullness - frameBits;

    if (bufFullness < 2) {
        bufFullness = inputBitsPerFrame;
        ret = MFX_BRC_ERR_BIG_FRAME;
        if (bufFullness > (double)mHRD.bufSize)
            bufFullness = (double)mHRD.bufSize;
    } else {
        bufFullness += inputBitsPerFrame;
        if (bufFullness > (double)mHRD.bufSize - 1) {
            bufFullness = (double)mHRD.bufSize - 1;
            if (mRCMode == MFX_RATECONTROL_CBR)
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

#define I_WEIGHT 1.2
#define P_WEIGHT 0.25
#define B_WEIGHT 0.2

#define BRC_MAX_LOAN_LENGTH 75
#define BRC_LOAN_RATIO 0.075

#define BRC_BIT_LOAN \
{ \
    if (picType == MFX_FRAMETYPE_I) { \
        if (mLoanLength) \
            bitsEncoded += mLoanLength * mLoanBitsPerFrame; \
        mLoanLength = video->GopPicSize; \
        if (mLoanLength > BRC_MAX_LOAN_LENGTH || mLoanLength == 0) \
            mLoanLength = BRC_MAX_LOAN_LENGTH; \
        int32_t bitsEncodedI = (int32_t)((double)bitsEncoded  / (mLoanLength * BRC_LOAN_RATIO + 1)); \
        mLoanBitsPerFrame = (bitsEncoded - bitsEncodedI) / mLoanLength; \
        bitsEncoded = bitsEncodedI; \
    } else if (mLoanLength) { \
        bitsEncoded += mLoanBitsPerFrame; \
        mLoanLength--; \
    } \
}

#if 0
#define BRC_BIT_LOAN_P \
{ \
    if (picType == MFX_FRAMETYPE_P) { \
        if (mLoanLengthP) \
            bitsEncoded += mLoanLengthP * mLoanBitsPerFrameP; \
        mLoanLengthP = video->GopRefDist; \
        if (mLoanLength > 16 || mLoanLength == 0) \
            mLoanLength = 16; \
        int32_t bitsEncodedP = (int32_t)((double)bitsEncoded  / (mLoanLengthP * 0.25 + 1)); \
        mLoanBitsPerFrameP = (bitsEncoded - bitsEncodedP) / mLoanLengthP; \
        bitsEncoded = bitsEncodedP; \
    } else if (mLoanLengthP && picType != MFX_FRAMETYPE_I) { \
        bitsEncoded += mLoanBitsPerFrameP; \
        mLoanLengthP--; \
    } \
}
#endif


mfxBRCStatus AV1BRC::PostPackFrame(AV1VideoParam *video, uint8_t sliceQpY, Frame *pFrame, int32_t totalFrameBits, int32_t overheadBits, int32_t repack)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    if (mBitrate == 0)
        return Sts;
    if (!pFrame)
        return MFX_ERR_NULL_PTR;

    int32_t bitsEncoded = totalFrameBits - overheadBits;
    double e2pe;
    int32_t qp, qpprev;
    uint32_t prevFrameType = mPicType;
    uint32_t picType = pFrame->m_picCodeType;

    mPoc = pFrame->m_poc;

    sliceQpY += mQuantOffset;

    int32_t layer = (picType == MFX_FRAMETYPE_I ? 0 : (picType == MFX_FRAMETYPE_P ?  1 : 1 + std::max(1, pFrame->m_pyramidLayer))); // should be 0 for I, 1 for P, etc. !!!
    double qstep = QP2Qstep(sliceQpY, mQuantOffset);

    if (video && video->AnalyzeCmplx) {

        if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {

            Sts = UpdateAndCheckHRD(totalFrameBits,  mHRD.inputBitsPerFrame, repack);

            // stuff bits right away in case of overflow
            // PostPackFrame is supposed to be called again after bit stuffing, hence don't update any stats here
            if (Sts & MFX_BRC_ERR_SMALL_FRAME) {

                brc_fprintf("OVER %d %d %d \n", pFrame->m_encOrder, totalFrameBits, (int)mHRD.prevBufFullness);

                Sts |= MFX_BRC_NOT_ENOUGH_BUFFER;
                return Sts;
            }

            mTotPrevCmplx = mTotComplxCnt > 0 ? mTotAvComplx  / mTotComplxCnt : 0;


            if (layer == 1) { // 08/04/16
                int32_t refQp = -1;
                if (pFrame->m_refPicList[0].m_refFramesCount > 0)
                    refQp = pFrame->m_refPicList[0].m_refFrames[0]->m_sliceQpY;
                if (sliceQpY < refQp) {
                    mPrevCmplxIP = pFrame->m_stats[mLowres]->m_avgIntraSatd;
                    mPrevQstepIP = qstep;
                    mPrevBitsIP = bitsEncoded;
                    mPrevQpIP = sliceQpY;
                    mSceneNum = pFrame->m_sceneOrder;
                }
            }

            mPrevCmplxLayer[layer] = pFrame->m_cmplx;
            mPrevQstepLayer[layer] = qstep;
            mPrevBitsLayer[layer] = bitsEncoded;
            mPrevQpLayer[layer] = sliceQpY;

            if (Sts & MFX_BRC_ERR_BIG_FRAME) { // assume the recode request will not be ignored
                mHRD.bufFullness = mHRD.prevBufFullness;

                mTotAvComplx  = pFrame->m_totAvComplx;
                mTotComplxCnt = pFrame->m_totComplxCnt;
                mComplxSum  = pFrame->m_complxSum;

                brc_fprintf("UNDR %d %d %.1f %.1f %.3f -> ", sliceQpY , pFrame->m_qpBase, mCmplxRate, mTotTarget, mQstepScale);

                if (sliceQpY != pFrame->m_qpBase) {
                    double qbase = QP2Qstep(pFrame->m_qpBase, mQuantOffset);
                    double w = sliceQpY > pFrame->m_qpBase ? qstep / qbase : (qbase / qstep - 1) * 0.3 + 1;
                    mCmplxRate += w * bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                    mTotTarget += w * mBitsDesiredFrame;
                } else {
                    mCmplxRate += bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                    mTotTarget += mBitsDesiredFrame;
                }
                mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);
                mQstepScale *= bitsEncoded / (mHRD.bufFullness / 2); // ???
                brc_fprintf(" %.1f %.1f %.3f \n", mCmplxRate, mTotTarget, mQstepScale);

                mPredBufFulness = (int)mHRD.prevBufFullness;
                mRealPredFullness = (int)mHRD.prevBufFullness;

                double minqstep = pFrame->m_CmplxQstep * mQstepScale;
                mMinQp = Qstep2QP(minqstep, mQuantOffset);

                if (mMinQp < sliceQpY + 1)
                    mMinQp = sliceQpY + 1;

                if (mMinQp > mQuantMax)
                    mMinQp = mQuantMax;

                brc_fprintf("UNDER %d %d %d %d %d %.3f %d \n", pFrame->m_encOrder, totalFrameBits, pFrame->m_predBits, (int)mHRD.prevBufFullness, mPredBufFulness, mQstepScale, mMinQp);

                return Sts;
            }

            mPredBufFulness += pFrame->m_predBits - totalFrameBits;
            mRealPredFullness += pFrame->m_predBits - totalFrameBits;
            BRC_CLIP(mRealPredFullness, 0, (int32_t)mHRD.bufSize);

            brc_fprintf("L%d %d %d %d %d %d %d %d %f \n", layer, pFrame->m_encOrder, sliceQpY, totalFrameBits, bitsEncoded, pFrame->m_predBits, (int)mHRD.bufFullness, mPredBufFulness, mPrevCmplxLayer[layer]);

            BRC_BIT_LOAN;
//            BRC_BIT_LOAN_P;

            if (mRCMode == MFX_RATECONTROL_CBR) {
                double fmax = (double)mHRD.bufSize * 0.9;
                if (mHRD.bufFullness > fmax) {
                    double w = (mHRD.bufFullness / fmax - 1) / 8.; // 16.
                    mCmplxRate *= 1. - w;
                }
            }

            double decay = 1 - 0.1*(double)mBitsDesiredFrame / mHRD.bufSize;
            mCmplxRate *= decay;
            mTotTarget *= decay;

            if (sliceQpY != pFrame->m_qpBase) {
                double qbase = QP2Qstep(pFrame->m_qpBase, mQuantOffset);
                double w = sliceQpY > pFrame->m_qpBase ? qstep / qbase : (qbase / qstep - 1) * 0.3 + 1;
                mCmplxRate += w * bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                mTotTarget += w * mBitsDesiredFrame;
            } else {
                mCmplxRate += bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                mTotTarget += mBitsDesiredFrame;
            }


            {
                double w = pFrame->m_encOrder / (double)100;
                if (w > 1)
                    w = 1;
                mQstepScale = (1-w)*mQstepScale + w*pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);
            }


            mBitsEncodedTotal += totalFrameBits;
            mBitsDesiredTotal += mBitsDesiredFrame;

            mTotalDeviation += bitsEncoded - mBitsDesiredFrame;

            brc_fprintf("pp %d %d %d %f %f %f %f \n", pFrame->m_encOrder, bitsEncoded, mBitsDesiredFrame, mCmplxRate, mTotTarget, mQstepScale, pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT));

            mEncOrderCoded = pFrame->m_encOrder;
            UpdateMiniGopData(pFrame, sliceQpY);

        } else { // AVBR

            mPrevQpLayer[layer] = sliceQpY;

            double decay = layer ? 0.5 : 0.25;

            mTotPrevCmplx = mTotComplxCnt > 0 ? mTotAvComplx  / mTotComplxCnt : 0;
            //mTotPrevCmplx = pFrame->m_avCmplx;
            //if (mTotPrevCmplx < BRC_MIN_CMPLX)
            //    mTotPrevCmplx = BRC_MIN_CMPLX;


            double cmpl = pow(pFrame->m_avCmplx, BRC_RATE_CMPLX_EXPONENT);

            if (cmpl > pow(BRC_MIN_CMPLX, BRC_RATE_CMPLX_EXPONENT)) {
                double coefn = bitsEncoded * qstep / cmpl;
                mEstCoeff[layer] *= decay;
                mEstCoeff[layer] += coefn;
                mEstCnt[layer] *= decay;
                mEstCnt[layer] += 1.;
            }

            BRC_BIT_LOAN

            int32_t qp = sliceQpY - layer;
            BRC_CLIP(qp, 1, mQuantMax);
            double qs = QP2Qstep(qp, mQuantOffset);

            mCmplxRate += bitsEncoded * pow(qs / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
            mTotTarget += mBitsDesiredFrame;

            mBitsEncodedTotal += totalFrameBits;
            mBitsDesiredTotal += mBitsDesiredFrame;
            mTotalDeviation += bitsEncoded - mBitsDesiredFrame;

            if (mTotalDeviation > 30*mBitsDesiredFrame)
                mCmplxRate *= 1.001;
            else if (mTotalDeviation < -30*mBitsDesiredFrame)
                mCmplxRate *= 0.999;

            mEncOrderCoded = pFrame->m_encOrder;

            mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);

            brc_fprintf("L%d %d %d %d %d %.1f %d %.3f %.3f %Id \n\n", layer, pFrame->m_encOrder, qp, bitsEncoded, mBitsDesiredFrame, mCmplxRate, (int)mTotTarget, mQstepScale, mBitsEncodedTotal/mTotTarget, mTotalDeviation);

        }

        return Sts;
    }

    if (!repack && mQuantUpdated <= 0) { // BRC reported buffer over/underflow but the application ignored it
        mQuantI = mQuantIprev;
        mQuantP = mQuantPprev;
        mQuantB = mQuantBprev;
        mRecode |= 2;
        mQp = mRCq;
        UpdateQuant(mBitsEncoded, totalFrameBits, layer);
    }

    mQuantIprev = mQuantI;
    mQuantPprev = mQuantP;
    mQuantBprev = mQuantB;

    mBitsEncoded = bitsEncoded;

    if (mSceneChange)
        if (mQuantUpdated == 1 && mPoc > mSChPoc + 1)
            mSceneChange &= ~16;

    double buffullness = 1.e12; // a big number
    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        buffullness = repack ? mHRD.prevBufFullness : mHRD.bufFullness;
        Sts = UpdateAndCheckHRD(totalFrameBits, mHRD.inputBitsPerFrame, repack);
    }

    qpprev = qp = mQp = sliceQpY;

    double fa_short0 = mRCfa_short;
    mRCfa_short += (bitsEncoded - mRCfa_short) / BRC_RCFAP_SHORT;

    {
        qstep = QP2Qstep(qp, mQuantOffset);
        double qstep_prev = QP2Qstep(mQPprev, mQuantOffset);
        double frameFactor = 1.0;
        double targetFrameSize = std::max((double)mBitsDesiredFrame, mRCfa);
        if (picType & MFX_FRAMETYPE_I)
            frameFactor = 1.5;

        e2pe = bitsEncoded * sqrt(qstep) / (mBitsEncodedPrev * sqrt(qstep_prev));

        double maxFrameSize;
        maxFrameSize = 2.5/9. * buffullness + 5./9. * targetFrameSize;
        BRC_CLIP(maxFrameSize, targetFrameSize, BRC_SCENE_CHANGE_RATIO2 * targetFrameSize * frameFactor);

        double famax = 1./9. * buffullness + 8./9. * mRCfa;

        int32_t maxqp = mQuantMax;
        if (mRCMode == MFX_RATECONTROL_CBR) {
            maxqp = std::min(maxqp, mHRD.overflowQuant - 1);
        }

        if (bitsEncoded >  maxFrameSize && qp < maxqp) {
            double targetSizeScaled = maxFrameSize * 0.8;
            double qstepnew = qstep * pow(bitsEncoded / targetSizeScaled, 0.9);
            int32_t qpnew = Qstep2QP(qstepnew, mQuantOffset);
            if (qpnew == qp)
              qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            if (qpnew > qp) {
                mQp = mRCq = mQuantI = mQuantP = qpnew;
                if (picType & MFX_FRAMETYPE_B)
                    mQuantB = qpnew;
                else {
                    SetQuantB();
                }

                mRCfa_short = fa_short0;

                if (e2pe > BRC_SCENE_CHANGE_RATIO1) { // scene change, resetting BRC statistics
                  mRCfa = mBitsDesiredFrame;
                  mRCqa = 1./qpnew;
                  mQp = mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;
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
                    mRecodedFrame_encOrder = pFrame->m_encOrder;
                    mQuantRecoded = qpnew;
                    brc_fprintf("recode1 %d %d %d %d \n", pFrame->m_encOrder, qpnew, bitsEncoded ,  (int)maxFrameSize);
                    return MFX_BRC_ERR_BIG_FRAME;
                }
            }
        }

        if (mRCfa_short > famax && (!repack) && qp < maxqp) {

            double qstepnew = qstep * mRCfa_short / (famax * 0.8);
            int32_t qpnew = Qstep2QP(qstepnew, mQuantOffset);
            if (qpnew == qp)
                qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            mRCfa = mBitsDesiredFrame;
            mRCqa = 1./qpnew;
            mQp = mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;

            mRCfa_short = mBitsDesiredFrame;

            if (mRecode) {
                mQuantUpdated = 0;
                mHRD.frameNum--;
                mMinQp = qp;
                mRecodedFrame_encOrder = pFrame->m_encOrder;
                mQuantRecoded = qpnew;
                brc_fprintf("recode2 %d %d \n", pFrame->m_encOrder, qpnew);
                return MFX_BRC_ERR_BIG_FRAME;
            }
        }
    }

    mPicType = picType;

    double fa = mRCfa;
    bool oldScene = false;
    if ((mSceneChange & 16) && (mPoc < mSChPoc) && (mBitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (double)mBitsEncodedP) && (double)mBitsEncoded < 1.5*fa)
        oldScene = true;

    if (Sts != MFX_BRC_OK && mRecode) {
        Sts = UpdateQuantHRD(totalFrameBits, Sts, overheadBits, layer, pFrame->m_encOrder == mRecodedFrame_encOrder);
        mRecodedFrame_encOrder = pFrame->m_encOrder;
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

            Sts = UpdateQuant(bitsEncoded, totalFrameBits,  layer, pFrame->m_encOrder == mRecodedFrame_encOrder);

            if (mPicType != MFX_FRAMETYPE_B) {
                mQuantPrev = mQuantP;
                mBitsEncodedP = mBitsEncoded;
            }

            mQuantP = mQuantI = mRCq;
        }
        mQuantUpdated = 1;
        //    mMaxBitsPerPic = mMaxBitsPerPicNot0;

        if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
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

#ifdef PRINT_BRC_STATS
    if (Sts & 1)
        brc_fprintf("underflow %d %d %d \n", pFrame->m_encOrder, mQuantRecoded, Sts);
    else if (Sts & 4)
        brc_fprintf("overflow %d %d %d \n", pFrame->m_encOrder, mQuantRecoded, Sts);
#endif

    return Sts;
};

mfxBRCStatus AV1BRC::UpdateQuant(int32_t bEncoded, int32_t totalPicBits, int32_t layer, int32_t recode)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    double  bo, qs, dq;
    int32_t  quant;
    uint32_t bitsPerPic = (uint32_t)mBitsDesiredFrame;
    mfxI64 totalBitsDeviation;

    if (recode)
        quant = mQuantRecoded;
    else
        //quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : ((mPicType == MFX_FRAMETYPE_P) ? mQuantP : (layer > 0 ? mQuantB + layer - 1 : mQuantB));
        quant = mQp;

    if (mRecode & 2) {
        mRCfa = bitsPerPic;
        mRCqa = mRCqa0;
        mRecode &= ~2;
    }

    mBitsEncodedTotal += totalPicBits;
    mBitsDesiredTotal += bitsPerPic;
    totalBitsDeviation = mBitsEncodedTotal - mBitsDesiredTotal;

    //if (mParams.HRDBufferSizeBytes > 0) {
    if (mRCMode == MFX_RATECONTROL_VBR) {
        mfxI64 targetFullness = std::min(mParams.HRDInitialDelayBytes << 3, (int32_t)mHRD.bufSize / 2);
        mfxI64 minTargetFullness = std::min(mHRD.bufSize / 2, mBitrate * 2); // half bufsize or 2 sec
        if (targetFullness < minTargetFullness)
            targetFullness = minTargetFullness;
        mfxI64 bufferDeviation = targetFullness - (int64_t)mHRD.bufFullness;
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

    int32_t bap = mRCbap;
    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        double bfRatio = mHRD.bufFullness / mBitsDesiredFrame;
        if (totalBitsDeviation > 0) {
            bap = (int32_t)bfRatio*3;
            bap = std::max(bap, 10);
            BRC_CLIP(bap, mRCbap/10, mRCbap);
        }
    }
    bo = (double)totalBitsDeviation / bap / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);

    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./mQuantMin);
    quant = (int32_t) (1. / dq + 0.5);

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

    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        double qstep = QP2Qstep(quant, mQuantOffset);
        double fullnessThreshold = MIN(bitsPerPic * 12, mHRD.bufSize*3/16);
        qs = 1.0;
        if (bEncoded > mHRD.bufFullness && mPicType != MFX_FRAMETYPE_I)
            qs = (double)bEncoded / (mHRD.bufFullness);

        if (mHRD.bufFullness < fullnessThreshold && (uint32_t)totalPicBits > bitsPerPic)
            qs *= sqrt((double)fullnessThreshold * 1.3 / mHRD.bufFullness); // ??? is often useless (quant == quant_old)

        if (qs > 1.0) {
            qstep *= qs;
            quant = Qstep2QP(qstep, mQuantOffset);
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

mfxBRCStatus AV1BRC::UpdateQuantHRD(int32_t totalFrameBits, mfxBRCStatus sts, int32_t overheadBits, int32_t layer, int32_t recode)
{
    int32_t quant, quant_prev;
    int32_t wantedBits = (sts == MFX_BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize * 3 / 4 : mHRD.minFrameSize * 5 / 4);
    int32_t bEncoded = totalFrameBits - overheadBits;
    double qstep, qstep_new;

    wantedBits -= overheadBits;
    if (wantedBits <= 0) // possible only if BRC_ERR_BIG_FRAME
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    if (recode)
        quant_prev = quant = mQuantRecoded;
    else
        //quant_prev = quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : ((mPicType == MFX_FRAMETYPE_P) ? mQuantP : (layer > 0 ? mQuantB + layer - 1 : mQuantB));
        quant_prev = quant = mQp;

    if (sts & MFX_BRC_ERR_BIG_FRAME)
        mHRD.underflowQuant = quant;
    else if (sts & MFX_BRC_ERR_SMALL_FRAME)
        mHRD.overflowQuant = quant;

    qstep = QP2Qstep(quant, mQuantOffset);
    qstep_new = qstep * sqrt((double)bEncoded / wantedBits);
//    qstep_new = qstep * sqrt(sqrt((double)bEncoded / wantedBits));
    quant = Qstep2QP(qstep_new, mQuantOffset);
    BRC_CLIP(quant, 1, mQuantMax);

    if (sts & MFX_BRC_ERR_SMALL_FRAME) // overflow
    {
        int32_t qpMin = std::max(mHRD.underflowQuant, mMinQp);
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

    mQuantRecoded = quant;

    return sts;
}

mfxStatus AV1BRC::GetInitialCPBRemovalDelay(uint32_t *initial_cpb_removal_delay, int32_t recode)
{
    uint32_t cpb_rem_del_u32;
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
    cpb_rem_del_u32 = (uint32_t)cpb_rem_del_u64;

    if (MFX_RATECONTROL_VBR == mRCMode) {
        mBF = temp2_u64 * cpb_rem_del_u32 / 90000;
        temp1_u64 = (mfxU64)cpb_rem_del_u32 * mMaxBitrate;
        uint32_t dec_buf_ful = (uint32_t)(temp1_u64 / (90000/8));
        if (recode)
            mHRD.prevBufFullness = (double)dec_buf_ful;
        else
            mHRD.bufFullness = (double)dec_buf_ful;
    }

    *initial_cpb_removal_delay = cpb_rem_del_u32;
    return MFX_ERR_NONE;
}

void  AV1BRC::GetMinMaxFrameSize(int32_t *minFrameSizeInBits, int32_t *maxFrameSizeInBits)
{
    if (minFrameSizeInBits)
      *minFrameSizeInBits = mHRD.minFrameSize;
    if (maxFrameSizeInBits)
      *maxFrameSizeInBits = mHRD.maxFrameSize;
}

#define CLIPVAL(MINVAL, MAXVAL, VAL) (std::max(MINVAL, std::min(MAXVAL, VAL)))
mfxF64 const INIT_RATE_COEFF[] = {
        1.109, 1.196, 1.225, 1.309, 1.369, 1.428, 1.490, 1.588, 1.627, 1.723, 1.800, 1.851, 1.916,
        2.043, 2.052, 2.140, 2.097, 2.096, 2.134, 2.221, 2.084, 2.153, 2.117, 2.014, 1.984, 2.006,
        1.801, 1.796, 1.682, 1.549, 1.485, 1.439, 1.248, 1.221, 1.133, 1.045, 0.990, 0.987, 0.895,
        0.921, 0.891, 0.887, 0.896, 0.925, 0.917, 0.942, 0.964, 0.997, 1.035, 1.098, 1.170, 1.275
    };
/*mfxF64 const QSTEP[52] = {
        0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
        2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
    12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
    57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070
};*/
mfxF64 const QSTEP[52] = {
    0.82,    0.93,    1.11,    1.24,     1.47,    1.65,    1.93,    2.12,    2.49,    2.80,
    3.15,    3.53,    4.10,    4.55,     5.24,    5.88,    6.44,    7.20,    8.36,    8.92,
    10.26,  11.55,    12.93,  14.81,    17.65,   19.30,   22.30,   25.46,   28.97,   33.22,
    38.50,  43.07,    50.52,  55.70,    64.34,   72.55,   82.25,   93.12,   108.95, 122.40,
    130.39,148.69,   185.05, 194.89,   243.18,  281.61,  290.58,   372.38,  378.27, 406.62,
    468.34,525.69
};


mfxF64 const INTRA_QSTEP_COEFF  = 2.0;
mfxF64 const INTRA_MODE_BITCOST = 0.0;
mfxF64 const INTER_MODE_BITCOST = 0.0;
mfxI32 const MAX_QP_CHANGE      = 1;
mfxF64 const LOG2_64            = 3;
mfxF64 const MIN_EST_RATE       = 0.1;
mfxF64 const NORM_EST_RATE      = 100.0;

mfxF64 const MIN_RATE_COEFF_CHANGE = 0.05;
mfxF64 const MAX_RATE_COEFF_CHANGE = 20.0;



mfxStatus VMEBrc::Init(const mfxVideoParam *init, AV1VideoParam &, mfxI32 )
{
    m_qpUpdateRange = 20;
    mfxU32 RegressionWindow = 20;
    static mfxF64 coeff[8]   = {1.3, 0.80, 0.7, 0.3, 0.25, 0.2, 0.15, 0.1};
    static mfxU32 RegressionWindows[8] = {2,3,6,10,20,20,20,20};

    mfxF64 fr = mfxF64(init->mfx.FrameInfo.FrameRateExtN) / init->mfx.FrameInfo.FrameRateExtD;
    m_totNumMb = init->mfx.FrameInfo.Width * init->mfx.FrameInfo.Height / 256;
    m_initTargetRate = 1000 * init->mfx.TargetKbps / fr / m_totNumMb;
    m_targetRateMin = m_initTargetRate;
    m_targetRateMax = m_initTargetRate;
    m_laData.resize(0);

    m_bPyr = true; //to do

   for (mfxU32 level = 0; level < 8; level++)
   {
   for (mfxU32 qp = 0; qp < 52; qp++)
        m_rateCoeffHistory[level][qp].Reset(m_bPyr ? RegressionWindows[level] : RegressionWindow, 100.0, 100.0 * (m_bPyr ?INIT_RATE_COEFF[qp]*coeff[level]:1));
   }

    m_framesBehind = 0;
    m_bitsBehind = 0.0;
    m_curQp = -1;
    m_curBaseQp = -1;
    m_lookAheadDep = 0;


    return MFX_ERR_NONE;
}

mfxStatus VMEBrc::SetFrameVMEData(const mfxExtLAFrameStatistics *pLaOut, mfxU32 width, mfxU32 height)
{
    mfxU32 resNum = 0;
    mfxU32 numLaFrames = pLaOut->NumFrame;
    mfxU32 k = height*width >> 7;

    UMC::AutomaticUMCMutex guard(m_mutex);

    while(resNum < pLaOut->NumStream)
    {
        if (pLaOut->FrameStat[resNum*numLaFrames].Height == height &&
            pLaOut->FrameStat[resNum*numLaFrames].Width  == width)
            break;
        resNum ++;
    }
    MFX_CHECK(resNum <  pLaOut->NumStream, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(pLaOut->NumFrame > 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxLAFrameInfo * pFrameData = pLaOut->FrameStat + numLaFrames*resNum;
    std::list<LaFrameData>::iterator it = m_laData.begin();
    while (m_laData.size()>0)
    {
        it = m_laData.begin();
        if (!((*it).bNotUsed))
            break;
        m_laData.pop_front();
    }

    // some frames can be stored already
    // start of stored sequence
    it = m_laData.begin();
     while (it != m_laData.end())
    {
        if ((*it).encOrder == pFrameData[0].FrameEncodeOrder)
            break;
        ++it;
    }
    mfxU32 ind  = 0;

    // check stored sequence
    while ((it != m_laData.end()) && (ind < numLaFrames))
    {
        MFX_CHECK((*it).encOrder == pFrameData[ind].FrameEncodeOrder, MFX_ERR_UNDEFINED_BEHAVIOR);
        ++ind;
        ++it;
    }
    MFX_CHECK(it == m_laData.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

    // store a new data
    for (; ind < numLaFrames; ind++)
    {
        LaFrameData data = {};

        data.encOrder  = pFrameData[ind].FrameEncodeOrder;
        data.dispOrder = pFrameData[ind].FrameDisplayOrder;
        data.interCost = pFrameData[ind].InterCost;
        data.intraCost = pFrameData[ind].IntraCost;
        data.propCost  = pFrameData[ind].DependencyCost;
        data.bframe    = (pFrameData[ind].FrameType & MFX_FRAMETYPE_B) != 0;
        data.layer     = pFrameData[ind].Layer;

        MFX_CHECK(data.intraCost, MFX_ERR_UNDEFINED_BEHAVIOR);

        for (mfxU32 qp = 0; qp < 52; qp++)
        {
            data.estRate[qp] = ((mfxF64)pFrameData[ind].EstimatedRate[qp])/(QSTEP[qp]*k);
            if ( m_bPyr)
            {
                if (data.estRate[qp] < 0.2)
                    data.estRate[qp] = 0.2;
            }
        }
        //printf("EncOrder %d, dispOrder %d, interCost %d,  intraCost %d, data.propCost %d, frametype %d\n",
        //    data.encOrder,data.dispOrder, data.interCost, data.intraCost, data.propCost, pFrameData[ind].FrameType );
        m_laData.push_back(data);
    }
    if (m_lookAheadDep == 0)
        m_lookAheadDep = numLaFrames;

    //printf("VMEBrc::SetFrameVMEData m_laData[0].encOrder %d\n", pFrameData[0].FrameEncodeOrder);

    return MFX_ERR_NONE;
}

mfxU8 SelectQp(mfxF64 erate[52], mfxF64 budget, mfxU8 minQP)
{
    for (mfxU8 qp = minQP; qp < 52; qp++)
        if (erate[qp] < budget)
            return (erate[qp - 1] + erate[qp] < 2 * budget) ? qp - 1 : qp;
    return 51;
}


mfxF64 GetTotalRate(std::list<VMEBrc::LaFrameData>::iterator start, std::list<VMEBrc::LaFrameData>::iterator end, mfxI32 baseQp)
{
    mfxF64 totalRate = 0.0;
    std::list<VMEBrc::LaFrameData>::iterator it = start;
    for (; it!=end; ++it)
        totalRate += (*it).estRateTotal[CLIPVAL(0, 51, baseQp + (*it).deltaQp)];
    return totalRate;
}



 mfxI32 SelectQp(std::list<VMEBrc::LaFrameData>::iterator start, std::list<VMEBrc::LaFrameData>::iterator end, mfxF64 budget, mfxU8 minQP)
{
    mfxF64 prevTotalRate = GetTotalRate(start,end, minQP);
    for (mfxU8 qp = minQP+1; qp < 52; qp++)
    {
        mfxF64 totalRate = GetTotalRate(start,end, qp);
        if (totalRate < budget)
            return (prevTotalRate + totalRate < 2 * budget) ? qp - 1 : qp;
        else
            prevTotalRate = totalRate;
    }
    return 51;
}

mfxU8 GetFrameTypeLetter(mfxU32 frameType)
{
    mfxU32 ref = (frameType & MFX_FRAMETYPE_REF) ? 0 : 'a' - 'A';
    if (frameType & MFX_FRAMETYPE_I)
        return mfxU8('I' + ref);
    if (frameType & MFX_FRAMETYPE_P)
        return mfxU8('P' + ref);
    if (frameType & MFX_FRAMETYPE_B)
        return mfxU8('B' + ref);
    return 'x';
}




void VMEBrc::PreEnc(mfxU32 /*frameType*/, std::vector<VmeData *> const & /*vmeData*/, mfxU32 /*curEncOrder*/)
{
}


mfxU32 VMEBrc::Report(mfxU32 frameType, mfxU32 dataLength, mfxU32 /*userDataLength*/, mfxU32 /*repack*/, mfxU32  picOrder, mfxU32 /* maxFrameSize */, mfxU32 /* qp */)
{
    frameType; // unused

    UMC::AutomaticUMCMutex guard(m_mutex);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "LookAheadBrc2::Report");
    //printf("VMEBrc::Report order %d\n", picOrder);
    mfxF64 realRatePerMb = 8 * dataLength / mfxF64(m_totNumMb);

    m_framesBehind++;
    m_bitsBehind += realRatePerMb;

    // start of sequence
    mfxU32 numFrames = 0;
    std::list<LaFrameData>::iterator start = m_laData.begin();
    for(;start != m_laData.end(); ++start)
    {
        if ((*start).encOrder == picOrder)
            break;
    }
    for (std::list<LaFrameData>::iterator it = start; it != m_laData.end(); ++it) numFrames++;
    numFrames = std::min(numFrames, m_lookAheadDep);

     mfxF64 framesBeyond = (mfxF64)(std::max((uint32_t)2, numFrames) - 1);
     m_targetRateMax = (m_initTargetRate * (m_framesBehind + m_lookAheadDep - 1) - m_bitsBehind) / framesBeyond;
     m_targetRateMin = (m_initTargetRate * (m_framesBehind + framesBeyond  ) - m_bitsBehind) / framesBeyond;


    if (start != m_laData.end())
    {
        mfxU32 level = (*start).layer < 8 ? (*start).layer : 7;
        mfxI32 curQp = (*start).qp;
        mfxF64 oldCoeff = m_rateCoeffHistory[level][curQp].GetCoeff();
        mfxF64 y = std::max(0.0, realRatePerMb);
        mfxF64 x = (*start).estRate[curQp];
        //printf("%4d)\tlevel\t%4d\tintra\t%8d\tinter\t%8d\tprop\t %8d\tqp\t%2d\tbase\t%2d\t%3d\test rate\t%5f\test rate total\t%5f\treal\t%5f\tk\t%5f\toldCoeff\t%5f \n",(*start).encOrder,level, (*start).intraCost, (*start).interCost,(*start).propCost, curQp, m_curBaseQp, (*start).deltaQp, (*start).estRate[curQp], (*start).estRateTotal[curQp],realRatePerMb, y/x*NORM_EST_RATE, oldCoeff*NORM_EST_RATE);
        mfxF64 minY = NORM_EST_RATE * INIT_RATE_COEFF[curQp] * MIN_RATE_COEFF_CHANGE;
        mfxF64 maxY = NORM_EST_RATE * INIT_RATE_COEFF[curQp] * MAX_RATE_COEFF_CHANGE;
        y = CLIPVAL(minY, maxY, y / x * NORM_EST_RATE);
        m_rateCoeffHistory[level][curQp].Add(NORM_EST_RATE, y);
        //mfxF64 ratio = m_rateCoeffHistory[curQp].GetCoeff() / oldCoeff;
        mfxF64 ratio = y / (oldCoeff*NORM_EST_RATE);
        for (mfxI32 i = -m_qpUpdateRange; i <= m_qpUpdateRange; i++)
            if (i != 0 && curQp + i >= 0 && curQp + i < 52)
            {
                mfxF64 r = ((ratio - 1.0) * (1.0 - ((mfxF64)abs(i))/((mfxF64)m_qpUpdateRange + 1.0)) + 1.0);
                m_rateCoeffHistory[level][curQp + i].Add(NORM_EST_RATE,
                    NORM_EST_RATE * m_rateCoeffHistory[level][curQp + i].GetCoeff() * r);
            }
        (*start).bNotUsed = 1;
    }
    return 0;
}
int32_t VMEBrc::GetQP(AV1VideoParam *video, Frame *pFrame[], int32_t numFrames)
{
     if ((!pFrame)||(numFrames == 0))
         return 26;
     return GetQP(*video, pFrame[0], 0 );
 }

mfxI32 VMEBrc::GetQP(AV1VideoParam &video, Frame *pFrame, mfxI32 *chromaQP )
{

    UMC::AutomaticUMCMutex guard(m_mutex);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VMEBrc::GetQp");

    if (!m_laData.size())
        return 26;

    mfxF64 totalEstRate[52] = {};
    mfxU16 maxLayer = 0;

     // start of sequence
    std::list<LaFrameData>::iterator start = m_laData.begin();
    while (start != m_laData.end())
    {
        if ((*start).encOrder == pFrame->m_encOrder)
            break;
        ++start;
    }
    MFX_CHECK(start != m_laData.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

    std::list<LaFrameData>::iterator it = start;
    mfxU32 numberOfFrames = 0;
    for(it = start;it != m_laData.end(); ++it)
        numberOfFrames++;

    numberOfFrames = std::min(numberOfFrames, m_lookAheadDep);

   // fill totalEstRate
   it = start;
   for(mfxU32 i=0; i < numberOfFrames ; i++)
   {
        for (mfxU32 qp = 0; qp < 52; qp++)
        {

            (*it).estRateTotal[qp] = std::max(MIN_EST_RATE, m_rateCoeffHistory[(*it).layer < 8 ? (*it).layer : 7][qp].GetCoeff() * (*it).estRate[qp]);
            totalEstRate[qp] += (*it).estRateTotal[qp];
        }
        ++it;
   }



   // calculate Qp
    mfxI32 maxDeltaQp = -100000;
    mfxI32 curQp = m_curBaseQp < 0 ? SelectQp(totalEstRate, m_targetRateMin * numberOfFrames, 8) : m_curBaseQp;
    mfxF64 strength = 0.01 * curQp + 1.2;

    // fill deltaQp
    it = start;
    for (mfxU32 i=0; i < numberOfFrames ; i++)
    {
        mfxU32 intraCost    = (*it).intraCost;
        mfxU32 interCost    = (*it).interCost;
        mfxU32 propCost     = (*it).propCost;
        mfxF64 ratio        = 1.0;//mfxF64(interCost) / intraCost;
        mfxF64 deltaQp      = log((intraCost + propCost * ratio) / intraCost) / log(2.0);
        (*it).deltaQp = (interCost >= intraCost * 0.9)
            ? -mfxI32(deltaQp * 1.5 * strength + 0.5)
            : -mfxI32(deltaQp * 1.0 * strength + 0.5);
        maxDeltaQp = std::max(maxDeltaQp, (*it).deltaQp);
        maxLayer = ((*it).layer > maxLayer) ? (*it).layer : maxLayer;
        ++it;
    }


    it = start;
    for (mfxU32 i=0; i < numberOfFrames ; i++)
    {
        (*it).deltaQp -= maxDeltaQp;
        ++it;
    }


    {
        mfxU8 minQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMax * numberOfFrames, 8);
        mfxU8 maxQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMin * numberOfFrames, 8);

        if (m_curBaseQp < 0)
            m_curBaseQp = minQp; // first frame
        else if (m_curBaseQp < minQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, (int)minQp);
        else if (m_curQp > maxQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, (int)maxQp);
        else
            {}; // do not change qp if last qp guarantees target rate interval
    }
    if ((*start).deltaQp == 0 && (*start).layer == 0 && maxLayer > 1)
    {
        (*start).deltaQp = -1;
    }
    m_curQp = CLIPVAL(1, 51, m_curBaseQp + (*start).deltaQp); // driver doesn't support qp=0
    //printf("%d) intra %d, inter %d, prop %d, delta %d, maxdelta %d, baseQP %d, qp %d \n",(*start).encOrder,(*start).intraCost, (*start).interCost, (*start).propCost, (*start).deltaQp, maxDeltaQp, m_curBaseQp,m_curQp );

    //printf("%d\t base QP %d\tdelta QP %d\tcurQp %d, rate (%f, %f), total rate %f (%f, %f), number of frames %d\n",
    //    (*start).dispOrder, m_curBaseQp, (*start).deltaQp, m_curQp, (mfxF32)m_targetRateMax*numberOfFrames,(mfxF32)m_targetRateMin*numberOfFrames,  (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp),  (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp + 1), (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp -1),numberOfFrames);
    //if ((*start).dispOrder > 1700 && (*start).dispOrder < 1800 )
    {
        //for (mfxU32 qp = 0; qp < 52; qp++)
        //{
        //    printf("....qp %d coeff hist %f\n", qp, (mfxF32)m_rateCoeffHistory[qp].GetCoeff());
       // }
    }
    (*start).qp = m_curQp;

    return mfxU8(m_curQp);
}
BrcIface * CreateBrc(mfxVideoParam const * video)

{
    if (video->mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT)
        return new VMEBrc;
    else
        return new AV1BRC;


}
} // namespace

#endif