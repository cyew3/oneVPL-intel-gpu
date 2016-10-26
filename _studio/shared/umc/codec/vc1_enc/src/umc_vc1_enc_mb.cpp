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

#include "umc_vc1_enc_mb.h"
#include "umc_structures.h"
#include "umc_vc1_enc_tables.h"
#include "umc_vc1_enc_debug.h"
#include <new>

namespace UMC_VC1_ENCODER
{
UMC::Status VC1EncoderMBData::InitBlocks(Ipp16s* pBuffer)
{
    if (!pBuffer)
        return UMC::UMC_ERR_NULL_PTR;

    m_pBlock[0] = pBuffer;
    m_pBlock[1] = pBuffer + 8;
    m_pBlock[2] = pBuffer + VC1_ENC_LUMA_SIZE*8;
    m_pBlock[3] = pBuffer + VC1_ENC_LUMA_SIZE*8 + 8;
    m_pBlock[4] = pBuffer + VC1_ENC_LUMA_SIZE*VC1_ENC_LUMA_SIZE;
    m_pBlock[5] = m_pBlock[4] + VC1_ENC_CHROMA_SIZE*VC1_ENC_CHROMA_SIZE;

    m_uiBlockStep[0]= VC1_ENC_LUMA_SIZE  *sizeof(Ipp16s);
    m_uiBlockStep[1]= VC1_ENC_LUMA_SIZE  *sizeof(Ipp16s);
    m_uiBlockStep[2]= VC1_ENC_LUMA_SIZE  *sizeof(Ipp16s);
    m_uiBlockStep[3]= VC1_ENC_LUMA_SIZE  *sizeof(Ipp16s);
    m_uiBlockStep[4]= VC1_ENC_CHROMA_SIZE*sizeof(Ipp16s);
    m_uiBlockStep[5]= VC1_ENC_CHROMA_SIZE*sizeof(Ipp16s);

    return UMC::UMC_OK;
}

UMC::Status VC1EncoderMBs::Init(Ipp8u* pPicBufer,   Ipp32u AllocatedMemSize,
                                Ipp32u MBsInRow,    Ipp32u MBsInCol,
                                bool   bNV12)
{

    Ipp32u   i=0, j=0;
    Ipp8u*   ptr = pPicBufer;

    Ipp16s*  pBufferData    =0;
    Ipp16s*  pBufferRecData =0;

    Ipp32s   memSize = AllocatedMemSize;
    Ipp32u   tempSize = 0;

    if ((!MBsInRow) || (!MBsInCol) || (!pPicBufer))
        return UMC::UMC_ERR_INIT;

    Close();

    /*-----------  buffer for MB Data      -----------------------*/

    pBufferData = (Ipp16s*)ptr;

    tempSize = UMC::align_value<Ipp32u>(sizeof(Ipp16s)*(MBsInRow*MBsInCol*VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS + 16));
    ptr = ptr + tempSize;
    memSize -= tempSize;
    if(memSize < 0)
        return UMC::UMC_ERR_ALLOC;

    /*-----------  buffer for Recon MB Data -----------------------*/

    pBufferRecData = (Ipp16s*)ptr;

    tempSize = UMC::align_value<Ipp32u>(sizeof(Ipp16s)*(MBsInRow*MBsInCol*VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS + 16));
    ptr = ptr + tempSize ;
    memSize  -= tempSize ;
    if(memSize < 0)
        return UMC::UMC_ERR_ALLOC;

    /*-----------   MB Info        -----------------------*/
    
    m_MBInfo = new(ptr)VC1EncoderMBInfo*[MBsInCol];
    if (!m_MBInfo)
        return UMC::UMC_ERR_ALLOC;

    tempSize = UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBInfo*) * MBsInCol); 
    ptr = ptr +  tempSize ;
    memSize   -= tempSize;
    if(memSize < 0)
        return UMC::UMC_ERR_ALLOC;

    for (i=0; i<MBsInCol; i++)
    {
        m_MBInfo[i] = new (ptr )VC1EncoderMBInfo[MBsInRow];
        if (!m_MBInfo[i])
            return UMC::UMC_ERR_ALLOC;

        tempSize = UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBInfo)*MBsInRow);
        ptr = ptr + tempSize;
        memSize  -= tempSize;
        if(memSize < 0)
            return UMC::UMC_ERR_ALLOC;
    }

    /*-----------   MB Data  & MB Recon Data    -----------------------*/

    m_MBData = new (ptr) VC1EncoderMBData*[MBsInCol];
    if (!m_MBData)
        return UMC::UMC_ERR_ALLOC;

    tempSize =  UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBData*)*MBsInCol);
    ptr = ptr + tempSize;
    memSize -=  tempSize;
    if(memSize < 0)
        return UMC::UMC_ERR_ALLOC;


    m_MBRecData = new (ptr) VC1EncoderMBData*[MBsInCol];
    if (!m_MBData)
        return UMC::UMC_ERR_ALLOC;

    tempSize = UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBData*)*MBsInCol);
    ptr = ptr + tempSize;
    memSize  -= tempSize;
    if(memSize < 0)
        return UMC::UMC_ERR_ALLOC;


    if (bNV12)
    {        
        for (i=0; i<MBsInCol; i++)
        {

            m_MBData[i] = (VC1EncoderMBDataNV12*)new(ptr) VC1EncoderMBDataNV12[MBsInRow];
            if (!m_MBData[i])
                return UMC::UMC_ERR_ALLOC;

            tempSize = UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBDataNV12)*MBsInRow);
            ptr = ptr + tempSize;
            memSize -=  tempSize;
            if(memSize < 0)
                return UMC::UMC_ERR_ALLOC;


            m_MBRecData[i] = (VC1EncoderMBDataNV12*)new(ptr) VC1EncoderMBDataNV12[MBsInRow];
            if (!m_MBData[i])
                return UMC::UMC_ERR_ALLOC;

            tempSize = UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBDataNV12)*MBsInRow);
            ptr = ptr + tempSize;
            memSize -=  tempSize;
            if(memSize < 0)
                return UMC::UMC_ERR_ALLOC;

          }
    }
    else
    {
        for (i=0; i<MBsInCol; i++)
        {

            m_MBData[i] = new(ptr) VC1EncoderMBData[MBsInRow];
            if (!m_MBData[i])
                return UMC::UMC_ERR_ALLOC;

            tempSize = UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBData)*MBsInRow);
            ptr = ptr + tempSize;
            memSize  -= tempSize;
            if(memSize < 0)
                return UMC::UMC_ERR_ALLOC;

            m_MBRecData[i] = new(ptr) VC1EncoderMBData[MBsInRow];
            if (!m_MBData[i])
                return UMC::UMC_ERR_ALLOC;

            tempSize = UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBData)*MBsInRow);
            ptr = ptr + tempSize;
            memSize -= tempSize;
            if(memSize < 0)
                return UMC::UMC_ERR_ALLOC;
        }    
    }
    
    /*------------- Init Data for MB Data and MB Recon Data--------*/

    for (i=0; i<MBsInCol; i++)
    {
        for(j=0;j<MBsInRow;j++)
        {
            m_MBData[i][j].InitBlocks(pBufferData);
            pBufferData += VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS;

            m_MBRecData[i][j].InitBlocks(pBufferRecData);
            pBufferRecData += VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS;
        }
    }

    m_uiMBsInRow = MBsInRow;
    m_uiMBsInCol = MBsInCol;

    return UMC::UMC_OK;
}

