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

#include "brc_routines.h"
#include "math.h"
#include <algorithm>

#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))
#define BRC_SCENE_CHANGE_RATIO1 20.0
#define BRC_SCENE_CHANGE_RATIO2 5.0

#define IPP_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define IPP_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )


mfxExtBuffer* Hevc_GetExtBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    if (extBuf != 0)
    {
        for (mfxU16 i = 0; i < numExtBuf; i++)
        {
            if (extBuf[i] != 0 && extBuf[i]->BufferId == id) // assuming aligned buffers
                return (extBuf[i]);
        }
    }

    return 0;
}

mfxStatus cBRCParams::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(par->mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
              par->mfx.RateControlMethod == MFX_RATECONTROL_VBR,
              MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 k = par->mfx.BRCParamMultiplier == 0 ?  1: par->mfx.BRCParamMultiplier;

    rateControlMethod  = par->mfx.RateControlMethod;
    targetbps = (((k*par->mfx.TargetKbps*1000) >> 6) << 6 );
    maxbps =    (((k*par->mfx.MaxKbps*1000) >> 6) << 6 );

    mfxExtCodingOption * pExtCO = (mfxExtCodingOption*)Hevc_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION);

    bHRDConformance =  (pExtCO && pExtCO->NalHrdConformance == MFX_CODINGOPTION_OFF) ? 0 : 1;

    if (bHRDConformance)
    {
        bufferSizeInBytes  = ((k*par->mfx.BufferSizeInKB*1000) >> 3) << 3;
        initialDelayInBytes =((k*par->mfx.InitialDelayInKB*1000) >> 3) << 3;
        bRec = 1;
        bPanic = 1;
    }
    MFX_CHECK (par->mfx.FrameInfo.FrameRateExtD != 0 &&
               par->mfx.FrameInfo.FrameRateExtN != 0,
               MFX_ERR_UNDEFINED_BEHAVIOR);

    frameRate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / (mfxF64)par->mfx.FrameInfo.FrameRateExtD;

    width = par->mfx.FrameInfo.Width;
    height =par->mfx.FrameInfo.Height;

    chromaFormat = par->mfx.FrameInfo.ChromaFormat;
    bitDepthLuma = par->mfx.FrameInfo.BitDepthLuma;
    quantOffset   = 6 * (bitDepthLuma - 8);

    inputBitsPerFrame    = (mfxU32)(targetbps / frameRate);
    maxInputBitsPerFrame = (mfxU32)(maxbps / frameRate);
    gopPicSize = par->mfx.GopPicSize;
    gopRefDist = par->mfx.GopRefDist;

    fAbPeriodLong  = 100;
    fAbPeriodShort = 5;
    dqAbPeriod = 100;
    bAbPeriod = 100;


    quantMax = 51 + quantOffset;
    quantMin = 1;

    return MFX_ERR_NONE;
}



enum
{
    MFX_BRC_RECODE_NONE           = 0,
    MFX_BRC_RECODE_QP             = 1,
    MFX_BRC_RECODE_PANIC          = 2,
};

   mfxF64 const QSTEP[88] = {
         0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
         2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
        12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
        57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070,
        256.000, 287.350, 322.540, 362.039, 406.375, 456.140, 512.000, 574.701, 645.080, 724.077, 812.749, 912.280,
        1024.000, 1149.401, 1290.159, 1448.155, 1625.499, 1824.561, 2048.000, 2298.802, 2580.318, 2896.309, 3250.997, 3649.121,
        4096.000, 4597.605, 5160.637, 5792.619, 6501.995, 7298.242, 8192.000, 9195.209, 10321.273, 11585.238, 13003.989, 14596.485
    };


mfxI32 QStep2QpFloor(mfxF64 qstep, mfxI32 qpoffset = 0) // QSTEP[qp] <= qstep, return 0<=qp<=51+mQuantOffset
{
    mfxU8 qp = mfxU8(std::upper_bound(QSTEP, QSTEP + 51 + qpoffset, qstep) - QSTEP);
    return qp > 0 ? qp - 1 : 0;
}

