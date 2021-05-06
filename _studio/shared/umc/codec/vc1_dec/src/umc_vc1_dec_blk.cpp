// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#include "ippi.h"

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_zigzag_tbl.h"
#include "umc_vc1_common_blk_order_tbl.h"
#include "umc_vc1_dec_run_level_tbl.h"
#include "umc_vc1_huffman.h"
#include "umc_vc1_dec_debug.h"

typedef uint8_t (*DCPrediction)(VC1DCBlkParam* CurrBlk, VC1DCPredictors* PredData,
                               int32_t blk_num, int16_t* pBlock,int16_t defaultDC, uint32_t PTYPE);

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
                               int32_t blk_num, int16_t* pBlock,int16_t defaultDC, uint32_t PTYPE)
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

            if (!(DCA==defaultDC && (PTYPE == VC1_I_FRAME ||  PTYPE == VC1_BI_FRAME)))
            {
                PredictACTop(pBlock, CurrQuant, DCPred.ACTOP[VC1_PredDCIndex[0][blk_num]],
                    DCPred.DoubleQuant[VC1_QuantIndex[0][blk_num]]);
                blkType = VC1_BLK_INTRA_TOP;
            }
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
            DCB = defaultDC;
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

            blkType = VC1_BLK_INTRA_LEFT;
            DC = CurrBlk->DC + defaultDC;
         }
        break;
    }

    pBlock[0] = DC;
    PredData->DC[blk_num] = DC;
    CurrBlk->DC = DC;
    return blkType;
}

static uint8_t GetDCPrediction(VC1DCBlkParam* CurrBlk,VC1DCPredictors* PredData,
                             int32_t blk_num, int16_t* pBlock,int16_t defaultDC, uint32_t PTYPE)
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
            if (!(DCA==defaultDC && (PTYPE == VC1_I_FRAME ||  PTYPE == VC1_BI_FRAME)))
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

            CurrBlk->DC = CurrBlk->DC + defaultDC;
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

VC1Status BLKLayer_Intra_Luma(VC1Context* pContext, int32_t blk_num, uint32_t bias, uint32_t ACPRED)
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

    uint32_t quant = CurrDC->DoubleQuant>>1;

    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                     &pContext->m_bitstream.bitOffset,
                                     &DCCOEF,
                                     pContext->m_picLayerHeader->m_pCurrLumaDCDiff);
    VM_ASSERT(ret == 0);

    if(DCCOEF != 0)
    {
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
           if(quant  == 1)
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
    if(!bias)
    {
        int32_t DCStepSize = pContext->CurrDC->DCStepSize;
        pBlock->blkType =  DCPredictionTable[ACPRED](CurrBlk, DCPred, blk_num, m_pBlock,
                                            (int16_t)((1024 +(DCStepSize>>1))/DCStepSize),
                                            pContext->m_picLayerHeader->PTYPE);
    }
    else
    pBlock->blkType = DCPredictionTable[ACPRED](CurrBlk, DCPred, blk_num,
                        m_pBlock,0,pContext->m_picLayerHeader->PTYPE);
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
                    m_pBlock, curr_scan,
                   pContext->m_picLayerHeader->m_pCurrIntraACDecSet,
                   &pContext->m_pSingleMB->EscInfo);
    }

    for(i = 1; i < 8; i++)

    {
        CurrBlk->ACLEFT[i] = m_pBlock[i*16];
        CurrBlk->ACTOP[i]  = m_pBlock[i];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
        VM_STRING("DC = %d\n"),pContext->CurrDC->DCBlkPred[blk_num].DC);
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

VC1Status BLKLayer_Intra_Chroma(VC1Context* pContext, int32_t blk_num, uint32_t bias, uint32_t ACPRED)
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

    uint32_t quant =  CurrDC->DoubleQuant>>1;

    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                     &pContext->m_bitstream.bitOffset,
                                     &DCCOEF,
                                     pContext->m_picLayerHeader->m_pCurrChromaDCDiff);
    VM_ASSERT(ret == 0);

    if(DCCOEF != 0)
    {
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
    if(!bias)
    {
        int32_t DCStepSize = pContext->CurrDC->DCStepSize;
        pBlock->blkType = DCPredictionTable[ACPRED](CurrBlk, DCPred, blk_num,
                        m_pBlock,(int16_t)((1024 +(DCStepSize>>1))/DCStepSize),
                        pContext->m_picLayerHeader->PTYPE);
    }
    else
    pBlock->blkType = DCPredictionTable[ACPRED](CurrBlk, DCPred, blk_num, m_pBlock,0,
                        pContext->m_picLayerHeader->PTYPE);
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
                     m_pBlock, curr_scan,
                     pContext->m_picLayerHeader->m_pCurrInterACDecSet,
                     &pContext->m_pSingleMB->EscInfo);
   }

    for(i = 1; i < 8; i++)

    {
        CurrBlk->ACLEFT[i] = m_pBlock[i*8];
        CurrBlk->ACTOP[i]  = m_pBlock[i];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
            VM_STRING("DC = %d\n"),pContext->CurrDC->DCBlkPred[blk_num].DC);
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

