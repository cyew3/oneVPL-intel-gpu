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

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common_blk_order_tbl.h"
#include "assert.h"
#include "umc_vc1_dec_exception.h"
#include "umc_vc1_huffman.h"

using namespace UMC;
using namespace UMC::VC1Exceptions;


inline int32_t DecodeSymbol(IppiBitstream* pBitstream,
                           int16_t* run,
                           int16_t* level,
                           const IppiACDecodeSet_VC1 * decodeSet,
                           IppiEscInfo_VC1* EscInfo)
{
    int ret;
    int32_t sign = 0;
    int32_t code = 0;
    int32_t escape_mode = 0;
    int32_t ESCLR = 0;

    int32_t tmp_run = 0;
    int32_t tmp_level = 0;
    int32_t last = 0;

    ret = DecodeHuffmanOne(&pBitstream->pBitstream,
                                      &pBitstream->bitOffset,
                                      &code, decodeSet->pRLTable);
#ifdef VC1_VLD_CHECK
    if (ret != 0)
        throw vc1_exception(vld);
#endif


    #ifdef VC1_DEBUG_ON
        if(code == IPPVC_ESCAPE)
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                    VM_STRING("Index = ESCAPE\n"));
    #endif

    if(code != IPPVC_ESCAPE)
    {
        tmp_run      = (code & 0X0000FF00)>>8;
        tmp_level    = code & 0X000000FF;
        last = code >> 16;

        VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, sign);

        tmp_level    = (1-(sign<<1))*tmp_level;
    }
    else
    {
        VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, escape_mode);

        if(escape_mode == 1)
        {
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                            VM_STRING("Index = ESCAPE Mode 1\n"));
#endif

            ret = DecodeHuffmanOne((&pBitstream->pBitstream), (&pBitstream->bitOffset), &code,
                                              decodeSet->pRLTable);

#ifdef VC1_VLD_CHECK
            if (ret != 0)
                throw vc1_exception(vld);
#endif


            tmp_run      = (code & 0X0000FF00)>>8;
            tmp_level    = code & 0X000000FF;
            last = code>>16;

            if (last)
                tmp_level = tmp_level + decodeSet->pDeltaLevelLast1[tmp_run];
            else
                tmp_level = tmp_level + decodeSet->pDeltaLevelLast0[tmp_run];

            VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, sign);
            tmp_level = (1-(sign<<1))*tmp_level;
        }
        else
        {
            VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, escape_mode);
            if(escape_mode == 1)
            {
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                VM_STRING("Index = ESCAPE Mode 2\n"));
#endif

                ret = DecodeHuffmanOne((&pBitstream->pBitstream), (&pBitstream->bitOffset),
                                                    &code, decodeSet->pRLTable);

#ifdef VC1_VLD_CHECK
                if (ret != 0)
                    throw vc1_exception(vld);
#endif

                tmp_run      = (code & 0X0000FF00)>>8;
                tmp_level    = code & 0X000000FF;
                last = (code&0x00FF0000)>>16;

                if (last)
                    tmp_run = tmp_run + decodeSet->pDeltaRunLast1[tmp_level] + 1;
                else
                    tmp_run = tmp_run + decodeSet->pDeltaRunLast0[tmp_level] + 1;

                VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, sign);
                tmp_level = (1-(sign<<1))*tmp_level;
            }
            else
            {
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                                                VM_STRING("Index = ESCAPE Mode 3\n"));
#endif
                VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, ESCLR);

                last = ESCLR;

                if (EscInfo->levelSize + EscInfo->runSize == 0)
                {
                    if(EscInfo->bEscapeMode3Tbl == VC1_ESCAPEMODE3_Conservative)
                    {
                        VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 3, EscInfo->levelSize);
                        if(EscInfo->levelSize == 0)
                        {
                            VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 2, EscInfo->levelSize);
                            EscInfo->levelSize += 8;
                        }
                    }
                    else
                    {
                        int32_t bit_count = 1;
                        VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, EscInfo->levelSize);
                        while((EscInfo->levelSize == 0) && (bit_count < 6))
                        {
                            VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, EscInfo->levelSize);
                            bit_count++;
                        }

                        if(bit_count == 6 && EscInfo->levelSize == 0)
                            bit_count++;

                        EscInfo->levelSize = bit_count + 1;
                    }

                    VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 2, EscInfo->runSize);
                    EscInfo->runSize = 3 + EscInfo->runSize;
                }

                VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), EscInfo->runSize, tmp_run);

                VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), 1, sign);

                VC1BitstreamParser::GetNBits((pBitstream->pBitstream), (pBitstream->bitOffset), EscInfo->levelSize,tmp_level);
                tmp_level = (1-(sign<<1))*tmp_level;
            }
        }
    }
    (*run) = (int16_t)tmp_run;
    (*level) = (int16_t)tmp_level;
    return last;
}


