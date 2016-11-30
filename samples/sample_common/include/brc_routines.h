/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/
#ifdef ENABLE_FUTURE_FEATURES

#include "sample_defs.h"
#include "mfxbrc.h"

#ifndef __PIPELINE_ENCODE_BRC_H__
#define __PIPELINE_ENCODE_BRC_H__

#define MFX_CHECK_NULL_PTR1(pointer)    MSDK_CHECK_POINTER(pointer, MFX_ERR_NULL_PTR);

#define MFX_CHECK_NULL_PTR2(pointer1,pointer2 )\
    MFX_CHECK_NULL_PTR1(pointer1);\
    MFX_CHECK_NULL_PTR1(pointer2);

#define MFX_CHECK_NULL_PTR3(pointer1,pointer2, pointer3 )\
    MFX_CHECK_NULL_PTR2(pointer1,pointer2);\
    MFX_CHECK_NULL_PTR1(pointer3);

#define MFX_CHECK(cond, error) MSDK_CHECK_NOT_EQUAL(cond, true, error)
#define MFX_CHECK_STS(sts) MFX_CHECK(sts == MFX_ERR_NONE, sts)


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
    mfxU32 maxFrameSizeInBytes;

    // Frame size params
    mfxU16 width;
    mfxU16 height;
    mfxU16 chromaFormat;
    mfxU16 bitDepthLuma;

    // GOP params
    mfxU16 gopPicSize;
    mfxU16 gopRefDist;

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
        maxFrameSizeInBytes(0),
        width(0),
        height(0),
        chromaFormat(0),
        bitDepthLuma(0),
        gopPicSize(0),
        gopRefDist(0),
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

namespace HEVCExtBRC
{
    inline mfxStatus Init  (mfxHDL pthis, mfxVideoParam* par)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((ExtBRC*)pthis)->Init(par) ;
    }
    inline mfxStatus Reset (mfxHDL pthis, mfxVideoParam* par)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((ExtBRC*)pthis)->Reset(par) ;
    }
    inline mfxStatus Close (mfxHDL pthis)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((ExtBRC*)pthis)->Close() ;
    }
    inline mfxStatus GetFrameCtrl (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl)
    {
       MFX_CHECK_NULL_PTR1(pthis);
       return ((ExtBRC*)pthis)->GetFrameCtrl(par,ctrl) ;
    }
    inline mfxStatus Update       (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((ExtBRC*)pthis)->Update(par,ctrl, status) ;
    }
    inline mfxStatus Create(mfxExtBRC & m_BRC)
    {
        MFX_CHECK(m_BRC.pthis == NULL, MFX_ERR_UNDEFINED_BEHAVIOR);
        m_BRC.pthis = new ExtBRC;
        m_BRC.Init = Init;
        m_BRC.Reset = Reset;
        m_BRC.Close = Close;
        m_BRC.GetFrameCtrl = GetFrameCtrl;
        m_BRC.Update = Update;
        return MFX_ERR_NONE;
    }
    inline mfxStatus Destroy(mfxExtBRC & m_BRC)
    {
        MFX_CHECK(m_BRC.pthis == NULL, MFX_ERR_NONE);
        delete (ExtBRC*)m_BRC.pthis;
        m_BRC.Init = 0;
        m_BRC.Reset = 0;
        m_BRC.Close = 0;
        m_BRC.GetFrameCtrl = 0;
        m_BRC.Update = 0;
        return MFX_ERR_NONE;
    }
}
#endif

#endif