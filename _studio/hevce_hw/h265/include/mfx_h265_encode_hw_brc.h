//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H265_BRC_HW_H__
#define __MFX_H265_BRC_HW_H__

#include "mfx_common.h"
#include "mfxla.h"
#include <vector>
#include "mfx_h265_encode_hw_utils.h"

#define NEW_BRC
#ifdef NEW_BRC
#include "mfxbrc.h"
#endif

namespace MfxHwH265Encode
{

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

typedef mfxI32 mfxBRCStatus;


class BrcIface
{
public:
    virtual ~BrcIface() {};
    virtual mfxStatus Init(MfxVideoParam &video, mfxI32 enableRecode = 1) = 0;
    virtual mfxStatus Reset(MfxVideoParam &video, mfxI32 enableRecode = 1) = 0;
    virtual mfxStatus Close() = 0;
    virtual void PreEnc(mfxU32 frameType, std::vector<VmeData *> const & vmeData, mfxU32 encOrder) = 0;    
    virtual mfxI32 GetQP(MfxVideoParam &video, Task &task)=0;
    virtual mfxStatus SetQP(mfxI32 qp, mfxU16 frameType, bool bLowDelay) = 0;
    virtual mfxBRCStatus   PostPackFrame(MfxVideoParam &video, Task &task, mfxI32 bitsEncodedFrame, mfxI32 overheadBits, mfxI32 recode = 0) =0;
    virtual mfxStatus SetFrameVMEData(const mfxExtLAFrameStatistics*, mfxU32 , mfxU32 ) = 0;
    virtual void GetMinMaxFrameSize(mfxI32 *minFrameSizeInBits, mfxI32 *maxFrameSizeInBits) = 0;
    virtual bool IsVMEBRC() = 0;

};

BrcIface * CreateBrc(MfxVideoParam &video);

class VMEBrc : public BrcIface
{
public:
    virtual ~VMEBrc() { Close(); }

    mfxStatus Init( MfxVideoParam &video, mfxI32 enableRecode = 1);
    mfxStatus Reset(MfxVideoParam &video, mfxI32 enableRecode = 1) 
    { 
        return  Init( video, enableRecode);
    }

    mfxStatus Close() {  return MFX_ERR_NONE;}
        
    mfxI32 GetQP(MfxVideoParam &video, Task &task);
    mfxStatus SetQP(mfxI32 /* qp */, mfxU16 /* frameType */,  bool /*bLowDelay*/) { return MFX_ERR_NONE;  }

    void PreEnc(mfxU32 frameType, std::vector<VmeData *> const & vmeData, mfxU32 encOrder);

    mfxBRCStatus   PostPackFrame(MfxVideoParam &video, Task &task, mfxI32 bitsEncodedFrame, mfxI32 overheadBits, mfxI32 recode = 0)
    {
        recode;
        overheadBits;
        video;
        Report(task.m_frameType, bitsEncodedFrame >> 3, 0, 0, task.m_eo, 0, 0); 
        return MFX_ERR_NONE;    
    }
    bool IsVMEBRC()  {return true;}
    mfxU32          Report(mfxU32 frameType, mfxU32 dataLength, mfxU32 userDataLength, mfxU32 repack, mfxU32 picOrder, mfxU32 maxFrameSize, mfxU32 qp); 
    mfxStatus       SetFrameVMEData(const mfxExtLAFrameStatistics *, mfxU32 widthMB, mfxU32 heightMB );
    void            GetMinMaxFrameSize(mfxI32 *minFrameSizeInBits, mfxI32 *maxFrameSizeInBits) {*minFrameSizeInBits = 0; *maxFrameSizeInBits = 0;}
        

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

    std::list <LaFrameData> m_laData;
    Regression<20>   m_rateCoeffHistory[52];
    UMC::Mutex    m_mutex;

};

#ifndef NEW_BRC
enum eMfxBRCRecode
{
    MFX_BRC_RECODE_NONE           = 0,
    MFX_BRC_RECODE_QP             = 1,
    MFX_BRC_RECODE_PANIC          = 2,
    MFX_BRC_RECODE_EXT_QP         = 3,
    MFX_BRC_RECODE_EXT_PANIC      = 4,
    MFX_BRC_EXT_FRAMESKIP         = 16
};

struct HRDState
{
    mfxF64 bufFullness;
    mfxF64 prevBufFullness;
    mfxI32 frameNum;
    mfxI32 minFrameSize;
    mfxI32 maxFrameSize;
    mfxI32 underflowQuant;
    mfxI32 overflowQuant;
};
struct BRCParams
{
    mfxU16 rateControlMethod;
    mfxU32 bufferSizeInBytes;
    mfxU32 initialDelayInBytes;
    mfxU32 targetbps;
    mfxU32 maxbps;
    mfxF64 frameRate;
    mfxU16 width;
    mfxU16 height;
    mfxU16 chromaFormat;
    mfxU16 bitDepthLuma;
    mfxF64 inputBitsPerFrame;
    mfxU16 gopPicSize;
    mfxU16 gopRefDist;

};

class H265BRC : public BrcIface
{

public:

