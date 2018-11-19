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
#include "umc_vc1_common_blk_order_tbl.h"

void RangeMapping(VC1Context* pContext, UMC::FrameMemID input, UMC::FrameMemID output)
{
    mfxSize roiSize;

    if (pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
    {
        int32_t YCoeff = -1;
        int32_t UVCoeff = -1;
        if (( (pContext->m_picLayerHeader->RANGEREDFRM-1) == *pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY)||
            (pContext->m_picLayerHeader->PTYPE != VC1_B_FRAME))
            // last frame!!!!!!!!!!!!!!
        {
            UVCoeff = YCoeff = *pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY;
        }
        else if ((pContext->m_picLayerHeader->RANGEREDFRM == 8)&&
            (pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME))
        {
            UVCoeff = YCoeff = 7;
        }
        //warning - B Range Reduction does not match anchor frame
        {
            roiSize.height = pContext->m_seqLayerHeader.heightMB * 16;
            roiSize.width  =  pContext->m_seqLayerHeader.widthMB * 16;
            // simple copy
            if (-1 == YCoeff)
                ippiCopy_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pY,
                pContext->m_frmBuff.m_pFrames[input].m_iYPitch,
                pContext->m_frmBuff.m_pFrames[output].m_pY,
                pContext->m_frmBuff.m_pFrames[output].m_iYPitch,
                roiSize);
            else
                _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pY,
                pContext->m_frmBuff.m_pFrames[input].m_iYPitch,
                pContext->m_frmBuff.m_pFrames[output].m_pY,
                pContext->m_frmBuff.m_pFrames[output].m_iYPitch,
                roiSize,YCoeff);

            roiSize.height >>= 1;
            roiSize.width >>= 1;

            if (-1 == UVCoeff)
            {
                ippiCopy_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pV,
                    pContext->m_frmBuff.m_pFrames[input].m_iVPitch,
                    pContext->m_frmBuff.m_pFrames[output].m_pV,
                    pContext->m_frmBuff.m_pFrames[output].m_iVPitch,
                    roiSize);

                ippiCopy_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pU,
                    pContext->m_frmBuff.m_pFrames[input].m_iUPitch,
                    pContext->m_frmBuff.m_pFrames[output].m_pU,
                    pContext->m_frmBuff.m_pFrames[output].m_iUPitch,
                    roiSize);
            }
            else
            {
                _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pV,
                    pContext->m_frmBuff.m_pFrames[input].m_iVPitch,
                    pContext->m_frmBuff.m_pFrames[output].m_pV,
                    pContext->m_frmBuff.m_pFrames[output].m_iVPitch,
                    roiSize, UVCoeff);

                _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pU,
                    pContext->m_frmBuff.m_pFrames[input].m_iUPitch,
                    pContext->m_frmBuff.m_pFrames[output].m_pU,
                    pContext->m_frmBuff.m_pFrames[output].m_iUPitch,
                    roiSize, UVCoeff);

            }
        }
    }
    else
    {
        if(pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
        {
            roiSize.height = 2*(pContext->m_seqLayerHeader.CODED_HEIGHT+1);
            roiSize.width  = 2*(pContext->m_seqLayerHeader.CODED_WIDTH+1);

            _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pY,
                pContext->m_frmBuff.m_pFrames[input].m_iYPitch,
                pContext->m_frmBuff.m_pFrames[output].m_pY,
                pContext->m_frmBuff.m_pFrames[output].m_iYPitch,
                roiSize, pContext->m_seqLayerHeader.RANGE_MAPY);
        }
        if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
        {
            roiSize.height = (pContext->m_seqLayerHeader.CODED_HEIGHT+1);
            roiSize.width  = (pContext->m_seqLayerHeader.CODED_WIDTH+1);

            _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pV,
                pContext->m_frmBuff.m_pFrames[input].m_iVPitch,
                pContext->m_frmBuff.m_pFrames[output].m_pV,
                pContext->m_frmBuff.m_pFrames[output].m_iVPitch,
                roiSize, pContext->m_seqLayerHeader.RANGE_MAPUV);

            _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[input].m_pU,
                pContext->m_frmBuff.m_pFrames[input].m_iUPitch,
                pContext->m_frmBuff.m_pFrames[output].m_pU,
                pContext->m_frmBuff.m_pFrames[output].m_iUPitch,
                roiSize, pContext->m_seqLayerHeader.RANGE_MAPUV);
        }
    }
}


