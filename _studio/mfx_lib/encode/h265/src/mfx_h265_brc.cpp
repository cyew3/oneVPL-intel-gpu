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

#define fprintf

#define BRC_QSTEP_SCALE_EXPONENT 0.7

mfxStatus H265BRC::Close()
{
    mfxStatus status = MFX_ERR_NONE;
    if (!mIsInit)
        return MFX_ERR_NOT_INITIALIZED;
    mIsInit = false;
    return status;
}


static Ipp64f QP2Qstep (Ipp32s QP)
{
    return (Ipp64f)(pow(2.0, (QP - 4.0) / 6.0));
}

static Ipp32s Qstep2QP (Ipp64f Qstep)
{
    return (Ipp32s)(4.0 + 6.0 * log(Qstep) / log(2.0));
}


mfxStatus H265BRC::InitHRD()
{
    mfxU64 bufSizeBits = mParams.HRDBufferSizeBytes << 3;
    Ipp32s bitsPerFrame = mBitsDesiredFrame;
    mfxU64 maxBitrate = mParams.maxBitrate;

    if (MFX_RATECONTROL_VBR != mRCMode)
        maxBitrate = mBitrate;

    if (maxBitrate < (mfxU64)mBitrate)
        maxBitrate = 0;

    mParams.maxBitrate = (Ipp32s)((maxBitrate >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE));
    mHRD.maxBitrate = mParams.maxBitrate;
    mHRD.inputBitsPerFrame = mHRD.maxBitrate / mFramerate;

    if (bufSizeBits > 0 && bufSizeBits < (mfxU64)(bitsPerFrame << 1))
        bufSizeBits = (bitsPerFrame << 1);

    mHRD.bufSize = (Ipp32u)((bufSizeBits >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE));
    mParams.HRDBufferSizeBytes = (Ipp32s)(mHRD.bufSize >> 3);

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

Ipp32s H265BRC::GetInitQP()
{
//  const Ipp64f x0 = 0, y0 = 1.19, x1 = 1.75, y1 = 1.75;
  Ipp32s fs, fsLuma;

  fsLuma = mParams.width * mParams.height;
  fs = fsLuma;
  if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
    fs += fsLuma / 2;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
    fs += fsLuma;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
    fs += fsLuma * 2;
  fs = fs * mParams.bitDepthLuma / 8;
  //Ipp32s q = (Ipp32s)(1. / 1.2 * pow(10.0, (log10(fs * 2. / 3. * mFramerate / mBitrate) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
  
  Ipp64f qstep = pow(1.5 * fs * mFramerate / mBitrate, 0.8);
  Ipp32s q = Qstep2QP(qstep);

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
//        mParams.HRDBufferSizeBytes = mParams.HRDInitialDelayBytes = 0;
        mParams.HRDBufferSizeBytes = params->mfx.BufferSizeInKB * brcParamMultiplier * 1000; // tmp !!! ???
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

mfxStatus H265BRC::Init(const mfxVideoParam *params,  H265VideoParam &video, Ipp32s enableRecode)
{
    mfxStatus status = MFX_ERR_NONE;

    if (!params)
        return MFX_ERR_NULL_PTR;

    status = SetParams(params, video);
    if (status != MFX_ERR_NONE)
        return status;

    mRecode = (enableRecode && (mParams.BRCMode != MFX_RATECONTROL_AVBR)) ? 1 : 0;

    mFramerate = (Ipp64f)mParams.frameRateExtN / mParams.frameRateExtD;
    mBitrate = mParams.targetBitrate;
    mRCMode = (mfxU16)mParams.BRCMode;

    if (mBitrate <= 0 || mFramerate <= 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mBitsDesiredFrame = (Ipp32s)(mBitrate / mFramerate);
    if (mBitsDesiredFrame < 10)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
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
    mRecodedFrame_encOrder = -1;


    if (video.AnalyzeCmplx && mRCMode == MFX_RATECONTROL_AVBR) {

        mTotalDeviation = 0;

        mNumLayers = video.BiPyramidLayers + 1;

        memset((void*)mEstCoeff, 0, mNumLayers*sizeof(Ipp64f));
        memset((void*)mEstCnt, 0, mNumLayers*sizeof(Ipp64f));
        memset((void*)mEncBits, 0, mNumLayers*sizeof(Ipp64f));
        memset((void*)mQstep, 0, mNumLayers*sizeof(Ipp64f));
        memset((void*)mAvCmplx, 0, mNumLayers*sizeof(Ipp64f));
        mTotAvComplx = 0;
        mTotComplxCnt = 0;
        mComplxSum = 0;
        mTotMeanComplx = 0;
        mTotTarget = mBitsDesiredFrame;
        mIBitsLoan = 0;
        mLoanLength = 0;
        mLoanBitsPerFrame = 0;

        mQstepBase = -1.;
        mQpBase = -1;

        Ipp32s fs;
        fs = mParams.width * mParams.height;
        if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
            fs += fs / 2;
        else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
            fs += fs;
        else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
            fs += fs * 2;
        fs = fs * mParams.bitDepthLuma / 8;

        //mCmplxRate = 2.*pow((Ipp64f)fs, 0.8);
        //mCmplxRate = 4.*pow((Ipp64f)fs, 0.8);
        mCmplxRate = 10.*pow((Ipp64f)fs, 0.8); // adjustment for subpel
        mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);

        Ipp64f sum = 1;
        Ipp64f layerRatio = 0.3; // tmp ???
        Ipp64f layerShare = layerRatio;
        for (Ipp32s i = 0; i < video.BiPyramidLayers; i++) {
            sum += layerShare;
            layerShare *= 2*layerRatio;
        }
        Ipp64f targetMiniGop = mBitsDesiredFrame * video.GopRefDist;
        mLayerTarget[1] = targetMiniGop / sum;
        mLayerTarget[0] = mLayerTarget[1] * 3;
        for (Ipp32s i = 0; i < video.BiPyramidLayers; i++)
            mLayerTarget[1+i] = mLayerTarget[i]*layerRatio;

        for (Ipp32s i = 0; i < mNumLayers; i++) {
            mEstCoeff[i] = mLayerTarget[i] * 10.;
            mEstCnt[i] = 1.;
        }

    } else {

        Ipp32s q = GetInitQP();

        if (!mRecode) {
            if (q - 6 > 10)
                mQuantMin = IPP_MAX(10, q - 10);
            else
                mQuantMin = IPP_MAX(q - 6, 2);

            if (q < mQuantMin)
                q = mQuantMin;
        }

        mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = q;

        mRCbap = 100;
        mRCqap = 100;
        mRCfap = 100;

        mRCq = q;
        mRCqa = mRCqa0 = 1. / (Ipp64f)mRCq;
        mRCfa = mBitsDesiredFrame;
        mRCfa_short = mBitsDesiredFrame;

        mSChPoc = 0;
        mSceneChange = 0;
        mBitsEncodedPrev = mBitsDesiredFrame;
        mPicType = MFX_FRAMETYPE_I;

        mMaxQp = 999;
        mMinQp = -1;
    }

    mIsInit = true;

    return status;
}


#define RESET_BRC_CORE \
{ \
    if (sizeNotChanged) { \
        mRCq = (Ipp32s)(1./mRCqa * pow(mRCfa/mBitsDesiredFrame, 0.32) + 0.5); \
        BRC_CLIP(mRCq, 1, mQuantMax); \
    } else \
        mRCq = GetInitQP(); \
    mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = mRCq; \
    mRCqa = mRCqa0 = 1./mRCq; \
    mRCfa = mBitsDesiredFrame; \
    mRCfa_short = mBitsDesiredFrame; \
}


mfxStatus H265BRC::Reset(mfxVideoParam *params, H265VideoParam &video, Ipp32s enableRecode)
{
    mfxStatus status;

    mfxU16 brcParamMultiplier = params->mfx.BRCParamMultiplier ? params->mfx.BRCParamMultiplier : 1;
    mfxU16 rcmode_new = params->mfx.RateControlMethod;
    Ipp32s bufSize_new = rcmode_new == MFX_RATECONTROL_AVBR ? 0 : (params->mfx.BufferSizeInKB * brcParamMultiplier * 8000 >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE);
    bool sizeNotChanged = (mParams.width == params->mfx.FrameInfo.Width &&
                           mParams.height == params->mfx.FrameInfo.Height &&
                           mParams.chromaFormat == params->mfx.FrameInfo.ChromaFormat);

    Ipp32s bufSize, maxBitrate;
    Ipp64f bufFullness;
    mfxU16 rcmode = mRCMode;
    Ipp32s targetBitrate = mBitrate;

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
        mFramerate = (Ipp64f)mParams.frameRateExtN / mParams.frameRateExtD;
        if (mBitrate <= 0 || mFramerate <= 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        mBitsDesiredFrame = (Ipp32s)(mBitrate / mFramerate);
        RESET_BRC_CORE
        return MFX_ERR_NONE;
    } else if (mParams.HRDBufferSizeBytes == 0 && bufSize_new > 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    Ipp32s maxBitrate_new = (params->mfx.MaxKbps * brcParamMultiplier * 1000 >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);
    Ipp32s targetBitrate_new = params->mfx.TargetKbps * brcParamMultiplier * 1000;
    Ipp32s targetBitrate_new_r = (targetBitrate_new >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);

    // framerate change not allowed in case of HRD
    if (mParams.frameRateExtN * params->mfx.FrameInfo.FrameRateExtD != mParams.frameRateExtD * params->mfx.FrameInfo.FrameRateExtN)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    bufSize = mHRD.bufSize;
    maxBitrate = (Ipp32s)mHRD.maxBitrate;
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
            Ipp64f bf_delta = (maxBitrate_new - maxBitrate) / mFramerate;
          // lower estimate for the fullness with the bitrate updated at tai;
          // for VBR the fullness encoded in buffering period SEI can be below the real buffer fullness
            bufFullness += bf_delta;
            if (bufFullness > (Ipp64f)(bufSize - 1))
                bufFullness = (Ipp64f)(bufSize - 1);

            mBF += (mfxI64)((maxBitrate_new >> 3) - mMaxBitrate) * (mfxI64)mParams.frameRateExtD;
            if (mBF > (mfxI64)(bufSize >> 3) * mParams.frameRateExtN)
                mBF = (mfxI64)(bufSize >> 3) * mParams.frameRateExtN;

            maxBitrate = maxBitrate_new;
        } else // HRD overflow is possible for CBR
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (bufSize_new > 0 && bufSize_new < bufSize) {
        bufSize = bufSize_new;
        if (bufFullness > (Ipp64f)(bufSize - 1)) {
            if (MFX_RATECONTROL_VBR == rcmode)
                bufFullness = (Ipp64f)(bufSize - 1);
            else
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if (targetBitrate > maxBitrate)
        targetBitrate = maxBitrate;

    mHRD.bufSize = bufSize;
    mHRD.maxBitrate = (Ipp64f)maxBitrate;
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
    mMaxBitrate = (Ipp32u)(maxBitrate >> 3);

    RESET_BRC_CORE

    mSceneChange = 0;
    mBitsEncodedPrev = mBitsDesiredFrame;

    mMaxQp = 999;
    mMinQp = -1;

    return MFX_ERR_NONE;
}

#define BRC_QSTEP_COMPL_EXPONENT 0.4
#define BRC_CMPLX_DECAY 0.1
#define BRC_MIN_CMPLX 0.1

static const Ipp64f brc_qstep_factor[8] = {pow(2., -1./6.), 1., pow(2., 1./6.),   pow(2., 2./6.), pow(2., 3./6.), pow(2., 4./6.), pow(2., 5./6.), 2.};
static const Ipp64f minQstep = QP2Qstep(1);
static const Ipp64f maxQstep = QP2Qstep(51);
static const Ipp64f maxQstepChange = pow(2, 0.5);

Ipp32s H265BRC::GetQP(H265VideoParam *video, H265Frame *frames[], Ipp32s numFrames)
{
    H265Frame *frame;
    Ipp32s i, j;
    Ipp32s qp;

    if (mRCMode == MFX_RATECONTROL_AVBR && video->AnalyzeCmplx) {

        if (!frames || numFrames <= 0) {
            return Qstep2QP(mQstepBase);
        }

        // limit ourselves to miniGOP+1 for the time being
        if (numFrames > video->GopRefDist + 1)
            numFrames = video->GopRefDist + 1;

        //if (numFrames > BRC_MAX_LA_LEN)
        //    numFrames = BRC_MAX_LA_LEN;

        Ipp32s len = numFrames;
    //    for (i = 0; i < numFrames; i++) {
        for (i = 1; i < numFrames; i++) {
            if (!frames[i] || frames[i]->m_sceneCut) {
                len = i;
                break;
            }
        }

        Ipp64f qstepBase = mQstepBase;
        qp = Qstep2QP(qstepBase);

        Ipp64f avCmplx = 0;
        for (i = 0; i < len; i++)
            avCmplx += frames[i]->m_avgBestSatd;
        avCmplx /= len;
        frames[0]->m_avCmplx = avCmplx;


        mTotAvComplx *= BRC_CMPLX_DECAY;
        mTotComplxCnt *= BRC_CMPLX_DECAY;

        if (avCmplx < BRC_MIN_CMPLX)
            avCmplx = BRC_MIN_CMPLX;

        mTotAvComplx += avCmplx; // framerate ???
        mTotComplxCnt += 1;
        Ipp64f complx = mTotAvComplx / mTotComplxCnt;

        mComplxSum += avCmplx;
        mTotMeanComplx = mComplxSum / (frames[0]->m_encOrder + 1);

fprintf(stderr, "%d: %d %d %.3f \n", frames[0]->m_encOrder, len, qp, complx);

        frames[0]->m_CmplxQstep = pow(complx, BRC_QSTEP_COMPL_EXPONENT);

        Ipp64f q = frames[0]->m_CmplxQstep * mQstepScale;

        Ipp64f targetBits = 0;
        for (i = 0; i < len; i++) { // don't include the last LA frame - consider one whole miniGOP only
            Ipp32s layer = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : 1) + frames[i]->m_pyramidLayer;
            targetBits += mLayerTarget[layer];
        }

        Ipp32s bufsize = mParams.HRDBufferSizeBytes*8;

        Ipp64f estBits = 0;
        Ipp64f cmpl = pow(avCmplx, 0.5); // use frames[0]->m_CmplxQstep (^0.4) ???
  

        for (i = 0; i < len; i++) { // don't include last LA frame - consider one whole miniGOP only
            Ipp32s layer = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : 1) + frames[i]->m_pyramidLayer;
            Ipp64f qstep = q * brc_qstep_factor[layer];
            BRC_CLIP(qstep, minQstep, maxQstep);
            if (layer == 0) 
                layer = 1;
            estBits += mEstCoeff[layer] * cmpl / (mEstCnt[layer] * qstep);
        }


        Ipp64s totdev;
    
        if (frames[0]->m_encOrder <= video->GopRefDist)
            totdev = mTotalDeviation + 0.1*(estBits - targetBits);
        else
            totdev = mTotalDeviation + 0.5*(estBits - targetBits);

        Ipp64f devtopLim = bufsize * 0.25;
        if (devtopLim < mTotalDeviation - targetBits*0.4)
            devtopLim = mTotalDeviation - targetBits*0.4;
        Ipp64f devbotLim = -bufsize*0.25;
        if (devbotLim > mTotalDeviation + targetBits*0.4)
            devbotLim = mTotalDeviation + targetBits*0.4;

        fprintf(stderr, "%7.2f %d, %9.0f %8d ", q, (int)totdev, estBits, bufsize);

        Ipp64f qf = 1, qf1;
        if (totdev > devtopLim)
            qf = totdev / devtopLim;
        else if (totdev < devbotLim)
            qf = devbotLim / totdev;

        fprintf(stderr, "qf %7.2f ", qf);

        if (frames[0]->m_encOrder > video->GopRefDist) {
            if (mTotalDeviation > bufsize) {
                qf1 = mTotalDeviation / (Ipp64f)bufsize;
                if (qf > 1) {
                    if (qf1 > qf)
                        qf = qf1;
                } else {
                    qf *= qf1;
                    if (qf < 1)
                        qf = 1;
                }
            } else if (mTotalDeviation < -bufsize) {
                qf1 = (Ipp64f)(-bufsize) / mTotalDeviation;
                if (qf < 1) {
                    if (qf1 < qf)
                        qf = qf1;
                } else {
                    qf *= qf1;
                    if (qf > 1)
                        qf = 1;
                }
            }
        }

        fprintf(stderr, "%7.2f ", qf);

        if (qf < 1) {
            Ipp64f w;
            if (avCmplx <= mTotMeanComplx) {
                w = 0.5 * avCmplx / mTotMeanComplx;
                qf = w*qf + (1 - w);
            } else {
                w = 0.5 * avCmplx / mTotMeanComplx;
                if (w > 1.)
                    w = 1.;
                qf = w*qf + (1 - w);
            }
        }

        fprintf(stderr, "%7.2f ", qf);

        q *= qf;

        if (mQstepBase > 0)
            BRC_CLIP(q, mQstepBase/maxQstepChange, mQstepBase*maxQstepChange);
        mQstepBase = q;

        Ipp32s l = (frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : 1) + frames[0]->m_pyramidLayer;
        q *= brc_qstep_factor[l];


        qp = Qstep2QP(q);
        BRC_CLIP(qp, mQuantMin, mQuantMax);


        {
            Ipp64f  estbits  = (mEstCoeff[l] * cmpl) / (mEstCnt[l] * q);
            //fprintf(stderr, "%d, %2d, %6.1f, %6.2f, %6.2f, %9.1f, %9.1f, %2.2f,   %7d, %d ", l, qp, q, frames[0]->m_avgBestSatd, cmpl, mEstCoeff[l],  mEstOff[l], mEstCnt[l],  (int)estbits, bufsize);
            fprintf(stderr, "%d, %2d, %7.2f, %7.3f, %7.3f, %7.3f, %7.3f, %7.3f, %7d, %4.3f \n", l, qp, q, frames[0]->m_avgBestSatd, avCmplx, complx, frames[0]->m_CmplxQstep, mCmplxRate / mTotTarget,  (int)estbits, frames[0]->m_intraRatio);
        }
        return qp - mQuantOffset;

    } else {

        Ipp32s qp = mQuantB;
        H265Frame* pFrame = NULL;

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
fprintf(stderr, "GetQP %d %d %d %d \n", pFrame->m_encOrder, qp, (int)pFrame->m_picCodeType, pFrame->m_pyramidLayer);
        }

        return qp - mQuantOffset;
    }        

}

mfxStatus H265BRC::SetQP(Ipp32s qp, mfxU16 frameType)
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


mfxBRCStatus H265BRC::UpdateAndCheckHRD(Ipp32s frameBits, Ipp64f inputBitsPerFrame, Ipp32s recode)
{
    mfxBRCStatus ret = MFX_ERR_NONE;
    Ipp64f bufFullness;

    if (!(recode & (MFX_BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
        mHRD.prevBufFullness = mHRD.bufFullness;
        mHRD.underflowQuant = 0;
        mHRD.overflowQuant = 999;
    } else { // frame is being recoded - restore buffer state
        mHRD.bufFullness = mHRD.prevBufFullness;
    }

    mHRD.maxFrameSize = (Ipp32s)(mHRD.bufFullness - 1);
    mHRD.minFrameSize = (mRCMode != MFX_RATECONTROL_CBR ? 0 : (Ipp32s)(mHRD.bufFullness + 1 + 1 + inputBitsPerFrame - mHRD.bufSize));
    if (mHRD.minFrameSize < 0)
        mHRD.minFrameSize = 0;

    bufFullness = mHRD.bufFullness - frameBits;

    if (bufFullness < 2) {
        bufFullness = inputBitsPerFrame;
        ret = MFX_BRC_ERR_BIG_FRAME;
        if (bufFullness > (Ipp64f)mHRD.bufSize)
            bufFullness = (Ipp64f)mHRD.bufSize;
    } else {
        bufFullness += inputBitsPerFrame;
        if (bufFullness > (Ipp64f)mHRD.bufSize - 1) {
            bufFullness = (Ipp64f)mHRD.bufSize - 1;
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

//#define BRC_SCENE_CHANGE_RATIO1 10.0
//#define BRC_SCENE_CHANGE_RATIO2 5.0

#define I_WEIGHT 1.2
#define P_WEIGHT 0.25
#define B_WEIGHT 0.2


#define BRC_MAX_QSTEP_FACTOR 1.55

#define BRC_MAX_LOAN_LENGTH 75
#define BRC_LOAN_RATIO 0.075

//#define fprintf

mfxBRCStatus H265BRC::PostPackFrame(H265VideoParam *video, Ipp8s sliceQpY, H265Frame *pFrame, Ipp32s totalFrameBits, Ipp32s overheadBits, Ipp32s repack)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    Ipp32s bitsEncoded = totalFrameBits - overheadBits;
    Ipp64f e2pe;
    Ipp32s qp, qpprev;
    Ipp32u prevFrameType = mPicType;
    Ipp32u picType = pFrame->m_picCodeType;

    if (mBitrate == 0)
        return Sts;

    if (!pFrame)
        return MFX_ERR_NULL_PTR;

    mPoc = pFrame->m_poc;

    Ipp32s layer = (picType == MFX_FRAMETYPE_I ? 0 : 1) + pFrame->m_pyramidLayer; // should be 0 for I, 1 for P, etc. !!!
    Ipp64f qstep = QP2Qstep(sliceQpY);
    Ipp64f decay = layer ? 0.5 : 0.25;

    if (video && mRCMode == MFX_RATECONTROL_AVBR && video->AnalyzeCmplx) {

        Ipp64f cmpl = pow(pFrame->m_avCmplx, 0.5); // use ^0.4 ???
        if (pFrame->m_avCmplx > BRC_MIN_CMPLX) {
            Ipp64f coefn = bitsEncoded * qstep / cmpl;
            mEstCoeff[layer] *= decay;
            mEstCoeff[layer] += coefn;
            mEstCnt[layer] *= decay;
            mEstCnt[layer] += 1.;
        }

        if (picType == MFX_FRAMETYPE_I) {
            if (mLoanLength)
                bitsEncoded += mLoanLength * mLoanBitsPerFrame;

            mLoanLength = video->GopPicSize;
            if (mLoanLength > BRC_MAX_LOAN_LENGTH)
                mLoanLength = BRC_MAX_LOAN_LENGTH;
            Ipp32s bitsEncodedI = (Ipp32s)((Ipp64f)bitsEncoded  / (mLoanLength * BRC_LOAN_RATIO + 1));
 
            mIBitsLoan = bitsEncoded - bitsEncodedI;
            mLoanBitsPerFrame = mIBitsLoan / mLoanLength;

            bitsEncoded = bitsEncodedI;

        } else if (mLoanLength) {
            bitsEncoded += mLoanBitsPerFrame;
            mLoanLength--;
        }

        mCmplxRate += bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
        mTotTarget += mBitsDesiredFrame;

        mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);

        mBitsEncodedTotal += totalFrameBits;        

        mEncBits[layer] *= decay;
        mEncBits[layer] += totalFrameBits;

        mQstep[layer] *= decay;
        mQstep[layer] += qstep;

        mAvCmplx[layer] *= decay;
        mAvCmplx[layer] += pFrame->m_avCmplx;

        mTotalDeviation += bitsEncoded - mBitsDesiredFrame;

        fprintf(stderr, " %d | %8d, %8I64d, %10.1f, %10.0f, %6.2f \n", pFrame->m_encOrder, bitsEncoded, mTotalDeviation, mCmplxRate, mTotTarget, qstep);

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

    Ipp64f inputBitsPerFrame = 1;

    if (mSceneChange)
        if (mQuantUpdated == 1 && mPoc > mSChPoc + 1)
            mSceneChange &= ~16;

    Ipp64f buffullness = 1.e12; // a big number
    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        inputBitsPerFrame = mHRD.inputBitsPerFrame;
        buffullness = repack ? mHRD.prevBufFullness : mHRD.bufFullness;
        Sts = UpdateAndCheckHRD(totalFrameBits, inputBitsPerFrame, repack);
    }

    //if (pFrame->m_encOrder == mRecodedFrame_encOrder)
    //    qpprev = qp = mQuantRecoded;
    //else
    //    qpprev = qp = (picType == MFX_FRAMETYPE_I) ? mQuantI : ((picType == MFX_FRAMETYPE_P) ? mQuantP : (layer > 0 ? mQuantB + layer - 1 : mQuantB));

    qpprev = qp = mQp = sliceQpY;

fprintf(stderr, "%d %d %d %d %d %.1f %d %d %d \n", pFrame->m_encOrder, totalFrameBits, (int)buffullness, Sts, sliceQpY, 1/mRCqa, (int)picType, pFrame->m_pyramidLayer, pFrame->m_encOrder == mRecodedFrame_encOrder);

    Ipp64f fa_short0 = mRCfa_short;
    mRCfa_short += (bitsEncoded - mRCfa_short) / BRC_RCFAP_SHORT;

    {
        qstep = QP2Qstep(qp);
        Ipp64f qstep_prev = QP2Qstep(mQPprev);
        Ipp64f frameFactor = 1.0;
        Ipp64f targetFrameSize = IPP_MAX((Ipp64f)mBitsDesiredFrame, mRCfa);
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

        Ipp64f maxFrameSize;
        maxFrameSize = 2.5/9. * buffullness + 5./9. * targetFrameSize;
        BRC_CLIP(maxFrameSize, targetFrameSize, BRC_SCENE_CHANGE_RATIO2 * targetFrameSize * frameFactor);

        Ipp64f famax = 1./9. * buffullness + 8./9. * mRCfa;

        Ipp32s maxqp = mQuantMax;
        if (mRCMode == MFX_RATECONTROL_CBR) {
            maxqp = IPP_MIN(maxqp, mHRD.overflowQuant - 1);
        }

        if (bitsEncoded >  maxFrameSize && qp < maxqp) {
            Ipp64f targetSizeScaled = maxFrameSize * 0.8;
            Ipp64f qstepnew = qstep * pow(bitsEncoded / targetSizeScaled, 0.9);
            Ipp32s qpnew = Qstep2QP(qstepnew);
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

fprintf(stderr, "recode1 %d %d %d %d \n", pFrame->m_encOrder, qpnew, bitsEncoded ,  (int)maxFrameSize);
                    return MFX_BRC_ERR_BIG_FRAME;
                }
            }
        }

        if (mRCfa_short > famax && (!repack) && qp < maxqp) {

            Ipp64f qstepnew = qstep * mRCfa_short / (famax * 0.8);
            Ipp32s qpnew = Qstep2QP(qstepnew);
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

fprintf(stderr, "recode2 %d %d \n", pFrame->m_encOrder, qpnew);
                return MFX_BRC_ERR_BIG_FRAME;
            }
        }
    }

    mPicType = picType;

    Ipp64f fa = mRCfa;
    bool oldScene = false;
    if ((mSceneChange & 16) && (mPoc < mSChPoc) && (mBitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (Ipp64f)mBitsEncodedP) && (Ipp64f)mBitsEncoded < 1.5*fa)
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

    return Sts;
};

mfxBRCStatus H265BRC::UpdateQuant(Ipp32s bEncoded, Ipp32s totalPicBits, Ipp32s layer, Ipp32s recode)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    Ipp64f  bo, qs, dq;
    Ipp32s  quant;
    Ipp32u bitsPerPic = (Ipp32u)mBitsDesiredFrame;
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
        mfxI64 targetFullness = IPP_MIN(mParams.HRDInitialDelayBytes << 3, (Ipp32s)mHRD.bufSize / 2);
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

    Ipp32s bap = mRCbap;
    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        Ipp64f bfRatio = mHRD.bufFullness / mBitsDesiredFrame;
        if (totalBitsDeviation > 0) {
            bap = (Ipp32s)bfRatio*3;
            bap = IPP_MAX(bap, 10);
            BRC_CLIP(bap, mRCbap/10, mRCbap);
        }
    }
    bo = (Ipp64f)totalBitsDeviation / bap / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);

    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./mQuantMin);
    quant = (Ipp32s) (1. / dq + 0.5);

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

    fprintf(stderr, "%d %.0f %d %d %f %f %f %f %d \n", bEncoded, mRCfa, quant, mRCq, 1/mRCqa, qs, bo, dq, (int)totalBitsDeviation);
    mRCq = quant;

    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        Ipp64f qstep = QP2Qstep(quant);
        Ipp64f fullnessThreshold = MIN(bitsPerPic * 12, mHRD.bufSize*3/16);
        qs = 1.0;
        if (bEncoded > mHRD.bufFullness && mPicType != MFX_FRAMETYPE_I)
            qs = (Ipp64f)bEncoded / (mHRD.bufFullness);

        if (mHRD.bufFullness < fullnessThreshold && (Ipp32u)totalPicBits > bitsPerPic)
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
    }

    return Sts;
}