IppStatus DecodeBlockACIntra_VC1(IppiBitstream* pBitstream, int16_t* pDst,
                 const  uint8_t* pZigzagTbl,  const IppiACDecodeSet_VC1 * pDecodeSet,
                 IppiEscInfo_VC1* pEscInfo)
{
    int32_t last_flag = 0;
    int16_t run = 0, level = 0;
    int32_t curr_position = 1;
    IppStatus sts = ippStsNoErr;

#ifdef VC1_VLD_CHECK
    if((!pBitstream)||(!pDst) || (!pDecodeSet) || (!pZigzagTbl) || (!pEscInfo) || (!pBitstream->pBitstream))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more

    if((pBitstream->bitOffset < 0) || (pBitstream->bitOffset>31))
        throw vc1_exception(internal_pipeline_error);// Global problem, no need to decode more
#endif

    do
    {
        last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
            VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
            run,
            (level < 0) ? '-' : '+',
            (level < 0) ? -level : level,
            last_flag);
#endif

#ifdef VC1_VLD_CHECK
        if(curr_position > 63)
        {
            throw vc1_exception(vld);
        }
#endif

        curr_position = curr_position + run;
        curr_position = curr_position & 63; 

        pDst[pZigzagTbl[curr_position]] = pDst[pZigzagTbl[curr_position]] + level;

        curr_position++;
    } while (last_flag == 0);

    return sts;
}


IppStatus DecodeBlockInter8x8_VC1(IppiBitstream* pBitstream, int16_t* pDst,
                      const  uint8_t* pZigzagTbl,  const IppiACDecodeSet_VC1 * pDecodeSet,
                      IppiEscInfo_VC1* pEscInfo, int32_t subBlockPattern)
{
    (void)subBlockPattern;

    int32_t last_flag = 0;
    int16_t run = 0, level = 0;
    int32_t curr_position = 0;
    IppStatus sts = ippStsNoErr;

#ifdef VC1_VLD_CHECK
    if((!pBitstream)||(!pDst) || (!pDecodeSet) || (!pZigzagTbl) || (!pEscInfo) || (!pBitstream->pBitstream))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more

    if((pBitstream->bitOffset < 0) || (pBitstream->bitOffset>31))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more
#endif

    do
    {
        last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
            VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
            run,
            (level < 0) ? '-' : '+',
            (level < 0) ? -level : level,
            last_flag);
#endif

#ifdef VC1_VLD_CHECK
        if(curr_position > 63)
        {
            throw vc1_exception(vld);
        }
#endif

        curr_position = curr_position + run;
        curr_position = curr_position & 63;
        pDst[pZigzagTbl[curr_position]] = pDst[pZigzagTbl[curr_position]] + level;

        curr_position++;
    } while (last_flag == 0);

    return sts;
}

IppStatus DecodeBlockInter4x8_VC1(IppiBitstream* pBitstream, int16_t* pDst,
                      const  uint8_t* pZigzagTbl,  const IppiACDecodeSet_VC1 * pDecodeSet,
                      IppiEscInfo_VC1* pEscInfo, int32_t subBlockPattern)
{
    int32_t last_flag = 0;
    int16_t run = 0, level = 0;
    int32_t curr_position = 0;
    IppStatus sts = ippStsNoErr;

#ifdef VC1_VLD_CHECK
    if((!pBitstream)||(!pDst) || (!pDecodeSet) || (!pZigzagTbl) || (!pEscInfo) || (!pBitstream->pBitstream))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more

    if((pBitstream->bitOffset < 0) || (pBitstream->bitOffset>31))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more
#endif

    if(subBlockPattern & VC1_SBP_0)
        do
        {
            last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

    #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
                run,
                (level < 0) ? '-' : '+',
                (level < 0) ? -level : level,
                last_flag);
    #endif

#ifdef VC1_VLD_CHECK
        if(curr_position > 63)
        {
            // we can decode next MB
            throw vc1_exception(vld);
        }
#endif

            curr_position = curr_position + run;
            curr_position = curr_position & 63;
            pDst[pZigzagTbl[curr_position]] = pDst[pZigzagTbl[curr_position]] + level;

            curr_position++;
        } while (last_flag == 0);

    curr_position = 0;
    if(subBlockPattern & VC1_SBP_1)
        do
        {
            last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

    #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
                run,
                (level < 0) ? '-' : '+',
                (level < 0) ? -level : level,
                last_flag);
    #endif

