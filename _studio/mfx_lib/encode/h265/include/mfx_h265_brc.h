//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_BRC_H__
#define __MFX_H265_BRC_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_set.h"
#include "mfx_h265_enc.h"
#include "umc_mutex.h"
#include "mfxla.h"
#include <vector>

namespace H265Enc {

#define BRC_CLIP(val, minval, maxval) val = Saturate(minval, maxval, val)

#define SetQuantB() \
    mQuantB = ((mQuantP + mQuantPrev + 1) >> 1) + 1; \
    BRC_CLIP(mQuantB, 1, mQuantMax)

#define MFX_H265_BITRATE_SCALE 0
#define  MFX_H265_CPBSIZE_SCALE 2

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


typedef struct _mfxBRC_FrameData
{
    Ipp64f cmplx;
    Ipp64f cmplxInter;
    Ipp32s dispOrder;
    Ipp32s encOrder;
    Ipp32s layer;

    Ipp64f qstep;
    Ipp32s qp;
    Ipp32s encBits;

} mfxBRC_FrameData;


typedef struct _mfxBRC_Params
{
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

struct MbData
{
    mfxU32      intraCost;
    mfxU32      interCost;
    mfxU32      propCost;
    mfxU8       w0;
    mfxU8       w1;
    mfxU16      dist;
    mfxU16      rate;
    mfxU16      lumaCoeffSum[4];
    mfxU8       lumaCoeffCnt[4];
    mfxI16Pair  costCenter0;
    mfxI16Pair  costCenter1;
    struct
    {
        mfxU32  intraMbFlag     : 1;
        mfxU32  skipMbFlag      : 1;
        mfxU32  mbType          : 5;
        mfxU32  reserved0       : 1;
        mfxU32  subMbShape      : 8;
        mfxU32  subMbPredMode   : 8;
        mfxU32  reserved1       : 8;
    };
    mfxI16Pair  mv[2]; // in sig-sag scan
};
struct VmeData
{
    VmeData()
        : used(false)
        , poc(mfxU32(-1))
        , pocL0(mfxU32(-1))
        , pocL1(mfxU32(-1))
        , intraCost(0)
        , interCost(0)
        , propCost(0) { }

    bool                used;
    mfxU32              poc;
    mfxU32              pocL0;
    mfxU32              pocL1;
    mfxU32              encOrder;
    mfxU32              intraCost;
    mfxU32              interCost;
    mfxU32              propCost;
    std::vector   <MbData> mb;
};

template <size_t N>
class Regression
{
public:
    static const mfxU32 MAX_WINDOW = N;

    Regression() {
        Zero(x);
        Zero(y);
    }
    void Reset(mfxU32 size, mfxF64 initX, mfxF64 initY) {
        windowSize = size;
        normX = initX;
        std::fill_n(x, windowSize, initX);
        std::fill_n(y, windowSize, initY);
        sumxx = initX * initX * windowSize;
        sumxy = initX * initY * windowSize;
    }
    void Add(mfxF64 newx, mfxF64 newy) {
        newy = newy / newx * normX;
        newx = normX;
        sumxy += newx * newy - x[0] * y[0];
        sumxx += newx * newx - x[0] * x[0];
        std::copy(x + 1, x + windowSize, x);
        std::copy(y + 1, y + windowSize, y);
        x[windowSize - 1] = newx;
        y[windowSize - 1] = newy;
    }

    mfxF64 GetCoeff() const {
        return sumxy / sumxx;
    }

//protected:
public: // temporary for debugging and dumping
    mfxF64 x[N];
    mfxF64 y[N];
    mfxU32 windowSize;
    mfxF64 normX;
    mfxF64 sumxy;
    mfxF64 sumxx;
};
class BrcIface
{
public:
    virtual ~BrcIface() {};
    virtual mfxStatus Init(const mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode = 1) = 0;
    virtual mfxStatus Reset(mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode = 1) = 0;
    virtual mfxStatus Close() = 0;
    virtual void PreEnc(mfxU32 frameType, std::vector<VmeData *> const & vmeData, mfxU32 encOrder) = 0;    
    virtual Ipp32s GetQP(H265VideoParam *video, Frame *pFrame[], Ipp32s numFrames)=0;
    virtual mfxStatus SetQP(Ipp32s qp, mfxU16 frameType) = 0;
    virtual mfxBRCStatus   PostPackFrame(H265VideoParam *video, Ipp8s sliceQpY, Frame *pFrame, Ipp32s bitsEncodedFrame, Ipp32s overheadBits, Ipp32s recode = 0) =0;
    virtual mfxStatus SetFrameVMEData(const mfxExtLAFrameStatistics*, mfxU32 , mfxU32 ) = 0;
    virtual void GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits) = 0;
    virtual bool IsVMEBRC() = 0;

};
BrcIface * CreateBrc(mfxVideoParam const * video);
class VMEBrc : public BrcIface
{
public:
    virtual ~VMEBrc() { Close(); }