mfxBRCStatus H265BRC::UpdateQuantHRD(Ipp32s totalFrameBits, mfxBRCStatus sts, Ipp32s overheadBits, Ipp32s layer, Ipp32s recode)
{
    Ipp32s quant, quant_prev;
    Ipp32s wantedBits = (sts == MFX_BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize * 3 / 4 : mHRD.minFrameSize * 5 / 4);
    Ipp32s bEncoded = totalFrameBits - overheadBits;
    Ipp64f qstep, qstep_new;

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

    qstep = QP2Qstep(quant);
    qstep_new = qstep * sqrt((Ipp64f)bEncoded / wantedBits);
//    qstep_new = qstep * sqrt(sqrt((Ipp64f)bEncoded / wantedBits));
    quant = Qstep2QP(qstep_new);
    BRC_CLIP(quant, 1, mQuantMax);

    if (sts & MFX_BRC_ERR_SMALL_FRAME) // overflow
    {
        Ipp32s qpMin = IPP_MAX(mHRD.underflowQuant, mMinQp);
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


fprintf(stderr, "%d \n", quant);
    if (quant == quant_prev)
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    mQuantRecoded = quant;

    return sts;
}

mfxStatus H265BRC::GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode)
{
    Ipp32u cpb_rem_del_u32;
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
    cpb_rem_del_u32 = (Ipp32u)cpb_rem_del_u64;

    if (MFX_RATECONTROL_VBR == mRCMode) {
        mBF = temp2_u64 * cpb_rem_del_u32 / 90000;
        temp1_u64 = (mfxU64)cpb_rem_del_u32 * mMaxBitrate;
        Ipp32u dec_buf_ful = (Ipp32u)(temp1_u64 / (90000/8));
        if (recode)
            mHRD.prevBufFullness = (Ipp64f)dec_buf_ful;
        else
            mHRD.bufFullness = (Ipp64f)dec_buf_ful;
    }

    *initial_cpb_removal_delay = cpb_rem_del_u32;
    return MFX_ERR_NONE;
}