#ifdef VC1_VLD_CHECK
            if(curr_position > 31)
                throw vc1_exception(vld);
#endif

            curr_position = curr_position + run;
            curr_position = curr_position & 31;
            pDst[pZigzagTbl[curr_position + 32]] = pDst[pZigzagTbl[curr_position + 32]] + level;

            curr_position++;
        } while (last_flag == 0);

        return sts;
}

IppStatus DecodeBlockInter8x4_VC1(IppiBitstream* pBitstream, int16_t* pDst,
                      const  uint8_t* pZigzagTbl,  const IppiACDecodeSet_VC1 * pDecodeSet,
                      IppiEscInfo_VC1* pEscInfo, int32_t subBlockPattern)
{
    int32_t last_flag = 0;
    int16_t run = 0, level = 0;
    int32_t curr_position = 0;
    IppStatus sts = ippStsNoErr;

#ifdef VC1_VLD_CHECK
    if((!pBitstream)||(!pDst) || (!pDecodeSet) || (!pZigzagTbl) || (!pEscInfo) || (!pBitstream->pBitstream))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more

    if((pBitstream->bitOffset < 0) || (pBitstream->bitOffset>31))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more
#endif

    if(subBlockPattern & VC1_SBP_0)
        do
        {
            last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

    #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
                run,
                (level < 0) ? '-' : '+',
                (level < 0) ? -level : level,
                last_flag);
    #endif

#ifdef VC1_VLD_CHECK
        if(curr_position > 63)
        {
            throw vc1_exception(vld);
        }
#endif
            curr_position = curr_position + run;
            curr_position = curr_position & 63;
            pDst[pZigzagTbl[curr_position]] = pDst[pZigzagTbl[curr_position]] + level;

            curr_position++;
        } while (last_flag == 0);


    curr_position = 0;
    if(subBlockPattern & VC1_SBP_1)
        do
        {
            last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

    #ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
                VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
                run,
                (level < 0) ? '-' : '+',
                (level < 0) ? -level : level,
                last_flag);
    #endif

#ifdef VC1_VLD_CHECK
        if(curr_position > 31)
        {
            throw vc1_exception(vld);
        }
#endif


            curr_position = curr_position + run;
            curr_position = curr_position & 31;
            pDst[pZigzagTbl[curr_position + 32]] = pDst[pZigzagTbl[curr_position + 32]] + level;

            curr_position++;
        } while (last_flag == 0);

        return sts;
}

IppStatus DecodeBlockInter4x4_VC1(IppiBitstream* pBitstream, int16_t* pDst,
                      const  uint8_t* pZigzagTbl,  const IppiACDecodeSet_VC1 * pDecodeSet,
                      IppiEscInfo_VC1* pEscInfo, int32_t subBlockPattern)
{
    int32_t last_flag = 0;
    int16_t run = 0, level = 0;
    int32_t curr_position = 0;
    IppStatus sts = ippStsNoErr;

#ifdef VC1_VLD_CHECK
    if((!pBitstream)||(!pDst) || (!pDecodeSet) || (!pZigzagTbl) || (!pEscInfo) || (!pBitstream->pBitstream))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more

    if((pBitstream->bitOffset < 0) || (pBitstream->bitOffset>31))
        throw vc1_exception(internal_pipeline_error); // Global problem, no need to decode more
#endif

    if(subBlockPattern & VC1_SBP_0)
    do
    {
        last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
            VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
            run,
            (level < 0) ? '-' : '+',
            (level < 0) ? -level : level,
            last_flag);
#endif

#ifdef VC1_VLD_CHECK
        if(curr_position > 63)
        {
            throw vc1_exception(vld);
        }
#endif

        curr_position = curr_position + run;
        curr_position = curr_position & 63;
        pDst[pZigzagTbl[curr_position]] = pDst[pZigzagTbl[curr_position]] + level;

        curr_position++;
    } while (last_flag == 0);


    curr_position = 0;

    if(subBlockPattern & VC1_SBP_1)
    do
    {
        last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
            VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
            run,
            (level < 0) ? '-' : '+',
            (level < 0) ? -level : level,
            last_flag);
#endif

#ifdef VC1_VLD_CHECK
        if((curr_position + run) > 47)
            throw vc1_exception(vld);