mfxI32 Qstep2QP(mfxF64 qstep, mfxI32 qpoffset = 0) // return 0<=qp<=51+mQuantOffset
{
    mfxI32 qp = QStep2QpFloor(qstep, qpoffset);

    // prevent going QSTEP index out of bounds
    if (qp >= (mfxI32)(sizeof(QSTEP)/sizeof(mfxF64)) - 1)
        return 0;
    return (qp == 51 + qpoffset || qstep < (QSTEP[qp] + QSTEP[qp + 1]) / 2) ? qp : qp + 1;
}
mfxF64 QP2Qstep(mfxI32 qp, mfxI32 qpoffset = 0)
{
    return QSTEP[IPP_MIN(51 + qpoffset, qp)];
}
#define BRC_CLIP(val, minval, maxval) val = Saturate(minval, maxval, val)



mfxF64  cHRD::GetBufferDiviation(mfxU32 targetBitrate)
{
    mfxI64 targetFullness = IPP_MIN(m_delayInBits, m_buffSizeInBits / 2);
    mfxI64 minTargetFullness = IPP_MIN(mfxU32(m_buffSizeInBits / 2),targetBitrate * 2); // half bufsize or 2 sec
    targetFullness = IPP_MAX(targetFullness , minTargetFullness);
     return targetFullness - m_bufFullness;
}

mfxU16 cHRD::UpdateAndCheckHRD(mfxI32 frameBits, mfxI32 recode, mfxI32 minQuant, mfxI32 maxQuant)
{
    mfxU16 brcStatus = MFX_BRC_OK ;

    if (recode  == 0)
    {
        m_prevBufFullness = m_bufFullness;
        m_underflowQuant = minQuant - 1;
        m_overflowQuant  = maxQuant + 1;
    }
    else
    { // frame is being recoded - restore buffer state
        m_bufFullness = m_prevBufFullness;
        m_frameNum--;
    }

    m_maxFrameSize = (mfxI32)(m_bufFullness - 1);
    m_minFrameSize = (!m_bCBR)? 0 : (mfxI32)(m_bufFullness + 1 + 1 + m_inputBitsPerFrame - m_buffSizeInBits);
    if (m_minFrameSize < 0)
        m_minFrameSize = 0;

   mfxF64  bufFullness = m_bufFullness - frameBits;

    if (bufFullness < 2)
    {
        bufFullness = m_inputBitsPerFrame;
        brcStatus = MFX_BRC_BIG_FRAME;
        if (bufFullness > m_buffSizeInBits)
            bufFullness = m_buffSizeInBits;
    }
    else
    {
        bufFullness += m_inputBitsPerFrame;
        if (bufFullness > m_buffSizeInBits - 1)
        {
            bufFullness = m_buffSizeInBits - 1;
            if (m_bCBR)
                brcStatus = MFX_BRC_SMALL_FRAME;
        }
    }
    m_frameNum++;
    if ( MFX_BRC_RECODE_PANIC == recode) // no use in changing QP
    {
        if (brcStatus == MFX_BRC_SMALL_FRAME)
            brcStatus =  MFX_BRC_PANIC_SMALL_FRAME ;
        if (brcStatus == MFX_BRC_BIG_FRAME)
            brcStatus =  MFX_BRC_PANIC_BIG_FRAME ;    }

    m_bufFullness = bufFullness;
    return brcStatus;
}

mfxStatus cHRD::UpdateMinMaxQPForRec( mfxU32 brcSts, mfxI32 qp)
{
    MFX_CHECK(brcSts == MFX_BRC_BIG_FRAME || brcSts == MFX_BRC_SMALL_FRAME, MFX_ERR_UNDEFINED_BEHAVIOR);
    if (brcSts == MFX_BRC_BIG_FRAME)
        m_underflowQuant = qp;
    else
        m_overflowQuant = qp;
    return MFX_ERR_NONE;
}
mfxI32 cHRD::GetTargetSize(mfxU32 brcSts)
{
     if (brcSts != MFX_BRC_BIG_FRAME && brcSts != MFX_BRC_SMALL_FRAME) return 0;
     return (brcSts == MFX_BRC_BIG_FRAME) ? m_maxFrameSize * 3 / 4 : m_minFrameSize * 5 / 4;
}

