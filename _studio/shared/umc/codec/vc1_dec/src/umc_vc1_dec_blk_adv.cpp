// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)
#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
#pragma warning (disable:1478)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include "ippi.h"

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_zigzag_tbl.h"
#include "umc_vc1_common_blk_order_tbl.h"
#include "umc_vc1_dec_run_level_tbl.h"

#include "umc_vc1_dec_debug.h"
#include "umc_vc1_huffman.h"
#include "umc_vc1_dec_exception.h"
using namespace UMC::VC1Exceptions;

typedef uint8_t (*DCPrediction)(VC1DCBlkParam* CurrBlk, VC1DCPredictors* PredData,
                              int32_t blk_num, int16_t* pBlock, uint32_t FCM);

//static mfxSize QuantSize[4] = {VC1_PIXEL_IN_BLOCK, VC1_PIXEL_IN_BLOCK,
//                                VC1_PIXEL_IN_BLOCK/2, VC1_PIXEL_IN_BLOCK,
//                                VC1_PIXEL_IN_BLOCK, VC1_PIXEL_IN_BLOCK/2,
//                               VC1_PIXEL_IN_BLOCK/2, VC1_PIXEL_IN_BLOCK/2};

typedef IppStatus (*Reconstruct)(int16_t* pSrcDst,
                                 int32_t srcDstStep,
                                 int32_t doubleQuant,
                                 uint32_t BlkType);

static Reconstruct Reconstruct_table[] = {
        _own_ippiReconstructInterUniform_VC1_16s_C1IR,
        _own_ippiReconstructInterNonuniform_VC1_16s_C1IR
};

IppStatus _own_ippiReconstructIntraUniform_VC1_16s_C1IR    (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant)
{
    mfxSize  DstSizeNZ;
    _own_ippiQuantInvIntraUniform_VC1_16s_C1IR(pSrcDst,
                                               srcDstStep,
                                               doubleQuant,
                                               &DstSizeNZ);

    //transformation
    ippiTransform8x8Inv_VC1_16s_C1IR(pSrcDst,
                                     srcDstStep,
                                     DstSizeNZ);
    return ippStsNoErr;
}
IppStatus _own_ippiReconstructIntraNonuniform_VC1_16s_C1IR (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant)
{
    mfxSize  DstSizeNZ;
    _own_ippiQuantInvIntraNonuniform_VC1_16s_C1IR(pSrcDst,
                                                  srcDstStep,
                                                  doubleQuant,
                                                  &DstSizeNZ);

   //transformation
    ippiTransform8x8Inv_VC1_16s_C1IR(pSrcDst,
                                     srcDstStep,
                                     DstSizeNZ);
    return ippStsNoErr;
}
// uint32_t BlkType -     VC1_BLK_INTER8X8   = 0x1,
//                      VC1_BLK_INTER8X4   = 0x2,
//                      VC1_BLK_INTER4X8   = 0x4,
//                      VC1_BLK_INTER4X4   = 0x8,
IppStatus _own_ippiReconstructInterUniform_VC1_16s_C1IR    (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant,uint32_t BlkType)
{
    mfxSize  DstSizeNZ[4];
    static mfxSize QuantSize[4] = {
                                    { VC1_PIXEL_IN_BLOCK,   VC1_PIXEL_IN_BLOCK },
                                    { VC1_PIXEL_IN_BLOCK/2, VC1_PIXEL_IN_BLOCK },
                                    { VC1_PIXEL_IN_BLOCK,   VC1_PIXEL_IN_BLOCK/2 },
                                    { VC1_PIXEL_IN_BLOCK/2, VC1_PIXEL_IN_BLOCK/2 }
                                  };

    //quantization and transformation
    if (VC1_BLK_INTER8X8 == BlkType)
    {
        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[0],
                                                   &DstSizeNZ[0]);

        ippiTransform8x8Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);


    }
    else if(VC1_BLK_INTER4X8 == BlkType)
    {
        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[1],
                                                   &DstSizeNZ[0]);

        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst+4,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[1],
                                                   &DstSizeNZ[1]);

        ippiTransform4x8Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);

        ippiTransform4x8Inv_VC1_16s_C1IR(pSrcDst+4,
                                         srcDstStep,
                                         DstSizeNZ[1]);
    }
    else if(VC1_BLK_INTER8X4 == BlkType)
    {
        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[2],
                                                   &DstSizeNZ[0]);

        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst + (srcDstStep << 1),
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[2],
                                                   &DstSizeNZ[1]);



        ippiTransform8x4Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);

        ippiTransform8x4Inv_VC1_16s_C1IR(pSrcDst + (srcDstStep << 1),
                                         srcDstStep,
                                         DstSizeNZ[1]);
    }
    else if(VC1_BLK_INTER4X4 == BlkType)
    {
        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[0]);

        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst + 4,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[1]);

        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst + (srcDstStep << 1),
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[2]);

        _own_ippiQuantInvInterUniform_VC1_16s_C1IR(pSrcDst + 4 + (srcDstStep << 1),
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[3]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst+ 4,
                                         srcDstStep,
                                         DstSizeNZ[1]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst + (srcDstStep<<1),
                                         srcDstStep,
                                         DstSizeNZ[2]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst + (srcDstStep<<1) + 4,
                                         srcDstStep,
                                         DstSizeNZ[3]);
    }
    return ippStsNoErr;
}
IppStatus _own_ippiReconstructInterNonuniform_VC1_16s_C1IR (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant,uint32_t BlkType)
{
    mfxSize  DstSizeNZ[4];
    static mfxSize QuantSize[4] = {
                                    { VC1_PIXEL_IN_BLOCK,   VC1_PIXEL_IN_BLOCK },
                                    { VC1_PIXEL_IN_BLOCK/2, VC1_PIXEL_IN_BLOCK },
                                    { VC1_PIXEL_IN_BLOCK,   VC1_PIXEL_IN_BLOCK/2 },
                                    { VC1_PIXEL_IN_BLOCK/2, VC1_PIXEL_IN_BLOCK/2 }
                                  };

    //quantization and transformation
    if (VC1_BLK_INTER8X8 == BlkType)
    {
        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[0],
                                                   &DstSizeNZ[0]);

        ippiTransform8x8Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);


    }
    else if(VC1_BLK_INTER4X8 == BlkType)
    {
        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[1],
                                                   &DstSizeNZ[0]);

        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst+4,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[1],
                                                   &DstSizeNZ[1]);

        ippiTransform4x8Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);

        ippiTransform4x8Inv_VC1_16s_C1IR(pSrcDst+4,
                                         srcDstStep,
                                         DstSizeNZ[1]);
    }
    else if(VC1_BLK_INTER8X4 == BlkType)
    {
        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[2],
                                                   &DstSizeNZ[0]);

        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst + (srcDstStep << 1),
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[2],
                                                   &DstSizeNZ[1]);



        ippiTransform8x4Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);

        ippiTransform8x4Inv_VC1_16s_C1IR(pSrcDst + (srcDstStep << 1),
                                         srcDstStep,
                                         DstSizeNZ[1]);
    }
    else if(VC1_BLK_INTER4X4 == BlkType)
    {
        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[0]);

        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst + 4,
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[1]);

        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst + (srcDstStep << 1),
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[2]);

        _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(pSrcDst + 4 + (srcDstStep << 1),
                                                   srcDstStep,
                                                   doubleQuant,
                                                   QuantSize[3],
                                                   &DstSizeNZ[3]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst,
                                         srcDstStep,
                                         DstSizeNZ[0]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst+ 4,
                                         srcDstStep,
                                         DstSizeNZ[1]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst + (srcDstStep<<1),
                                         srcDstStep,
                                         DstSizeNZ[2]);

        ippiTransform4x4Inv_VC1_16s_C1IR(pSrcDst + (srcDstStep<<1) + 4,
                                         srcDstStep,
                                         DstSizeNZ[3]);
    }
    return ippStsNoErr;
}
inline static void PredictACLeft(int16_t* pCurrAC, uint32_t CurrQuant,
                                 int16_t* pPredAC, uint32_t PredQuant,
                                 int32_t Step)
{
    int32_t i;
    int32_t Scale = VC1_DQScaleTbl[(CurrQuant-1)&63] * (PredQuant-1);
    uint32_t step = Step;

    for (i = 1; i<VC1_PIXEL_IN_BLOCK; i++, step+=Step)
        pCurrAC[step] = pCurrAC[step] + (int16_t)((pPredAC[i] * Scale + 0x20000)>>18);
}