    H265BRC()
    {
        m_IsInit = false;
        memset(&m_par, 0, sizeof(m_par));
        memset(&m_hrdState, 0, sizeof(m_hrdState));
        ResetParams();
    }
    virtual ~H265BRC()
    {
        Close();
    }




    virtual mfxStatus   Init(MfxVideoParam &video, mfxI32 enableRecode = 1);
    virtual mfxStatus   Close();
    virtual mfxStatus   Reset(MfxVideoParam &video, mfxI32 enableRecode = 1);
    virtual mfxBRCStatus PostPackFrame(MfxVideoParam &video, Task &task, mfxI32 bitsEncodedFrame, mfxI32 overheadBits, mfxI32 recode = 0);
    virtual mfxI32      GetQP(MfxVideoParam &video, Task &task);
    virtual mfxStatus   SetQP(mfxI32 qp, Ipp16u frameType, bool bLowDelay);
    //virtual mfxStatus   GetInitialCPBRemovalDelay(mfxU32 *initial_cpb_removal_delay, mfxI32 recode = 0);
    virtual void        GetMinMaxFrameSize(mfxI32 *minFrameSizeInBits, mfxI32 *maxFrameSizeInBits);


    virtual void        PreEnc(mfxU32 /* frameType */, std::vector<VmeData *> const & /* vmeData */, mfxU32 /* encOrder */) {}
    virtual mfxStatus SetFrameVMEData(const mfxExtLAFrameStatistics*, mfxU32 , mfxU32 )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    virtual bool IsVMEBRC()  {return false;}

protected:

    bool   m_IsInit;

    BRCParams m_par;
    HRDState  m_hrdState;

    mfxI32  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantMin, mQuantPrev, mQuantOffset, mQPprev;
    mfxI32  mMinQp;
    mfxI32  mMaxQp;

    //mfxI64  mBF, mBFsaved; // buffer fullness
    mfxI32  mBitsDesiredFrame; //average frame size
    Ipp16u  mQuantUpdated;
    mfxU32  mRecodedFrame_encOrder;
    bool    m_bRecodeFrame;
    mfxU32  mPicType;

    mfxI32 mRecode;
    mfxI64  mBitsEncodedTotal, mBitsDesiredTotal;
    mfxI32  mQp;
    mfxI32  mRCfap, mRCqap, mRCbap, mRCq;
    mfxF64  mRCqa, mRCfa, mRCqa0;
    mfxF64  mRCfa_short;
    mfxI32  mQuantRecoded;
    mfxI32  mQuantIprev, mQuantPprev, mQuantBprev;
    mfxI32  mBitsEncoded;
    mfxI32  mSceneChange;
    mfxI32  mBitsEncodedP, mBitsEncodedPrev;
    mfxI32  mPoc, mSChPoc;
    mfxU32  mEOLastIntra;

    void   ResetParams();
    mfxU32 GetInitQP();
    mfxBRCStatus UpdateAndCheckHRD(mfxI32 frameBits, mfxF64 inputBitsPerFrame, mfxI32 recode);
    void  SetParamsForRecoding (mfxI32 encOrder);

    mfxBRCStatus UpdateQuant(mfxI32 bEncoded, mfxI32 totalPicBits, mfxI32 layer = 0, mfxI32 recode = 0);
    mfxBRCStatus UpdateQuantHRD(mfxI32 bEncoded, mfxBRCStatus sts, mfxI32 overheadBits = 0, mfxI32 layer = 0, mfxI32 recode = 0);
    mfxStatus   SetParams(MfxVideoParam &video);

    bool        isFrameBeforeIntra (mfxU32 order);


    mfxStatus InitHRD();
 
};
#endif
}

#ifdef NEW_BRC
namespace MfxHwH265EncodeBRC
{
class cBRCParams
{
public:
    mfxU16 rateControlMethod; // CBR or VBR

