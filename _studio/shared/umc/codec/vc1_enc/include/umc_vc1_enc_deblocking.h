//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_ENC_DEBLOCKING_H__
#define __UMC_VC1_ENC_DEBLOCKING_H__

#include "umc_vc1_enc_mb.h"
#include "umc_vc1_common_defs.h"
namespace UMC_VC1_ENCODER
{

void GetInternalBlockEdge(VC1EncoderMBInfo *pCur,
                          Ipp8u& YFlagUp, Ipp8u& YFlagBot, Ipp8u& UFlagH, Ipp8u& VFlagH,
                          Ipp8u& YFlagL,  Ipp8u& YFlagR,   Ipp8u& UFlagV, Ipp8u& VFlagV);

typedef void (*fGetExternalEdge)(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer,
                                Ipp8u& YFlag, Ipp8u& UFlag, Ipp8u& VFlag);
typedef void (*fGetInternalEdge)(VC1EncoderMBInfo *pCur, Ipp8u& YFlagV, Ipp8u& YFlagH);

extern fGetExternalEdge GetExternalEdge[2][2]; //4 MV, VTS
extern fGetInternalEdge GetInternalEdge[2][2]; //4 MV, VTS

extern fGetExternalEdge GetFieldExternalEdge[2][2]; //4 MV, VTS
extern fGetInternalEdge GetFieldInternalEdge[2][2]; //4 MV, VTS

extern fGetExternalEdge GetExternalEdge_SM[2][2]; //4 MV, VTS
extern fGetInternalEdge GetInternalEdge_SM[2][2]; //4 MV, VTS

//-----------------------Deblocking I frames-----------------------------------------
inline void Deblock_I_LumaMB(Ipp8u* pY, Ipp32s YStep, Ipp32s quant)
{
    Ipp8u* pSrc = pY;
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

inline void Deblock_I_LumaLeftMB(Ipp8u* pY, Ipp32s YStep, Ipp32s quant)
{
    Ipp8u* pSrc = pY;
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

inline void Deblock_I_LumaLeftBottomMB(Ipp8u* pY, Ipp32s YStep, Ipp32s quant)
{
    Ipp8u* pSrc = pY;
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

inline void Deblock_I_LumaLeftTopBottomMB(Ipp8u* pY, Ipp32s YStep, Ipp32s quant)
{
    Ipp8u* pSrc = pY;
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
inline void Deblock_I_LumaBottomMB(Ipp8u* pY, Ipp32s YStep, Ipp32s quant)
{
    Ipp8u* pSrc = pY;
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

inline void Deblock_I_LumaTopBottomMB(Ipp8u* pY, Ipp32s YStep, Ipp32s quant)
{
    Ipp8u* pSrc = pY;
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

inline void Deblock_I_ChromaMB_YV12(Ipp8u* pU, Ipp32s UStep, Ipp32s quant)
{
    Ipp8u* pSrc = pU;
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

inline void Deblock_I_ChromaMB_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant)
{
    Ipp8u* pSrc = pUV;
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

inline void Deblock_I_ChromaLeftMB_YV12(Ipp8u* pU, Ipp32s UStep, Ipp32s quant)
{
    Ipp8u* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaLeftMB_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant)
{
    Ipp8u* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock top MB/current MB edge
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}



inline void Deblock_I_ChromaBottomMB_YV12(Ipp8u* pU, Ipp32s UStep, Ipp32s quant)
{
    Ipp8u* pSrc = pU;
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

inline void Deblock_I_ChromaBottomMB_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant)
{
    Ipp8u* pSrc = pUV;
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

inline void Deblock_I_ChromaTopBottomMB_YV12(Ipp8u* pU, Ipp32s UStep, Ipp32s quant)
{
    Ipp8u* pSrc = pU;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //vertical left MB/current MB edge
   pSrc = pU;
   Sts =  _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(pSrc, quant, UStep, 0);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_ChromaTopBottomMB_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant)
{
    Ipp8u* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //vertical left MB/current MB edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, 0, 0);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_I_FrameRow_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width, Ipp32u quant)
{
    Ipp8u* pYMBDebl = pPlanes[0];
    Ipp8u* pUMBDebl = pPlanes[1];
    Ipp8u* pVMBDebl = pPlanes[2];
    Ipp32u i = 1;

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

inline void Deblock_I_FrameRow_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width, Ipp32u quant)
{
    Ipp8u* pYMBDebl  = pPlanes[0];
    Ipp8u* pUVMBDebl = pPlanes[1];
    Ipp32u i = 1;

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

inline void Deblock_I_FrameBottomRow_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width, Ipp32u quant)
{
    Ipp8u* pYMBDebl = pPlanes[0];
    Ipp8u* pUMBDebl = pPlanes[1];
    Ipp8u* pVMBDebl = pPlanes[2];
    Ipp32u i = 1;

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

inline void Deblock_I_FrameBottomRow_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width, Ipp32u quant)
{
    Ipp8u* pYMBDebl  = pPlanes[0];
    Ipp8u* pUVMBDebl = pPlanes[1];
    Ipp32u i = 1;

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

inline void Deblock_I_FrameTopBottomRow_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width, Ipp32u quant)
{
    Ipp8u* pYMBDebl = pPlanes[0];
    Ipp8u* pUMBDebl = pPlanes[1];
    Ipp8u* pVMBDebl = pPlanes[2];
    Ipp32u i = 1;

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

inline void Deblock_I_FrameTopBottomRow_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width, Ipp32u quant)
{
    Ipp8u* pYMBDebl  = pPlanes[0];
    Ipp8u* pUVMBDebl = pPlanes[1];
    Ipp32u i = 1;

    Deblock_I_LumaLeftTopBottomMB(pYMBDebl, step[0], quant);
   
    for(i = 1; i < width; i++)
    {
        pYMBDebl  += VC1_ENC_LUMA_SIZE;
        pUVMBDebl += VC1_ENC_CHROMA_SIZE*2;

        Deblock_I_LumaTopBottomMB       (pYMBDebl, step[0], quant);
        Deblock_I_ChromaTopBottomMB_YV12(pUVMBDebl, step[1], quant);
    }
}

inline void Deblock_I_MB_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_MB_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}

inline void Deblock_I_LeftMB_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaLeftMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_LeftMB_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaLeftMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}

inline void Deblock_I_LeftBottomMB_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaLeftBottomMB (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaLeftMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_LeftBottomMB_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaLeftBottomMB (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaLeftMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}


inline void Deblock_I_LeftTopBottomMB_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaLeftTopBottomMB(pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
}

inline void Deblock_I_LeftTopBottomMB_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaLeftTopBottomMB(pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
}

inline void Deblock_I_TopBottomMB_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaTopBottomMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaTopBottomMB_YV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaTopBottomMB_YV12(pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_TopBottomMB_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaTopBottomMB       (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaTopBottomMB_NV12(pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}

inline void Deblock_I_BottomMB_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaBottomMB         (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaBottomMB_YV12  (pPlanes[1] + j * VC1_ENC_CHROMA_SIZE, step[1], quant);
    Deblock_I_ChromaBottomMB_YV12  (pPlanes[2] + j * VC1_ENC_CHROMA_SIZE, step[2], quant);
}

inline void Deblock_I_BottomMB_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j)
{
    Deblock_I_LumaBottomMB         (pPlanes[0] + j * VC1_ENC_LUMA_SIZE, step[0], quant);
    Deblock_I_ChromaBottomMB_NV12  (pPlanes[1] + j * VC1_ENC_CHROMA_SIZE*2, step[1], quant);
}
inline void no_Deblocking_I_MB(Ipp8u* /*pPlanes*/[3], Ipp32u /*step*/[3], Ipp32u /*quant*/, Ipp32s /*j*/)
{
};

//-----------------------Deblocking P frames Variable Transform--------------------------------------

inline void Deblock_P_LumaLeftMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32u TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();

    Ipp32s TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* pLeftTop,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32u TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();

    Ipp32s LeftTopTopVerEdge = pTop->GetLumaExVerEdge();
    Ipp32s TopVerEdge        = pTop->GetLumaInVerEdge();
    Ipp32s TopLeftVerEdge    = pTop->GetLumaAdLefEdge();

    Ipp32s LeftTopRightVerEdge  = pLeftTop->GetLumaAdRigEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaRightMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* pLeftTop,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32u TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();

    Ipp32s LeftTopRightVerEdge = pLeftTop->GetLumaAdRigEdge();
    Ipp32s LeftTopTopVerEdge   = pTop->GetLumaExVerEdge();

    Ipp32s TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();
    Ipp32s TopRightVerEdge = pTop->GetLumaAdRigEdge();

    Ipp8u* pSrc = pY;
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


inline void Deblock_P_LumaLeftBottomMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32u TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();

    Ipp32s TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();

    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    Ipp32s CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaLeftTopBottomMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    Ipp32s CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaBottomMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* pLeftTop,
                                VC1EncoderMBInfo* pLeft)
{
    Ipp32s TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32u TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();

    Ipp32s TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();

    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    Ipp32s CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    Ipp32s LeftTopRightVerEdge = pLeftTop->GetLumaAdRigEdge();
    Ipp32s LefTopTopVerEdge    = pTop->GetLumaExVerEdge();

    Ipp32s LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    Ipp32s LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();


    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaTopBottomMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* pLeft)
{
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    Ipp32s CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    Ipp32s LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    Ipp32s LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaRightBottomMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* pLeftTop,
                                VC1EncoderMBInfo* pLeft)
{
    Ipp32s TopUpHorEdge    = pTop->GetLumaAdUppEdge();
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32u TopBotHorEdge   = pTop->GetLumaAdBotEdge();

    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();

    Ipp32s TopLeftVerEdge  = pTop->GetLumaAdLefEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();

    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    Ipp32s CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    Ipp32s LeftTopRightVerEdge = pLeftTop->GetLumaAdRigEdge();
    Ipp32s LefTopTopVerEdge    = pTop->GetLumaExVerEdge();

    Ipp32s LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    Ipp32s LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();

    Ipp32s TopRightVerEdge = pTop->GetLumaAdRigEdge();
    Ipp32s CurRightVerEdge = pCur->GetLumaAdRigEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaRightTopBottomMB_VT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* pLeft)
{
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurUpHorEdge    = pCur->GetLumaAdUppEdge();
    Ipp32s CurBotHorEdge   = pCur->GetLumaAdBotEdge();

    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s CurLeftVerEdge  = pCur->GetLumaAdLefEdge();

    Ipp32s LeftCurVerEdge      = pCur->GetLumaExVerEdge();
    Ipp32s LeftRightVerEdge    = pLeft->GetLumaAdRigEdge();

    Ipp32s CurRightVerEdge = pCur->GetLumaAdRigEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_Chroma_LeftMB_VT_YV12(Ipp8u* pU, Ipp32s UStep, 
                                              Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* pTop,
                                              VC1EncoderMBInfo* /*pTopLeft*/,
                                              VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();
    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_LeftMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                      VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_MB_VT_YV12(Ipp8u* pU, Ipp32s UStep,
                                        Ipp8u* pV, Ipp32s VStep,Ipp32s quant,
                                        VC1EncoderMBInfo* pCur,
                                        VC1EncoderMBInfo* pTop,
                                        VC1EncoderMBInfo* pTopLeft,
                                        VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();

    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();

    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_MB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* pTopLeft,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();

    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();

    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_RightMB_VT_YV12(Ipp8u* pU, Ipp32s UStep,
                                             Ipp8u* pV, Ipp32s VStep,Ipp32s quant,
                                             VC1EncoderMBInfo* pCur,
                                             VC1EncoderMBInfo* pTop,
                                             VC1EncoderMBInfo* pTopLeft,
                                             VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();

    Ipp32s TopVerEdgeU        = pTop->GetUAdVerEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();

    Ipp32s TopVerEdgeV        = pTop->GetVAdVerEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_RightMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* pTopLeft,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();

    Ipp32s TopVerEdgeU        = pTop->GetUAdVerEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();

    Ipp32s TopVerEdgeV        = pTop->GetVAdVerEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_LeftBottomMB_VT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                  Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                  VC1EncoderMBInfo* pCur,
                                                  VC1EncoderMBInfo* pTop,
                                                  VC1EncoderMBInfo* /*pTopLeft*/,
                                                  VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_LeftBottomMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                      VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_BottomMB_VT_YV12(Ipp8u* pU, Ipp32s UStep,
                                              Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* pTop,
                                              VC1EncoderMBInfo* pTopLeft,
                                              VC1EncoderMBInfo* pLeft)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    Ipp32s LeftCurVerEdgeU = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    Ipp32s LeftCurVerEdgeV = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV    = pLeft->GetUAdVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_BottomMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                           VC1EncoderMBInfo* pCur,
                                           VC1EncoderMBInfo* pTop,
                                           VC1EncoderMBInfo* pTopLeft,
                                           VC1EncoderMBInfo* pLeft)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();

    Ipp32s LeftCurVerEdgeU = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();

    Ipp32s LeftCurVerEdgeV = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV    = pLeft->GetUAdVerEdge();

    Ipp8u* pSrc = pUV;
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


inline void Deblock_P_Chroma_RightBottomMB_VT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                   Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* pTop,
                                                   VC1EncoderMBInfo* pTopLeft,
                                                   VC1EncoderMBInfo* pLeft)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();
    Ipp32s TopVerEdgeU        = pTop->GetUAdVerEdge();

    Ipp32s CurVerEdgeU        = pCur->GetUAdVerEdge();
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    Ipp32s TopVerEdgeV        = pTop->GetVAdVerEdge();

    Ipp32s CurVerEdgeV        = pCur->GetVAdVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_RightBottomMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* pTop,
                                                   VC1EncoderMBInfo* pTopLeft,
                                                   VC1EncoderMBInfo* pLeft)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopHorEdgeU         = pTop->GetUAdHorEdge();
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftTopVerEdgeU    = pTopLeft->GetUAdVerEdge();
    Ipp32s TopVerEdgeU        = pTop->GetUAdVerEdge();

    Ipp32s CurVerEdgeU        = pCur->GetUAdVerEdge();
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopHorEdgeV         = pTop->GetVAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftTopVerEdgeV    = pTopLeft->GetVAdVerEdge();
    Ipp32s TopVerEdgeV        = pTop->GetVAdVerEdge();

    Ipp32s CurVerEdgeV        = pCur->GetVAdVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_LeftTopBottomMB_VT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                       Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                       VC1EncoderMBInfo* pCur,
                                                       VC1EncoderMBInfo* /*pTop*/,
                                                       VC1EncoderMBInfo* /*pTopLeft*/,
                                                       VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();
    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_LeftTopBottomMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* /*pTop*/,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();
    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp8u* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock cur MB internal hor edge
   pSrc = pUV + 4*UVStep;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, CurHorEdgeU, CurHorEdgeV);
   assert(Sts == ippStsNoErr);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_TopBottomMB_VT_YV12(Ipp8u* pU, Ipp32s UStep, 
                                                 Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                 VC1EncoderMBInfo* pCur,
                                                 VC1EncoderMBInfo* /*pTop*/,
                                                 VC1EncoderMBInfo* /*pTopLeft*/,
                                                 VC1EncoderMBInfo* pLeft)
{
    Ipp32s CurHorEdgeU     = pCur->GetUAdHorEdge();

    Ipp32s LeftCurVerEdgeU = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    Ipp32s CurHorEdgeV     = pCur->GetVAdHorEdge();

    Ipp32s LeftCurVerEdgeV = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV    = pLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_TopBottomMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                           VC1EncoderMBInfo* pCur,
                                           VC1EncoderMBInfo* /*pTop*/,
                                           VC1EncoderMBInfo* /*pTopLeft*/,
                                           VC1EncoderMBInfo* pLeft)
{
    Ipp32s CurHorEdgeU    = pCur->GetUAdHorEdge();

    Ipp32s LeftCurVerEdgeU = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU    = pLeft->GetUAdVerEdge();

    Ipp32s CurHorEdgeV     = pCur->GetVAdHorEdge();

    Ipp32s LeftCurVerEdgeV = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV    = pLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_RightTopBottomMB_VT_YV12(Ipp8u* pU, Ipp32s UStep, 
                                                      Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                      VC1EncoderMBInfo* pCur,
                                                      VC1EncoderMBInfo* /*pTop*/,
                                                      VC1EncoderMBInfo* /*pTopLeft*/,
                                                      VC1EncoderMBInfo* pLeft)
{
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s CurVerEdgeU        = pCur->GetUAdVerEdge();
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp32s CurVerEdgeV        = pCur->GetVAdVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_RightTopBottomMB_VT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                      VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* /*pTop*/,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* pLeft)
{
    Ipp32s CurHorEdgeU         = pCur->GetUAdHorEdge();

    Ipp32s CurVerEdgeU        = pCur->GetUAdVerEdge();
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    Ipp32s LeftVerEdgeU       = pLeft->GetUAdVerEdge();

    Ipp32s CurHorEdgeV         = pCur->GetVAdHorEdge();

    Ipp32s CurVerEdgeV        = pCur->GetVAdVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();
    Ipp32s LeftVerEdgeV       = pLeft->GetVAdVerEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_LumaLeftMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();

    Ipp32s LeftTopTopVerEdge = pTop->GetLumaExVerEdge();
    Ipp32s TopVerEdge        = pTop->GetLumaInVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaRightMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pLeftTop*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();
    Ipp32s LeftTopTopVerEdge   = pTop->GetLumaExVerEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();

    Ipp8u* pSrc = pY;
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


inline void Deblock_P_LumaLeftBottomMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaBottomMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s LefTopTopVerEdge    = pTop->GetLumaExVerEdge();
    Ipp32s LeftCurVerEdge      = pCur->GetLumaExVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaRightBottomMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* pTop,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopHorEdge      = pTop->GetLumaInHorEdge();
    Ipp32s TopCurHorEdge   = pCur->GetLumaExHorEdge();
    Ipp32s TopVerEdge      = pTop->GetLumaInVerEdge();
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s LefTopTopVerEdge    = pTop->GetLumaExVerEdge();
    Ipp32s LeftCurVerEdge      = pCur->GetLumaExVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaLeftTopBottomMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaTopBottomMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s LeftCurVerEdge  = pCur->GetLumaExVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_LumaRightTopBottomMB_NoVT(Ipp8u* pY, Ipp32s YStep, Ipp32s quant,
                                VC1EncoderMBInfo* pCur,
                                VC1EncoderMBInfo* /*pTop*/,
                                VC1EncoderMBInfo* /*pLeftTop*/,
                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s CurHorEdge      = pCur->GetLumaInHorEdge();
    Ipp32s CurVerEdge      = pCur->GetLumaInVerEdge();
    Ipp32s LeftCurVerEdge  = pCur->GetLumaExVerEdge();

    Ipp8u* pSrc = pY;
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

inline void Deblock_P_Chroma_LeftMB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep, Ipp8u* pV,
                                              Ipp32s VStep, Ipp32s quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* /*pTop*/,
                                              VC1EncoderMBInfo* /*pTopLeft*/,
                                              VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_LeftMB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                              VC1EncoderMBInfo* pCur,
                                              VC1EncoderMBInfo* /*pTop*/,
                                              VC1EncoderMBInfo* /*pTopLeft*/,
                                              VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp8u* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_MB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep, 
                                          Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                          VC1EncoderMBInfo* pCur,
                                          VC1EncoderMBInfo* pTop,
                                          VC1EncoderMBInfo* /*pTopLeft*/,
                                          VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();

    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_MB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();

    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_RightMB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep,
                                               Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                               VC1EncoderMBInfo* pCur,
                                               VC1EncoderMBInfo* pTop,
                                               VC1EncoderMBInfo* /*pTopLeft*/,
                                               VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU     = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    Ipp32s TopCurHorEdgeV     = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_RightMB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                     VC1EncoderMBInfo* pCur,
                                     VC1EncoderMBInfo* pTop,
                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU     = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();

    Ipp32s TopCurHorEdgeV     = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_LeftBottomMB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                    Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                    VC1EncoderMBInfo* pCur,
                                                    VC1EncoderMBInfo* /*pTop*/,
                                                    VC1EncoderMBInfo* /*pTopLeft*/,
                                                    VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_LeftBottomMB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                                    VC1EncoderMBInfo* pCur,
                                                    VC1EncoderMBInfo* /*pTop*/,
                                                    VC1EncoderMBInfo* /*pTopLeft*/,
                                                    VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();

    Ipp8u* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
   //horizontal deblock Cur MB/Top MB internal hor edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(pSrc, quant, UVStep, TopCurHorEdgeU, TopCurHorEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_BottomMB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                VC1EncoderMBInfo* pCur,
                                                VC1EncoderMBInfo* pTop,
                                                VC1EncoderMBInfo* /*pTopLeft*/,
                                                VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeU = pCur->GetUExVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftCurVerEdgeV = pCur->GetVExVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_BottomMB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                           VC1EncoderMBInfo* pCur,
                                           VC1EncoderMBInfo* pTop,
                                           VC1EncoderMBInfo* /*pTopLeft*/,
                                           VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV  = pTop->GetVExVerEdge();
    Ipp32s LeftCurVerEdgeV     = pCur->GetVExVerEdge();

    Ipp32s TopCurHorEdgeU     = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_RightBottomMB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                     Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                     VC1EncoderMBInfo* pCur,
                                                     VC1EncoderMBInfo* pTop,
                                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_RightBottomMB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                                     VC1EncoderMBInfo* pCur,
                                                     VC1EncoderMBInfo* pTop,
                                                     VC1EncoderMBInfo* /*pTopLeft*/,
                                                     VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s TopCurHorEdgeU      = pCur->GetUExHorEdge();
    Ipp32s TopLeftTopVerEdgeU = pTop->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();

    Ipp32s TopCurHorEdgeV      = pCur->GetVExHorEdge();
    Ipp32s TopLeftTopVerEdgeV = pTop->GetVExVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    Ipp8u* pSrc = pUV;
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

inline void Deblock_P_Chroma_TopBottomMB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                   Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* /*pTop*/,
                                                   VC1EncoderMBInfo* /*pTopLeft*/,
                                                   VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s LeftCurVerEdgeU = pCur->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeV = pCur->GetVExVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_TopBottomMB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                                   VC1EncoderMBInfo* pCur,
                                                   VC1EncoderMBInfo* /*pTop*/,
                                                   VC1EncoderMBInfo* /*pTopLeft*/,
                                                   VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s LeftCurVerEdgeU = pCur->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeV = pCur->GetVExVerEdge();

    Ipp8u* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_Chroma_RightTopBottomMB_NoVT_YV12(Ipp8u* pU, Ipp32s UStep,
                                                        Ipp8u* pV, Ipp32s VStep, Ipp32s quant,
                                                         VC1EncoderMBInfo* pCur,
                                                         VC1EncoderMBInfo* /*pTop*/,
                                                         VC1EncoderMBInfo* /*pTopLeft*/,
                                                         VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    Ipp8u* pSrc = pU;
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

inline void Deblock_P_Chroma_RightTopBottomMB_NoVT_NV12(Ipp8u* pUV, Ipp32s UVStep, Ipp32s quant,
                                                         VC1EncoderMBInfo* pCur,
                                                         VC1EncoderMBInfo* /*pTop*/,
                                                         VC1EncoderMBInfo* /*pTopLeft*/,
                                                         VC1EncoderMBInfo* /*pLeft*/)
{
    Ipp32s LeftCurVerEdgeU    = pCur->GetUExVerEdge();
    Ipp32s LeftCurVerEdgeV    = pCur->GetVExVerEdge();

    Ipp8u* pSrc = pUV;
    IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
  //vertical cur MB/left MB ver edge
   pSrc = pUV;
   Sts =  _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(pSrc, quant, UVStep, LeftCurVerEdgeU, LeftCurVerEdgeV);
   assert(Sts == ippStsNoErr);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
}

inline void Deblock_P_LeftMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                    pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2],quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                     pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                          pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                      pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaBottomMB_VT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                           pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftTopBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                             pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftTopBottomMB_VT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftTopBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                         pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_VT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_VT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                              pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_VT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightTopBottomMB_VT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_VT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                      pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2],quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                  pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_MB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_MB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                         pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                            pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftBottomMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_LeftBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                        pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_BottomMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_BottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                             pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightBottomMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftTopBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_LeftTopBottomMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                 VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaLeftTopBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaTopBottomMB_NoVT     (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                           pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_TopBottomMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                             VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaTopBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_TopBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_NoVT_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightTopBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_NoVT_YV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE, step[1],
                                                pPlanes[2] + j*VC1_ENC_CHROMA_SIZE, step[2], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RightTopBottomMB_NoVT_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, VC1EncoderMBInfo* pCur,
                                  VC1EncoderMBInfo* pTop, VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s j)
{
    Deblock_P_LumaRightTopBottomMB_NoVT        (pPlanes[0] + j*VC1_ENC_LUMA_SIZE, step[0], quant, pCur, pTop, pLeftTop, pLeft);
    Deblock_P_Chroma_RightTopBottomMB_NoVT_NV12(pPlanes[1] + j*VC1_ENC_CHROMA_SIZE*2, step[1], quant, pCur, pTop, pLeftTop, pLeft);
}

inline void Deblock_P_RowVts_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_RowVts_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_BottomRowVts_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_BottomRowVts_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_RowNoVts_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_RowNoVts_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_BottomRowNoVts_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_BottomRowNoVts_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_TopBottomRowNoVts_YV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void Deblock_P_TopBottomRowNoVts_NV12(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u width,  Ipp32u quant, VC1EncoderMBs* pMBs)
{
    VC1EncoderMBInfo  * pCur     = NULL;
    VC1EncoderMBInfo  * pTop     = NULL;
    VC1EncoderMBInfo  * pLeftTop = NULL;
    VC1EncoderMBInfo  * pLeft    = NULL;
    Ipp32u j = 0;

    Ipp8u* DeblkPlanes[3] = {pPlanes[0], pPlanes[1], pPlanes[2]};

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

inline void no_Deblocking_P_MB(Ipp8u* /*pPlanes*/[3], Ipp32u /*step*/[3], Ipp32u /*quant*/, VC1EncoderMBInfo* /*pCur*/,
                                  VC1EncoderMBInfo* /*pTop*/, VC1EncoderMBInfo* /*pLeftTop*/, VC1EncoderMBInfo* /*pLeft*/, Ipp32s/* j*/)
{
};

typedef void(*fDeblock_I_MB)(Ipp8u* pPlanes[3], Ipp32u step[3], Ipp32u quant, Ipp32s j);

extern fDeblock_I_MB Deblk_I_MBFunction_YV12[8];
extern fDeblock_I_MB Deblk_I_MBFunction_NV12[8];

typedef void(*fDeblock_P_MB)(Ipp8u* pPlanes[3],      Ipp32u step[3],             Ipp32u quant,
                             VC1EncoderMBInfo* pCur,     VC1EncoderMBInfo* pTop,
                             VC1EncoderMBInfo* pLeftTop, VC1EncoderMBInfo* pLeft, Ipp32s);
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