inline static void PredictACTop(int16_t* pCurrAC, uint32_t CurrQuant,
                                int16_t* pPredAC, uint32_t PredQuant)
{
    int32_t i;
    int32_t Scale = VC1_DQScaleTbl[(CurrQuant-1)&63] * (PredQuant-1);

    for (i = 1; i<VC1_PIXEL_IN_BLOCK; i++)
        pCurrAC[i] = pCurrAC[i] + (int16_t)((pPredAC[i] * Scale + 0x20000)>>18);
}


static uint8_t GetDCACPrediction(VC1DCBlkParam* CurrBlk, VC1DCPredictors* PredData,
                               int32_t blk_num, int16_t* pBlock, uint32_t FCM)
{
    uint8_t blkType = VC1_BLK_INTRA;

    VC1DCPredictors DCPred;
    uint8_t PredPattern;
    uint32_t CurrQuant = PredData->DoubleQuant[2];

    int16_t DCA, DCB, DCC, DC = 0;
    uint32_t step = VC1_pixel_table[blk_num];

    DCPred = *PredData;
    PredPattern = DCPred.BlkPattern[blk_num];

    switch(PredPattern)
    {
    case 7:
        {
            DCA = DCPred.DC[VC1_PredDCIndex[0][blk_num]];
            DCB = DCPred.DC[VC1_PredDCIndex[1][blk_num]];
            DCC = DCPred.DC[VC1_PredDCIndex[2][blk_num]];

            if (vc1_abs_16s(DCB - DCA) <= vc1_abs_16s(DCB - DCC))
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC left prediction\n");
                #endif

                DC = CurrBlk->DC + DCC;
                PredictACLeft(pBlock, CurrQuant, DCPred.ACLEFT[VC1_PredDCIndex[2][blk_num]],
                    DCPred.DoubleQuant[VC1_QuantIndex[1][blk_num]], step);

                blkType =  VC1_BLK_INTRA_LEFT;
            }
            else
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC top prediction\n");
                #endif

                DC = CurrBlk->DC + DCA;
                PredictACTop(pBlock, CurrQuant, DCPred.ACTOP[VC1_PredDCIndex[0][blk_num]],
                    DCPred.DoubleQuant[VC1_QuantIndex[0][blk_num]]);

                blkType = VC1_BLK_INTRA_TOP;
            }
        }
        break;
    case 4:
    case 6:
        {
            //A is available, C - not
            #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                            "DC top prediction\n");
            #endif

            DCA = DCPred.DC[VC1_PredDCIndex[0][blk_num]];
            DC = CurrBlk->DC + DCA;

            PredictACTop(pBlock, CurrQuant, DCPred.ACTOP[VC1_PredDCIndex[0][blk_num]],
                DCPred.DoubleQuant[VC1_QuantIndex[0][blk_num]]);
            blkType = VC1_BLK_INTRA_TOP;
        }
        break;
    case 1:
    case 3:
        {
            //C is available, A - not
            #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                            "DC left prediction\n");
            #endif

            DCC = DCPred.DC[VC1_PredDCIndex[2][blk_num]];
            DC = CurrBlk->DC + DCC;
            PredictACLeft(pBlock, CurrQuant, DCPred.ACLEFT[VC1_PredDCIndex[2][blk_num]],
                DCPred.DoubleQuant[VC1_QuantIndex[1][blk_num]], step);
            blkType = VC1_BLK_INTRA_LEFT;
        }
        break;
    case 5:
        {
            DCA = DCPred.DC[VC1_PredDCIndex[0][blk_num]];
            DCC = DCPred.DC[VC1_PredDCIndex[2][blk_num]];

            if (vc1_abs_16s(DCA) <= vc1_abs_16s(DCC))
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC left prediction\n");
                #endif

                DC= CurrBlk->DC + DCC;
                PredictACLeft(pBlock, CurrQuant, DCPred.ACLEFT[VC1_PredDCIndex[2][blk_num]],
                    DCPred.DoubleQuant[VC1_QuantIndex[1][blk_num]], step);
                blkType = VC1_BLK_INTRA_LEFT;
            }
            else
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC top prediction\n");
                #endif

                DC = CurrBlk->DC + DCA;
                PredictACTop(pBlock, CurrQuant, DCPred.ACTOP[VC1_PredDCIndex[0][blk_num]],
                    DCPred.DoubleQuant[VC1_QuantIndex[0][blk_num]]);
                blkType = VC1_BLK_INTRA_TOP;
            }
        }
        break;
    case 0:
    case 2:
        {
            // A, C unavailable
            #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                            "DC left prediction\n");
            #endif

            DC = CurrBlk->DC;
            blkType = VC1_BLK_INTRA_LEFT;

            if(FCM)
                blkType = VC1_BLK_INTRA;
        }
        break;
    }

    pBlock[0] = DC;
    PredData->DC[blk_num] = DC;
    CurrBlk->DC = DC;
    return blkType;
}