void  H265BRC::GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits)
{
    if (minFrameSizeInBits)
      *minFrameSizeInBits = mHRD.minFrameSize;
    if (maxFrameSizeInBits)
      *maxFrameSizeInBits = mHRD.maxFrameSize;
}

#define CLIPVAL(MINVAL, MAXVAL, VAL) (IPP_MAX(MINVAL, IPP_MIN(MAXVAL, VAL)))
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



mfxStatus VMEBrc::Init(const mfxVideoParam *init, H265VideoParam &, mfxI32 )
{
    m_qpUpdateRange = 40;
    mfxU32 RegressionWindow = 20;

    mfxF64 fr = mfxF64(init->mfx.FrameInfo.FrameRateExtN) / init->mfx.FrameInfo.FrameRateExtD;
    m_totNumMb = init->mfx.FrameInfo.Width * init->mfx.FrameInfo.Height / 256;
    m_initTargetRate = 1000 * init->mfx.TargetKbps / fr / m_totNumMb;
    m_targetRateMin = m_initTargetRate;
    m_targetRateMax = m_initTargetRate;
    m_laData.resize(0);

   for (mfxU32 qp = 0; qp < 52; qp++)
        m_rateCoeffHistory[qp].Reset(RegressionWindow, 100.0, 100.0 * INIT_RATE_COEFF[qp]);
    m_framesBehind = 0;
    m_bitsBehind = 0.0;
    m_curQp = -1;
    m_curBaseQp = -1;
    m_lookAheadDep = 0;
    m_bPyr = true; //to do

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
    m_lookAheadDep = IPP_MAX(numLaFrames, m_lookAheadDep);

    // 
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
    static int size = 0;
    static int nframe = 0;
    size += dataLength;
    nframe++;

    m_framesBehind++;
    m_bitsBehind += realRatePerMb;
    mfxF64 framesBeyond = (mfxF64)(IPP_MAX(2, m_laData.size()) - 1);

    m_targetRateMax = (m_initTargetRate * (m_framesBehind + ((mfxF64)m_laData.size() - 1)) - m_bitsBehind) / framesBeyond;
    m_targetRateMin = (m_initTargetRate * (m_framesBehind + (framesBeyond   )) - m_bitsBehind) / framesBeyond;

    // start of sequence
    std::list<LaFrameData>::iterator start = m_laData.begin();
    for(;start != m_laData.end(); ++start)
    {
        if ((*start).encOrder == picOrder)
            break;
    }
    if (start != m_laData.end())
    {
       mfxI32 curQp = (*start).qp;
        mfxF64 oldCoeff = m_rateCoeffHistory[curQp].GetCoeff();
        mfxF64 y = IPP_MAX(0.0, realRatePerMb);
        mfxF64 x = (*start).estRate[curQp];
        //printf("%d) intra %d, inter %d, prop %d, qp %d,  est rate %f, real %f \t k %f \n",(*start).encOrder,(*start).intraCost, (*start).interCost,(*start).propCost, curQp, (*start).estRate[curQp],realRatePerMb, x/y);
        mfxF64 minY = NORM_EST_RATE * INIT_RATE_COEFF[curQp] * MIN_RATE_COEFF_CHANGE;
        mfxF64 maxY = NORM_EST_RATE * INIT_RATE_COEFF[curQp] * MAX_RATE_COEFF_CHANGE;
        y = CLIPVAL(minY, maxY, y / x * NORM_EST_RATE); 
        m_rateCoeffHistory[curQp].Add(NORM_EST_RATE, y);
        //mfxF64 ratio = m_rateCoeffHistory[curQp].GetCoeff() / oldCoeff;
        mfxF64 ratio = y / (oldCoeff*NORM_EST_RATE);
        for (mfxI32 i = -m_qpUpdateRange; i <= m_qpUpdateRange; i++)
            if (i != 0 && curQp + i >= 0 && curQp + i < 52)
            {
                mfxF64 r = ((ratio - 1.0) * (1.0 - ((mfxF64)abs(i))/((mfxF64)m_qpUpdateRange + 1.0)) + 1.0);
                m_rateCoeffHistory[curQp + i].Add(NORM_EST_RATE,
                    NORM_EST_RATE * m_rateCoeffHistory[curQp + i].GetCoeff() * r);
            }
        (*start).bNotUsed = 1;
    }
    return 0;
}
Ipp32s VMEBrc::GetQP(H265VideoParam *video, H265Frame *pFrame[], Ipp32s numFrames)
{
     if ((!pFrame)||(numFrames == 0))
         return 26;
     return GetQP(*video, pFrame[0], 0 ); 
 }