    mfxStatus Init(const mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode = 1);
    mfxStatus Reset(mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode = 1) 
    { 
        return  Init( init,video, enableRecode);
    }

    mfxStatus Close() {  return MFX_ERR_NONE;}
        
    Ipp32s GetQP(H265VideoParam *video, Frame *pFrame[], Ipp32s numFrames);
    mfxStatus SetQP(Ipp32s /* qp */, mfxU16 /* frameType */) { return MFX_ERR_NONE;  }

    void PreEnc(mfxU32 frameType, std::vector<VmeData *> const & vmeData, mfxU32 encOrder);

    mfxBRCStatus   PostPackFrame(H265VideoParam *video, Ipp8s sliceQpY, Frame *pFrame, Ipp32s bitsEncodedFrame, Ipp32s overheadBits, Ipp32s recode = 0)
    {
        Report(pFrame->m_picCodeType, bitsEncodedFrame >> 3, 0, 0, pFrame->m_encOrder, 0, 0); 
        return MFX_ERR_NONE;    
    }
    bool IsVMEBRC()  {return true;}
    mfxU32          Report(mfxU32 frameType, mfxU32 dataLength, mfxU32 userDataLength, mfxU32 repack, mfxU32 picOrder, mfxU32 maxFrameSize, mfxU32 qp); 
    mfxStatus       SetFrameVMEData(const mfxExtLAFrameStatistics *, Ipp32u widthMB, Ipp32u heightMB );
    void            GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits) {*minFrameSizeInBits = 0; *maxFrameSizeInBits = 0;}
        

public:
    struct LaFrameData
    {
        mfxU32  encOrder;
        mfxU32  dispOrder;
        mfxI32  poc;
        mfxI32  deltaQp;
        mfxF64  estRate[52];
        mfxF64  estRateTotal[52];
        mfxU32  interCost;
        mfxU32  intraCost;
        mfxU32  propCost;
        mfxU32  bframe;
        mfxI32  qp;
        mfxU16   layer;
        bool    bNotUsed;
    };

protected:
    mfxU32  m_lookAheadDep;
    mfxU32  m_totNumMb;
    mfxF64  m_initTargetRate;
    mfxF64  m_targetRateMin;
    mfxF64  m_targetRateMax;
    mfxU32  m_framesBehind;
    mfxF64  m_bitsBehind;
    mfxI32  m_curBaseQp;
    mfxI32  m_curQp;
    mfxU16  m_qpUpdateRange;
    bool    m_bPyr;

    std::list <LaFrameData> m_laData;
    Regression<20>   m_rateCoeffHistory[8][52];
    UMC::Mutex    m_mutex;

    mfxI32 GetQP(H265VideoParam &video, Frame *pFrame, mfxI32 *chromaQP );
};
class H265BRC : public BrcIface
{

public:

    H265BRC()
    {
        mIsInit = 0;
        mLowres = 0;
        mBitrate = 0;
        mTotMeanComplx = 0.0;
        mBitsEncodedTotal = 0;
        mBitsEncodedPrev = 0;
        mBitsEncoded = 0;
        mBitsEncodedP = 0;
        mTotPrevCmplx = 0.0;
        mRecodedFrame_encOrder = 0;
        mSChPoc = 0;
        mQstepBase = 0.0;
        mCmplxRate = 0.0;
        mQuantMax = 0;
        mMaxBitrate = 0;
        mLoanLength = 0;
        mEncOrderCoded = -1;
        mRCq = 0;
        mRCbap = 0;
        mRCqa = 0.0;
        mRCqa0 = 0.0;
        mRCqap = 0;
        mRCfa = 0;
        mRCfap = 0;
        mRCfa_short = 0.0;
        mQuantOffset = 0;
        mSceneChange = 0;
        mSceneNum = 0;
        mQp = 0;
        mQPprev = 0;
        mBitsDesiredTotal = 0;
        mBitsDesiredFrame = 0;
        mTotTarget = 0.0;
        mPrevCmplxIP = 0.0;
        mPrevBitsIP = 0;
        mPrevQpIP = 0;
        mPrevQstepIP = 0;
        mMinQp = 0;
        mMaxQp = 999;
        mQuantMin = 0;
        mComplxSum = 0.0;
        mPredBufFulness = 0;
        mTotAvComplx = 0.0;
        mTotComplxCnt = 0;
        mRealPredFullness = 0;
        mFramerate = 0.0;
        mQuantI = 0;
        mQuantP = 0;
        mQuantB = 0;
        mQuantPrev = 0;
        mQuantIprev = 0;
        mQuantPprev = 0;
        mQuantBprev = 0;
        mQuantUpdated = 0;
        mRecode = 0;
        mQuantRecoded = 0;
        mQstepScale = 0.0;
        mQstepScale0 = 0.0;
        mTotalDeviation = 0;
        mLoanBitsPerFrame = 0;
        mBF = 0;
        mBFsaved = 0;
        mPicType = MFX_FRAMETYPE_I;
        mPoc = 0;
        mNumLayers = 1;
        mRCMode = 0;
        Zero(mHRD);
        Zero(mParams);
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

    mfxBRCStatus PostPackFrame(H265VideoParam *video, Ipp8s sliceQpY, Frame *pFrame, Ipp32s bitsEncodedFrame, Ipp32s overheadBits, Ipp32s recode = 0);

    Ipp32s GetQP(H265VideoParam *video, Frame *pFrame[], Ipp32s numFrames);
    Ipp32s PredictFrameSize(H265VideoParam *video, Frame *frames[], Ipp32s qp, mfxBRC_FrameData *refFrData);
    void SetMiniGopData(Frame *frames[], Ipp32s numFrames);
    void UpdateMiniGopData(Frame *frame,  Ipp8s qp);

    mfxStatus SetQP(Ipp32s qp, Ipp16u frameType);

    mfxStatus GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode = 0);

    void GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits);
    void PreEnc(mfxU32 /* frameType */, std::vector<VmeData *> const & /* vmeData */, mfxU32 /* encOrder */) {}
    virtual mfxStatus SetFrameVMEData(const mfxExtLAFrameStatistics*, mfxU32 , mfxU32 )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    bool IsVMEBRC()  {return false;}


protected:
    mfxBRC_Params mParams;
    Ipp8u mLowres;// 0 - use origin resolution, 1 - lowres
    bool   mIsInit;
    Ipp32u mBitrate;
    Ipp64f mFramerate;
    Ipp16u mRCMode;
    Ipp16u mQuantUpdated;

    Ipp32s  mBitsDesiredFrame;
    Ipp64s  mBitsEncodedTotal, mBitsDesiredTotal;

    Ipp32u  mPicType;

    Ipp64s mTotalDeviation;
    Ipp64f mEstCoeff[8];
    Ipp64f mEstCnt[8];
    Ipp64f mLayerTarget[8];

    std::vector<mfxBRC_FrameData> mMiniGopFrames;
    Ipp32s mEncOrderCoded;

    Ipp64f mPrevCmplxLayer[8];
    Ipp32s mPrevBitsLayer[8];
    Ipp64f mPrevQstepLayer[8];
    Ipp32s mPrevQpLayer[8];

    Ipp64f mPrevCmplxIP;
    Ipp32s mPrevBitsIP;
    Ipp64f mPrevQstepIP;
    Ipp32s mPrevQpIP;
    Ipp32s mSceneNum;


    Ipp64f mTotAvComplx;
    Ipp64f mTotComplxCnt;
    Ipp64f mTotPrevCmplx;

    Ipp64f mComplxSum;
    Ipp64f mTotMeanComplx;
    Ipp64f mCmplxRate;
    Ipp64f mTotTarget;
    
    Ipp64f mQstepScale;
    Ipp64f mQstepScale0;
    Ipp32s mLoanLength;
    Ipp32s mLoanBitsPerFrame;

    //Ipp32s mLoanLengthP;
    //Ipp32s mLoanBitsPerFrameP;

    Ipp64f mQstepBase;

    Ipp32s mPredBufFulness;
    Ipp32s mRealPredFullness;

    Ipp32s mNumLayers;

    Ipp32s  mQp;
    Ipp32s  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantMin, mQuantPrev, mQuantOffset, mQPprev;
    Ipp32s  mRCfap, mRCqap, mRCbap, mRCq;
    Ipp64f  mRCqa, mRCfa, mRCqa0;
    Ipp64f  mRCfa_short;

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

    Ipp32s mMinQp;
    Ipp32s mMaxQp;
};

} // namespace

#endif // __MFX_H265_BRC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