Ipp32u VC1EncoderMBs::CalcAllocMemorySize(Ipp32u MBsInRow, Ipp32u MBsInCol, bool bNV12)
{
    Ipp32u mem_size = 0;
    Ipp32u i = 0;

    //buffers for MB Data and Rec MB Data
    mem_size = 2 * (UMC::align_value<Ipp32u>(sizeof(Ipp16s)*(MBsInRow*MBsInCol*VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS + 16)));

    //m_MBInfo
    
    mem_size += UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBInfo*)*MBsInCol);
    for (i = 0; i < MBsInCol; i++)
    {
        //m_MBInfo[i]
        mem_size += UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBInfo)*MBsInRow);
    }
    
    //m_MBData and m_MBRecData
    
    mem_size += 2* (UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBData*)*MBsInCol));
    if (bNV12)
    {
        for (i = 0; i < MBsInCol; i++)
        {
            //m_MBData[i] and m_MBRecData[i]
            mem_size += 2*(UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBDataNV12)*MBsInRow));
        }
    }
    else
    {
        
        for (i = 0; i < MBsInCol; i++)
        {
            //m_MBData[i] and m_MBRecData[i]
            mem_size += 2*(UMC::align_value<Ipp32u>(sizeof(VC1EncoderMBData)*MBsInRow));
        }
    }
    return mem_size;
}