    mfxU16 bHRDConformance;  // is HRD compliance  needed
    mfxU16 bRec;             // is Recoding possible
    mfxU16 bPanic;           // is Panic mode possible

    // HRD params
    mfxU32 bufferSizeInBytes;
    mfxU32 initialDelayInBytes;

    // RC params
    mfxU32 targetbps;
    mfxU32 maxbps;
    mfxF64 frameRate;    
    mfxU32 inputBitsPerFrame;
    mfxU32 maxInputBitsPerFrame;
    mfxU32 maxFrameSizeInBits;

    // Frame size params
    mfxU16 width;
    mfxU16 height;
    mfxU16 chromaFormat;
    mfxU16 bitDepthLuma;

    // GOP params
    mfxU16 gopPicSize;
    mfxU16 gopRefDist;
    bool   bPyr;

    //BRC accurancy params
    mfxF64 fAbPeriodLong;   // number on frames to calculate abberation from target frame 
    mfxF64 fAbPeriodShort;  // number on frames to calculate abberation from target frame 
    mfxF64 dqAbPeriod;      // number on frames to calculate abberation from dequant 
    mfxF64 bAbPeriod;       // number of frames to calculate abberation from target bitrate

    //QP parameters
    mfxI32   quantOffset;
    mfxI32   quantMax;
    mfxI32   quantMin;

public:
    cBRCParams():
        rateControlMethod(0),
        bHRDConformance(0),
        bRec(0),
        bPanic(0),
        bufferSizeInBytes(0),
        initialDelayInBytes(0),
        targetbps(0),
        maxbps(0),
        frameRate(0),    
        inputBitsPerFrame(0),
        maxInputBitsPerFrame(0),
        maxFrameSizeInBits(0),
        width(0),
        height(0),
        chromaFormat(0),
        bitDepthLuma(0),
        gopPicSize(0),
        gopRefDist(0),
        bPyr(0),
        fAbPeriodLong(0),
        fAbPeriodShort(0),
        dqAbPeriod(0),
        bAbPeriod(0),
        quantOffset(0),
        quantMax(0),
        quantMin(0)
    {}

    mfxStatus Init(mfxVideoParam* par);
};
class cHRD
{
public:
    cHRD():
        m_bufFullness(0),
        m_prevBufFullness(0),
        m_frameNum(0),
        m_minFrameSize(0),
        m_maxFrameSize(0),
        m_underflowQuant(0),
        m_overflowQuant(0),
        m_buffSizeInBits(0),
        m_delayInBits(0),
        m_inputBitsPerFrame(0),
        m_bCBR (false)
    {}

    void      Init(mfxU32 buffSizeInBytes, mfxU32 delayInBytes, mfxU32 inputBitsPerFrame, bool cbr);
    mfxU16    UpdateAndCheckHRD(mfxI32 frameBits, mfxI32 recode, mfxI32 minQuant, mfxI32 maxQuant);
    mfxStatus UpdateMinMaxQPForRec(mfxU32 brcSts, mfxI32 qp);
    mfxI32    GetTargetSize(mfxU32 brcSts);
    mfxI32    GetMaxFrameSize() { return m_maxFrameSize;}
    mfxI32    GetMinFrameSize() { return m_minFrameSize;}
    mfxI32    GetMaxQuant() { return m_overflowQuant -  1;}
    mfxI32    GetMinQuant() { return m_underflowQuant + 1;}
    mfxF64    GetBufferDiviation(mfxU32 targetBitrate);
 


private:
    mfxF64 m_bufFullness;
    mfxF64 m_prevBufFullness;
    mfxI32 m_frameNum;
    mfxI32 m_minFrameSize;
    mfxI32 m_maxFrameSize;
    mfxI32 m_underflowQuant;
    mfxI32 m_overflowQuant;

private:
    mfxI32 m_buffSizeInBits;
    mfxI32 m_delayInBits;
    mfxU32 m_inputBitsPerFrame;    
    bool   m_bCBR;

};
struct BRC_Ctx
{
    mfxI32 QuantI;  //currect qp for intra frames
    mfxI32 QuantP;  //currect qp for P frames
    mfxI32 QuantB;  //currect qp for B frames

    mfxI32 Quant;           // qp for last encoded frame
    mfxI32 QuantMin;        // qp Min for last encoded frame (is used for recoding)
    mfxI32 QuantMax;        // qp Max for last encoded frame (is used for recoding)