mfxI32 GetNewQP(mfxF64 totalFrameBits, mfxF64 targetFrameSizeInBits, mfxI32 minQP , mfxI32 maxQP, mfxI32 qp , mfxI32 qp_offset, mfxF64 f_pow, bool bStrict = false)
{
    mfxF64 qstep = 0, qstep_new = 0;
    mfxI32 qp_new = qp;

    qstep = QP2Qstep(qp, qp_offset);
    qstep_new = qstep * pow(totalFrameBits / targetFrameSizeInBits, f_pow);
    qp_new = Qstep2QP(qstep_new, qp_offset);

    if (totalFrameBits < targetFrameSizeInBits) // overflow
    {
        if (qp <= minQP)
        {
            return qp; // QP change is impossible
        }
        qp_new  = IPP_MAX (qp_new , (minQP + qp + 1) >> 1);
        if (bStrict)
            qp_new  = IPP_MIN (qp_new, qp - 1);
    }
    else // underflow
    {
        if (qp >= maxQP)
        {
            return qp; // QP change is impossible
        }
        qp_new  = IPP_MIN (qp_new , (maxQP + qp + 1) >> 1);
        if (bStrict)
            qp_new  = IPP_MAX (qp_new, qp + 1);
    }
    return qp_new;
}




void cHRD::Init(mfxU32 buffSizeInBytes, mfxU32 delayInBytes, mfxU32 inputBitsPerFrame, bool bCBR)
{
    m_bufFullness = m_prevBufFullness= delayInBytes << 3;
    m_delayInBits = delayInBytes << 3;
    m_buffSizeInBits = buffSizeInBytes << 3;
    m_inputBitsPerFrame =inputBitsPerFrame;
    m_bCBR = bCBR;

    m_underflowQuant = 0;
    m_overflowQuant = 999;
    m_frameNum = 0;
    m_minFrameSize = 0;
    m_maxFrameSize = 0;

}

void UpdateQPParams(mfxI32 qp, mfxU32 type , BRC_Ctx  &ctx, mfxU32 /* rec_num */, mfxI32 minQuant, mfxI32 maxQuant, mfxU32 level)
{
    ctx.Quant = qp;
    if (type == MFX_FRAMETYPE_I)
    {
        ctx.QuantI = qp;
        ctx.QuantP = qp + 1;
        ctx.QuantB = qp + 2;
    }
    else if (type == MFX_FRAMETYPE_P)
    {
        qp -= level;
        ctx.QuantI = (3*ctx.QuantI + qp ) >> 2;
        ctx.QuantI = IPP_MIN(ctx.QuantI, qp - 1);
        ctx.QuantP = qp;
        ctx.QuantB = qp + 1;
    }
    else if (type == MFX_FRAMETYPE_B)
    {
        level = level > 0 ? level - 1: 0;
        qp -= level;
        ctx.QuantI = (3*ctx.QuantI + qp ) >> 2;
        ctx.QuantI = IPP_MIN(ctx.QuantI, qp - 2);
        ctx.QuantP = qp - 1;
        ctx.QuantB = qp;
    }
    BRC_CLIP(ctx.QuantI, minQuant, maxQuant);
    BRC_CLIP(ctx.QuantP, minQuant, maxQuant);
    BRC_CLIP(ctx.QuantB, minQuant, maxQuant);
}

mfxI32 GetRawFrameSize(mfxU32 lumaSize, mfxU16 chromaFormat, mfxU16 bitDepthLuma)
{
    mfxI32 frameSize = lumaSize;

  if (chromaFormat == MFX_CHROMAFORMAT_YUV420)
    frameSize += lumaSize / 2;
  else if (chromaFormat == MFX_CHROMAFORMAT_YUV422)
   frameSize += lumaSize;
  else if (chromaFormat == MFX_CHROMAFORMAT_YUV444)
    frameSize += lumaSize * 2;

  frameSize = frameSize * bitDepthLuma / 8;
  return frameSize*8; //frame size in bits
}