mfxI32 VMEBrc::GetQP(H265VideoParam &video, H265Frame *pFrame, mfxI32 *chromaQP )
{

    UMC::AutomaticUMCMutex guard(m_mutex);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VMEBrc::GetQp");

    if (!m_laData.size())
        return 26;

    mfxF64 totalEstRate[52] = {};  

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
    for(it = start;it != m_laData.end(); ++it) numberOfFrames++;



   // fill totalEstRate
   
   for(it = start;it != m_laData.end(); ++it)
   {
        for (mfxU32 qp = 0; qp < 52; qp++)
        {
            
            (*it).estRateTotal[qp] = IPP_MAX(MIN_EST_RATE, m_rateCoeffHistory[qp].GetCoeff() * (*it).estRate[qp]);
            totalEstRate[qp] += (*it).estRateTotal[qp];        
        }
   }
   


   // calculate Qp
    mfxI32 maxDeltaQp = -100000;
    mfxI32 curQp = m_curBaseQp < 0 ? SelectQp(totalEstRate, m_targetRateMin * numberOfFrames, 8) : m_curBaseQp;
    mfxF64 strength = 0.01 * curQp + 1.2;

    // fill deltaQp
    for (it = start; it != m_laData.end(); ++it)
    {
        mfxU32 intraCost    = (*it).intraCost;
        mfxU32 interCost    = (*it).interCost;
        mfxU32 propCost     = (*it).propCost;
        mfxF64 ratio        = 1.0;//mfxF64(interCost) / intraCost;
        mfxF64 deltaQp      = log((intraCost + propCost * ratio) / intraCost) / log(2.0);
        (*it).deltaQp = (interCost >= intraCost * 0.9)
            ? -mfxI32(deltaQp * 2.0 * strength + 0.5)
            : -mfxI32(deltaQp * 1.0 * strength + 0.5);
        maxDeltaQp = IPP_MAX(maxDeltaQp, (*it).deltaQp);
    }
 
    for (it = start; it != m_laData.end(); ++it)
        (*it).deltaQp -= maxDeltaQp;

    
    //if ((*start).layer == 0)
    {
        mfxU8 minQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMax * numberOfFrames, 8);
        mfxU8 maxQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMin * numberOfFrames, 8);

        if (m_curBaseQp < 0)
            m_curBaseQp = minQp; // first frame
        else if (m_curBaseQp < minQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, minQp);
        else if (m_curQp > maxQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, maxQp);
        else
            ; // do not change qp if last qp guarantees target rate interval
    }
    m_curQp = CLIPVAL(1, 51, m_curBaseQp + (*start).deltaQp); // driver doesn't support qp=0
    // printf("%d) intra %d, inter %d, prop %d, delta %d, maxdelta %d, baseQP %d, qp %d \n",(*start).encOrder,(*start).intraCost, (*start).interCost, (*start).propCost, (*start).deltaQp, maxDeltaQp, m_curBaseQp,m_curQp );

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
        return new H265BRC;


}
} // namespace

#endif