static uint8_t GetDCPrediction(VC1DCBlkParam* CurrBlk,VC1DCPredictors* PredData,
                             int32_t blk_num, int16_t* pBlock, uint32_t FCM)
{
    uint8_t blkType = VC1_BLK_INTRA;

    VC1DCPredictors DCPred;
    uint8_t PredPattern;
    int16_t DCA, DCB, DCC = 0;

    DCPred = *PredData;

    PredPattern = DCPred.BlkPattern[blk_num];

    switch(PredPattern)
    {
    case 7:
        {
            DCA = DCPred.DC[VC1_PredDCIndex[0][blk_num]];
            DCB = DCPred.DC[VC1_PredDCIndex[1][blk_num]];
            DCC = DCPred.DC[VC1_PredDCIndex[2][blk_num]];

            if (vc1_abs_16s(DCB - DCA) <= vc1_abs_16s(DCB - DCC))
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC left prediction\n");
                #endif

                CurrBlk->DC = CurrBlk->DC + DCC;
                blkType =  VC1_BLK_INTRA_LEFT;
            }
            else
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC top prediction\n");
                #endif

                CurrBlk->DC = CurrBlk->DC + DCA;
                blkType = VC1_BLK_INTRA_TOP;
            }
        }
        break;
    case 4:
    case 6:
        {
            //A is available, C - not
            #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC top prediction\n");
            #endif

            DCA = DCPred.DC[VC1_PredDCIndex[0][blk_num]];

            CurrBlk->DC = CurrBlk->DC + DCA;
            blkType = VC1_BLK_INTRA_TOP;
        }
        break;
    case 1:
    case 3:
        {
            //C is available, A - not
            #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC left prediction\n");
            #endif

            DCC = DCPred.DC[VC1_PredDCIndex[2][blk_num]];
            CurrBlk->DC = CurrBlk->DC + DCC;
            blkType = VC1_BLK_INTRA_LEFT;
        }
        break;
    case 5:
        {
            DCA = DCPred.DC[VC1_PredDCIndex[0][blk_num]];
            DCC = DCPred.DC[VC1_PredDCIndex[2][blk_num]];

            if (vc1_abs_16s(DCA) <= vc1_abs_16s(DCC))
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC left prediction\n");
                #endif

                CurrBlk->DC = CurrBlk->DC + DCC;
                blkType = VC1_BLK_INTRA_LEFT;
            }
            else
            {
                #ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "DC top prediction\n");
                #endif

                CurrBlk->DC = CurrBlk->DC + DCA;
                blkType = VC1_BLK_INTRA_TOP;
            }
        }
        break;
    case 0:
    case 2:
        {
            // A, C unavailable
            #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                             "DC left prediction\n");
            #endif

            blkType = VC1_BLK_INTRA_LEFT;
            if(FCM)
                blkType = VC1_BLK_INTRA;
        }
        break;
    }

    pBlock[0] = CurrBlk->DC;
    PredData->DC[blk_num] = CurrBlk->DC;
    return blkType;
}

