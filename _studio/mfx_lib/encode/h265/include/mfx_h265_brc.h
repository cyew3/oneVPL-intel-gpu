/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013 - 2014 Intel Corporation. All Rights Reserved.
//
//
//          Bitrate control
//
*/

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_BRC_H__
#define __MFX_H265_BRC_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_set.h"
#include "mfx_h265_enc.h"

namespace H265Enc {

#define BRC_CLIP(val, minval, maxval) val = Saturate(minval, maxval, val)

#define SetQuantB() \
    mQuantB = ((mQuantP + mQuantPrev + 1) >> 1) + 1; \
    BRC_CLIP(mQuantB, 1, mQuantMax)

#define MFX_H265_BITRATE_SCALE 0
#define  MFX_H265_CPBSIZE_SCALE 3

enum eMfxBRCStatus
{
    MFX_BRC_ERROR                   = -1,
    MFX_BRC_OK                      = 0x0,
    MFX_BRC_ERR_BIG_FRAME           = 0x1,
    MFX_BRC_BIG_FRAME               = 0x2,
    MFX_BRC_ERR_SMALL_FRAME         = 0x4,
    MFX_BRC_SMALL_FRAME             = 0x8,
    MFX_BRC_NOT_ENOUGH_BUFFER       = 0x10
};

enum eMfxBRCRecode
{
    MFX_BRC_RECODE_NONE           = 0,
    MFX_BRC_RECODE_QP             = 1,
    MFX_BRC_RECODE_PANIC          = 2,
    MFX_BRC_RECODE_EXT_QP         = 3,
    MFX_BRC_RECODE_EXT_PANIC      = 4,
    MFX_BRC_EXT_FRAMESKIP         = 16
};

typedef Ipp32s mfxBRCStatus;

typedef struct _mfxBRC_HRDState
{
    Ipp32u bufSize;
    Ipp64f bufFullness;
    Ipp64f prevBufFullness;
    Ipp64f maxBitrate;
    Ipp64f inputBitsPerFrame;
    Ipp32s frameNum;
    Ipp32s minFrameSize;
    Ipp32s maxFrameSize;
    Ipp32s underflowQuant;
    Ipp32s overflowQuant;
} mfxBRC_HRDState;


typedef struct _mfxBRC_Params
{
    // ???
    // either need this to with GetParams(mfxBRC_Params*) or use GetParams(mfxVideoParam*) to report BRC params to the encoder
    Ipp32s  BRCMode;
    Ipp32s  targetBitrate;

    Ipp32s  HRDInitialDelayBytes;
    Ipp32s  HRDBufferSizeBytes;
    Ipp32s  maxBitrate;

    Ipp32u  frameRateExtD;
    Ipp32u  frameRateExtN;

    Ipp16u width;
    Ipp16u height;
    Ipp16u chromaFormat;
    Ipp32s bitDepthLuma;

} mfxBRC_Params;

class H265BRC
{

public:

    H265BRC()
    {
        mIsInit = 0;
    }
    virtual ~H265BRC()
    {
        Close();
    }

    // Initialize with specified parameter(s)
    mfxStatus Init(const mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode = 1);

    mfxStatus Close();

    mfxStatus Reset(mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode = 1);
    mfxStatus SetParams(const mfxVideoParam *init, H265VideoParam &video);
    mfxStatus GetParams(mfxVideoParam *init);

    mfxBRCStatus PostPackFrame(H265VideoParam *video, Ipp8s sliceQpY, H265Frame *pFrame, Ipp32s bitsEncodedFrame, Ipp32s overheadBits, Ipp32s recode = 0);

    Ipp32s GetQP(H265VideoParam *video, H265Frame *pFrame[], Ipp32s numFrames);
    mfxStatus SetQP(Ipp32s qp, Ipp16u frameType);

    mfxStatus GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode = 0);

    void GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits);


protected:
    mfxBRC_Params mParams;
    bool   mIsInit;
    Ipp32u mBitrate;
    Ipp64f mFramerate;
    Ipp16u mRCMode;
    Ipp16u mQuantUpdated;

    Ipp32s  mBitsDesiredFrame;
    Ipp64s  mBitsEncodedTotal, mBitsDesiredTotal;
    Ipp64s  mBitsPredictedTotal;

    Ipp32u  mPicType;

    Ipp64s mTotalDeviation;
    Ipp64f mEstCoeff[8];
    Ipp64f mEstCnt[8];
    Ipp64f mLayerTarget[8];

    Ipp64f mEncBits[8];

    Ipp64f mQstep[8];
    Ipp64f mAvCmplx[8];

    Ipp64f mTotAvComplx;
    Ipp64f mTotComplxCnt;

    Ipp64f mComplxSum;
    Ipp64f mTotMeanComplx;


    Ipp64f mCmplxRate;
    Ipp64f mTotTarget;
    Ipp64f mQstepScale;
    Ipp32s mIBitsLoan;
    Ipp32s mLoanLength;
    Ipp32s mLoanBitsPerFrame;

    Ipp64f mQstepBase;
    Ipp32s mQpBase;

    Ipp32s mNumLayers;

    Ipp32s  mQp;
    Ipp32s  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantMin, mQuantPrev, mQuantOffset, mQPprev;
    Ipp32s  mRCfap, mRCqap, mRCbap, mRCq;
    Ipp64f  mRCqa, mRCfa, mRCqa0;
    Ipp64f  mRCfa_short;

    Ipp64f  mRCqa1, mRCfa1;
    Ipp32s  mRCfap1, mRCqap1, mRCbap1;

    mfxI32 mRecodedFrame_encOrder;
    mfxI32 mQuantRecoded;

    Ipp32s  mQuantIprev, mQuantPprev, mQuantBprev;
    Ipp32s  mBitsEncoded;

    Ipp32s mRecode;
    Ipp32s GetInitQP();
    mfxBRCStatus UpdateQuant(Ipp32s bEncoded, Ipp32s totalPicBits, Ipp32s layer = 0, Ipp32s recode = 0);
    mfxBRCStatus UpdateQuantHRD(Ipp32s bEncoded, mfxBRCStatus sts, Ipp32s overheadBits = 0, Ipp32s layer = 0, Ipp32s recode = 0);
    mfxBRCStatus UpdateAndCheckHRD(Ipp32s frameBits, Ipp64f inputBitsPerFrame, Ipp32s recode);
    mfxBRC_HRDState mHRD;
    mfxStatus InitHRD();
    Ipp32s mSceneChange;
    Ipp32s mBitsEncodedP, mBitsEncodedPrev;
    Ipp32s mPoc, mSChPoc;
    Ipp32u mMaxBitrate;
    Ipp64s mBF, mBFsaved;
    Ipp64f mRCfsize;

    Ipp32s mMinQp;
    Ipp32s mMaxQp;
};

} // namespace

#endif // __MFX_H265_BRC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