UMC::Status   VC1EncoderMBs::Close()
{
    m_MBInfo = NULL;
    m_MBData = NULL;
    return UMC::UMC_OK;
}

UMC::Status   VC1EncoderMBs::NextMB()
{
    return (++m_iCurrMBIndex >= m_uiMBsInRow)? UMC::UMC_ERR_FAILED : UMC::UMC_OK;
}

UMC::Status   VC1EncoderMBs::NextRow()
{
    m_iCurrMBIndex  = 0;
    m_iPrevRowIndex = m_iCurrRowIndex;
    m_iCurrRowIndex = (m_iCurrRowIndex + 1)% m_uiMBsInCol;

    return UMC::UMC_OK;
}
UMC::Status            VC1EncoderMBs::StartRow()
{
    m_iCurrMBIndex  = 0;
    return UMC::UMC_OK;
}
UMC::Status   VC1EncoderMBs::Reset()
{
    m_iCurrMBIndex  = 0;
    m_iPrevRowIndex = -1;
    m_iCurrRowIndex = 0;

    return UMC::UMC_OK;
}

VC1EncoderMBInfo*     VC1EncoderMBs::GetCurrMBInfo()
{
    return (&m_MBInfo[m_iCurrRowIndex][m_iCurrMBIndex]);
}
VC1EncoderMBInfo*       VC1EncoderMBs::GetPevMBInfo(Ipp32s x, Ipp32s y)
{
   Ipp32s row = (y>0)? m_iPrevRowIndex:m_iCurrRowIndex;
   return ((m_iCurrMBIndex - x <0 || row <0)? 0 : &m_MBInfo[row][m_iCurrMBIndex - x]);
}

VC1EncoderMBInfo*     VC1EncoderMBs::GetTopMBInfo()
{
    return ((m_iPrevRowIndex == -1)? 0:&m_MBInfo[m_iPrevRowIndex][m_iCurrMBIndex]);
}

VC1EncoderMBInfo*     VC1EncoderMBs::GetLeftMBInfo()
{
    return ((m_iCurrMBIndex  ==  0)? 0:&m_MBInfo[m_iCurrRowIndex][m_iCurrMBIndex - 1]);
}

VC1EncoderMBInfo*     VC1EncoderMBs::GetTopLeftMBInfo()
{
    return ((m_iCurrMBIndex  ==  0 || m_iPrevRowIndex == -1)? 0:&m_MBInfo[m_iPrevRowIndex][m_iCurrMBIndex - 1]);
}