static const DCPrediction DCPredictionTable[] =
{
        (DCPrediction)(GetDCPrediction),
        (DCPrediction)(GetDCACPrediction)
};

VC1Status BLKLayer_Intra_Luma_Adv(VC1Context* pContext, int32_t blk_num, uint32_t ACPRED)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    VC1Block* pBlock    = &pContext->m_pCurrMB->m_pBlocks[blk_num];
    VC1DCMBParam*  CurrDC = pContext->CurrDC;
    VC1DCBlkParam* CurrBlk = &CurrDC->DCBlkPred[blk_num];
    VC1DCPredictors* DCPred = &pContext->DCPred;

    int ret;
    int32_t DCCOEF;
    int32_t DCSIGN;
    uint32_t i = 0;

// need to calculate bits for residual data
    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                     &pContext->m_bitstream.bitOffset,
                                     &DCCOEF,
                                     pContext->m_picLayerHeader->m_pCurrLumaDCDiff);
#ifdef VC1_VLD_CHECK
    if (ret != 0)
        throw vc1_exception(vld);
#endif

    if(DCCOEF != 0)
    {
        uint32_t quant =  (CurrDC->DoubleQuant >> 1);

        if(DCCOEF == IPPVC_ESCAPE)
        {
           if(quant == 1)
           {
              VC1_GET_BITS(10, DCCOEF);
           }
           else if(quant == 2)
           {
              VC1_GET_BITS(9, DCCOEF);
           }
           else // pContext->m_pCurrMB->MQUANT is > 2
           {
              VC1_GET_BITS(8, DCCOEF);
           }
        }
        else
        {  // DCCOEF is not IPPVC_ESCAPE
           int32_t tmp;
           if(quant == 1)
           {
              VC1_GET_BITS(2, tmp);
               DCCOEF = DCCOEF*4 + tmp - 3;
           }
           else if(quant == 2)
           {
              VC1_GET_BITS(1, tmp);
              DCCOEF = DCCOEF*2 + tmp - 1;
           }
        }

        VC1_GET_BITS(1, DCSIGN);
         DCCOEF = (1 - (DCSIGN<<1))* DCCOEF;
    }

    CurrBlk->DC = (int16_t)DCCOEF;

    pBlock->blkType = DCPredictionTable[ACPRED](CurrBlk, DCPred, blk_num, m_pBlock, pContext->m_picLayerHeader->FCM);

    #ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                        VM_STRING("DC diff = %d\n"),DCCOEF);
    #endif

    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        const uint8_t* curr_scan = pContext->m_pSingleMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
        if(curr_scan==NULL)
            return VC1_FAIL;

        DecodeBlockACIntra_VC1(&pContext->m_bitstream,
                    m_pBlock,
                    curr_scan, pContext->m_picLayerHeader->m_pCurrIntraACDecSet,
                    &pContext->m_pSingleMB->EscInfo);
    }

    for(i = 1; i < 8; i++)
    {
        CurrBlk->ACLEFT[i] = m_pBlock[i*16];
        CurrBlk->ACTOP[i]  = m_pBlock[i];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                            VM_STRING("DC = %d\n"),
                                            pContext->CurrDC->DCBlkPred[blk_num].DC);
    //NEED!
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                            "Block %d\n", blk_num);
                for(uint32_t k = 0; k<8; k++)
                {
                    for (uint32_t t = 0; t<8; t++)
                    {
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "%d  ", m_pBlock[k*16 + t]);
                    }
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS, "\n");
                }
#endif

    return VC1_OK;
}

VC1Status BLKLayer_Intra_Chroma_Adv(VC1Context* pContext, int32_t blk_num,uint32_t ACPRED)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    VC1Block* pBlock    = &pContext->m_pCurrMB->m_pBlocks[blk_num];
    VC1DCMBParam*  CurrDC = pContext->CurrDC;
    VC1DCBlkParam* CurrBlk = &CurrDC->DCBlkPred[blk_num];

    VC1DCPredictors* DCPred = &pContext->DCPred;

    int ret;
    int32_t DCCOEF;
    int32_t DCSIGN;
    uint32_t i = 0;

    // need to calculate bits for residual data
    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                     &pContext->m_bitstream.bitOffset,
                                     &DCCOEF,
                                     pContext->m_picLayerHeader->m_pCurrChromaDCDiff);
#ifdef VC1_VLD_CHECK
    if (ret != 0)
        throw vc1_exception(vld);
#endif

    if(DCCOEF != 0)
    {
        uint32_t quant =  (CurrDC->DoubleQuant >> 1);

        if(DCCOEF == IPPVC_ESCAPE)
        {
           if(quant == 1)
           {
              VC1_GET_BITS(10, DCCOEF);
           }
           else if(quant == 2)
           {
              VC1_GET_BITS(9, DCCOEF);
           }
           else // pContext->m_pCurrMB->MQUANT is > 2
           {
              VC1_GET_BITS(8, DCCOEF);
           }
        }
        else
        {  // DCCOEF is not IPPVC_ESCAPE
           int32_t tmp;
           if(quant == 1)
           {
              VC1_GET_BITS(2, tmp);
              DCCOEF = DCCOEF*4 + tmp - 3;
           }
           else if(quant == 2)
           {
              VC1_GET_BITS(1, tmp);
              DCCOEF = DCCOEF*2 + tmp - 1;
           }
        }

        VC1_GET_BITS(1, DCSIGN);

        DCCOEF = (1 - (DCSIGN<<1))* DCCOEF;
    }

    CurrBlk->DC = (int16_t)DCCOEF;


    pBlock->blkType = DCPredictionTable[ACPRED](CurrBlk, DCPred, blk_num, m_pBlock, pContext->m_picLayerHeader->FCM);

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,VM_STRING("DC diff = %d\n"),DCCOEF);
#endif

    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        const uint8_t* curr_scan = pContext->m_pSingleMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
        if(curr_scan==NULL)
            return VC1_FAIL;

        DecodeBlockACIntra_VC1(&pContext->m_bitstream, m_pBlock,
                     curr_scan, pContext->m_picLayerHeader->m_pCurrInterACDecSet,
                     &pContext->m_pSingleMB->EscInfo);
   }

    for(i = 1; i < 8; i++)

    {
        CurrBlk->ACLEFT[i] = m_pBlock[i*8];
        CurrBlk->ACTOP[i]  = m_pBlock[i];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                            VM_STRING("DC = %d\n"),
                                            pContext->CurrDC->DCBlkPred[blk_num].DC);