mfxStatus ExtBRC::Init (mfxVideoParam* par)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK(!m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);
    sts = m_par.Init(par);
    MFX_CHECK_STS(sts);

    if (m_par.bHRDConformance)
    {
        m_hrd.Init(m_par.bufferSizeInBytes, m_par.initialDelayInBytes, m_par.maxInputBitsPerFrame, m_par.rateControlMethod == MFX_RATECONTROL_CBR);
    }
    memset(&m_ctx, 0, sizeof(m_ctx));

    m_ctx.fAbLong  = m_par.inputBitsPerFrame;
    m_ctx.fAbShort = m_par.inputBitsPerFrame;

    mfxI32 rawSize = GetRawFrameSize(m_par.width * m_par.height ,m_par.chromaFormat, m_par.bitDepthLuma);
    mfxI32 qp = GetNewQP(rawSize, m_par.inputBitsPerFrame, m_par.quantMin, m_par.quantMax, 1 , m_par.quantOffset, 0.5);
    MSDK_CHECK_ERROR(qp, 0, MFX_ERR_NULL_PTR);

    UpdateQPParams(qp,MFX_FRAMETYPE_I , m_ctx, 0, m_par.quantMin, m_par.quantMax, 0);

    m_ctx.dQuantAb = 1./qp;

    m_bInit = true;
    return sts;
}

mfxU16 GetFrameType(mfxU16 m_frameType, mfxU16 level, mfxU16 gopRegDist)
{
    if (m_frameType & MFX_FRAMETYPE_IDR)
        return MFX_FRAMETYPE_I;
    else if (m_frameType & MFX_FRAMETYPE_I)
        return MFX_FRAMETYPE_I;
    else if (m_frameType & MFX_FRAMETYPE_P)
        return MFX_FRAMETYPE_P;
    else if ((m_frameType & MFX_FRAMETYPE_REF) && (level == 0 || gopRegDist == 1))
        return MFX_FRAMETYPE_P; //low delay B
    else
        return MFX_FRAMETYPE_B;
}


bool  isFrameBeforeIntra (mfxU32 order, mfxU32 intraOrder, mfxU32 gopPicSize, mfxU32 gopRefDist)
 {
     mfxI32 distance0 = gopPicSize*3/4;
     mfxI32 distance1 = gopPicSize - gopRefDist*3;
     return (order - intraOrder) > (mfxU32)(IPP_MAX(distance0, distance1));
 }
mfxStatus SetRecodeParams(mfxU16 brcStatus, mfxI32 qp, mfxI32 qp_new, mfxI32 minQP, mfxI32 maxQP, BRC_Ctx &ctx, mfxBRCFrameStatus* status)
{
    ctx.bToRecode = 1;

    if (brcStatus == MFX_BRC_BIG_FRAME || brcStatus == MFX_BRC_PANIC_BIG_FRAME )
    {
         MFX_CHECK(qp_new >= qp, MFX_ERR_UNDEFINED_BEHAVIOR);
         ctx.Quant = qp_new;
         ctx.QuantMax = maxQP;
         if (brcStatus == MFX_BRC_BIG_FRAME && qp_new > qp)
         {
            ctx.QuantMin = IPP_MAX(qp + 1, minQP); //limit QP range for recoding
            status->BRCStatus = MFX_BRC_BIG_FRAME;

         }
         else
         {
             ctx.QuantMin = minQP;
             ctx.bPanic = 1;
             status->BRCStatus = MFX_BRC_PANIC_BIG_FRAME;
         }
    }
    else if (brcStatus == MFX_BRC_SMALL_FRAME || brcStatus == MFX_BRC_PANIC_SMALL_FRAME)
    {
         MFX_CHECK(qp_new <= qp, MFX_ERR_UNDEFINED_BEHAVIOR);

         ctx.Quant = qp_new;
         ctx.QuantMin = minQP; //limit QP range for recoding

         if (brcStatus == MFX_BRC_SMALL_FRAME && qp_new < qp)
         {
            ctx.QuantMax = IPP_MIN (qp - 1, maxQP);
            status->BRCStatus = MFX_BRC_SMALL_FRAME;
         }
         else
         {
            ctx.QuantMax = maxQP;
            status->BRCStatus = MFX_BRC_PANIC_SMALL_FRAME;
            ctx.bPanic = 1;
         }
    }
    //printf("recode %d , qp %d new %d, status %d\n", ctx.encOrder, qp, qp_new, status->BRCStatus);
    return MFX_ERR_NONE;
}
mfxI32 GetNewQPTotal(mfxF64 bo, mfxF64 dQP, mfxI32 minQP , mfxI32 maxQP, mfxI32 qp)
{
    BRC_CLIP(bo, -1.0, 1.0);
    BRC_CLIP(dQP, 1./maxQP, 1./minQP);
    dQP = dQP + (1./maxQP - dQP) * bo;
    BRC_CLIP(dQP, 1./maxQP, 1./minQP);
    mfxI32 quant_new = (mfxI32) (1. / dQP + 0.5);


    if (quant_new >= qp + 5)
        quant_new = qp + 2;
    else if (quant_new > qp + 3)
        quant_new = qp + 1;
    else if (quant_new <= qp - 5)
        quant_new = qp - 2;
    else if (quant_new < qp - 2)
        quant_new = qp - 1;

    return quant_new;
}


