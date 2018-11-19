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

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_intens_comp_tbl.h"

#ifdef ALLOW_SW_VC1_FALLBACK
void SZTables(VC1Context* pContext)
{
    int32_t CurrIndex = pContext->m_frmBuff.m_iCurrIndex;

    pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[0] = 0;
    pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[1] = 0;
    pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTableCurr[0] = 0;
    pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTableCurr[1] = 0;

    pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[0] = 0;
    pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[1] = 0;
    pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTableCurr[0] = 0;
    pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTableCurr[1] = 0;
}
void CreateComplexICTablesForFields(VC1Context* pContext)
{
    int32_t PrevIndex = pContext->m_frmBuff.m_iPrevIndex;
    int32_t CurrIndex = pContext->m_frmBuff.m_iCurrIndex;
    int32_t i;
    {
        {
            if (pContext->m_frmBuff.m_pFrames[CurrIndex].ICFieldMask & 4) // top of first
            {
                pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[0] = pContext->LumaTable[0];
                pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[0] = pContext->ChromaTable[0];
            }

            if (pContext->m_frmBuff.m_pFrames[CurrIndex].ICFieldMask & 8) // bottom of first
            {
                pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[1] = pContext->LumaTable[1];
                pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[1] = pContext->ChromaTable[1];
            }

            if (pContext->m_frmBuff.m_pFrames[CurrIndex].ICFieldMask & 1) // top of second
            {
                pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTableCurr[0] = pContext->LumaTable[2];
                pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTableCurr[0] = pContext->ChromaTable[2];
            }
        }
    }

    if (pContext->m_frmBuff.m_pFrames[PrevIndex].FCM == VC1_FieldInterlace)
    {
        int32_t j;
        for (j = 0; j < 2; j++)
        {
            if (pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j])
            {
                if(pContext->m_frmBuff.m_pFrames[PrevIndex].LumaTableCurr[j])
                {
                    //need to twice IC
                    uint8_t temp_luma[256];
                    uint8_t temp_chroma[256];
                    for (i = 0; i < 256; i++)
                    {
                        temp_luma[i] = pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j][pContext->m_frmBuff.m_pFrames[PrevIndex].LumaTableCurr[j][i]];
                        temp_chroma[i] = pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[j][pContext->m_frmBuff.m_pFrames[PrevIndex].ChromaTableCurr[j][i]];
                    }

                    for (i = 0; i < 256; i++)
                    {
                        pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j][i] = temp_luma[i];
                        pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[j][i] = temp_chroma[i];
                    }
                }
            }
            else if(pContext->m_frmBuff.m_pFrames[PrevIndex].LumaTableCurr[j])
            {
                pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j] = pContext->m_frmBuff.m_pFrames[PrevIndex].LumaTableCurr[j];
                pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[j] = pContext->m_frmBuff.m_pFrames[PrevIndex].ChromaTableCurr[j];
            }
        }
    }
    else
    {
    }
}
void CreateComplexICTablesForFrame(VC1Context* pContext)
{
    int32_t PrevIndex = pContext->m_frmBuff.m_iPrevIndex;
    int32_t i;
    {
        int32_t CurrIndex = pContext->m_frmBuff.m_iCurrIndex;

        if (pContext->m_bIntensityCompensation)
        {
            pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[0] = pContext->LumaTable[0];
            pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[1] = pContext->LumaTable[0];

            pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[0] = pContext->ChromaTable[0];
            pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[1] = pContext->ChromaTable[0];

            pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTableCurr[0] = 0;
            pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTableCurr[1] = 0;

            pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTableCurr[0] = 0;
            pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTableCurr[1] = 0;
        }

        if (pContext->m_frmBuff.m_pFrames[PrevIndex].FCM == VC1_FieldInterlace)
        {
            int32_t j;
            int32_t ICFieldNum = -1;
            for (j = 0; j < 2; j++)
            {
                if (pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j])
                {
                    //need to twice IC
                    if(pContext->m_frmBuff.m_pFrames[PrevIndex].LumaTableCurr[j])
                    {
                        ICFieldNum = j;
                        uint8_t temp_luma[256];
                        uint8_t temp_chroma[256];
                        for (i = 0; i < 256; i++)
                        {
                            temp_luma[i] = pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j][pContext->m_frmBuff.m_pFrames[PrevIndex].LumaTableCurr[j][i]];
                            temp_chroma[i] = pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[j][pContext->m_frmBuff.m_pFrames[PrevIndex].ChromaTableCurr[j][i]];
                        }

                        pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j] = pContext->LumaTable[1];
                        pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[j] = pContext->ChromaTable[1];


                        for (i = 0; i < 256; i++)
                        {
                            pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j][i] = temp_luma[i];
                            pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[j][i] = temp_chroma[i];
                        }
                    }
                }
                else
                {
                    pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[j] = pContext->m_frmBuff.m_pFrames[PrevIndex].LumaTableCurr[j];
                    pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[j] = pContext->m_frmBuff.m_pFrames[PrevIndex].ChromaTableCurr[j];
                }
            }
        }
    }
}
void UpdateICTablesForSecondField(VC1Context* pContext)
{
    int32_t CurrIndex = pContext->m_frmBuff.m_iCurrIndex;
    int32_t i;

    if ((pContext->m_frmBuff.m_pFrames[CurrIndex].ICFieldMask & 2)&&
        (pContext->m_frmBuff.m_pFrames[CurrIndex].ICFieldMask & 8))
    {
        uint8_t temp_luma[256];
        uint8_t temp_chroma[256];

        for (i = 0; i < 256; i++)
        {
            temp_luma[i] = pContext->LumaTable[3][pContext->LumaTable[1][i]];
            temp_chroma[i] = pContext->ChromaTable[3][pContext->ChromaTable[1][i]];
        }
        for (i = 0; i < 256; i++)
        {
            pContext->LumaTable[1][i] = temp_luma[i];
            pContext->ChromaTable[1][i] = temp_chroma[i];
        }
    }
    else if (pContext->m_frmBuff.m_pFrames[CurrIndex].ICFieldMask & 2)
    {
        pContext->m_frmBuff.m_pFrames[CurrIndex].LumaTablePrev[1] = pContext->LumaTable[3];
        pContext->m_frmBuff.m_pFrames[CurrIndex].ChromaTablePrev[1] = pContext->ChromaTable[3];

    }
}

#endif // #ifdef ALLOW_SW_VC1_FALLBACK

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