//NEED!
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                            "Block %d\n", blk_num);
                for(uint32_t k = 0; k<8; k++)
                {
                    for (uint32_t t = 0; t<8; t++)
                    {
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "%d  ", m_pBlock[k*8 + t]);
                    }
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS, "\n");
                }
#endif

    return VC1_OK;
}

VC1Status VC1ProcessDiffIntra(VC1Context* pContext, int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock; //memory for 16s diffs
    mfxSize  roiSize;
    roiSize.height = VC1_PIXEL_IN_BLOCK;
    roiSize.width = VC1_PIXEL_IN_BLOCK;
    int16_t bias = 128;


    if ((pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)&&
        ((pContext->m_picLayerHeader->PTYPE == VC1_I_FRAME)||
         (pContext->m_picLayerHeader->PTYPE == VC1_BI_FRAME)))
         bias = pContext->m_pCurrMB->bias;

     *(pContext->m_pBlock+VC1_BlkStart[blk_num]) = *(pContext->m_pBlock+VC1_BlkStart[blk_num])
                                                   * (int16_t)pContext->CurrDC->DCStepSize;

     if(pContext->m_picLayerHeader->QuantizationType == VC1_QUANTIZER_UNIFORM)
         _own_ippiReconstructIntraUniform_VC1_16s_C1IR(pContext->m_pBlock+ VC1_BlkStart[blk_num],
                                                  VC1_pixel_table[blk_num]*2,
                                                  pContext->CurrDC->DoubleQuant);
     else
         _own_ippiReconstructIntraNonuniform_VC1_16s_C1IR(pContext->m_pBlock+ VC1_BlkStart[blk_num],
                                                  VC1_pixel_table[blk_num]*2,
                                                  pContext->CurrDC->DoubleQuant);
        roiSize.height = VC1_PIXEL_IN_BLOCK;
        roiSize.width = VC1_PIXEL_IN_BLOCK;


        ippiAddC_16s_C1IRSfs(bias, m_pBlock + VC1_BlkStart[blk_num],
                                    2*VC1_pixel_table[blk_num], roiSize, 0);
    return VC1_OK;
}

VC1Status BLKLayer_Inter_Luma_Adv(VC1Context* pContext, int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    VC1Block* pBlock    = &pContext->m_pCurrMB->m_pBlocks[blk_num];
    const uint8_t* curr_scan = NULL;
    uint32_t numCoef = 0;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader * picHeader = pContext->m_picLayerHeader;

    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        switch (pBlock->blkType)
        {
        case VC1_BLK_INTER8X8:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan == NULL)
                    return VC1_FAIL;
#endif

                sMB->m_pSingleBlock[blk_num].numCoef = VC1_SBP_0;
                DecodeBlockInter8x8_VC1(&pContext->m_bitstream, m_pBlock,
                            curr_scan,picHeader->m_pCurrInterACDecSet,
                            &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS, "Inter\n");
#endif
            }
            break;
        case VC1_BLK_INTER8X4:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                if (sMB->m_ubNumFirstCodedBlk < blk_num || picHeader->TTFRM ==  pBlock->blkType)
                     numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                 DecodeBlockInter8x4_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS, "Inter 8x4\n");
#endif
            }
            break;
        case VC1_BLK_INTER4X8:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                if (sMB->m_ubNumFirstCodedBlk < blk_num || picHeader->TTFRM == pBlock->blkType)
                    numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                 DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);

#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                        "Inter 4x8\n");
#endif
            }
            break;
        case VC1_BLK_INTER4X4:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                        "Inter 4x4\n");
#endif
            }
            break;

        case VC1_BLK_INTER:
            {
                numCoef = GetTTBLK(pContext, blk_num);

                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                switch (pBlock->blkType)
                {
                case VC1_BLK_INTER8X8:
                    {
                        DecodeBlockInter8x8_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan, picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
#ifdef VC1_DEBUG_ON
                       VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter\n");
#endif
                    }
                    break;
                case VC1_BLK_INTER8X4:
                    {
                        DecodeBlockInter8x4_VC1(&pContext->m_bitstream, m_pBlock,
                                        curr_scan, picHeader->m_pCurrInterACDecSet,
                                        &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 8x4\n");
#endif
                    }
                    break;
                case VC1_BLK_INTER4X8:
                    {
                     DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                        curr_scan, picHeader->m_pCurrInterACDecSet,
                                        &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 4x8\n");
#endif
                    }
                    break;
                case VC1_BLK_INTER4X4:
                    {
                        numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                        DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                        curr_scan, picHeader->m_pCurrInterACDecSet,
                                        &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                    "Inter 4x4\n");
#endif
                    }
                    break;
                }
            }
            break;
        default:
            VM_ASSERT(0);
        }
    }