mfxStatus ExtBRC::Update(mfxBRCFrameParam* frame_par, mfxBRCFrameCtrl* frame_ctrl, mfxBRCFrameStatus* status)
{
    mfxStatus sts       = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR3(frame_par, frame_ctrl, status);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    mfxU16 &brcSts       = status->BRCStatus;
    status->MinFrameSize  = 0;


    mfxI32 bitsEncoded  = frame_par->CodedFrameSize*8;
    mfxU32 picType      = GetFrameType(frame_par->FrameType, frame_par->PyramidLayer, m_par.gopRefDist);
    mfxI32 qpY          = frame_ctrl->QpY + m_par.quantOffset;
    mfxI32 layer        = frame_par->PyramidLayer;
    mfxF64 qstep        = QP2Qstep(qpY, m_par.quantOffset);

    mfxF64 fAbLong  = m_ctx.fAbLong   + (bitsEncoded - m_ctx.fAbLong)  / m_par.fAbPeriodLong;
    mfxF64 fAbShort = m_ctx.fAbShort  + (bitsEncoded - m_ctx.fAbShort) / m_par.fAbPeriodShort;
    mfxF64 eRate    = bitsEncoded * sqrt(qstep);
    mfxF64 e2pe     = (m_ctx.eRate == 0) ? (BRC_SCENE_CHANGE_RATIO2 + 1) : eRate / m_ctx.eRate;

    brcSts = MFX_BRC_OK;
    if (m_par.bRec && m_ctx.bToRecode &&  (m_ctx.encOrder != frame_par->EncodedOrder || frame_par->NumRecode == 0))
    {
        // Frame must be recoded, but encoder calls BR for another frame
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if (frame_par->NumRecode == 0)
    {
        // Set context for new frame
        if (picType == MFX_FRAMETYPE_I)
            m_ctx.LastIEncOrder = frame_par->EncodedOrder;
        m_ctx.encOrder = frame_par->EncodedOrder;
        m_ctx.poc = frame_par->DisplayOrder;
        m_ctx.bToRecode = 0;
        m_ctx.bPanic = 0;
        m_ctx.QuantMin = m_par.quantMin;
        m_ctx.QuantMax = m_par.quantMax;
        m_ctx.Quant = qpY;


        if (m_ctx.SceneChange && ( m_ctx.poc > m_ctx.SChPoc + 1 || m_ctx.poc == 0))
            m_ctx.SceneChange &= ~16;

        //printf("m_ctx.SceneChange %d, m_ctx.poc %d, m_ctx.SChPoc, m_ctx.poc %d \n", m_ctx.SceneChange, m_ctx.poc, m_ctx.SChPoc, m_ctx.poc);
    }
    if (e2pe > BRC_SCENE_CHANGE_RATIO2)
    {
      // scene change, resetting BRC statistics
        m_ctx.fAbLong  = m_par.inputBitsPerFrame;
        m_ctx.fAbShort = m_par.inputBitsPerFrame;
        m_ctx.SceneChange |= 1;
        if (picType != MFX_FRAMETYPE_B)
        {
            m_ctx.dQuantAb  = 1./m_ctx.Quant;
            m_ctx.SceneChange |= 16;
            m_ctx.SChPoc = frame_par->DisplayOrder;
            //printf("   m_ctx.SceneChange %d, order %d\n", m_ctx.SceneChange, frame_par->DisplayOrder);
        }

    }
    if (m_par.bHRDConformance)
    {
       //check hrd
        brcSts = m_hrd.UpdateAndCheckHRD(bitsEncoded,frame_par->NumRecode, m_par.quantMin, m_par.quantMax);
        MFX_CHECK(brcSts ==  MFX_BRC_OK || (!m_ctx.bPanic), MFX_ERR_NOT_ENOUGH_BUFFER);
        if (brcSts == MFX_BRC_BIG_FRAME || brcSts == MFX_BRC_SMALL_FRAME)
            m_hrd.UpdateMinMaxQPForRec(brcSts, qpY);
        status->MinFrameSize = m_hrd.GetMinFrameSize() + 7;
        //printf("%d: poc %d, size %d QP %d (%d %d), HRD sts %d, maxFrameSize %d, type %d \n",frame_par->EncodedOrder, frame_par->DisplayOrder, bitsEncoded, m_ctx.Quant, m_ctx.QuantMin, m_ctx.QuantMax, brcSts,  m_hrd.GetMaxFrameSize(), frame_par->FrameType);
    }

    if (frame_par->NumRecode < 2)
    // Check other condions for recoding (update qp is it is needed)
    {
        mfxF64 targetFrameSize = IPP_MAX((mfxF64)m_par.inputBitsPerFrame, fAbLong);
        mfxF64 frameFactor = (picType == MFX_FRAMETYPE_I) ? 3.0 : 1.0;
        mfxF64 maxFrameSize = BRC_SCENE_CHANGE_RATIO2 * targetFrameSize * frameFactor;
        mfxI32 quantMax = m_ctx.QuantMax;
        mfxI32 quantMin = m_ctx.QuantMin;
        mfxI32 quant = qpY;


        if (m_par.bHRDConformance)
        {
            maxFrameSize = IPP_MIN(maxFrameSize, 2.5/9. * m_hrd.GetMaxFrameSize() + 6.5/9. * targetFrameSize);
            quantMax     = IPP_MIN(m_hrd.GetMaxQuant(), quantMax);
            quantMin     = IPP_MAX(m_hrd.GetMinQuant(), quantMin);
        }
        maxFrameSize = IPP_MAX(maxFrameSize, targetFrameSize);

        if (bitsEncoded >  maxFrameSize && quant < quantMax)
        {
            mfxI32 quant_new = GetNewQP(bitsEncoded, (mfxU32)maxFrameSize, quantMin , quantMax, quant ,m_par.quantOffset, 1);
            if (quant_new > quant)
            {
                //printf("    recode 1-0: %d:  k %5f bitsEncoded %d maxFrameSize %d, targetSize %d, fAbLong %f, inputBitsPerFrame %d, qp %d new %d\n",frame_par->EncodedOrder, bitsEncoded/maxFrameSize, (int)bitsEncoded, (int)maxFrameSize,(int)targetFrameSize, fAbLong, m_par.inputBitsPerFrame, quant, quant_new);

                UpdateQPParams(quant_new ,picType, m_ctx, 0, quantMin , quantMax, layer);

                if (m_par.bRec)
                {
                    SetRecodeParams(MFX_BRC_BIG_FRAME,quant,quant_new, quantMin, quantMax, m_ctx, status);
                    return sts;
                }
            } //(quant_new > quant)
        } //bitsEncoded >  maxFrameSize

        if (bitsEncoded >  maxFrameSize && quant == quantMax &&
            picType != MFX_FRAMETYPE_I && m_par.bPanic &&
            (!m_ctx.bPanic) && isFrameBeforeIntra(m_ctx.encOrder, m_ctx.LastIEncOrder, m_par.gopPicSize, m_par.gopRefDist))
        {
            //skip frames before intra
            SetRecodeParams(MFX_BRC_PANIC_BIG_FRAME,quant,quant, quantMin ,quantMax, m_ctx, status);
            return sts;
        }
        if (m_par.bHRDConformance && frame_par->NumRecode == 0 && (quant < quantMax))
        {
            mfxF64 FAMax = 1./9. * m_hrd.GetMaxFrameSize() + 8./9. * fAbLong;

            if (fAbShort > FAMax)
            {
                mfxI32 quant_new = GetNewQP(fAbShort, FAMax, quantMin , quantMax, quant ,m_par.quantOffset, 0.5);
                MSDK_CHECK_ERROR(quant_new, 0, MFX_ERR_NULL_PTR);

                //printf("    recode 2-0: %d:  FAMax %f, fAbShort %f, quant_new %d\n",frame_par->EncodedOrder, FAMax, fAbShort, quant_new);

                if (quant_new > quant)
                {
                    UpdateQPParams(quant_new ,picType, m_ctx, 0, quantMin , quantMax, layer);
                    m_ctx.fAbLong  = m_par.inputBitsPerFrame;
                    m_ctx.fAbShort = m_par.inputBitsPerFrame;
                    m_ctx.dQuantAb = 1./quant_new;
                    if (m_par.bRec)
                    {
                        SetRecodeParams(MFX_BRC_BIG_FRAME,quant,quant_new, quantMin, quantMax, m_ctx, status);
                        return sts;
                    }
                }//quant_new > quant
            }
        }//m_par.bHRDConformance
    }
    if (m_par.bHRDConformance && brcSts != MFX_BRC_OK && m_par.bRec)
    {
        mfxI32 quant = m_ctx.Quant;
        mfxI32 quant_new = quant;
        if (brcSts == MFX_BRC_BIG_FRAME || brcSts == MFX_BRC_SMALL_FRAME)
        {
            quant_new = GetNewQP(bitsEncoded, m_hrd.GetTargetSize(brcSts), m_ctx.QuantMin , m_ctx.QuantMax,quant,m_par.quantOffset, 1, true);
        }
        if (quant_new != quant)
        {
           //printf("    recode hrd: %d, quant %d, quant_new %d, qpY %d, m_ctx.Quant %d, min %d max%d \n",frame_par->EncodedOrder, quant,quant_new, qpY, m_ctx.Quant,m_ctx.QuantMin , m_ctx.QuantMax);

           UpdateQPParams(quant_new ,picType, m_ctx, 0, m_ctx.QuantMin , m_ctx.QuantMax, layer);
        }
        SetRecodeParams(brcSts,quant,quant_new, m_ctx.QuantMin , m_ctx.QuantMax, m_ctx, status);
    }
    else
    {
        // no recoding are needed. Save context params

        mfxF64 k = 1./m_ctx.Quant;
        mfxF64 dqAbPeriod = m_par.dqAbPeriod;
        if (m_ctx.bToRecode)
            dqAbPeriod = (k < m_ctx.dQuantAb)? 16:25;
        m_ctx.dQuantAb += (k - m_ctx.dQuantAb)/dqAbPeriod;
        BRC_CLIP(m_ctx.dQuantAb, 1./m_par.quantMax , 1./m_par.quantMin);

        m_ctx.fAbLong  = fAbLong;
        m_ctx.fAbShort = fAbShort;
        if (picType != MFX_FRAMETYPE_B)
            m_ctx.eRate = eRate;

        bool oldScene = false;
        if ((m_ctx.SceneChange & 16) && (m_ctx.poc < m_ctx.SChPoc) && (bitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (mfxF64)m_ctx.LastNonBFrameSize) && (mfxF64)bitsEncoded < 1.5*fAbLong)
            oldScene = true;

        if (picType != MFX_FRAMETYPE_B)
            m_ctx.LastNonBFrameSize = bitsEncoded;

        if (!m_ctx.bPanic&& !oldScene)
        {
            //Update QP
            m_ctx.totalDiviation += (bitsEncoded - m_par.inputBitsPerFrame);

            mfxF64 totDiv = m_ctx.totalDiviation;
            mfxF64 dequant_new = m_ctx.dQuantAb*pow(m_par.inputBitsPerFrame/m_ctx.fAbLong, 1.2);
            mfxF64 bAbPreriod = m_par.bAbPeriod;

            if (m_par.bHRDConformance && totDiv > 0 )
            {
                if (m_par.rateControlMethod == MFX_RATECONTROL_VBR)
                {
                    totDiv = IPP_MAX(totDiv, m_hrd.GetBufferDiviation(m_par.targetbps));
                }
                bAbPreriod = (mfxF64)m_hrd.GetMaxFrameSize() / m_par.inputBitsPerFrame*3;
                BRC_CLIP(bAbPreriod , m_par.bAbPeriod/10, m_par.bAbPeriod);
            }
            mfxI32 quant_new = GetNewQPTotal(totDiv /bAbPreriod /(mfxF64)m_par.inputBitsPerFrame, dequant_new, m_ctx.QuantMin , m_ctx.QuantMax, m_ctx.Quant);
            //printf("   Update QP %d: totalDiviation %f, bAbPreriod %f, QP %d (%d %d), qp_new %d\n",frame_par->EncodedOrder,totDiv , bAbPreriod, m_ctx.Quant, m_ctx.QuantMin, m_ctx.QuantMax,quant_new);
            if (quant_new != m_ctx.Quant && !m_ctx.bToRecode)
            {
                UpdateQPParams(quant_new ,picType, m_ctx, 0, m_ctx.QuantMin , m_ctx.QuantMax, layer);
            }
            //printf("QP: I %d, P %d, B %d layer %d\n", m_ctx.QuantI, m_ctx.QuantP, m_ctx.QuantB, layer);
        }
        m_ctx.bToRecode = 0;
    }
    return sts;

}

mfxStatus ExtBRC::GetFrameCtrl (mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl)
{
    MFX_CHECK_NULL_PTR2(par, ctrl);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    mfxI32 qp = 0;
    if (par->EncodedOrder == m_ctx.encOrder)
    {
        qp = m_ctx.Quant;
    }
    else
    {
        mfxU16 type = GetFrameType(par->FrameType,par->PyramidLayer, m_par.gopRefDist);

        if (type == MFX_FRAMETYPE_I)
            qp = m_ctx.QuantI;
        else if (type == MFX_FRAMETYPE_P)
            qp =  m_ctx.QuantP + par->PyramidLayer;
        else
             qp =  m_ctx.QuantB + (par->PyramidLayer > 0 ? par->PyramidLayer - 1 : 0);
    }
    BRC_CLIP(qp, m_par.quantMin, m_par.quantMax);

    ctrl->QpY = qp - m_par.quantOffset;
    return MFX_ERR_NONE;
}

mfxStatus ExtBRC::Reset(mfxVideoParam *par )
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK (m_par.bHRDConformance == 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK (m_par.rateControlMethod == MFX_RATECONTROL_VBR, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    sts = m_par.Init(par);
    MFX_CHECK_STS(sts);

    MFX_CHECK (m_par.bHRDConformance == 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK (m_par.rateControlMethod == MFX_RATECONTROL_VBR, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    m_ctx.Quant = (mfxI32)(1./m_ctx.dQuantAb * pow(m_ctx.fAbLong/m_par.inputBitsPerFrame, 0.32) + 0.5);
    BRC_CLIP(m_ctx.Quant, m_par.quantMin, m_par.quantMax);

    UpdateQPParams(m_ctx.Quant, MFX_FRAMETYPE_I , m_ctx, 0, m_par.quantMin, m_par.quantMax, 0);

    m_ctx.dQuantAb = 1./m_ctx.Quant;
    m_ctx.fAbLong  = m_par.inputBitsPerFrame;
    m_ctx.fAbShort = m_par.inputBitsPerFrame;

    return sts;
}

#endif