#endif

        curr_position = curr_position + run;
        pDst[pZigzagTbl[curr_position + 16]] = pDst[pZigzagTbl[curr_position + 16]] + level;

        curr_position++;
    } while (last_flag == 0);


    curr_position = 0;

    if(subBlockPattern & VC1_SBP_2)
    do
    {
        last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
            VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
            run,
            (level < 0) ? '-' : '+',
            (level < 0) ? -level : level,
            last_flag);
#endif

#ifdef VC1_VLD_CHECK
        if(curr_position > 31)
            throw vc1_exception(vld);
#endif

        curr_position = curr_position + run;
        curr_position = curr_position & 31;
        pDst[pZigzagTbl[curr_position + 32]] = pDst[pZigzagTbl[curr_position + 32]] + level;

        curr_position++;
    } while (last_flag == 0);


    curr_position = 0;
    if(subBlockPattern & VC1_SBP_3)
    do
    {
        last_flag = DecodeSymbol(pBitstream, &run, &level, pDecodeSet, pEscInfo);

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_COEFFS,
            VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
            run,
            (level < 0) ? '-' : '+',
            (level < 0) ? -level : level,
            last_flag);
#endif

#ifdef VC1_VLD_CHECK
        if((curr_position + run) > 15)
            throw vc1_exception(vld);
#endif

        curr_position = curr_position + run;
        pDst[pZigzagTbl[curr_position + 48]] = pDst[pZigzagTbl[curr_position + 48]] + level;

        curr_position++;
    } while (last_flag == 0);

    return sts;
}

uint8_t GetSubBlockPattern_8x4_4x8(VC1Context* pContext,int32_t blk_num)
{
    int32_t   Value;

    //Table 67: 8x4 and 4x8 Transform sub-block pattern code-table for Progressive pictures
    //              8x4 Sub-block pattern       4x8 Sub-block pattern
    //SUBBLKPAT VLC     Top Bottom              Left Right
    //10                      X                        X
    //0                  X    X                   X    X
    //11                 X                        X

    VC1_GET_BITS(1, Value);
    if(0 != Value)
    {

        VC1_GET_BITS(1, Value);
        Value++;

        if(1 == Value)//for codeword "10b"
            Value = VC1_SBP_1;
        else //for codeword "11b"
            Value = VC1_SBP_0;
    }
    else
    {
        Value = VC1_SBP_0|VC1_SBP_1;
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TT,
                VM_STRING("Sub-block pattern 4x8|8x4: %d\n"), Value);
#endif

    pContext->m_pSingleMB->m_pSingleBlock[blk_num].numCoef = (uint8_t)Value;
    return ((uint8_t)Value);
}

uint8_t GetSubBlockPattern_4x4(VC1Context* pContext,int32_t blk_num)
{
    int ret;
    int32_t   Value;

    ret = DecodeHuffmanOne(   &pContext->m_bitstream.pBitstream,
        &pContext->m_bitstream.bitOffset,
        &Value,
        pContext->m_picLayerHeader->m_pCurrSBPtbl);

#ifdef VC1_VLD_CHECK
    if (ret != 0)
        throw vc1_exception(vld);
#endif

    pContext->m_pSingleMB->m_pSingleBlock[blk_num].numCoef = (uint8_t)Value;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TT,
        VM_STRING("Sub-block pattern 4x4: %d\n"),
        pContext->m_pSingleMB->m_pSingleBlock[blk_num].numCoef);
#endif

    return ((uint8_t)Value);
}

