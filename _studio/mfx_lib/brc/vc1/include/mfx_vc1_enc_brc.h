//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VC1_VIDEO_BRC)

#ifndef _MFX_VC1_ENC_BRC_H_
#define _MFX_VC1_ENC_BRC_H_

#include "mfxvideo++int.h"
#include "mfxvideo.h"
#include "mfx_memory_allocator.h"

#define VC1_MIN_QUANT      1
#define VC1_MAX_QUANT      31
#define vc1_Clip(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r
#define vc1_ClipL(a, l) if (a < (l)) a = l

#define VC1_RATIO_ILIM     1.9
#define VC1_RATIO_PLIM     1.5
#define VC1_RATIO_BLIM     1.45

#define VC1_MIN_RATIO_HALF     0.8
#define VC1_RATIO_MIN 0.6

#define VC1_RATIO_IMIN     0.4
#define VC1_RATIO_PMIN     0.4
#define VC1_RATIO_BMIN     1.4

#define VC1_GOOD_COMPRESSION 0.90
#define VC1_POOR_REF_FRAME   0.75
#define VC1_BUFFER_OVERFLOW 1.1

#define VC1_P_SIZE         0.8
#define VC1_B_SIZE         0.6

enum
{
  VC1_BRC_OK                      = 0x0,
  VC1_BRC_ERR_BIG_FRAME           = 0x1,
  VC1_BRC_BIG_FRAME               = 0x2,
  VC1_BRC_ERR_SMALL_FRAME         = 0x4,
  VC1_BRC_SMALL_FRAME             = 0x8,
  VC1_BRC_NOT_ENOUGH_BUFFER       = 0x10
};

typedef struct sHRDState
{
  mfxI32 bufSize;
  mfxI32 bufFullness;
  mfxI32 bufInitFullness;
  mfxI32 prevBufFullness;
  mfxI32 maxBitrate;
  mfxI32 inputBitsPerFrame;
  mfxI32 maxInputBitsPerFrame;
  mfxU16 frameCnt;
  mfxU16 prevFrameCnt;
  mfxI32 minFrameSize;
  mfxI32 maxFrameSize;

//  Ipp32s vc1_decPrevFullness;
//  Ipp32s vc1_decBufferSize;
//  Ipp32s vc1_BitRate;
//  Ipp64f vc1_FrameRate;
//  Ipp32s vc1_decInitFull;
//  Ipp32s vc1_dec_delay;
//  Ipp32s vc1_enc_delay;
//  Ipp32s vc1_Prev_enc_delay;

} HRDState;

class MFXVideoRcVc1 : public VideoBRC
{
    Ipp8u*                      m_pBRCBuffer;
    mfxHDL                      m_BRCID;
    VideoCORE*                  m_pMFXCore;

public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    MFXVideoRcVc1(VideoCORE *core, mfxStatus *status): VideoBRC()
    {
        m_pMFXCore = core;
        m_pBRCBuffer = NULL;
        m_BRCID      = 0;
        *status = MFX_ERR_NONE;
    }
    virtual mfxStatus FrameENCUpdate(mfxFrameCUC *) {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus FramePAKRefine(mfxFrameCUC *) {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus FramePAKRecord(mfxFrameCUC *) {return MFX_ERR_UNSUPPORTED;}


    virtual ~MFXVideoRcVc1(void);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus FrameEncUpdate(mfxFrameCUC *cuc);
    virtual mfxStatus FramePakRefine(mfxFrameCUC *cuc);
    virtual mfxStatus FramePakRecord(mfxFrameCUC *cuc);

    virtual mfxStatus SliceEncUpdate(mfxFrameCUC *cuc);
    virtual mfxStatus SlicePakRefine(mfxFrameCUC *cuc);
    virtual mfxStatus SlicePakRecord(mfxFrameCUC *cuc);
protected:
    mfxU16 GetPicType(mfxFrameParam *frameParam);
    mfxStatus SetUniformQPs(mfxFrameCUC *cuc, mfxU8 qp2, mfxU8 uniformQP);
    mfxI32 CheckFrameCompression(mfxU16 picType, mfxI32 currSize);
    void CompleteFrame(mfxU16 picType, mfxU32 currSize);
    void SetGOPParams();
    void CorrectGOPQuant(mfxU16 picType, mfxF32 ratio);
    mfxStatus InitHRD(mfxVideoParam *par);
    mfxI32 MFXVideoRcVc1::UpdateAndCheckHRD(mfxI32 frameBits);


    mfxU8  mRCMode;
    mfxU16 mPicType;
    mfxF64 mFramerate;
    mfxU32 mBitrate;

    Ipp32u mGOPLength;         //number of frames in GOP
    mfxU32 mBFrLength;         //number of successive B frames
    mfxI32 mSizeAberration;    //coded size aberration
    mfxI32 mIGOPSize;         //I frame size in current GOP
    mfxI32 mPGOPSize;         //P frames size in current GOP
    mfxI32 mBGOPSize;         //B frames size in current GOP
    mfxI32 mIdealFrameSize;    //"ideal" frame size

    mfxF32 mIPsize;           //I/P frame size ratio
    mfxF32 mIBsize;           //I/B frame size ratio

    mfxU8 mIQuant;
    mfxU8 mPQuant;
    mfxU8 mBQuant;
    mfxU8 mIHalf;
    mfxU8 mPHalf;
    mfxU8 mBHalf;
    mfxU8 mLimIQuant;
    mfxU8 mLimPQuant;
    mfxU8 mLimBQuant;
    mfxU8 mPQIndex;
    mfxU8 mHalfPQ;
    mfxU8 mFailPQuant;
    mfxU8 mFailBQuant;
    mfxU32 mPoorRefFrame;
    mfxI32 mGOPHalfFlag;       //less than half of GOP was coded
    mfxI32 mFailGOP;

    mfxI32 mNeedISize;        //wishful coded I frame size
    mfxI32 mNeedPSize;        //wishful coded I frame size
    mfxI32 mNeedBSize;        //wishful coded I frame size

    mfxU32 mFrameSize;
    mfxU32 mPrevDataLength;

    mfxI32 mCurrISize;         //last coded I frame size
    mfxI32 mCurrPSize;         //last coded P frame size
    mfxI32 mCurrBSize;         //last coded B frame size

    mfxU32 mCurrFrameInGOP;    //number of coded frames in current GOP
    mfxI32 mGOPSize;           //"ideal" GOP size
    mfxI32 mCurrGOPSize;       //current GOP size
    mfxI32 mNextGOPSize;       //planned GOP size

    mfxI32 mPNum;              //number P frame in GOP
    mfxI32 mBNum;              //number B frame in GOP
    mfxI32 mCurrGOPLength;
    mfxI32 mCurrINum;          //number coded I frame in current GOP
    mfxI32 mCurrPNum;          //number coded P frame in current GOP
    mfxI32 mCurrBNum;          //number coded B frame in current GOP

    mfxI32 mAveragePQuant;
    mfxI32 mAverageIQuant;
    mfxI32 mAverageBQuant;

    HRDState mHRD;
    mfxU8 mFrameCompleted;

//    mfxU8 mProfile;
//    mfxU8 mLevel;
};

#endif //_MFX_VC1_ENC_BRC_H_
#endif //MFX_ENABLE_VC1_VIDEO_BRC
