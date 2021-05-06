// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_h264_segment_decoder.h"

namespace UMC
{

int32_t H264SegmentDecoder::GetColocatedLocation(H264DecoderFrameEx *pRefFrame,
                                                int32_t Field,
                                                int32_t &block,
                                                int32_t *scale)
{
    int32_t location = -1;
    uint32_t cur_pic_struct = m_pCurrentFrame->m_PictureStructureForDec;
    uint32_t ref_pic_struct = pRefFrame->m_PictureStructureForDec;
    int32_t xCol = block & 3;
    int32_t yCol = block - xCol;

    if (cur_pic_struct==FRM_STRUCTURE && ref_pic_struct != FLD_STRUCTURE)
    {
        if (scale)
            *scale = 0;
        return m_CurMBAddr;
    }
    else if (cur_pic_struct==AFRM_STRUCTURE && ref_pic_struct != FLD_STRUCTURE)
    {
        int32_t preColMBAddr=m_CurMBAddr;
        H264DecoderMacroblockGlobalInfo *preColMB = &pRefFrame->m_mbinfo.mbs[preColMBAddr];
        int32_t cur_mbfdf = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
        int32_t ref_mbfdf = pGetMBFieldDecodingFlag(preColMB);

        if (cur_mbfdf == ref_mbfdf)
        {
            if (scale)
                *scale = 0;
            return m_CurMBAddr;
        }
        else if (cur_mbfdf > ref_mbfdf) //current - field reference - frame
        {
            if ((preColMBAddr & 1))
            {
                preColMBAddr-=1;//get pair
                if (yCol >= 8)
                    preColMBAddr += 1;//get pair again
            }
            else
            {
                if (yCol >= 8)
                    preColMBAddr += 1;//get pair
            }

            yCol *= 2;
            yCol &= 15;
            if (scale)
                *scale = 1;
        }
        else
        {
            int32_t curPOC = m_pCurrentFrame->PicOrderCnt(0, 3);
            int32_t topPOC = pRefFrame->PicOrderCnt(0,1);
            int32_t bottomPOC = pRefFrame->PicOrderCnt(1,1);

            preColMBAddr &= -2; // == (preColMBAddr/2)*2;
            if (abs(topPOC - curPOC) >= abs(bottomPOC - curPOC))
                preColMBAddr += 1;

            yCol = (m_CurMBAddr & 1)*8 + 4*(yCol/8);
            if (scale)
                *scale = -1;
        }

        block = yCol + xCol;
        return preColMBAddr;
    }
    else if (cur_pic_struct==FLD_STRUCTURE && ref_pic_struct==FLD_STRUCTURE)
    {
        if (scale)
            *scale = 0;
        int32_t RefField = Field;

        if(RefField > m_field_index)
        {
            return (m_CurMBAddr + m_pCurrentFrame->totalMBs);
        }

        return RefField < m_field_index ? (m_CurMBAddr - m_pCurrentFrame->totalMBs) : m_CurMBAddr;
    }
    else if (cur_pic_struct == FLD_STRUCTURE && ref_pic_struct == FRM_STRUCTURE)
    {
        uint32_t PicWidthInMbs = mb_width;
        uint32_t CurrMbAddr = m_field_index ? m_CurMBAddr - m_pCurrentFrame->totalMBs : m_CurMBAddr;
        if(scale)
            *scale = 1;
        yCol = ((2*yCol)&15);
        block = yCol+xCol;
        return 2*PicWidthInMbs*(CurrMbAddr/PicWidthInMbs) +
            (CurrMbAddr%PicWidthInMbs) + PicWidthInMbs*(yCol/8);
    }
    else if (cur_pic_struct == FRM_STRUCTURE && ref_pic_struct == FLD_STRUCTURE)
    {
        if (scale)
            *scale=-1;

        uint32_t PicWidthInMbs = mb_width;
        uint32_t CurrMbAddr = m_CurMBAddr;
        yCol = 8*((CurrMbAddr/PicWidthInMbs)&1) + 4 * (yCol/8);
        block = yCol+xCol;

        int32_t curPOC = m_pCurrentFrame->PicOrderCnt(0, 3);
        int32_t topPOC = pRefFrame->PicOrderCnt(pRefFrame->GetNumberByParity(0), 1);
        int32_t bottomPOC = pRefFrame->PicOrderCnt(pRefFrame->GetNumberByParity(1), 1);

        int32_t add = 0;
        if (abs(curPOC - topPOC) >= abs(curPOC - bottomPOC))
        {
            add = pRefFrame->totalMBs;
        }

        return (PicWidthInMbs*(CurrMbAddr/(2*PicWidthInMbs))+(CurrMbAddr%PicWidthInMbs)) + add;
    }
    else if (cur_pic_struct == FLD_STRUCTURE && ref_pic_struct == AFRM_STRUCTURE)
    {
        uint32_t CurrMbAddr = m_CurMBAddr;
        if (m_field_index)
            CurrMbAddr -= m_pCurrentFrame->totalMBs;
        int32_t bottom_field_flag = m_field_index;
        int32_t preColMBAddr = 2*CurrMbAddr;

        H264DecoderMacroblockGlobalInfo *preColMB = &pRefFrame->m_mbinfo.mbs[preColMBAddr];
        int32_t col_mbfdf = pGetMBFieldDecodingFlag(preColMB);

        if (!col_mbfdf)
        {
            if (yCol >= 8)
                preColMBAddr += 1;
            yCol = ((2*yCol)&15);
            if(scale)
                *scale=1;
        }
        else
        {
            if (bottom_field_flag)
                preColMBAddr += 1;
            if(scale)
                *scale=0;
        }

        block = yCol + xCol;
        return preColMBAddr;
    }
    else if (cur_pic_struct == AFRM_STRUCTURE && ref_pic_struct == FLD_STRUCTURE)
    {
        uint32_t CurrMbAddr = m_CurMBAddr;
        int32_t preColMBAddr = CurrMbAddr;

        int32_t cur_mbfdf = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
        int32_t cur_mbbf = (m_CurMBAddr & 1);
        int32_t curPOC = m_pCurrentFrame->PicOrderCnt(0, 3);
        int32_t topPOC = pRefFrame->PicOrderCnt(pRefFrame->GetNumberByParity(0), 1);
        int32_t bottomPOC = pRefFrame->PicOrderCnt(pRefFrame->GetNumberByParity(1), 1);

        int32_t bottom_field_flag = cur_mbfdf ? cur_mbbf :
                    abs(curPOC - topPOC) >= abs(curPOC - bottomPOC);

        if (cur_mbbf)
            preColMBAddr-=1;
        preColMBAddr = preColMBAddr/2;

        if (!cur_mbfdf)
        {
            yCol = 8*cur_mbbf + 4*(yCol/8);
            if (scale)
                *scale = -1;
        }
        else
        {
            if(scale)
                *scale = 0;
        }

        block = yCol + xCol;
        VM_ASSERT(preColMBAddr +(bottom_field_flag)*pRefFrame->totalMBs < m_pCurrentFrame->totalMBs);
        return preColMBAddr + (bottom_field_flag)*pRefFrame->totalMBs;
    }
    else // ARFM and FRM it's non-standard case.
    {
        VM_ASSERT(0);

        int32_t preColMBAddr=m_CurMBAddr;
        H264DecoderMacroblockGlobalInfo *preColMB = &pRefFrame->m_mbinfo.mbs[preColMBAddr];
        int32_t cur_mbfdf = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
        int32_t ref_mbfdf = pGetMBFieldDecodingFlag(preColMB);

        if (cur_mbfdf == ref_mbfdf)
        {
            if (scale)
                *scale = 0;
            return m_CurMBAddr;
        }
        else if (cur_mbfdf > ref_mbfdf) //current - field reference - frame
        {
            if ((preColMBAddr & 1))
            {
                preColMBAddr-=1;//get pair
                if (yCol >= 8)
                    preColMBAddr += 1;//get pair again
            }
            else
            {
                if (yCol >= 8)
                    preColMBAddr += 1;//get pair
            }

            yCol *= 2;
            yCol &= 15;
            if (scale)
                *scale = 1;
        }
        else
        {
            int32_t curPOC = m_pCurrentFrame->PicOrderCnt(0, 3);
            int32_t topPOC = pRefFrame->PicOrderCnt(0,1);
            int32_t bottomPOC = pRefFrame->PicOrderCnt(1,1);

            preColMBAddr &= -2; // == (preColMBAddr/2)*2;
            if (abs(topPOC - curPOC) >= abs(bottomPOC - curPOC))
                preColMBAddr += 1;

            yCol = (m_CurMBAddr & 1)*8 + 4*(yCol/8);
            if (scale)
                *scale = -1;
        }

        block = yCol + xCol;
        location = preColMBAddr;
    }

    return location;

} // int32_t H264SegmentDecoder::GetColocatedLocation(DecodedFrame *pRefFrame, uint8_t Field, int32_t &block, int8_t *scale)

void H264SegmentDecoder::AdjustIndex(int32_t ref_mb_is_bottom, int32_t ref_mb_is_field, int8_t &RefIdx)
{
    if (RefIdx<0)
    {
        RefIdx=0;
        return;
    }

    if (ref_mb_is_field) //both are AFRM
    {
        int32_t cur_mb_is_bottom = (m_CurMBAddr & 1);
        int32_t cur_mb_is_field = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
        if (cur_mb_is_field)
        {
            bool same_parity = (((RefIdx&1) ^ ref_mb_is_bottom) == cur_mb_is_bottom);
            if (same_parity)
            {
                RefIdx&=-2;
            }
            else
            {
                RefIdx|=1;
            }
        }
        else if (m_pCurrentFrame->m_PictureStructureForDec!=AFRM_STRUCTURE)
        {
            RefIdx>>=1;
        }
    }
} // void H264SegmentDecoder::AdjustIndex(uint8_t ref_mb_is_bottom, int8_t ref_mb_is_field, int8_t &RefIdx)

} // namespace UMC
#endif // MFX_ENABLE_H264_VIDEO_DECODE
