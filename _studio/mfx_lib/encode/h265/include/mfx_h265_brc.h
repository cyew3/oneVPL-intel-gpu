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

typedef mfxI32 mfxBRCStatus;

typedef struct _mfxBRC_HRDState
{
    mfxU32 bufSize;
    mfxF64 bufFullness;
    mfxF64 prevBufFullness;
    mfxF64 maxBitrate;
    mfxF64 inputBitsPerFrame;
    mfxI32 frameNum;
    mfxI32 minFrameSize;
    mfxI32 maxFrameSize;
    mfxI32 underflowQuant;
    mfxI32 overflowQuant;
} mfxBRC_HRDState;


typedef struct _mfxBRC_Params
{
    // ???
    // either need this to with GetParams(mfxBRC_Params*) or use GetParams(mfxVideoParam*) to report BRC params to the encoder
    mfxI32  BRCMode;
    mfxI32  targetBitrate;

    mfxI32  HRDInitialDelayBytes;
    mfxI32  HRDBufferSizeBytes;
    mfxI32  maxBitrate;

    mfxU32  frameRateExtD;
    mfxU32  frameRateExtN;

    mfxU16 width;
    mfxU16 height;
    mfxU16 chromaFormat;
    mfxI32 bitDepthLuma;

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
    mfxStatus Init(const mfxVideoParam *init, H265VideoParam &video, mfxI32 enableRecode = 1);

    mfxStatus Close();

    mfxStatus Reset(mfxVideoParam *init, H265VideoParam &video, mfxI32 enableRecode = 1);
    mfxStatus SetParams(const mfxVideoParam *init, H265VideoParam &video);
    mfxStatus GetParams(mfxVideoParam *init);

    //mfxBRCStatus PostPackFrame(mfxU16 picType, mfxI32 bitsEncodedFrame, mfxI32 overheadBits, mfxI32 recode = 0, mfxI32 poc = 0);
    mfxBRCStatus PostPackFrame(H265VideoParam &video, Ipp8s sliceQpY, H265Frame *pFrame, mfxI32 bitsEncodedFrame, mfxI32 overheadBits, mfxI32 recode = 0);

    mfxI32 GetQP(H265VideoParam &video, H265Frame* frames[], Ipp32s framesCount);
    mfxStatus SetQP(mfxI32 qp, mfxU16 frameType);

    mfxStatus GetInitialCPBRemovalDelay(mfxU32 *initial_cpb_removal_delay, mfxI32 recode = 0);

    void GetMinMaxFrameSize(mfxI32 *minFrameSizeInBits, mfxI32 *maxFrameSizeInBits);


protected:
    mfxBRC_Params mParams;
    bool   mIsInit;
    mfxU32 mBitrate;
    mfxF64 mFramerate;
    mfxU16 mRCMode;
    mfxU16 mQuantUpdated;

    mfxI32  mBitsDesiredFrame;
    mfxI64  mBitsEncodedTotal, mBitsDesiredTotal;

    mfxU32  mPicType;

    mfxI8  mBpyramidLayers[129];
    mfxI32 mBpyramidLayersLen;
    mfxI32 mNumLayers;
    mfxI8  mDeltaQp[8];
    mfxI8  mQp[8];
    mfxI32 *mFrameSizeHist;
    


    mfxI32  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantMin, mQuantPrev, mQuantOffset, mQPprev;
    mfxI32  mRCfap, mRCqap, mRCbap, mRCq;
    mfxF64  mRCqa, mRCfa, mRCqa0;
    mfxF64  mRCfa_short;

    mfxF64  mRCqa1, mRCfa1;
    mfxI32  mRCfap1, mRCqap1, mRCbap1;

    mfxI32  mQuantIprev, mQuantPprev, mQuantBprev;
    mfxI32  mBitsEncoded;
    mfxU16  mPictureFlags, mPictureFlagsPrev;

    mfxI32 mRecode;
    mfxI32 GetInitQP();
    mfxBRCStatus UpdateQuant(mfxI32 bEncoded, mfxI32 totalPicBits);
//    mfxBRCStatus UpdateQuant_ScCh(mfxI32 bEncoded, mfxI32 totalPicBits);
    mfxBRCStatus UpdateQuantHRD(mfxI32 bEncoded, mfxBRCStatus sts, mfxI32 overheadBits = 0);
    mfxBRCStatus UpdateAndCheckHRD(mfxI32 frameBits, mfxF64 inputBitsPerFrame, mfxI32 recode);
    mfxBRC_HRDState mHRD;
    mfxStatus InitHRD();
//    mfxU64 mMaxBitsPerPic, mMaxBitsPerPicNot0;
    mfxI32 mSceneChange;
    mfxI32 mBitsEncodedP, mBitsEncodedPrev;
    mfxI32 mPoc, mSChPoc;
    mfxU32 mMaxBitrate;
    mfxI64 mBF, mBFsaved;
    mfxF64 mRCfsize;

    mfxI32 mMinQp;
    mfxI32 mMaxQp;
//    mfxI32 mScChFrameCnt;
//    mfxI32 mScChLength;
};

} // namespace

#endif // __MFX_H265_BRC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
