// Copyright (c) 2008-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_ENC_DEBLOCKING_H__
#define __UMC_VC1_ENC_DEBLOCKING_H__

#include "umc_vc1_enc_mb.h"
#include "umc_vc1_common_defs.h"
namespace UMC_VC1_ENCODER
{

void GetInternalBlockEdge(VC1EncoderMBInfo *pCur,
                          uint8_t& YFlagUp, uint8_t& YFlagBot, uint8_t& UFlagH, uint8_t& VFlagH,
                          uint8_t& YFlagL,  uint8_t& YFlagR,   uint8_t& UFlagV, uint8_t& VFlagV);

typedef void (*fGetExternalEdge)(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer,
                                uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag);
typedef void (*fGetInternalEdge)(VC1EncoderMBInfo *pCur, uint8_t& YFlagV, uint8_t& YFlagH);

extern fGetExternalEdge GetExternalEdge[2][2]; //4 MV, VTS
extern fGetInternalEdge GetInternalEdge[2][2]; //4 MV, VTS

extern fGetExternalEdge GetFieldExternalEdge[2][2]; //4 MV, VTS
extern fGetInternalEdge GetFieldInternalEdge[2][2]; //4 MV, VTS

extern fGetExternalEdge GetExternalEdge_SM[2][2]; //4 MV, VTS
extern fGetInternalEdge GetInternalEdge_SM[2][2]; //4 MV, VTS

//-----------------------Deblocking I frames-----------------------------------------
inline void Deblock_I_LumaMB(uint8_t* pY, int32_t YStep, int32_t quant)
{
    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock top MB internal edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //horizontal deblock top MB/current MB edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical top left MB/top MB edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_LumaLeftMB(uint8_t* pY, int32_t YStep, int32_t quant)
{
    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock top MB internal edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //horizontal deblock top MB/current MB edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_LumaLeftBottomMB(uint8_t* pY, int32_t YStep, int32_t quant)
{
    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock top MB internal edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //horizontal deblock top MB/current MB edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //horizontal deblock current MB internal edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical current MB internal edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_LumaLeftTopBottomMB(uint8_t* pY, int32_t YStep, int32_t quant)
{
    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock current MB internal edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical current MB internal edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}
inline void Deblock_I_LumaBottomMB(uint8_t* pY, int32_t YStep, int32_t quant)
{
    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock top MB internal edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //horizontal deblock top MB/current MB edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //horizontal deblock current MB internal edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical top left MB/top MB edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical left MB/current MB edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical current MB internal edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_LumaTopBottomMB(uint8_t* pY, int32_t YStep, int32_t quant)
{
    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock current MB internal edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical left MB/current MB edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

    //vertical current MB internal edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, 0);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaMB_YV12(uint8_t* pU, int32_t UStep, int32_t quant)
{
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);

   //vertical top left MB/top MB edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaMB_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant)
{
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);

   //vertical top left MB/top MB edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaLeftMB_YV12(uint8_t* pU, int32_t UStep, int32_t quant)
{
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaLeftMB_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant)
{
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}



inline void Deblock_I_ChromaBottomMB_YV12(uint8_t* pU, int32_t UStep, int32_t quant)
{
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);

   //vertical top left MB/top MB edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);

   //vertical left MB/current MB edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaBottomMB_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant)
{
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);

   //vertical top left MB/top MB edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);

   //vertical left MB/current MB edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaTopBottomMB_YV12(uint8_t* pU, int32_t UStep, int32_t quant)
{
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //vertical left MB/current MB edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaTopBottomMB_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant)
{
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //vertical left MB/current MB edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_FrameRow_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width, uint32_t quant)
{
    uint8_t* pYMBDebl = pPlanes[0];
    uint8_t* pUMBDebl = pPlanes[1];
    uint8_t* pVMBDebl = pPlanes[2];
    uint32_t i = 1;

    Deblock_I_LumaLeftMB       (pYMBDebl, step[0], quant);
    Deblock_I_ChromaLeftMB_YV12(pUMBDebl, step[1], quant);
    Deblock_I_ChromaLeftMB_YV12(pVMBDebl, step[2], quant);

    for(i = 1; i < width; i++)
    {
        pYMBDebl += VC1_ENC_LUMA_SIZE;
        pUMBDebl += VC1_ENC_CHROMA_SIZE;
        pVMBDebl += VC1_ENC_CHROMA_SIZE;

        Deblock_I_LumaMB       (pYMBDebl, step[0], quant);
        Deblock_I_ChromaMB_YV12(pUMBDebl, step[1], quant);
        Deblock_I_ChromaMB_YV12(pVMBDebl, step[2], quant);
    }
}

inline void Deblock_I_FrameRow_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width, uint32_t quant)
{
    uint8_t* pYMBDebl  = pPlanes[0];
    uint8_t* pUVMBDebl = pPlanes[1];
    uint32_t i = 1;

    Deblock_I_LumaLeftMB       (pYMBDebl, step[0], quant);
    Deblock_I_ChromaLeftMB_NV12(pUVMBDebl, step[1], quant);

    for(i = 1; i < width; i++)
    {
        pYMBDebl += VC1_ENC_LUMA_SIZE;
        pUVMBDebl += VC1_ENC_CHROMA_SIZE*2;

        Deblock_I_LumaMB       (pYMBDebl,  step[0], quant);
        Deblock_I_ChromaMB_NV12(pUVMBDebl, step[1], quant);
    }
}

inline void Deblock_I_FrameBottomRow_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width, uint32_t quant)
{
    uint8_t* pYMBDebl = pPlanes[0];
    uint8_t* pUMBDebl = pPlanes[1];
    uint8_t* pVMBDebl = pPlanes[2];
    uint32_t i = 1;

    Deblock_I_LumaLeftBottomMB     (pYMBDebl, step[0], quant);
    Deblock_I_ChromaLeftMB_YV12    (pUMBDebl, step[1], quant);
    Deblock_I_ChromaLeftMB_YV12    (pVMBDebl, step[2], quant);

    for(i = 1; i < width; i++)
    {
        pYMBDebl += VC1_ENC_LUMA_SIZE;
        pUMBDebl += VC1_ENC_CHROMA_SIZE;
        pVMBDebl += VC1_ENC_CHROMA_SIZE;

        Deblock_I_LumaBottomMB       (pYMBDebl, step[0], quant);
        Deblock_I_ChromaBottomMB_YV12(pUMBDebl, step[1], quant);
        Deblock_I_ChromaBottomMB_YV12(pVMBDebl, step[2], quant);
    }
}

inline void Deblock_I_FrameBottomRow_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width, uint32_t quant)
{
    uint8_t* pYMBDebl  = pPlanes[0];
    uint8_t* pUVMBDebl = pPlanes[1];
    uint32_t i = 1;

    Deblock_I_LumaLeftBottomMB     (pYMBDebl, step[0], quant);
    Deblock_I_ChromaLeftMB_NV12    (pUVMBDebl, step[1], quant);

    for(i = 1; i < width; i++)
    {
        pYMBDebl += VC1_ENC_LUMA_SIZE;
        pUVMBDebl += VC1_ENC_CHROMA_SIZE*2;

        Deblock_I_LumaBottomMB       (pYMBDebl, step[0], quant);
        Deblock_I_ChromaBottomMB_NV12(pUVMBDebl, step[1], quant);
    }
}

inline void Deblock_I_FrameTopBottomRow_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width, uint32_t quant)
{
    uint8_t* pYMBDebl = pPlanes[0];
    uint8_t* pUMBDebl = pPlanes[1];
    uint8_t* pVMBDebl = pPlanes[2];
    uint32_t i = 1;

    Deblock_I_LumaLeftTopBottomMB(pYMBDebl, step[0], quant);
   
    for(i = 1; i < width; i++)
    {
        pYMBDebl += VC1_ENC_LUMA_SIZE;
        pUMBDebl += VC1_ENC_CHROMA_SIZE;
        pVMBDebl += VC1_ENC_CHROMA_SIZE;

        Deblock_I_LumaTopBottomMB       (pYMBDebl, step[0], quant);
        Deblock_I_ChromaTopBottomMB_YV12(pUMBDebl, step[1], quant);
        Deblock_I_ChromaTopBottomMB_YV12(pVMBDebl, step[2], quant);
    }
}

inline void Deblock_I_FrameTopBottomRow_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width, uint32_t quant)
{
    uint8_t* pYMBDebl  = pPlanes[0];
    uint8_t* pUVMBDebl = pPlanes[1];
    uint32_t i = 1;

    Deblock_I_LumaLeftTopBottomMB(pYMBDebl, step[0], quant);
   
    for(i = 1; i < width; i++)
    {
        pYMBDebl  += VC1_ENC_LUMA_SIZE;
        pUVMBDebl += VC1_ENC_CHROMA_SIZE*2;

        Deblock_I_LumaTopBottomMB       (pYMBDebl, step[0], quant);
        Deblock_I_ChromaTopBottomMB_YV12(pUVMBDebl, step[1], quant);
    }
}

inline void Deblock_I_MB_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_MB_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}

inline void Deblock_I_LeftMB_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaLeftMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_LeftMB_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaLeftMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}

inline void Deblock_I_LeftBottomMB_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaLeftBottomMB (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_LeftBottomMB_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaLeftBottomMB (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}


inline void Deblock_I_LeftTopBottomMB_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaLeftTopBottomMB(pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
}

inline void Deblock_I_LeftTopBottomMB_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaLeftTopBottomMB(pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
}

inline void Deblock_I_TopBottomMB_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaTopBottomMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaTopBottomMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaTopBottomMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_TopBottomMB_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaTopBottomMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaTopBottomMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}

inline void Deblock_I_BottomMB_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaBottomMB         (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaBottomMB_YV12  (pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaBottomMB_YV12  (pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_BottomMB_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j)
{
    Deblock_I_LumaBottomMB         (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaBottomMB_NV12  (pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}
inline void no_Deblocking_I_MB(uint8_t* /*pPlanes*/[3], uint32_t /*step*/[3], uint32_t /*quant*/, int32_t /*j*/)
{
};

//-----------------------Deblocking P frames Variable Transform--------------------------------------

inline void Deblock_P_LumaLeftMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    uint32_t TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();

    int32_t TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB top internal hor edge
    pSrc = pY - 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB bottom internal hor edge
    pSrc = pY - 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopLeftVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* pLeftTop,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    uint32_t TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();

    int32_t LeftTopTopVerEdge = pTop->GetLumaExVerEdge();
    int32_t TopVerEdge        = pTop->GetLumaInVerEdge();
    int32_t TopLeftVerEdge    = pTop->GetLumaAdLefEdge();

    int32_t LeftTopRightVerEdge  = pLeftTop->GetLumaAdRigEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB top internal hor edge
    pSrc = pY - 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB bottom internal hor edge
    pSrc = pY - 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left top right internal ver edge
    pSrc = pY - 16*YStep - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB left internal ver edge
    pSrc = pY - 16*YStep + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopLeftVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaRightMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* pLeftTop,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    uint32_t TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();

    int32_t LeftTopRightVerEdge = pLeftTop->GetLumaAdRigEdge();
    int32_t LeftTopTopVerEdge   = pTop->GetLumaExVerEdge();

    int32_t TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();
    int32_t TopRightVerEdge = pTop->GetLumaAdRigEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB top internal hor edge
    pSrc = pY - 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB bottom internal hor edge
    pSrc = pY - 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left top right internal ver edge
    pSrc = pY - 16*YStep - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB left internal ver edge
    pSrc = pY - 16*YStep + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopLeftVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB left internal ver edge
    pSrc = pY - 16*YStep + 12;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopRightVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}


inline void Deblock_P_LumaLeftBottomMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    uint32_t TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();

    int32_t TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();

    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    int32_t CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB top internal hor edge
    pSrc = pY - 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB bottom internal hor edge
    pSrc = pY - 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopBotHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal top hor edge
    pSrc = pY + 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal bottom hor edge
    pSrc = pY + 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopLeftVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal left ver edge
    pSrc = pY + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurLeftVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaLeftTopBottomMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    int32_t CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal top hor edge
    pSrc = pY + 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal bottom hor edge
    pSrc = pY + 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal left ver edge
    pSrc = pY + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurLeftVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaBottomMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* pLeftTop,
                                VC1EncoderMBInfo* pLeft)
{
    int32_t TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    uint32_t TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();

    int32_t TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();

    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    int32_t CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    int32_t LeftTopRightVerEdge = pLeftTop->GetLumaAdRigEdge();
    int32_t LefTopTopVerEdge    = pTop->GetLumaExVerEdge();

    int32_t LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    int32_t LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();


    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB top internal hor edge
    pSrc = pY - 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB bottom internal hor edge
    pSrc = pY - 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopBotHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal top hor edge
    pSrc = pY + 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal bottom hor edge
    pSrc = pY + 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LefTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left top right internal ver edge
    pSrc = pY - 16*YStep - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopLeftVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left MB right internal ver edge
    pSrc = pY - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal left ver edge
    pSrc = pY + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurLeftVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaTopBottomMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* pLeft)
{
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    int32_t CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    int32_t LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    int32_t LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal top hor edge
    pSrc = pY + 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal bottom hor edge
    pSrc = pY + 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left MB left internal ver edge
    pSrc = pY - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal right ver edge
    pSrc = pY + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurLeftVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaRightBottomMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* pLeftTop,
                                VC1EncoderMBInfo* pLeft)
{
    int32_t TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    uint32_t TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();

    int32_t TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();

    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    int32_t CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    int32_t LeftTopRightVerEdge = pLeftTop->GetLumaAdRigEdge();
    int32_t LefTopTopVerEdge    = pTop->GetLumaExVerEdge();

    int32_t LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    int32_t LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();

    int32_t TopRightVerEdge = pTop->GetLumaAdRigEdge();
    int32_t CurRightVerEdge = pCur->GetLumaAdRigEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB top internal hor edge
    pSrc = pY - 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal Top MB bottom internal hor edge
    pSrc = pY - 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopBotHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal top hor edge
    pSrc = pY + 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal bottom hor edge
    pSrc = pY + 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LefTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left top right internal ver edge
    pSrc = pY - 16*YStep - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopLeftVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left MB right internal ver edge
    pSrc = pY - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal left ver edge
    pSrc = pY + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurLeftVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal right ver edge
    pSrc = pY - 16*YStep + 12;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal right ver edge
    pSrc = pY + 12;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurRightVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaRightTopBottomMB_VT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* pLeft)
{
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    int32_t CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    int32_t LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    int32_t LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();

    int32_t CurRightVerEdge = pCur->GetLumaAdRigEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal top hor edge
    pSrc = pY + 4*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurUpHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal bottom hor edge
    pSrc = pY + 12*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurBotHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical left MB right internal ver edge
    pSrc = pY - 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftRightVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal left ver edge
    pSrc = pY + 4;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurLeftVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal right ver edge
    pSrc = pY + 12;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurRightVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftMB_VT_YV12(uint8_t* pU, int32_t UStep, 
                                              uint8_t* pV, int32_t VStep, int32_t quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* pTop,
                                              VC1EncoderMBInfo* /*pTopLeft*/,
                                              VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pU - 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopHorEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                      VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pUV - 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopHorEdgeU, TopHorEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_MB_VT_YV12(uint8_t* pU, int32_t UStep,
                                        uint8_t* pV, int32_t VStep,int32_t quant,
                                        VC1EncoderMBInfo* pCur,
                                        VC1EncoderMBInfo* pTop,
                                        VC1EncoderMBInfo* pTopLeft,
                                        VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();

    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();

    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pU - 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pU - 8*UStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_MB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* pTopLeft,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();

    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();

    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pUV - 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopHorEdgeU, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pUV - 8*UVStep - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftTopVerEdgeU, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightMB_VT_YV12(uint8_t* pU, int32_t UStep,
                                             uint8_t* pV, int32_t VStep,int32_t quant,
                                             VC1EncoderMBInfo* pCur,
                                             VC1EncoderMBInfo* pTop,
                                             VC1EncoderMBInfo* pTopLeft,
                                             VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();

    int32_t TopVerEdgeU        = pTop->GetUAdVerEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();

    int32_t TopVerEdgeV        = pTop->GetVAdVerEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pU - 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pU - 8*UStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB internal ver edge
   pSrc = pU - 8*UStep + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* pTopLeft,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();

    int32_t TopVerEdgeU        = pTop->GetUAdVerEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();

    int32_t TopVerEdgeV        = pTop->GetVAdVerEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pUV - 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopHorEdgeU, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pUV - 8*UVStep - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftTopVerEdgeU, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB internal ver edge
   pSrc = pUV - 8*UVStep + 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopVerEdgeU, TopVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftBottomMB_VT_YV12(uint8_t* pU, int32_t UStep,
                                                  uint8_t* pV, int32_t VStep, int32_t quant,
                                                  VC1EncoderMBInfo* pCur,
                                                  VC1EncoderMBInfo* pTop,
                                                  VC1EncoderMBInfo* /*pTopLeft*/,
                                                  VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pU - 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock cur MB internal hor edge
   pSrc = pU + 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftBottomMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                      VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pUV - 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopHorEdgeU, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock cur MB internal hor edge
   pSrc = pUV + 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurHorEdgeU, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_BottomMB_VT_YV12(uint8_t* pU, int32_t UStep,
                                              uint8_t* pV, int32_t VStep, int32_t quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* pTop,
                                              VC1EncoderMBInfo* pTopLeft,
                                              VC1EncoderMBInfo* pLeft)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    int32_t LeftCurVerEdgeU = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    int32_t LeftCurVerEdgeV = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV    = pLeft->GetUAdVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pU - 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock cur MB internal hor edge
   pSrc = pU + 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pU - 8*UStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pU - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_BottomMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                           VC1EncoderMBInfo* pCur,
                                           VC1EncoderMBInfo* pTop,
                                           VC1EncoderMBInfo* pTopLeft,
                                           VC1EncoderMBInfo* pLeft)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    int32_t LeftCurVerEdgeU = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    int32_t LeftCurVerEdgeV = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV    = pLeft->GetUAdVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pUV - 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopHorEdgeU, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock cur MB internal hor edge
   pSrc = pUV + 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurHorEdgeU, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pUV - 8*UVStep - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftTopVerEdgeU, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pUV - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftVerEdgeU, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}


inline void Deblock_P_Chroma_RightBottomMB_VT_YV12(uint8_t* pU, int32_t UStep,
                                                   uint8_t* pV, int32_t VStep, int32_t quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* pTop,
                                                   VC1EncoderMBInfo* pTopLeft,
                                                   VC1EncoderMBInfo* pLeft)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();
    int32_t TopVerEdgeU        = pTop->GetUAdVerEdge();

    int32_t CurVerEdgeU        = pCur->GetUAdVerEdge();
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    int32_t TopVerEdgeV        = pTop->GetVAdVerEdge();

    int32_t CurVerEdgeV        = pCur->GetVAdVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pU - 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock cur MB internal hor edge
   pSrc = pU + 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pU - 8*UStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB ver edge
   pSrc = pU - 8*UStep + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pU - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pU + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightBottomMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* pTop,
                                                   VC1EncoderMBInfo* pTopLeft,
                                                   VC1EncoderMBInfo* pLeft)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopHorEdgeU         = pTop->GetUAdHorEdge();
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();
    int32_t TopVerEdgeU        = pTop->GetUAdVerEdge();

    int32_t CurVerEdgeU        = pCur->GetUAdVerEdge();
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopHorEdgeV         = pTop->GetVAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    int32_t TopVerEdgeV        = pTop->GetVAdVerEdge();

    int32_t CurVerEdgeV        = pCur->GetVAdVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock Top MB internal hor edge
   pSrc = pUV - 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopHorEdgeU, TopHorEdgeV);
   assert(Sts == ippStsNoErr);

   //horizontal deblock cur MB internal hor edge
   pSrc = pUV + 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurHorEdgeU, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pUV - 8*UVStep - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftTopVerEdgeU, LeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB ver edge
   pSrc = pUV - 8*UVStep + 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopVerEdgeU, TopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pUV - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftVerEdgeU, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pUV + 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurVerEdgeU, CurVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftTopBottomMB_VT_YV12(uint8_t* pU, int32_t UStep,
                                                       uint8_t* pV, int32_t VStep, int32_t quant,
                                                       VC1EncoderMBInfo* pCur,
                                                       VC1EncoderMBInfo* /*pTop*/,
                                                       VC1EncoderMBInfo* /*pTopLeft*/,
                                                       VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock cur MB internal hor edge
   pSrc = pU + 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftTopBottomMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* /*pTop*/,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();
    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock cur MB internal hor edge
   pSrc = pUV + 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurHorEdgeU, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_TopBottomMB_VT_YV12(uint8_t* pU, int32_t UStep, 
                                                 uint8_t* pV, int32_t VStep, int32_t quant,
                                                 VC1EncoderMBInfo* pCur,
                                                 VC1EncoderMBInfo* /*pTop*/,
                                                 VC1EncoderMBInfo* /*pTopLeft*/,
                                                 VC1EncoderMBInfo* pLeft)
{
    int32_t CurHorEdgeU     = pCur->GetUAdHorEdge();

    int32_t LeftCurVerEdgeU = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    int32_t CurHorEdgeV     = pCur->GetVAdHorEdge();

    int32_t LeftCurVerEdgeV = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV    = pLeft->GetVAdVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock cur MB internal hor edge
   pSrc = pU + 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pU - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_TopBottomMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                           VC1EncoderMBInfo* pCur,
                                           VC1EncoderMBInfo* /*pTop*/,
                                           VC1EncoderMBInfo* /*pTopLeft*/,
                                           VC1EncoderMBInfo* pLeft)
{
    int32_t CurHorEdgeU    = pCur->GetUAdHorEdge();

    int32_t LeftCurVerEdgeU = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    int32_t CurHorEdgeV     = pCur->GetVAdHorEdge();

    int32_t LeftCurVerEdgeV = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV    = pLeft->GetVAdVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock cur MB internal hor edge
   pSrc = pUV + 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurHorEdgeU, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top left MB ver edge
   pSrc = pUV - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftVerEdgeU, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightTopBottomMB_VT_YV12(uint8_t* pU, int32_t UStep, 
                                                      uint8_t* pV, int32_t VStep, int32_t quant,
                                                      VC1EncoderMBInfo* pCur,
                                                      VC1EncoderMBInfo* /*pTop*/,
                                                      VC1EncoderMBInfo* /*pTopLeft*/,
                                                      VC1EncoderMBInfo* pLeft)
{
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t CurVerEdgeU        = pCur->GetUAdVerEdge();
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    int32_t CurVerEdgeV        = pCur->GetVAdVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock cur MB internal hor edge
   pSrc = pU + 4*UStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4*VStep;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pU - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pU + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, CurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV + 4;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, CurVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightTopBottomMB_VT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                      VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* /*pTop*/,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* pLeft)
{
    int32_t CurHorEdgeU         = pCur->GetUAdHorEdge();

    int32_t CurVerEdgeU        = pCur->GetUAdVerEdge();
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    int32_t LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    int32_t CurHorEdgeV         = pCur->GetVAdHorEdge();

    int32_t CurVerEdgeV        = pCur->GetVAdVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    int32_t LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock cur MB internal hor edge
   pSrc = pUV + 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurHorEdgeU, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pUV - 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftVerEdgeU, LeftVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical left MB ver edge
   pSrc = pUV + 4;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurVerEdgeU, CurVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}
//-----------------------Deblocking P frames No Variable Transform--------------------------------------

inline void Deblock_P_LumaLeftMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();

    int32_t LeftTopTopVerEdge = pTop->GetLumaExVerEdge();
    int32_t TopVerEdge        = pTop->GetLumaInVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaRightMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pLeftTop*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();
    int32_t LeftTopTopVerEdge   = pTop->GetLumaExVerEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}


inline void Deblock_P_LumaLeftBottomMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaBottomMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t LefTopTopVerEdge    = pTop->GetLumaExVerEdge();
    int32_t LeftCurVerEdge      = pCur->GetLumaExVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LefTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaRightBottomMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopHorEdge      = pTop->GetLumaInHorEdge();
    int32_t TopCurHorEdge   = pCur->GetLumaExHorEdge();
    int32_t TopVerEdge      = pTop->GetLumaInVerEdge();
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t LefTopTopVerEdge    = pTop->GetLumaExVerEdge();
    int32_t LeftCurVerEdge      = pCur->GetLumaExVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal deblock Top MB internal hor edge
    pSrc = pY - 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal TopMB/Cur MB hor edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, TopCurHorEdge);
    assert(Sts == ippStsNoErr);

    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical Left top/Top MB intrnal ver edge
    pSrc = pY - 16*YStep;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LefTopTopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical Top MB internal ver edge
    pSrc = pY - 16*YStep + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, TopVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaLeftTopBottomMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaTopBottomMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t LeftCurVerEdge  = pCur->GetLumaExVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LumaRightTopBottomMB_NoVT(uint8_t* pY, int32_t YStep, int32_t quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t CurHorEdge      = pCur->GetLumaInHorEdge();
    int32_t CurVerEdge      = pCur->GetLumaInVerEdge();
    int32_t LeftCurVerEdge  = pCur->GetLumaExVerEdge();

    uint8_t* pSrc = pY;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    //horizontal cur MB internal hor edge
    pSrc = pY + 8*YStep;
    Sts = _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR  (pSrc,  quant, YStep, CurHorEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur /left MB internal ver edge
    pSrc = pY;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, LeftCurVerEdge);
    assert(Sts == ippStsNoErr);

    //vertical cur MB internal ver edge
    pSrc = pY + 8;
    Sts = _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR (pSrc,  quant, YStep, CurVerEdge);
    assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftMB_NoVT_YV12(uint8_t* pU, int32_t UStep, uint8_t* pV,
                                              int32_t VStep, int32_t quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* /*pTop*/,
                                              VC1EncoderMBInfo* /*pTopLeft*/,
                                              VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftMB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* /*pTop*/,
                                              VC1EncoderMBInfo* /*pTopLeft*/,
                                              VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_MB_NoVT_YV12(uint8_t* pU, int32_t UStep, 
                                          uint8_t* pV, int32_t VStep, int32_t quant,
                                          VC1EncoderMBInfo* pCur,
                                          VC1EncoderMBInfo* pTop,
                                          VC1EncoderMBInfo* /*pTopLeft*/,
                                          VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();

    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_MB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();

    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightMB_NoVT_YV12(uint8_t* pU, int32_t UStep,
                                               uint8_t* pV, int32_t VStep, int32_t quant,
                                               VC1EncoderMBInfo* pCur,
                                               VC1EncoderMBInfo* pTop,
                                               VC1EncoderMBInfo* /*pTopLeft*/,
                                               VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU     = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    int32_t TopCurHorEdgeV     = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightMB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU     = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    int32_t TopCurHorEdgeV     = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftBottomMB_NoVT_YV12(uint8_t* pU, int32_t UStep,
                                                    uint8_t* pV, int32_t VStep, int32_t quant,
                                                    VC1EncoderMBInfo* pCur,
                                                    VC1EncoderMBInfo* /*pTop*/,
                                                    VC1EncoderMBInfo* /*pTopLeft*/,
                                                    VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_LeftBottomMB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                                    VC1EncoderMBInfo* pCur,
                                                    VC1EncoderMBInfo* /*pTop*/,
                                                    VC1EncoderMBInfo* /*pTopLeft*/,
                                                    VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_BottomMB_NoVT_YV12(uint8_t* pU, int32_t UStep,
                                                uint8_t* pV, int32_t VStep, int32_t quant,
                                                VC1EncoderMBInfo* pCur,
                                                VC1EncoderMBInfo* pTop,
                                                VC1EncoderMBInfo* /*pTopLeft*/,
                                                VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftCurVerEdgeU = pCur->GetUExVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftCurVerEdgeV = pCur->GetVExVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_BottomMB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                           VC1EncoderMBInfo* pCur,
                                           VC1EncoderMBInfo* pTop,
                                           VC1EncoderMBInfo* /*pTopLeft*/,
                                           VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV  = pTop->GetVExVerEdge();
    int32_t LeftCurVerEdgeV     = pCur->GetVExVerEdge();

    int32_t TopCurHorEdgeU     = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightBottomMB_NoVT_YV12(uint8_t* pU, int32_t UStep,
                                                     uint8_t* pV, int32_t VStep, int32_t quant,
                                                     VC1EncoderMBInfo* pCur,
                                                     VC1EncoderMBInfo* pTop,
                                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopCurHorEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pU - 8*UStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, TopLeftTopVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV - 8*VStep;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightBottomMB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                                     VC1EncoderMBInfo* pCur,
                                                     VC1EncoderMBInfo* pTop,
                                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                                     VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t TopCurHorEdgeU      = pCur->GetUExHorEdge();
    int32_t TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();

    int32_t TopCurHorEdgeV      = pCur->GetVExHorEdge();
    int32_t TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical top MB/top left MB ver edge
   pSrc = pUV - 8*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopLeftTopVerEdgeU, TopLeftTopVerEdgeV);
   assert(Sts == ippStsNoErr);

  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_TopBottomMB_NoVT_YV12(uint8_t* pU, int32_t UStep,
                                                   uint8_t* pV, int32_t VStep, int32_t quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* /*pTop*/,
                                                   VC1EncoderMBInfo* /*pTopLeft*/,
                                                   VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t LeftCurVerEdgeU = pCur->GetUExVerEdge();
    int32_t LeftCurVerEdgeV = pCur->GetVExVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_TopBottomMB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* /*pTop*/,
                                                   VC1EncoderMBInfo* /*pTopLeft*/,
                                                   VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t LeftCurVerEdgeU = pCur->GetUExVerEdge();
    int32_t LeftCurVerEdgeV = pCur->GetVExVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightTopBottomMB_NoVT_YV12(uint8_t* pU, int32_t UStep,
                                                        uint8_t* pV, int32_t VStep, int32_t quant,
                                                         VC1EncoderMBInfo* pCur,
                                                         VC1EncoderMBInfo* /*pTop*/,
                                                         VC1EncoderMBInfo* /*pTopLeft*/,
                                                         VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    uint8_t* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
  //vertical cur MB/left MB ver edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, LeftCurVerEdgeU);
   assert(Sts == ippStsNoErr);

   pSrc = pV;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, VStep, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightTopBottomMB_NoVT_NV12(uint8_t* pUV, int32_t UVStep, int32_t quant,
                                                         VC1EncoderMBInfo* pCur,
                                                         VC1EncoderMBInfo* /*pTop*/,
                                                         VC1EncoderMBInfo* /*pTopLeft*/,
                                                         VC1EncoderMBInfo* /*pLeft*/)
{
    int32_t LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    int32_t LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    uint8_t* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LeftMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                    pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2],quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                     pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                          pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                      pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaBottomMB_VT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                           pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftTopBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                             pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftTopBottomMB_VT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftTopBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                         pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_VT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                              pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_VT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                      pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2],quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                  pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                         pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                            pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                        pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                             pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftTopBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaLeftTopBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaTopBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                           pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaTopBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_NoVT_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightTopBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                                pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_NoVT_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t j)
{
    Deblock_P_LumaRightTopBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RowVts_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftMB_VT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_MB_VT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightMB_VT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);
 }

inline void Deblock_P_RowVts_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftMB_VT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_MB_VT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightMB_VT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);
 }

inline void Deblock_P_BottomRowVts_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftBottomMB_VT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_BottomMB_VT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightBottomMB_VT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void Deblock_P_BottomRowVts_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftBottomMB_VT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_BottomMB_VT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightBottomMB_VT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void Deblock_P_RowNoVts_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftMB_NoVT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_MB_NoVT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightMB_NoVT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void Deblock_P_RowNoVts_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftMB_NoVT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_MB_NoVT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightMB_NoVT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void Deblock_P_BottomRowNoVts_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftBottomMB_NoVT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_BottomMB_NoVT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightBottomMB_NoVT_YV12(DeblkPlanes, step,  quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void Deblock_P_BottomRowNoVts_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftBottomMB_NoVT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_BottomMB_NoVT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightBottomMB_NoVT_NV12(DeblkPlanes, step,  quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void Deblock_P_TopBottomRowNoVts_YV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftTopBottomMB_NoVT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_TopBottomMB_NoVT_YV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightTopBottomMB_NoVT_YV12(DeblkPlanes, step,  quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void Deblock_P_TopBottomRowNoVts_NV12(uint8_t* pPlanes[3], uint32_t step[3], uint32_t width,  uint32_t quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    uint32_t j = 0;

    uint8_t* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_LeftTopBottomMB_NoVT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

    pMBs->NextMB();

    for(j = 1; j < width - 1; j++)
    {
        pCur     = pMBs->GetCurrMBInfo();
        pTop     = pMBs->GetPevMBInfo(0, 1);
        pLeftTop = pMBs->GetPevMBInfo(1, 1);
        pLeft    = pMBs->GetPevMBInfo(1, 0);

        Deblock_P_TopBottomMB_NoVT_NV12(DeblkPlanes, step, quant, pCur, pTop, pLeftTop, pLeft, j);

        pMBs->NextMB();
    }

    pCur     = pMBs->GetCurrMBInfo();
    pTop     = pMBs->GetPevMBInfo(0, 1);
    pLeftTop = pMBs->GetPevMBInfo(1, 1);
    pLeft    = pMBs->GetPevMBInfo(1, 0);

    Deblock_P_RightTopBottomMB_NoVT_NV12(DeblkPlanes, step,  quant, pCur, pTop, pLeftTop, pLeft, j);
}

inline void no_Deblocking_P_MB(uint8_t* /*pPlanes*/[3], uint32_t /*step*/[3], uint32_t /*quant*/, VC1EncoderMBInfo* /*pCur*/,
                                  VC1EncoderMBInfo* /*pTop*/, VC1EncoderMBInfo* /*pLeftTop*/, VC1EncoderMBInfo* /*pLeft*/, int32_t/* j*/)
{
};

typedef void(*fDeblock_I_MB)(uint8_t* pPlanes[3], uint32_t step[3], uint32_t quant, int32_t j);

extern fDeblock_I_MB Deblk_I_MBFunction_YV12[8];
extern fDeblock_I_MB Deblk_I_MBFunction_NV12[8];

typedef void(*fDeblock_P_MB)(uint8_t* pPlanes[3],      uint32_t step[3],             uint32_t quant,
                             VC1EncoderMBInfo* pCur,     VC1EncoderMBInfo* pTop,
                             VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, int32_t);
//extern fDeblock_P_MB Deblk_P_MBFunction_YV12[2][16];
//extern fDeblock_P_MB Deblk_P_MBFunction_NV12[2][16];

extern fDeblock_P_MB Deblk_P_MBFunction_NoVTS_YV12[16];
extern fDeblock_P_MB Deblk_P_MBFunction_VTS_YV12[16];
extern fDeblock_P_MB Deblk_P_MBFunction_NoVTS_NV12[16];
extern fDeblock_P_MB Deblk_P_MBFunction_VTS_NV12[16];

extern fDeblock_P_MB* Deblk_P_MBFunction_YV12[2];
extern fDeblock_P_MB* Deblk_P_MBFunction_NV12[2];
}
#endif
#endif