    bool   bToRecode;       // last frame is needed in recoding
    bool   bPanic;          // last frame is needed in panic
    mfxU32 encOrder;        // encoding order of last encoded frame
    mfxU32 poc;             // poc of last encoded frame
    mfxI32 SceneChange;     // scene change parameter of last encoded frame
    mfxU32 SChPoc;          // poc of frame with scene change
    mfxU32 LastIEncOrder;   // encoded order if last intra frame
    mfxU32 LastNonBFrameSize; // encoded frame size of last non B frame (is used for sceneChange)

    mfxF64 fAbLong;         // frame abberation (long period)
    mfxF64 fAbShort;        // frame abberation (short period)
    mfxF64 dQuantAb;        // dequant abberation
    mfxI32 totalDiviation;   // divation from  target bitrate (total)

    mfxF64 eRate;               // eRate of last encoded frame, this parameter is used for scene change calculation
};
class ExtBRC
{
private:
    cBRCParams m_par;
    cHRD       m_hrd;
    bool       m_bInit;
    BRC_Ctx    m_ctx;

public:
    ExtBRC():
        m_par(),
        m_hrd(),
        m_bInit(false)
    {
        memset(&m_ctx, 0, sizeof(m_ctx));

    }
    mfxStatus Init (mfxVideoParam* par);
    mfxStatus Reset(mfxVideoParam* par);
    mfxStatus Close () {m_bInit = false; return MFX_ERR_NONE;}
    mfxStatus GetFrameCtrl (mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl);
    mfxStatus Update (mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status);
};
}
namespace HEVCExtBRC
{
    inline mfxStatus Init  (mfxHDL pthis, mfxVideoParam* par)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((MfxHwH265EncodeBRC::ExtBRC*)pthis)->Init(par) ;    
    }
    inline mfxStatus Reset (mfxHDL pthis, mfxVideoParam* par)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((MfxHwH265EncodeBRC::ExtBRC*)pthis)->Reset(par) ;
    }
    inline mfxStatus Close (mfxHDL pthis)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((MfxHwH265EncodeBRC::ExtBRC*)pthis)->Close() ;    
    }
    inline mfxStatus GetFrameCtrl (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl)
    {
       MFX_CHECK_NULL_PTR1(pthis);
       return ((MfxHwH265EncodeBRC::ExtBRC*)pthis)->GetFrameCtrl(par,ctrl) ;    
    }
    inline mfxStatus Update       (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((MfxHwH265EncodeBRC::ExtBRC*)pthis)->Update(par,ctrl, status) ;
    }
    inline mfxStatus Create(mfxExtBRC & m_BRC)
    {
        MFX_CHECK(m_BRC.pthis == NULL, MFX_ERR_UNDEFINED_BEHAVIOR);
        m_BRC.pthis = new MfxHwH265EncodeBRC::ExtBRC;
        m_BRC.Init = Init;
        m_BRC.Reset = Reset;
        m_BRC.Close = Close;
        m_BRC.GetFrameCtrl = GetFrameCtrl;
        m_BRC.Update = Update; 
        return MFX_ERR_NONE;
    }
    inline mfxStatus Destroy(mfxExtBRC & m_BRC)
    {

        MFX_CHECK(m_BRC.pthis != NULL, MFX_ERR_NONE);
        delete (MfxHwH265EncodeBRC::ExtBRC*)m_BRC.pthis;

        m_BRC.pthis = 0;
        m_BRC.Init = 0;
        m_BRC.Reset = 0;
        m_BRC.Close = 0;
        m_BRC.GetFrameCtrl = 0;
        m_BRC.Update = 0;   
        return MFX_ERR_NONE;
    }
}