#ifdef VC1_DEBUG_ON

//NEED!
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "Block %d\n", blk_num);
                for(uint32_t k = 0; k<8; k++)
                {
                    for (uint32_t t = 0; t<8; t++)
                    {
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "%d  ", m_pBlock[k*16 + t]);
                    }
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS, "\n");
                }
#endif

    return VC1_OK;
}

VC1Status BLKLayer_Inter_Chroma_Adv(VC1Context* pContext, int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    VC1Block* pBlock    = &pContext->m_pCurrMB->m_pBlocks[blk_num];
    const uint8_t* curr_scan = NULL;
    uint32_t numCoef = 0;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader * picHeader = pContext->m_picLayerHeader;

    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        switch (pBlock->blkType)
        {
        case VC1_BLK_INTER8X8:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                sMB->m_pSingleBlock[blk_num].numCoef = VC1_SBP_0;
                DecodeBlockInter8x8_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                            &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 8x8\n");
#endif
            }
            break;
        case VC1_BLK_INTER8X4:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                if (sMB->m_ubNumFirstCodedBlk < blk_num || picHeader->TTFRM ==  pBlock->blkType)
                    numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                 DecodeBlockInter8x4_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 8x4\n");
#endif
            }
            break;
        case VC1_BLK_INTER4X8:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                if (sMB->m_ubNumFirstCodedBlk < blk_num || picHeader->TTFRM ==  pBlock->blkType)
                    numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                 DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 4x8\n");
#endif
            }
            break;

        case VC1_BLK_INTER4X4:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan,picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 4x4\n");
#endif
            }
            break;

        case VC1_BLK_INTER:
            {
                numCoef = GetTTBLK(pContext, blk_num);

                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];

#ifdef VC1_VLD_CHECK
                if(curr_scan==NULL)
                    return VC1_FAIL;
#endif

                switch (pBlock->blkType)
                {
                case VC1_BLK_INTER8X8:
                    DecodeBlockInter8x8_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 8x8\n");
#endif
                    break;
                case VC1_BLK_INTER8X4:
                    DecodeBlockInter8x4_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan, picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 8x4\n");
#endif
                    break;
                case VC1_BLK_INTER4X8:
                     DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan, picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 4x8\n");
#endif
                    break;
                case VC1_BLK_INTER4X4:
                    numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                    DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan, picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, numCoef);
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 4x4\n");
#endif
                    break;
                }
            }
            break;

        default:
            VM_ASSERT(0);
        }
    }
#ifdef VC1_DEBUG_ON
    //NEED!
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "Block %d\n", blk_num);
                for(uint32_t k = 0; k<8; k++)
                {
                    for (uint32_t t = 0; t<8; t++)
                    {
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                "%d  ", m_pBlock[k*8 + t]);
                    }
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS, "\n");
                }
#endif

    return VC1_OK;
}