uint8_t GetTTBLK(VC1Context* pContext, int32_t blk_num)
{
    int ret;
    int32_t Value;
    VC1MB *pMB = pContext->m_pCurrMB;
    VC1Block *pBlock = &pMB->m_pBlocks[blk_num];
    uint8_t numCoef = 0;

    ret = DecodeHuffmanOne(   &pContext->m_bitstream.pBitstream,
                                        &pContext->m_bitstream.bitOffset,
                                        &Value,
                                        pContext->m_picLayerHeader->m_pCurrTTBLKtbl);
#ifdef VC1_VLD_CHECK
    if (ret != 0)
        throw vc1_exception(vld);
#endif

    switch(Value)
    {
        case VC1_SBP_8X8_BLK:
            numCoef = VC1_SBP_0;
            pBlock->blkType  = VC1_BLK_INTER8X8;
        break;

        case VC1_SBP_8X4_BOTTOM_BLK:
            numCoef = VC1_SBP_1;
            pBlock->blkType = VC1_BLK_INTER8X4;
        break;

        case VC1_SBP_8X4_TOP_BLK:
            numCoef = VC1_SBP_0;
            pBlock->blkType = VC1_BLK_INTER8X4;
        break;

        case VC1_SBP_8X4_BOTH_BLK:
            numCoef = VC1_SBP_0|VC1_SBP_1;
            pBlock->blkType = VC1_BLK_INTER8X4;
        break;

        case VC1_SBP_4X8_RIGHT_BLK:
            numCoef = VC1_SBP_1;
            pBlock->blkType = VC1_BLK_INTER4X8;
        break;

        case VC1_SBP_4X8_LEFT_BLK:
            numCoef = VC1_SBP_0;
            pBlock->blkType = VC1_BLK_INTER4X8;
        break;

        case VC1_SBP_4X8_BOTH_BLK:
            numCoef = VC1_SBP_0|VC1_SBP_1;
            pBlock->blkType = VC1_BLK_INTER4X8;
        break;

        case VC1_SBP_4X4_BLK:
            numCoef = VC1_SBP_0|VC1_SBP_1|VC1_SBP_2|VC1_SBP_3;
            pBlock->blkType = VC1_BLK_INTER4X4;
        break;

        default:
            VM_ASSERT(0);
    }

    pContext->m_pSingleMB->m_pSingleBlock[blk_num].numCoef = numCoef;
    return numCoef;
}

uint32_t GetDCStepSize(int32_t MQUANT)
{
    uint32_t DCStepSize;

    if(MQUANT < 4)
        DCStepSize = 1 << MQUANT;
    else
        DCStepSize = MQUANT/2 + 6;

    return DCStepSize;
}

#ifdef _OWN_FUNCTION
IppStatus _own_ippiQuantInvIntraUniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                  int32_t doubleQuant, mfxSize* pDstSizeNZ)
{
    int32_t i = 1;
    int32_t j = 0;
    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;

    int16_t* pSrc = pSrcDst;

    for(j = 0; j < 8; j++)
    {
       for(i; i < 8; i++)
       {
           pSrc[i] = (int16_t)(pSrc[i]*doubleQuant);
           S = !pSrc[i] ;
           X[i] = X[i] + S;
           Y[j] = Y[j] + S;
       }
       i = 0;
       pSrc = (int16_t*)((uint8_t*)pSrc + srcDstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}

IppStatus _own_ippiQuantInvIntraNonuniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                          int32_t doubleQuant, mfxSize* pDstSizeNZ)
{
    int32_t i = 1;
    int32_t j = 0;
    int16_t* pSrc = pSrcDst;
    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;



    for(j = 0; j < 8; j++)
    {
        for(i; i < 8; i++)
        {
            pSrc[i] = (int16_t)(pSrc[i]*doubleQuant) + (int16_t)(VC1_SIGN(pSrc[i])*(doubleQuant>>1));
            S = !pSrc[i] ;
            X[i] = X[i] + S;
            Y[j] = Y[j] + S;
        }
        i = 0;
        pSrc = (int16_t*)((uint8_t*)pSrc + srcDstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
IppStatus _own_ippiQuantInvInterUniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                       int32_t doubleQuant, mfxSize roiSize,
                                                       mfxSize* pDstSizeNZ)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;

    int16_t* pSrc = pSrcDst;

    for(j = 0; j < roiSize.height; j++)
    {
       for(i = 0; i < roiSize.width; i++)
       {
           pSrc[i] = (int16_t)(pSrc[i]*doubleQuant);
           S = !pSrc[i] ;
           X[i] = X[i] + S;
           Y[j] = Y[j] + S;
       }
       pSrc = (int16_t*)((uint8_t*)pSrc + srcDstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
IppStatus _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                          int32_t doubleQuant, mfxSize roiSize,
                                                          mfxSize* pDstSizeNZ)
{
    int32_t i = 0;
    int32_t j = 0;
    int16_t* pSrc = pSrcDst;
    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;


    for(j = 0; j < roiSize.height; j++)
    {
        for(i; i < roiSize.width; i++)
        {
            pSrc[i] = (int16_t)(pSrc[i]*doubleQuant) + (int16_t)(VC1_SIGN(pSrc[i])*(doubleQuant>>1));
            S = !pSrc[i] ;
            X[i] = X[i] + S;
            Y[j] = Y[j] + S;
        }
        i = 0;
        pSrc = (int16_t*)((uint8_t*)pSrc + srcDstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
#endif

#endif //MFX_ENABLE_VC1_VIDEO_DECODE




