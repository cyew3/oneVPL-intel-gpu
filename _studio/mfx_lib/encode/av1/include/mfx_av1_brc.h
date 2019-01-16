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

#ifndef __MFX_AV1_BRC_H__
#define __MFX_AV1_BRC_H__

#include "mfx_av1_defs.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_enc.h"
#include "umc_mutex.h"
#include "mfxla.h"
#include <vector>

namespace AV1Enc {

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

typedef int32_t mfxBRCStatus;

struct mfxBRC_HRDState
{
    uint32_t bufSize;
    double bufFullness;
    double prevBufFullness;
    double maxBitrate;
    double inputBitsPerFrame;
    int32_t frameNum;
    int32_t minFrameSize;
    int32_t maxFrameSize;
    int32_t underflowQuant;
    int32_t overflowQuant;
};


struct mfxBRC_FrameData
{
    double cmplx;
    double cmplxInter;
    int32_t dispOrder;
    int32_t encOrder;
    int32_t layer;

    double qstep;
    int32_t qp;
    int32_t encBits;

};


struct mfxBRC_Params
{
    int32_t  BRCMode;
    int32_t  targetBitrate;

    int32_t  HRDInitialDelayBytes;
    int32_t  HRDBufferSizeBytes;
    int32_t  maxBitrate;

    uint32_t  frameRateExtD;
    uint32_t  frameRateExtN;

    uint16_t width;
    uint16_t height;
    uint16_t chromaFormat;
    int32_t bitDepthLuma;

};

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
    virtual mfxStatus Init(const mfxVideoParam *init, AV1VideoParam &video, int32_t enableRecode = 1) = 0;
    virtual mfxStatus Reset(mfxVideoParam *init, AV1VideoParam &video, int32_t enableRecode = 1) = 0;
    virtual mfxStatus Close() = 0;
    virtual void PreEnc(mfxU32 frameType, std::vector<VmeData *> const & vmeData, mfxU32 encOrder) = 0;
    virtual int32_t GetQP(AV1VideoParam *video, Frame *pFrame[], int32_t numFrames)=0;
    virtual mfxStatus SetQP(int32_t qp, mfxU16 frameType) = 0;
    virtual mfxBRCStatus   PostPackFrame(AV1VideoParam *video, uint8_t sliceQpY, Frame *pFrame, int32_t bitsEncodedFrame, int32_t overheadBits, int32_t recode = 0) =0;
    virtual mfxStatus SetFrameVMEData(const mfxExtLAFrameStatistics*, mfxU32 , mfxU32 ) = 0;
    virtual void GetMinMaxFrameSize(int32_t *minFrameSizeInBits, int32_t *maxFrameSizeInBits) = 0;
    virtual bool IsVMEBRC() = 0;

};
BrcIface * CreateBrc(mfxVideoParam const * video);
class VMEBrc : public BrcIface
{
public:
    virtual ~VMEBrc() { Close(); }

    mfxStatus Init(const mfxVideoParam *init, AV1VideoParam &video, int32_t enableRecode = 1);
    mfxStatus Reset(mfxVideoParam *init, AV1VideoParam &video, int32_t enableRecode = 1)
    {
        return  Init( init,video, enableRecode);
    }

    mfxStatus Close() {  return MFX_ERR_NONE;}

    int32_t GetQP(AV1VideoParam *video, Frame *pFrame[], int32_t numFrames);
    mfxStatus SetQP(int32_t /* qp */, mfxU16 /* frameType */) { return MFX_ERR_NONE;  }

    void PreEnc(mfxU32 frameType, std::vector<VmeData *> const & vmeData, mfxU32 encOrder);

    mfxBRCStatus   PostPackFrame(AV1VideoParam *video, uint8_t sliceQpY, Frame *pFrame, int32_t bitsEncodedFrame, int32_t overheadBits, int32_t recode = 0)
    {
        Report(pFrame->m_picCodeType, bitsEncodedFrame >> 3, 0, 0, pFrame->m_encOrder, 0, 0);
        return MFX_ERR_NONE;
    }
    bool IsVMEBRC()  {return true;}
    mfxU32          Report(mfxU32 frameType, mfxU32 dataLength, mfxU32 userDataLength, mfxU32 repack, mfxU32 picOrder, mfxU32 maxFrameSize, mfxU32 qp);
    mfxStatus       SetFrameVMEData(const mfxExtLAFrameStatistics *, uint32_t widthMB, uint32_t heightMB );
    void            GetMinMaxFrameSize(int32_t *minFrameSizeInBits, int32_t *maxFrameSizeInBits) {*minFrameSizeInBits = 0; *maxFrameSizeInBits = 0;}


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

    mfxI32 GetQP(AV1VideoParam &video, Frame *pFrame, mfxI32 *chromaQP );
};
class AV1BRC : public BrcIface
{

public:

    AV1BRC()
    {
        mIsInit = 0;
        mLowres = 0;
    }
    virtual ~AV1BRC()
    {
        Close();
    }

    // Initialize with specified parameter(s)
    mfxStatus Init(const mfxVideoParam *init, AV1VideoParam &video, int32_t enableRecode = 1);

    mfxStatus Close();

    mfxStatus Reset(mfxVideoParam *init, AV1VideoParam &video, int32_t enableRecode = 1);
    mfxStatus SetParams(const mfxVideoParam *init, AV1VideoParam &video);
    mfxStatus GetParams(mfxVideoParam *init);

    mfxBRCStatus PostPackFrame(AV1VideoParam *video, uint8_t sliceQpY, Frame *pFrame, int32_t bitsEncodedFrame, int32_t overheadBits, int32_t recode = 0);

    int32_t GetQP(AV1VideoParam *video, Frame *pFrame[], int32_t numFrames);
    int32_t PredictFrameSize(AV1VideoParam *video, Frame *frames[], int32_t qp, mfxBRC_FrameData *refFrData);
    void SetMiniGopData(Frame *frames[], int32_t numFrames);
    void UpdateMiniGopData(Frame *frame,  int8_t qp);

    mfxStatus SetQP(int32_t qp, uint16_t frameType);

    mfxStatus GetInitialCPBRemovalDelay(uint32_t *initial_cpb_removal_delay, int32_t recode = 0);

    void GetMinMaxFrameSize(int32_t *minFrameSizeInBits, int32_t *maxFrameSizeInBits);
    void PreEnc(mfxU32 /* frameType */, std::vector<VmeData *> const & /* vmeData */, mfxU32 /* encOrder */) {}
    virtual mfxStatus SetFrameVMEData(const mfxExtLAFrameStatistics*, mfxU32 , mfxU32 )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    bool IsVMEBRC()  {return false;}


protected:
    mfxBRC_Params mParams;
    uint8_t mLowres;// 0 - use origin resolution, 1 - lowres
    bool   mIsInit;
    uint32_t mBitrate;
    double mFramerate;
    uint16_t mRCMode;
    uint16_t mQuantUpdated;

    int32_t  mBitsDesiredFrame;
    int64_t  mBitsEncodedTotal, mBitsDesiredTotal;

    uint32_t  mPicType;

    int64_t mTotalDeviation;
    double mEstCoeff[8];
    double mEstCnt[8];
    double mLayerTarget[8];

    std::vector<mfxBRC_FrameData> mMiniGopFrames;
    int32_t mEncOrderCoded;

    double mPrevCmplxLayer[8];
    int32_t mPrevBitsLayer[8];
    double mPrevQstepLayer[8];
    int32_t mPrevQpLayer[8];

    double mPrevCmplxIP;
    int32_t mPrevBitsIP;
    double mPrevQstepIP;
    int32_t mPrevQpIP;
    int32_t mSceneNum;


    double mTotAvComplx;
    double mTotComplxCnt;
    double mTotPrevCmplx;

    double mComplxSum;
    double mTotMeanComplx;
    double mCmplxRate;
    double mTotTarget;

    double mQstepScale;
    double mQstepScale0;
    int32_t mLoanLength;
    int32_t mLoanBitsPerFrame;

    //int32_t mLoanLengthP;
    //int32_t mLoanBitsPerFrameP;

    double mQstepBase;

    int32_t mPredBufFulness;
    int32_t mRealPredFullness;

    int32_t mNumLayers;

    int32_t  mQp;
    int32_t  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantMin, mQuantPrev, mQuantOffset, mQPprev;
    int32_t  mRCfap, mRCqap, mRCbap, mRCq;
    double  mRCqa, mRCfa, mRCqa0;
    double  mRCfa_short;

    mfxI32 mRecodedFrame_encOrder;
    mfxI32 mQuantRecoded;

    int32_t  mQuantIprev, mQuantPprev, mQuantBprev;
    int32_t  mBitsEncoded;

    int32_t mRecode;
    int32_t GetInitQP();
    mfxBRCStatus UpdateQuant(int32_t bEncoded, int32_t totalPicBits, int32_t layer = 0, int32_t recode = 0);
    mfxBRCStatus UpdateQuantHRD(int32_t bEncoded, mfxBRCStatus sts, int32_t overheadBits = 0, int32_t layer = 0, int32_t recode = 0);
    mfxBRCStatus UpdateAndCheckHRD(int32_t frameBits, double inputBitsPerFrame, int32_t recode);
    mfxBRC_HRDState mHRD;
    mfxStatus InitHRD();
    int32_t mSceneChange;
    int32_t mBitsEncodedP, mBitsEncodedPrev;
    int32_t mPoc, mSChPoc;
    uint32_t mMaxBitrate;
    int64_t mBF, mBFsaved;

    int32_t mMinQp;
    int32_t mMaxQp;
};

} // namespace

#endif // __MFX_AV1_BRC_H__

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