VC1EncoderMBInfo*     VC1EncoderMBs::GetTopRightMBInfo()
{
    return ((m_iCurrMBIndex  >= m_uiMBsInRow-1 || m_iPrevRowIndex == -1)? 0:&m_MBInfo[m_iPrevRowIndex][m_iCurrMBIndex + 1]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetCurrMBData()
{
    return (&m_MBData[m_iCurrRowIndex][m_iCurrMBIndex]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetTopMBData()
{
    return ((m_iPrevRowIndex == -1)? 0:&m_MBData[m_iPrevRowIndex][m_iCurrMBIndex]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetLeftMBData()
{
    return ((m_iCurrMBIndex  ==  0)? 0:&m_MBData[m_iCurrRowIndex][m_iCurrMBIndex - 1]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetTopLeftMBData()
{
    return ((m_iCurrMBIndex  ==  0 || m_iPrevRowIndex == -1)? 0:&m_MBData[m_iPrevRowIndex][m_iCurrMBIndex - 1]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetRecCurrMBData()
{
    return (&m_MBRecData[m_iCurrRowIndex][m_iCurrMBIndex]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetRecTopMBData()
{
    return ((m_iPrevRowIndex == -1)? 0:&m_MBRecData[m_iPrevRowIndex][m_iCurrMBIndex]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetRecLeftMBData()
{
    return ((m_iCurrMBIndex  ==  0)? 0:&m_MBRecData[m_iCurrRowIndex][m_iCurrMBIndex - 1]);
}

VC1EncoderMBData*     VC1EncoderMBs::GetRecTopLeftMBData()
{
    return ((m_iCurrMBIndex  ==  0 || m_iPrevRowIndex == -1)? 0:&m_MBRecData[m_iPrevRowIndex][m_iCurrMBIndex - 1]);
}

UMC::Status VC1EncoderCodedMB::WriteMBHeaderI_SM(VC1EncoderBitStreamSM* pCodedMB, bool /*bBitplanesRaw*/)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    if (!pCodedMB)
        return UMC::UMC_ERR_NULL_PTR;

    ret = pCodedMB->PutBits(VLCTableCBPCY_I[2*(m_uiMBCBPCY&0x3F)], VLCTableCBPCY_I[2*(m_uiMBCBPCY&0x3F)+1]);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedMB->PutBits(m_iACPrediction, 1);
    return ret;
}

UMC::Status VC1EncoderCodedMB::WriteMBHeaderI_ADV(VC1EncoderBitStreamAdv* pCodedMB, bool bBitplanesRaw, bool bOverlapMB)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    if (!pCodedMB)
        return UMC::UMC_ERR_NULL_PTR;

#ifdef VC1_ME_MB_STATICTICS
      Ipp16u MBStart = (Ipp16u)pCodedMB->GetCurrBit();
#endif

    ret = pCodedMB->PutBits(VLCTableCBPCY_I[2*(m_uiMBCBPCY&0x3F)], VLCTableCBPCY_I[2*(m_uiMBCBPCY&0x3F)+1]);
    if (ret != UMC::UMC_OK) return ret;

    if (bBitplanesRaw)
    {
        ret = pCodedMB->PutBits(m_iACPrediction, 1);
        if (ret != UMC::UMC_OK) return ret;
        if (bOverlapMB)
        {
            ret = pCodedMB->PutBits( m_bOverFlag, 1);
            if (ret != UMC::UMC_OK) return ret;
        }
    }

#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole += (Ipp16u)pCodedMB->GetCurrBit()- MBStart;
#endif
    return ret;
}

typedef void (*SaveResidualXxY) (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]);

#define SAVE_RESIDUAL(subblock,intra,num,shiftConst,andConst) \
{ \
    uRunArr[blk][nPairs] = 0;\
        for (Ipp32s i = intra; i<num; i++) {\
            Ipp32s  pos    = pScanMatrix[i];\
            Ipp16s  value = *((Ipp16s*)((Ipp8u*)pSubBlk + step*(pos>>shiftConst)) + (pos&andConst));\
            if (!value) {\
                uRunArr[blk][nPairs]++;\
            } else {\
                iLevelArr[blk][nPairs++] = value;\
                uRunArr[blk][nPairs]   = 0;\
            }\
        }\
    nPairsArr[blk][subblock] = (Ipp8u)(nPairs - nPairsPrev);\
}

void SaveResidual8x8Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);
    SAVE_RESIDUAL(0,0,64,3,7);
}

void SaveResidual8x4Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);
    SAVE_RESIDUAL(0,0,32,3,7);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk + 4*step);
    SAVE_RESIDUAL(1,0,32,3,7);
}

void SaveResidual4x8Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);

    SAVE_RESIDUAL(0,0,32,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk)+4;
    SAVE_RESIDUAL(1,0,32,2,3);
}

void SaveResidual4x4Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);
    SAVE_RESIDUAL(0,0,16,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk)+4;
    SAVE_RESIDUAL(1,0,16,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk + 4*step);
    SAVE_RESIDUAL(2,0,16,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk + 4*step)+4;
    SAVE_RESIDUAL(3,0,16,2,3);
}

static void SaveResidual8x8Intra (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);
    SAVE_RESIDUAL(0,1,64,3,7);
}

static void SaveResidual8x4Intra (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);
    SAVE_RESIDUAL(0,1,32,3,7);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk + 4*step);
    SAVE_RESIDUAL(1,1,32,3,7);
}

static void SaveResidual4x8Intra (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);
    SAVE_RESIDUAL(0,1,32,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk)+4;
    SAVE_RESIDUAL(1,1,32,2,3);
}