VC1Status BLKLayer_Inter_Luma(VC1Context* pContext, int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    VC1Block* pBlock    = &pContext->m_pCurrMB->m_pBlocks[blk_num];
    const uint8_t* curr_scan = NULL;
    uint8_t numCoef = 0;

    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader * picHeader = pContext->m_picLayerHeader;

    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        switch (pBlock->blkType)
        {
        case VC1_BLK_INTER8X8:
            {
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                            "Inter\n");
#endif
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                sMB->m_pSingleBlock[blk_num].numCoef = VC1_SBP_0;
                DecodeBlockInter8x8_VC1(&pContext->m_bitstream,
                            m_pBlock, curr_scan,
                            picHeader->m_pCurrInterACDecSet,
                            &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
            }
            break;
        case VC1_BLK_INTER8X4:
            {
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                            "Inter 8x4\n");
#endif
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                if (sMB->m_ubNumFirstCodedBlk < blk_num || picHeader->TTFRM == pBlock->blkType)
                    numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                 DecodeBlockInter8x4_VC1(&pContext->m_bitstream,
                                m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
            }
            break;
        case VC1_BLK_INTER4X8:
            {
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                        "Inter 4x8\n");
#endif
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                if (sMB->m_ubNumFirstCodedBlk < blk_num ||  picHeader->TTFRM ==
                    pBlock->blkType)
                    numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                   DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan,
                                picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
            }
            break;
        case VC1_BLK_INTER4X4:
            {
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                        "Inter 4x4\n");
#endif
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
            }
            break;

        case VC1_BLK_INTER:
            {
                numCoef = GetTTBLK(pContext, blk_num);

                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                switch (pBlock->blkType)
                {
                case VC1_BLK_INTER8X8:
                    {
#ifdef VC1_DEBUG_ON
                       VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                    "Inter\n");
#endif
                       DecodeBlockInter8x8_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan,  picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
                    }
                    break;
                case VC1_BLK_INTER8X4:
                    {
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 8x4\n");
#endif
                        DecodeBlockInter8x4_VC1(&pContext->m_bitstream, m_pBlock,
                                        curr_scan,  picHeader->m_pCurrInterACDecSet,
                                        &pContext->m_pSingleMB->EscInfo, numCoef);
                    }
                    break;
                case VC1_BLK_INTER4X8:
                    {
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 4x8\n");
#endif
                        DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                        curr_scan, picHeader->m_pCurrInterACDecSet,
                                        &pContext->m_pSingleMB->EscInfo, numCoef);
                    }
                    break;
                case VC1_BLK_INTER4X4:
                    {
#ifdef VC1_DEBUG_ON
                        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                                                "Inter 4x4\n");
#endif
                        numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                        DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                        curr_scan,picHeader->m_pCurrInterACDecSet,
                                        &pContext->m_pSingleMB->EscInfo, numCoef);
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
                                                                "%d  ",m_pBlock[k*16 + t]);
                    }
                    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS, "\n");
                }
#endif
    return VC1_OK;
}
VC1Status BLKLayer_Inter_Chroma(VC1Context* pContext, int32_t blk_num)
{
    int16_t*   m_pBlock  = pContext->m_pBlock + VC1_BlkStart[blk_num];
    VC1Block* pBlock    = &pContext->m_pCurrMB->m_pBlocks[blk_num];
    const uint8_t* curr_scan = NULL;
    uint8_t numCoef = 0;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader * picHeader = pContext->m_picLayerHeader;

    if(pContext->m_pCurrMB->m_cbpBits & (1<<(5-blk_num)))
    {
        switch (pBlock->blkType)
        {
        case VC1_BLK_INTER8X8:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                sMB->m_pSingleBlock[blk_num].numCoef = VC1_SBP_0;
                DecodeBlockInter8x8_VC1(&pContext->m_bitstream, m_pBlock,
                            curr_scan,picHeader->m_pCurrInterACDecSet,
                            &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
            }
            break;
        case VC1_BLK_INTER8X4:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                if (sMB->m_ubNumFirstCodedBlk < blk_num || picHeader->TTFRM ==  pBlock->blkType)
                    numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                DecodeBlockInter8x4_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan,picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
            }
            break;
        case VC1_BLK_INTER4X8:
            {
                curr_scan =sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                if (sMB->m_ubNumFirstCodedBlk < blk_num || picHeader->TTFRM == pBlock->blkType)
                    numCoef = GetSubBlockPattern_8x4_4x8(pContext, blk_num);
                else
                    numCoef = sMB->m_pSingleBlock[blk_num].numCoef;

                DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
            }
            break;

        case VC1_BLK_INTER4X4:
            {
                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan,picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, numCoef);
            }
            break;

        case VC1_BLK_INTER:
            {
                numCoef = GetTTBLK(pContext, blk_num);

                curr_scan = sMB->ZigzagTable[VC1_BlockTable[pBlock->blkType]];
                if(curr_scan==NULL)
                    return VC1_FAIL;

                switch (pBlock->blkType)
                {
                case VC1_BLK_INTER8X8:
                    DecodeBlockInter8x8_VC1(&pContext->m_bitstream, m_pBlock,
                                curr_scan, picHeader->m_pCurrInterACDecSet,
                                &pContext->m_pSingleMB->EscInfo, VC1_SBP_0);
                    break;
                case VC1_BLK_INTER8X4:
                     DecodeBlockInter8x4_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan, picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, numCoef);
                    break;
                case VC1_BLK_INTER4X8:
                     DecodeBlockInter4x8_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan, picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, numCoef);
                    break;
                case VC1_BLK_INTER4X4:
                    numCoef = GetSubBlockPattern_4x4(pContext, blk_num);

                    DecodeBlockInter4x4_VC1(&pContext->m_bitstream, m_pBlock,
                                    curr_scan,picHeader->m_pCurrInterACDecSet,
                                    &pContext->m_pSingleMB->EscInfo, numCoef);
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


#endif //MFX_ENABLE_VC1_VIDEO_DECODE