namespace MfxHwH265Encode
{
class H265BRCNew : public BrcIface
{

public:
    H265BRCNew():
        m_minSize(0),
        m_pBRC(0)
    {
        memset(&m_BRCLocal,0, sizeof(m_BRCLocal));
    }    
    virtual ~H265BRCNew()
    {
        Close();
    }
    virtual mfxStatus   Init(MfxVideoParam &video, mfxI32 )
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (video.m_ext.extBRC.pthis)
        {
            m_pBRC = &video.m_ext.extBRC;
            MFX_CHECK_NULL_PTR3(m_pBRC->Init, m_pBRC->Close, m_pBRC->Reset);
            MFX_CHECK_NULL_PTR2(m_pBRC->GetFrameCtrl, m_pBRC->Update);        
        }
        else
        {
            sts = HEVCExtBRC::Create(m_BRCLocal);
            MFX_CHECK_STS(sts);
            m_pBRC = &m_BRCLocal;
        }
        return m_pBRC->Init(m_pBRC->pthis, &video);
    }
    virtual mfxStatus   Close()
    {
        mfxStatus sts = MFX_ERR_NONE;
        sts =  m_pBRC->Close(m_pBRC->pthis);
        MFX_CHECK_STS(sts);
        HEVCExtBRC::Destroy(m_BRCLocal);
        return sts;
    }
    virtual mfxStatus   Reset(MfxVideoParam &video, mfxI32 )
    {
        return m_pBRC->Reset(m_pBRC->pthis,&video);    
    }
    virtual mfxBRCStatus PostPackFrame(MfxVideoParam &, Task &task, mfxI32 bitsEncodedFrame, mfxI32 , mfxI32 )
    {
        mfxBRCFrameParam frame_par={};
        mfxBRCFrameCtrl  frame_ctrl={};
        mfxBRCFrameStatus frame_sts = {};
        

        frame_ctrl.QpY = task.m_qpY;
        InitFramePar(task,frame_par);
        frame_par.CodedFrameSize = bitsEncodedFrame/8;  // Size of frame in bytes after encoding


        mfxStatus sts = m_pBRC->Update(m_pBRC->pthis,&frame_par, &frame_ctrl, &frame_sts);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_BRC_ERROR);

        m_minSize = frame_sts.MinFrameSize;

        switch (frame_sts.BRCStatus)
        {
        case ::MFX_BRC_OK:
            return MFX_BRC_OK;
        case ::MFX_BRC_BIG_FRAME: 
            return MFX_BRC_ERR_BIG_FRAME;
        case ::MFX_BRC_SMALL_FRAME: 
            return MFX_BRC_ERR_SMALL_FRAME;
        case ::MFX_BRC_PANIC_BIG_FRAME: 
            return MFX_BRC_ERR_BIG_FRAME | MFX_BRC_NOT_ENOUGH_BUFFER;
        case ::MFX_BRC_PANIC_SMALL_FRAME:  
            return MFX_BRC_ERR_SMALL_FRAME| MFX_BRC_NOT_ENOUGH_BUFFER;
        }
        return MFX_BRC_OK;    
    }
    virtual mfxI32      GetQP(MfxVideoParam &, Task &task)
    {
        mfxBRCFrameParam frame_par={};
        mfxBRCFrameCtrl  frame_ctrl={};

        InitFramePar(task,frame_par);
        m_pBRC->GetFrameCtrl(m_pBRC->pthis,&frame_par, &frame_ctrl);
        return frame_ctrl.QpY;    
    }
    virtual mfxStatus   SetQP(mfxI32 , Ipp16u , bool )
    {
        return MFX_ERR_NONE;    
    }
    //virtual mfxStatus   GetInitialCPBRemovalDelay(mfxU32 *initial_cpb_removal_delay, mfxI32 recode = 0);
    virtual void        GetMinMaxFrameSize(mfxI32 *minFrameSizeInBits, mfxI32 *)
    {
        *minFrameSizeInBits = m_minSize;
    
    }


    virtual void        PreEnc(mfxU32 /* frameType */, std::vector<VmeData *> const & /* vmeData */, mfxU32 /* encOrder */) {}
    virtual mfxStatus SetFrameVMEData(const mfxExtLAFrameStatistics*, mfxU32 , mfxU32 )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    virtual bool IsVMEBRC()  {return false;}

private:
    mfxU32      m_minSize;
    mfxExtBRC * m_pBRC;
    mfxExtBRC   m_BRCLocal;

protected:
    void InitFramePar(Task &task, mfxBRCFrameParam & frame_par)
    {
        frame_par.EncodedOrder = task.m_eo;    // Frame number in a sequence of reordered frames starting from encoder Init()
        frame_par.DisplayOrder = task.m_poc;    // Frame number in a sequence of frames in display order starting from last IDR
        frame_par.FrameType = task.m_frameType;       // See FrameType enumerator
        frame_par.PyramidLayer = (mfxU16)task.m_level;    // B-pyramid or P-pyramid layer, frame belongs to
        frame_par.NumRecode = (mfxU16)task.m_recode;       // Number of recodings performed for this frame    
    }

 
};
}

#endif
#endif