static void SaveResidual4x4Intra (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]){
    Ipp32s  nPairs     = 0;
    Ipp32s  nPairsPrev = 0;
    Ipp16s* pSubBlk = (Ipp16s*)((Ipp8u*)pBlk);
    SAVE_RESIDUAL(0,1,16,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk)+4;
    SAVE_RESIDUAL(1,1,16,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk + 4*step);
    SAVE_RESIDUAL(2,1,16,2,3);
    nPairsPrev =  nPairs;

    pSubBlk = (Ipp16s*)((Ipp8u*)pBlk + 4*step)+4;
    SAVE_RESIDUAL(3,1,16,2,3);
}

SaveResidualXxY SaveResidualFuncTab[2][4] = {
    {&SaveResidual8x8Inter,
    &SaveResidual8x4Inter,
    &SaveResidual4x8Inter,
    &SaveResidual4x4Inter},
    {&SaveResidual8x8Intra,
    &SaveResidual8x4Intra,
    &SaveResidual4x8Intra,
    &SaveResidual4x4Intra}
};

void VC1EncoderCodedMB::SaveResidual (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk)
{
    Ipp32s  blkIntra = ( m_MBtype == VC1_ENC_I_MB ||
                        m_MBtype == VC1_ENC_P_MB_INTRA  ||
                        ((m_MBtype == VC1_ENC_P_MB_4MV)&&(m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk))))||
                        m_MBtype == VC1_ENC_B_MB_INTRA);

    if (blkIntra)
    {
        m_iDC[blk] = pBlk[0];
    }

    SaveResidualFuncTab[blkIntra][m_tsType[blk]](pBlk,step,pScanMatrix,blk,m_uRun,m_iLevel,m_nPairs);

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetRunLevelCoefs(m_uRun[blk], m_iLevel[blk],m_nPairs[blk],blk);
#endif
    return;
}

void VC1EncoderCodedMB::GetResiduals (Ipp16s* pBlk, Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk)
{
    static const Ipp32s   nSubblk [4]     = {1,2,2,4};
    static const Ipp32s   xSizeSubblk [4] = {8,8,4,4};
    static const Ipp32s   ySizeSubblk [4] = {8,4,8,4};
    static const Ipp32s   xSubblock [4][4]= {{0,0,0,0}, //8x8
                                            {0,0,0,0}, //8x4
                                            {0,4,0,0}, //4x8
                                            {0,4,0,4}};//4x4
    static const Ipp32s  ySubblock [4][4]= {{0,0,0,0}, //8x8
                                            {0,4,0,0}, //8x4
                                            {0,0,0,0}, //4x8
                                            {0,0,4,4}};//4x4

    Ipp32s nPairs = 0;
    Ipp32s pos    = 0;
    Ipp32s pos1   = 0;
    Ipp32s Num    = 0; //num of run/level

    Ipp16s* pSubBlk = NULL;
    Ipp16s  value   = 0;

    bool   blkIntra = ( m_MBtype == VC1_ENC_I_MB ||
                        m_MBtype == VC1_ENC_P_MB_INTRA  ||
                        ((m_MBtype == VC1_ENC_P_MB_4MV)&&(m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk))))||
                        m_MBtype == VC1_ENC_B_MB_INTRA);
    eTransformType type = m_tsType[blk];

    if (blkIntra)
    {
       pBlk[0] = m_iDC[blk];
    }

    for (Ipp32s subblk = 0; subblk < (nSubblk[type]); subblk++)
    {
        pSubBlk = (Ipp16s*)((Ipp8u*)pBlk + ySubblock[type][subblk]*step)+xSubblock[type][subblk];

        pos = blkIntra - 1;
        nPairs = m_nPairs[blk][subblk];

        for(Ipp32s i = 0; i < nPairs; i++)
        {
            value = m_iLevel[blk][i + Num];

            pos = pos + 1 + m_uRun[blk][i + Num];

            pos1 = pScanMatrix[pos];
            pos1 = (step/2*(pos1/xSizeSubblk[type])) + (pos1&(xSizeSubblk[type]-1));

            pSubBlk[pos1] = value;
        }
        blkIntra = 0;
        Num = Num + nPairs;
    }//for subblock
}

}
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