VC1Status VC1ProcessDiffInter(VC1Context* pContext,int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num]; //memory for 16s diffs

    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        //quantization and transformation
        Reconstruct_table[pContext->m_picLayerHeader->QuantizationType](m_pBlock,
                                                                        VC1_pixel_table[blk_num]*2,
                                                                        pContext->CurrDC->DoubleQuant,
                                                                        pContext->m_pCurrMB->m_pBlocks[blk_num].blkType);
    }
    return VC1_OK;
}
VC1Status VC1ProcessDiffSpeedUpIntra(VC1Context* pContext,int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[0]; //memory for 16s diffs
    mfxSize  roiSize;
    roiSize.height = VC1_PIXEL_IN_BLOCK;
    roiSize.width = VC1_PIXEL_IN_BLOCK;
    mfxSize  DstSizeNZ;
    int16_t bias = 128;

    m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    //DC
    *(pContext->m_pBlock+VC1_BlkStart[blk_num]) = *(pContext->m_pBlock+VC1_BlkStart[blk_num])
        * (int16_t)pContext->CurrDC->DCStepSize;

    if ((pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)&&
        ((pContext->m_picLayerHeader->PTYPE == VC1_I_FRAME)||
        (pContext->m_picLayerHeader->PTYPE == VC1_BI_FRAME)))
        bias = pContext->m_pCurrMB->bias;

    if (pContext->m_picLayerHeader->QuantizationType == VC1_QUANTIZER_UNIFORM)
    {
        _own_ippiQuantInvIntraUniform_VC1_16s_C1IR(m_pBlock,
            VC1_pixel_table[blk_num]*2,
            pContext->CurrDC->DoubleQuant,
            &DstSizeNZ);
    }
    else
    {
        _own_ippiQuantInvIntraNonuniform_VC1_16s_C1IR(m_pBlock,
            VC1_pixel_table[blk_num]*2,
            pContext->CurrDC->DoubleQuant,
            &DstSizeNZ);

    }

    ippiTransform8x8Inv_VC1_16s_C1IR(m_pBlock,
                                     VC1_pixel_table[blk_num]*2,
                                     DstSizeNZ);
    ippiAddC_16s_C1IRSfs(bias, m_pBlock, 2*VC1_pixel_table[blk_num], roiSize, 0);
    return VC1_OK;

}
VC1Status VC1ProcessDiffSpeedUpInter(VC1Context* pContext,int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num]; //memory for 16s diffs
    mfxSize  roiSize;
    roiSize.height = VC1_PIXEL_IN_BLOCK;
    roiSize.width = VC1_PIXEL_IN_BLOCK;
    mfxSize  DstSizeNZ;
    mfxSize QuantSize = {4,4};
    m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    VC1Block* pBlock    = &pContext->m_pCurrMB->m_pBlocks[blk_num];
    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        if (pContext->m_picLayerHeader->QuantizationType == VC1_QUANTIZER_UNIFORM)
        {
            _own_ippiQuantInvInterUniform_VC1_16s_C1IR(m_pBlock,
                VC1_pixel_table[blk_num]*2,
                pContext->CurrDC->DoubleQuant,
                QuantSize,
                &DstSizeNZ);
        }
        else
        {
            _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(m_pBlock,
                VC1_pixel_table[blk_num]*2,
                pContext->CurrDC->DoubleQuant,
                QuantSize,
                &DstSizeNZ);
        }

        if (VC1_BLK_INTER8X8 == pBlock->blkType)
        {
            ippiTransform8x8Inv_VC1_16s_C1IR(m_pBlock,
                                            VC1_pixel_table[blk_num]*2,
                                            DstSizeNZ);
        }
        else if(VC1_BLK_INTER4X8 == pBlock->blkType)
        {
            ippiTransform4x8Inv_VC1_16s_C1IR(m_pBlock,
                                            VC1_pixel_table[blk_num]*2,
                                            DstSizeNZ);

        }
        else if(VC1_BLK_INTER8X4 == pBlock->blkType)
        {

            ippiTransform8x4Inv_VC1_16s_C1IR(m_pBlock,
                                            VC1_pixel_table[blk_num]*2,
                                            DstSizeNZ);

        }
        else if(VC1_BLK_INTER4X4 == pBlock->blkType)
        {
        ippiTransform4x4Inv_VC1_16s_C1IR(m_pBlock,
                                        VC1_pixel_table[blk_num]*2,
                                        DstSizeNZ);
        }
        }
    return VC1_OK;
}
void write_Intraluma_to_interlace_frame_Adv(VC1MB * pCurrMB, int16_t* pBlock)
{
    mfxSize roiSize;
    uint32_t planeStep[2] = {pCurrMB->currYPitch,
                           pCurrMB->currYPitch << 1};

    uint32_t planeOffset[2] = {pCurrMB->currYPitch << 3,
                             pCurrMB->currYPitch};

    roiSize.height = VC1_PIXEL_IN_BLOCK;
    roiSize.width = VC1_PIXEL_IN_LUMA;

    ippiConvert_16s8u_C1R(pBlock,
                          VC1_PIXEL_IN_LUMA << 1,
                          pCurrMB->currYPlane,
                          planeStep[pCurrMB->FIELDTX],
                          roiSize);

    ippiConvert_16s8u_C1R(pBlock + 128,
                          VC1_PIXEL_IN_LUMA << 1,
                          pCurrMB->currYPlane +  planeOffset[pCurrMB->FIELDTX],
                          planeStep[pCurrMB->FIELDTX],
                          roiSize);
}


void write_Interluma_to_interlace_frame_MC_Adv(VC1MB * pCurrMB,
                                               const uint8_t* pDst,
                                               uint32_t dstStep,
                                               int16_t* pBlock)
{
    uint8_t fieldFlag = (uint8_t)(pCurrMB->FIELDTX*2 + VC1_IS_MVFIELD(pCurrMB->mbType));

    uint32_t predOffset[4] = {dstStep << 3, dstStep << 3, dstStep, dstStep << 3};

    int32_t predStep[4] = {static_cast<int32_t>(dstStep),   static_cast<int32_t>(dstStep),
                          static_cast<int32_t>(dstStep << 1), static_cast<int32_t>(dstStep)};

    uint32_t planeOffset[4] = {pCurrMB->currYPitch << 3 ,  pCurrMB->currYPitch,
                               pCurrMB->currYPitch,  pCurrMB->currYPitch};

    uint32_t planeStep[4] = {pCurrMB->currYPitch,      pCurrMB->currYPitch << 1,
                           pCurrMB->currYPitch << 1,    pCurrMB->currYPitch << 1};
    // Skip MB
    if (pCurrMB->SkipAndDirectFlag & 2)
    {
        mfxSize  roiSize;
        roiSize.width = 16;
        roiSize.height = 8;
        ippiCopy_8u_C1R(pDst,
                        predStep[fieldFlag],
                        pCurrMB->currYPlane,
                        planeStep[fieldFlag],
                        roiSize);
        ippiCopy_8u_C1R(pDst + predOffset[fieldFlag],
                        predStep[fieldFlag],
                        pCurrMB->currYPlane + planeOffset[fieldFlag],
                        planeStep[fieldFlag],
                        roiSize);

    }
    else
    {
        uint16_t blockOffset[4] = {128, VC1_PIXEL_IN_LUMA, 128, 128};

        uint16_t blockStep[4] = {VC1_PIXEL_IN_LUMA << 1,   VC1_PIXEL_IN_LUMA << 2,
            VC1_PIXEL_IN_LUMA << 1,   VC1_PIXEL_IN_LUMA << 1};

        ippiMC16x8_8u_C1(pDst,  predStep[fieldFlag],
                         pBlock, blockStep[fieldFlag],
                         pCurrMB->currYPlane,
                         planeStep[fieldFlag], 0, 0);

        ippiMC16x8_8u_C1(pDst + predOffset[fieldFlag],
                         predStep[fieldFlag],
                         pBlock + blockOffset[fieldFlag],
                         blockStep[fieldFlag],
                         pCurrMB->currYPlane + planeOffset[fieldFlag],
                         planeStep[fieldFlag], 0, 0);
    }
}