void RangeRefFrame(VC1Context* pContext)
{
    uint32_t index= pContext->m_frmBuff.m_iPrevIndex;
    mfxSize roiSize;
    uint32_t width = (pContext->m_seqLayerHeader.widthMB * VC1_PIXEL_IN_LUMA);

    if(*pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY == -1
        && *pContext->m_frmBuff.m_pFrames[index].pRANGE_MAPY == 7)
    {
        roiSize.height = 2*(pContext->m_seqLayerHeader.CODED_HEIGHT+1);
        roiSize.width  = 2*(pContext->m_seqLayerHeader.CODED_WIDTH+1);

        _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[index].m_pY,
            pContext->m_frmBuff.m_pFrames[index].m_iYPitch,
            pContext->m_frmBuff.m_pFrames[index].m_pY,
            pContext->m_frmBuff.m_pFrames[index].m_iYPitch,
            roiSize,7);

        roiSize.height >>= 1;
        roiSize.width  >>= 1;
        width >>= 1;

        _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[index].m_pV,
            pContext->m_frmBuff.m_pFrames[index].m_iVPitch,
            pContext->m_frmBuff.m_pFrames[index].m_pV,
            pContext->m_frmBuff.m_pFrames[index].m_iVPitch,
            roiSize,7);

        _own_ippiRangeMap_VC1_8u_C1R(pContext->m_frmBuff.m_pFrames[index].m_pU,
            pContext->m_frmBuff.m_pFrames[index].m_iUPitch,
            pContext->m_frmBuff.m_pFrames[index].m_pU,
            pContext->m_frmBuff.m_pFrames[index].m_iUPitch,
            roiSize,7);

        *pContext->m_frmBuff.m_pFrames[index].pRANGE_MAPY = -2;
    }
    else if(*pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY == 7
        && *pContext->m_frmBuff.m_pFrames[index].pRANGE_MAPY   == -1)
    {
        RangeDown(pContext->m_frmBuff.m_pFrames[index].m_pY,
            pContext->m_frmBuff.m_pFrames[index].m_iYPitch,
            pContext->m_frmBuff.m_pFrames[index].m_pY,
            pContext->m_frmBuff.m_pFrames[index].m_iYPitch,
            width,
            2*(pContext->m_seqLayerHeader.CODED_HEIGHT+1));
        width /=2;

        RangeDown(pContext->m_frmBuff.m_pFrames[index].m_pV,
            pContext->m_frmBuff.m_pFrames[index].m_iVPitch,
            pContext->m_frmBuff.m_pFrames[index].m_pV,
            pContext->m_frmBuff.m_pFrames[index].m_iVPitch,
            width,
            (pContext->m_seqLayerHeader.CODED_HEIGHT+1));

        RangeDown(pContext->m_frmBuff.m_pFrames[index].m_pU,
            pContext->m_frmBuff.m_pFrames[index].m_iUPitch,
            pContext->m_frmBuff.m_pFrames[index].m_pU,
            pContext->m_frmBuff.m_pFrames[index].m_iUPitch,
            width,
            (pContext->m_seqLayerHeader.CODED_HEIGHT+1));

        *pContext->m_frmBuff.m_pFrames[index].pRANGE_MAPY = -2;
    }
}

void RangeDown(uint8_t* pSrc, int32_t srcStep,
               uint8_t* pDst, int32_t dstStep,
               int32_t width, int32_t height)
{
    //pData - frame poiinter
    //width - number pixels in raw
    //height - number pixels in column
    int32_t i=0;
    int32_t j=0;
    int32_t temp;

    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            temp = pSrc[i*srcStep+j];

            temp = ((temp - 128)>>1 ) + 128;
            pDst[i*dstStep+j] = (uint8_t)temp;
        }
    }
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
