//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_BRC)

#ifndef _MFX_H264_ENC_BRC_H_
#define _MFX_H264_ENC_BRC_H_

#include "mfxvideo++int.h"
#include "mfxvideo.h"
//#include "umc_h264_avbr.h"
#include "mfx_umc_alloc_wrapper.h"


#define SetQuantB() \
  mQuantB = ((mQuantP + mQuantPrev) * 563 >> 10) + 1; \
  h264_Clip(mQuantB, 1, mQuantMax)

#define h264_Clip(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r
#define h264_ClipL(a, l) if (a < (l)) a = l
#define h264_ClipR(a, r) if (a > (r)) a = r
#define h264_Abs(a) ((a) >= 0 ? (a) : -(a))

enum
{
  H264_BRC_OK                      = 0x0,
  H264_BRC_ERR_BIG_FRAME           = 0x1,
  H264_BRC_BIG_FRAME               = 0x2,
  H264_BRC_ERR_SMALL_FRAME         = 0x4,
  H264_BRC_SMALL_FRAME             = 0x8,
  H264_BRC_NOT_ENOUGH_BUFFER       = 0x10
};


typedef struct sHRDState_H264
{
  mfxI32 bufSize;
  mfxI32 bufFullness;
  mfxI32 prevBufFullness;
  mfxU32 maxBitrate;
  mfxI32 inputBitsPerFrame;
  mfxI32 maxInputBitsPerFrame;
  mfxU16 frameCnt;
  mfxU16 prevFrameCnt;
  mfxI32 minFrameSize;
  mfxI32 maxFrameSize;
} HRDState_H264;


class MFXVideoRcH264 : public VideoBRC
{
//    UMC_H264_ENCODER::EnumPicCodType m_CurPicType;
    Ipp8u*                      m_pBRCBuffer;
    mfxHDL                      m_BRCID;
    VideoCORE*                  m_pMFXCore;

public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    MFXVideoRcH264(VideoCORE *core, mfxStatus *status) : VideoBRC(), mHRD()
    {
        m_pMFXCore = core;
        m_pBRCBuffer = NULL;
        m_BRCID      = 0;
        *status = MFX_ERR_NONE;
        m_curPicInitQpMinus26 = NO_INIT_QP;
        mQP = 0;
        mBitsDesiredFrame = 0;
        mQuantP = 0;
        mQuantBprev = 0;
        mQuantB = 0;
        mRCfap = 0;
        mRCqa = 0;
        mRCfa = 0;
        mQuantMax = 0;
        mBitsDesiredTotal = 0;
        mRCMode = 0;
        mRCbap = 0;
        mQuantPprev = 0;
        mRCqap = 0;
        mFramerate = 0;
        mQuantIprev = 0;
        mRCq = 0;
        mRCqb = 0;
        mQuantI = 0;
        mBitrate = 0;
        mQuantPrev = 0;
        mQPprev = 0;
        mBitsEncoded = 0;
        mBitsEncodedTotal = 0;
        mQuantOffset = 0;
        mQuantUpdated = 0;
        mBitDepth = 0;
        mFrameType = 0;
        mPrevDataLength = 0;
    }
    virtual ~MFXVideoRcH264(void);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus FrameENCUpdate(mfxFrameCUC *cuc);
    virtual mfxStatus FramePAKRefine(mfxFrameCUC *cuc);
    virtual mfxStatus FramePAKRecord(mfxFrameCUC *cuc);

/*
    virtual mfxStatus SliceEncUpdate(mfxFrameCUC *cuc);
    virtual mfxStatus SlicePakRefine(mfxFrameCUC *cuc);
    virtual mfxStatus SlicePakRecord(mfxFrameCUC *cuc);
*/

protected:
    mfxStatus SetUniformQPs(mfxFrameCUC *cuc, mfxU8 qp);
    mfxI32 GetInitQP(mfxI32 fPixels, mfxU8 chromaFormat, mfxI32 alpha);
    mfxStatus InitHRD(mfxVideoParam *par);
    mfxI32 UpdateAndCheckHRD(mfxI32 frameBits);
    void UpdateQuant(mfxI32 bEncoded);
    void UpdateQuantHRD(mfxI32 bEncoded, mfxI32 sts);

    mfxF64  mFramerate;
    mfxI32  mRCMode;                 // rate control mode CBR/VBR

    mfxI32  mBitDepth;
    mfxU32  mBitrate;
    mfxI32  mBitsDesiredFrame;
    mfxI32  mBitsEncoded;
    mfxI64  mBitsEncodedTotal, mBitsDesiredTotal;
    mfxU8   *mRCqb;
    mfxI32  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantPrev, mQuantOffset;
    mfxI32  mQuantIprev, mQuantPprev, mQuantBprev;
    mfxI32  mQP, mQPprev;
//    mfxI32  mMethod;
    mfxI32  mRCfap, mRCqap, mRCbap;
    mfxI32  mRCq;
    mfxF64  mRCqa, mRCfa;

    mfxU32 mPrevDataLength;
    HRDState_H264 mHRD;
    mfxU16 mFrameType;
    mfxU8 mQuantUpdated;

    static const mfxI8 NO_INIT_QP = 0x7f;
    mfxI8 m_curPicInitQpMinus26;
//    mfxI32 mBitsWanted;

};


#endif //_MFX_H264_ENC_BRC_H_
#endif //MFX_ENABLE_H264_VIDEO_ENCODER