void write_Interluma_to_interlace_frame_MC_Adv_Copy(VC1MB * pCurrMB,
                                                   int16_t* pBlock)
{
    uint8_t pPred[64*4];
    uint8_t fieldFlag = (uint8_t)(pCurrMB->FIELDTX*2 + VC1_IS_MVFIELD(pCurrMB->mbType));

    uint16_t predOffset[4] = {64*2, 64*2, VC1_PIXEL_IN_LUMA, 64*2};
    uint16_t predStep[4] = {VC1_PIXEL_IN_LUMA,
        VC1_PIXEL_IN_LUMA,
        2*VC1_PIXEL_IN_LUMA,
        VC1_PIXEL_IN_LUMA
    };
    uint16_t blockOffset[4] = {64*2, VC1_PIXEL_IN_LUMA, 64*2, 64*2};
    uint16_t blockStep[4] = {2*VC1_PIXEL_IN_LUMA,
        2*2*VC1_PIXEL_IN_LUMA,
        2*VC1_PIXEL_IN_LUMA,
        2*VC1_PIXEL_IN_LUMA
    };

    uint32_t planeOffset[4] = {8*pCurrMB->currYPitch,
        pCurrMB->currYPitch,
        pCurrMB->currYPitch,
        pCurrMB->currYPitch};
    uint32_t planeStep[4] = {pCurrMB->currYPitch,
        pCurrMB->currYPitch*2,
        pCurrMB->currYPitch*2,
        pCurrMB->currYPitch*2
    };

    ippiCopy16x16_8u_C1R(pCurrMB->currYPlane,
                         pCurrMB->currYPitch,
                         pPred,
                         VC1_PIXEL_IN_LUMA);

    ippiMC16x8_8u_C1(pPred,
        predStep[fieldFlag],
        pBlock,
        blockStep[fieldFlag],
        pCurrMB->currYPlane,
        planeStep[fieldFlag], 0, 0);

    ippiMC16x8_8u_C1(pPred + predOffset[fieldFlag],
        predStep[fieldFlag],
        pBlock + blockOffset[fieldFlag],
        blockStep[fieldFlag],
        pCurrMB->currYPlane + planeOffset[fieldFlag],
        planeStep[fieldFlag], 0, 0);
}
void write_Interluma_to_interlace_B_frame_MC_Adv(VC1MB * pCurrMB,
                                               const uint8_t* pDst1, uint32_t dstStep1,
                                               const uint8_t* pDst2, uint32_t dstStep2,
                                               int16_t* pBlock)
{
    uint8_t pPred[256]={0};

    uint8_t fieldFlag = (uint8_t)(pCurrMB->FIELDTX*2 + VC1_IS_MVFIELD(pCurrMB->mbType));

    uint16_t predOffset[4] = {8*VC1_PIXEL_IN_LUMA, 8*VC1_PIXEL_IN_LUMA, VC1_PIXEL_IN_LUMA, 8*VC1_PIXEL_IN_LUMA};

    uint16_t predStep[4] = {VC1_PIXEL_IN_LUMA,   VC1_PIXEL_IN_LUMA,   2*VC1_PIXEL_IN_LUMA, VC1_PIXEL_IN_LUMA};

    uint32_t planeOffset[4] = {8*pCurrMB->currYPitch,  pCurrMB->currYPitch,
                               pCurrMB->currYPitch,  pCurrMB->currYPitch};
    uint32_t planeStep[4] = {pCurrMB->currYPitch,      pCurrMB->currYPitch*2,
                           pCurrMB->currYPitch*2,    pCurrMB->currYPitch*2};

    ippiAverage16x16_8u_C1R(pDst1, dstStep1,  pDst2, dstStep2,
                            pPred, VC1_PIXEL_IN_LUMA);

        // Skip MB
    if (pCurrMB->SkipAndDirectFlag & 2)
    {
        mfxSize  roiSize;
        roiSize.width = 16;
        roiSize.height = 8;
        ippiCopy_8u_C1R(pPred,
                        predStep[fieldFlag],
                        pCurrMB->currYPlane,
                        planeStep[fieldFlag],
                        roiSize);
        ippiCopy_8u_C1R(pPred + predOffset[fieldFlag],
                        predStep[fieldFlag],
                        pCurrMB->currYPlane + planeOffset[fieldFlag],
                        planeStep[fieldFlag],
                        roiSize);
    }
    else
    {
        uint16_t blockOffset[4] = {64*2, VC1_PIXEL_IN_LUMA, 64*2, 64*2};
        uint16_t blockStep[4] = {2*VC1_PIXEL_IN_LUMA,   2*2*VC1_PIXEL_IN_LUMA,
                           2*VC1_PIXEL_IN_LUMA,   2*VC1_PIXEL_IN_LUMA};

        ippiMC16x8_8u_C1(pPred,  predStep[fieldFlag],
                         pBlock, blockStep[fieldFlag],
                         pCurrMB->currYPlane,
                         planeStep[fieldFlag], 0, 0);
        ippiMC16x8_8u_C1(pPred + predOffset[fieldFlag],
                         predStep[fieldFlag],
                         pBlock + blockOffset[fieldFlag],
                         blockStep[fieldFlag],
                         pCurrMB->currYPlane + planeOffset[fieldFlag],
                         planeStep[fieldFlag], 0, 0);
    }
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
