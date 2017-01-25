//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_common_blk_order_tbl.h"
#include "umc_vc1_huffman.h"

static const Ipp8u esbp_lut[] = {1,2,2,2,4,4,4,8};

#ifdef _OWN_FUNCTION

#define CLIP(x) (!(x&~255)?x:(x<0?0:255))

static const int BicubicFilterParams[4][4] =
{
    {-4,    53,     18,     -3}, // 1/4
    {-1,    9,      9,      -1}, // 1/2
    {-3,    18,     53,     -4}  // 3/4
};

static const char BicubicVertFilterShift[4][3] =
{
    {6, 4, 6},
    {5, 3, 5},
    {3, 1, 3},
    {5, 3, 5}
};

static const char BicubicHorizFilterShift[3][4] =
{
    {6, 7, 7, 7},
    {4, 7, 7, 7},
    {6, 7, 7, 7}
};

typedef IppStatus (*ippiBicubicInterpolate)     (const Ipp8u* pSrc,
                                                 Ipp32s srcStep,
                                                 Ipp8u *pDst,
                                                 Ipp32s dstStep,
                                                 Ipp32s dx,
                                                 Ipp32s dy,
                                                 Ipp32s roundControl);


static IppStatus ippiInterpolate16x16QPBicubicIC_VC1_8u_C1R (const Ipp8u* pSrc,
                                                             Ipp32s srcStep,
                                                             Ipp8u *pDst,
                                                             Ipp32s dstStep,
                                                             Ipp32s dx,
                                                             Ipp32s dy,
                                                             Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    unsigned choose_int = ( (1==(0==(dx))) |(((1==(0 == (dy))) << 1)));

    short TempBlock[(17 + 3) * 17];
    int i, j;
    int PBPL          = 17 + 3;
    int R             = roundControl;

    short         *pDst16 = TempBlock + 1;
    short         *pSource16 = TempBlock + 1;
    int Pixel;
    int Fy0, Fy1, Fy2, Fy3;
    int Fx0, Fx1, Fx2, Fx3;



    const int (*FP) = BicubicFilterParams[(dy) - 1];
    int Abs, Shift;
    Fy0          = FP[0];
    Fy1          = FP[1];
    Fy2          = FP[2];
    Fy3          = FP[3];
    FP = BicubicFilterParams[dx - 1];
    Fx0          = FP[0];
    Fx1          = FP[1];
    Fx2          = FP[2];
    Fx3          = FP[3];

    switch(choose_int)
    {
    case 0:

        Shift           = BicubicVertFilterShift[(dx)][(dy) - 1];
        Abs             = 1 << (Shift - 1);

        /* vertical filter */
        for(j = 0; j < 16; j++)
        {
            for(i = -1; i < 3; i++)
            {
                Pixel = ((pSrc[i - srcStep] * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]  * Fy2 +
                    pSrc[i + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);

                pDst16[i] = (short)Pixel;

            }
            for(;i < 16+2; i++)
            {
                Pixel = ((pSrc[i - srcStep]  * Fy0 +
                    pSrc[i]              * Fy1 +
                    pSrc[i  + srcStep]  * Fy2 +
                    pSrc[i  + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);
                pDst16[i] = (short)Pixel;
                pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                    pDst16[i-3] * Fx1 +
                    pDst16[i-2] * Fx2 +
                    pDst16[i-1] * Fx3 + 64 - R) >> 7);
            }
            pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                pDst16[i-3] * Fx1 +
                pDst16[i-2] * Fx2 +
                pDst16[i-1] * Fx3 + 64 - R) >> 7);
            pDst   += dstStep;
            pSrc   += srcStep;
            pDst16 += PBPL;
        }

        break;
    case 1:

        Shift           = BicubicVertFilterShift[0][(dy) - 1];
        Abs             = 1 << (Shift - 1);

        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {

                pDst[i] = CLIP((pSrc[i - srcStep]   * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]   * Fy2 +
                    pSrc[i + 2*srcStep] * Fy3 +
                    Abs - 1 + R) >> Shift);
            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;

    case 2:
        Shift           = BicubicHorizFilterShift[(dx) - 1][0];
        Abs             = 1 << (Shift - 1);

        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = CLIP((pSrc[i-1]   * Fx0 +
                    pSrc[i]     * Fx1 +
                    pSrc[i+1]   * Fx2 +
                    pSrc[i + 2] * Fx3 +
                    Abs - R) >> Shift);
            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;
    case 3:
        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = pSrc[i];
            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;
    default:
        break;
    }
    return ret;
}

static IppStatus ippiInterpolate16x8QPBicubicIC_VC1_8u_C1R (const Ipp8u* pSrc,
                                                            Ipp32s srcStep,
                                                            Ipp8u *pDst,
                                                            Ipp32s dstStep,
                                                            Ipp32s dx,
                                                            Ipp32s dy,
                                                            Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    unsigned choose_int = ( (1==(0==(dx))) |(((1==(0 == (dy))) << 1)));
    static unsigned inter_flag = 0;


    short TempBlock[(17 + 3) * 17];
    int i, j;
    int PBPL          = 17 + 3;
    int R             = roundControl;

    short         *pDst16 = TempBlock + 1;
    short         *pSource16 = TempBlock + 1;
    int Pixel;
    int Fy0, Fy1, Fy2, Fy3;
    int Fx0, Fx1, Fx2, Fx3;
    const int (*FP) = BicubicFilterParams[(dy) - 1];
    int Abs, Shift;
    Fy0          = FP[0];
    Fy1          = FP[1];
    Fy2          = FP[2];
    Fy3          = FP[3];

    FP = BicubicFilterParams[dx - 1];
    Fx0          =  FP[0];
    Fx1          =  FP[1];
    Fx2          =  FP[2];
    Fx3          =  FP[3];

    switch(choose_int)
    {
    case 0:

        Shift           = BicubicVertFilterShift[(dx)][(dy) - 1];
        Abs             = 1 << (Shift - 1);
        /* vertical filter */

        for(j = 0; j < 8; j++)
        {
            for(i = -1; i < 3; i++)
            {
                Pixel = ((pSrc[i - srcStep] * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]  * Fy2 +
                    pSrc[i + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);

                pDst16[i] = (short)Pixel;

            }
            for(;i < 16+2; i++)
            {
                Pixel = ((pSrc[i - srcStep]  * Fy0 +
                    pSrc[i]              * Fy1 +
                    pSrc[i  + srcStep]  * Fy2 +
                    pSrc[i  + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);
                pDst16[i] = (short)Pixel;
                pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                    pDst16[i-3] * Fx1 +
                    pDst16[i-2] * Fx2 +
                    pDst16[i-1] * Fx3 + 64 - R) >> 7);
            }
            pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                pDst16[i-3] * Fx1 +
                pDst16[i-2] * Fx2 +
                pDst16[i-1] * Fx3 + 64 - R) >> 7);
            pDst   += dstStep;
            pSrc   += srcStep;
            pDst16 += PBPL;
        }
        break;
    case 1:

        Shift           = BicubicVertFilterShift[0][(dy) - 1];
        Abs             = 1 << (Shift - 1);

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 16; i++)
            {

                pDst[i] = CLIP((pSrc[i - srcStep]   * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]   * Fy2 +
                    pSrc[i + 2*srcStep] * Fy3 +
                    Abs - 1 + R) >> Shift);
            }
            pSrc += srcStep;
            pDst += dstStep;
        }

        break;
    case 2:
        Shift           = BicubicHorizFilterShift[(dx) - 1][0];
        Abs             = 1 << (Shift - 1);

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = CLIP((pSrc[i-1]   * Fx0 +
                    pSrc[i]     * Fx1 +
                    pSrc[i+1]   * Fx2 +
                    pSrc[i + 2] * Fx3 +
                    Abs - R) >> Shift);
            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;

    case 3:

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = pSrc[i];

            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;
    default:
        break;
    }
    return ret;
}

static IppStatus ippiInterpolate8x8QPBicubicIC_VC1_8u_C1R (const Ipp8u* pSrc,
                                                           Ipp32s srcStep,
                                                           Ipp8u *pDst,
                                                           Ipp32s dstStep,
                                                           Ipp32s dx,
                                                           Ipp32s dy,
                                                           Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;

    unsigned choose_int = ( (1==(0==(dx))) |(((1==(0 == (dy))) << 1)));
    short TempBlock[(17 + 3) * 17];
    int i, j;
    int PBPL          = 17 + 3;
    int R             = roundControl;

    short         *pDst16 = TempBlock + 1;
    short         *pSource16 = TempBlock + 1;
    int Pixel;
    int Fy0, Fy1, Fy2, Fy3;
    int Fx0, Fx1, Fx2, Fx3;
    const int (*FP) = BicubicFilterParams[(dy) - 1];
    int Abs, Shift;
    Fy0          = FP[0];
    Fy1          = FP[1];
    Fy2          = FP[2];
    Fy3          = FP[3];

    FP = BicubicFilterParams[dx - 1];
    Fx0          = FP[0];
    Fx1          = FP[1];
    Fx2          = FP[2];
    Fx3          = FP[3];

    switch(choose_int)
    {
    case 0:

        Shift           = BicubicVertFilterShift[(dx)][(dy) - 1];
        Abs             = 1 << (Shift - 1);

        FP = BicubicFilterParams[dx - 1];
        Fx0          = (int)FP[0];
        Fx1          = (int)FP[1];
        Fx2          = (int)FP[2];
        Fx3          = (int)FP[3];

        /* vertical filter */

        for(j = 0; j < 8; j++)
        {
            for(i = -1; i < 3; i++)
            {
                Pixel = ((pSrc[i - srcStep] * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]  * Fy2 +
                    pSrc[i + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);

                pDst16[i] = (short)Pixel;

            }
            for(;i < 8+2; i++)
            {
                Pixel = ((pSrc[i - srcStep]  * Fy0 +
                    pSrc[i]              * Fy1 +
                    pSrc[i  + srcStep]  * Fy2 +
                    pSrc[i  + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);
                pDst16[i] = (short)Pixel;
                pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                    pDst16[i-3] * Fx1 +
                    pDst16[i-2] * Fx2 +
                    pDst16[i-1] * Fx3 + 64 - R) >> 7);
            }
            pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                pDst16[i-3] * Fx1 +
                pDst16[i-2] * Fx2 +
                pDst16[i-1] * Fx3 + 64 - R) >> 7);
            pDst   += dstStep;
            pSrc   += srcStep;
            pDst16 += PBPL;
        }
        break;
    case 1:

        Shift           = BicubicVertFilterShift[0][(dy) - 1];
        Abs             = 1 << (Shift - 1);

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 8; i++)
            {
                pDst[i] = CLIP((pSrc[i - srcStep]   * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]   * Fy2 +
                    pSrc[i + 2*srcStep] * Fy3 +
                    Abs - 1 + R) >> Shift);
            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;
    case 2:
        Shift           = BicubicHorizFilterShift[(dx) - 1][0];
        Abs             = 1 << (Shift - 1);

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 8; i++)
            {
                pDst[i] = CLIP((pSrc[i-1]   * Fx0 +
                    pSrc[i]     * Fx1 +
                    pSrc[i+1]   * Fx2 +
                    pSrc[i + 2] * Fx3 +
                    Abs - R) >> Shift);
            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;
    case 3:

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 8; i++)
            {
                pDst[i] = pSrc[i];
            }
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;
    default:
        break;
    }
    return ret;
}


static const ippiBicubicInterpolate ippiBicubicInterpolate_table[] = {
    (ippiBicubicInterpolate)(ippiInterpolate8x8QPBicubicIC_VC1_8u_C1R),
    (ippiBicubicInterpolate)(ippiInterpolate16x8QPBicubicIC_VC1_8u_C1R),
    (ippiBicubicInterpolate)(ippiInterpolate16x16QPBicubicIC_VC1_8u_C1R),
};


IppStatus _own_ippiInterpolateQPBicubicIC_VC1_8u_C1R   (_IppVCInterpolate_8u* inter_struct)
{
    IppStatus ret = ippStsNoErr;
    int pinterp = (inter_struct->roiSize.width >> 4) + (inter_struct->roiSize.height >> 4);
    ret= ippiBicubicInterpolate_table[pinterp](inter_struct->pSrc,
        inter_struct->srcStep,
        inter_struct->pDst,
        inter_struct->dstStep,
        inter_struct->dx,
        inter_struct->dy,
        inter_struct->roundControl);
    return ret;
}
#endif
void DecodeTransformInfo(VC1Context* pContext)
{
    Ipp32u i;
    if(pContext->m_seqLayerHeader.VSTRANSFORM)
    {
        if (pContext->m_picLayerHeader->TTMBF == 0 &&
            (pContext->m_pCurrMB->mbType != VC1_MB_INTRA) )
        {
            GetTTMB(pContext);
        }
        else
        {
            if (!pContext->m_pCurrMB->mbType == VC1_MB_INTRA)
            {
                for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
                {
                    if (!VC1_IS_BLKINTRA(pContext->m_pCurrMB->m_pBlocks[i].blkType))
                        pContext->m_pCurrMB->m_pBlocks[i].blkType = (Ipp8u)pContext->m_picLayerHeader->TTFRM;
                }
            }
            else
                for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
                    pContext->m_pCurrMB->m_pBlocks[i].blkType = VC1_BLK_INTRA;

        }
    }
}

VC1Status GetTTMB(VC1Context* pContext)
{
    int ret;
    VC1MB *pMB = pContext->m_pCurrMB;
    Ipp32s eSBP;
    Ipp8u Count, Limit, FirstBlock = 0;

    while ( (FirstBlock <  VC1_NUM_OF_BLOCKS)             &&
        ((0 == ((1 << (5-FirstBlock)) & pMB->m_cbpBits)) ||
        (pMB->m_pBlocks[FirstBlock].blkType == VC1_BLK_INTRA) )
        )
    {
        FirstBlock++;
    }

    if(FirstBlock == VC1_NUM_OF_BLOCKS)
    {
        return VC1_OK;
    }

    ret = DecodeHuffmanOne(  &pContext->m_bitstream.pBitstream,
        &pContext->m_bitstream.bitOffset,
        &eSBP,
        pContext->m_picLayerHeader->m_pCurrTTMBtbl);
    VM_ASSERT(ret == 0);

    Limit = VC1_NUM_OF_BLOCKS;

    if(eSBP < VC1_SBP_8X8_MB)
    {
        Limit = FirstBlock + 1;
    }
    else
    {
        eSBP -= VC1_SBP_8X8_MB;
    }

    pContext->m_pSingleMB->m_ubNumFirstCodedBlk = FirstBlock;

    pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = 0;

    switch(eSBP)
    {
    case VC1_SBP_8X8_BLK:
        pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = VC1_SBP_0;
        break;

    case VC1_SBP_8X4_BOTTOM_BLK:
        pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = VC1_SBP_1;
        break;

    case VC1_SBP_8X4_TOP_BLK:
        pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = VC1_SBP_0;
        break;

    case VC1_SBP_8X4_BOTH_BLK:
        pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = VC1_SBP_0|VC1_SBP_1;
        break;

    case VC1_SBP_4X8_RIGHT_BLK:
        pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = VC1_SBP_1;
        break;

    case VC1_SBP_4X8_LEFT_BLK:
        pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = VC1_SBP_0;
        break;

    case VC1_SBP_4X8_BOTH_BLK:
        pContext->m_pSingleMB->m_pSingleBlock[FirstBlock].numCoef = VC1_SBP_0|VC1_SBP_1;
        break;

    case VC1_SBP_4X4_BLK:
        //nothing to do
        break;

    default:
        return VC1_FAIL;
    }

    for(Count = FirstBlock; Count < Limit; Count++)
    {
        if(!(pMB->m_pBlocks[Count].blkType == VC1_BLK_INTRA))
        {
            VM_ASSERT(eSBP<8);
            pMB->m_pBlocks[Count].blkType = VC1_LUT_SET(eSBP,esbp_lut);
        }
    }

    for( ; Count < VC1_NUM_OF_BLOCKS; Count++)
    {
        if(!(pMB->m_pBlocks[Count].blkType == VC1_BLK_INTRA))
        {
            pMB->m_pBlocks[Count].blkType = VC1_BLK_INTER;
        }
    }

    return VC1_OK;
}
static void GetEdgeValue(Ipp32s Edge, Ipp32s offset, Ipp32s* Current)
{
    if((*Current) >= Edge) {
        (*Current) = Edge - 1 + offset;
    }
    if ((*Current) <= 0) {
        (*Current) = offset;
    }
}

IppStatus ippiPXInterpolatePXICBicubicBlock_VC1_8u_C1R(const IppVCInterpolateBlockIC_8u* interpolateInfo)
{

#if 0// ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T) || (_IPP_ARCH == _IPP_ARCH_LP64))
    Ipp64s i,j;
#else /* !((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T))) */
    Ipp32s i,j;
#endif /* ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T)) */

    Ipp32s   nfield;
    Ipp8u tmpBlk [19*19] = {0};
    IppiSize tmpBlkSize;
    Ipp32s   tmpBlkStep = 19;
    Ipp8u*   pTmpBlkData = tmpBlk;
    Ipp8u    shift;
    IppiPoint position;

    Ipp32u step = 0;

    Ipp32s TopFieldPOffsetTP = 0;
    Ipp32s BottomFieldPOffsetTP = 1;

    Ipp32s TopFieldPOffsetBP = 2;
    Ipp32s BottomFieldPOffsetBP = 1;

    Ipp32u SCoef;
    Ipp32s serv;

    // minimal distance from current point for interpolation.
    Ipp32s left = 1;
    Ipp32s right;
    Ipp32s top = 1; //<< shift;

    Ipp32s bottom;
    Ipp32s paddingLeft,paddingRight,paddingTop,paddingBottom;

    Ipp32s RefBlockStep;
    const Ipp8u* pRefBlock;
    const Ipp8u* pLUT[2];
    const Ipp8u* pTemp;
    IppVCInterpolate_8u inter_struct;
    Ipp32u isPredBottom;
    Ipp32s frameHeightShifted;
    Ipp32s OutOfBoundaryFlag = 0; //all block out of boundary flag

    shift = (interpolateInfo->fieldPrediction)? 1:0;
    SCoef = (interpolateInfo->fieldPrediction)? 1:0; // in case of interlace fields we should use half of frame height

    frameHeightShifted = interpolateInfo->sizeFrame.height >> SCoef;
    right = interpolateInfo->sizeFrame.width - (interpolateInfo->sizeBlock.width + 2);
    bottom = frameHeightShifted  - (interpolateInfo->sizeBlock.height + 2);
    RefBlockStep = interpolateInfo->srcStep << shift;
    isPredBottom = interpolateInfo->isPredBottom;

    tmpBlkSize.width = interpolateInfo->sizeBlock.width + 3;
    tmpBlkSize.height = interpolateInfo->sizeBlock.height + 3;

    position = interpolateInfo->pointRefBlockPos;

    paddingLeft    = (position.x < left) ?  left  - position.x: 0;
    paddingRight   = (position.x > right)?  position.x - right: 0;
    paddingTop     = (position.y < top)?    (top - position.y): 0;
    paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;

    pRefBlock = interpolateInfo->pSrc + (position.x + paddingLeft - 1) +
        (position.y + paddingTop - 1)* RefBlockStep;

    if(interpolateInfo->pLUTBottom || interpolateInfo->pLUTTop || paddingLeft || paddingRight || paddingTop || paddingBottom)
    {
        if (isPredBottom)
        {
            pLUT[0] = (interpolateInfo->fieldPrediction)?interpolateInfo->pLUTBottom:interpolateInfo->pLUTTop;
            pLUT[1] = interpolateInfo->pLUTBottom;
        }
        else
        {
            pLUT[1] = (interpolateInfo->fieldPrediction)?interpolateInfo->pLUTTop:interpolateInfo->pLUTBottom;
            pLUT[0] = interpolateInfo->pLUTTop;
        }

        if ((position.y + paddingTop - 1) % 2)
        {
            pTemp = pLUT[0];
            pLUT[0] = pLUT[1];
            pLUT[1] = pTemp;
        }

        if (interpolateInfo->oppositePadding == 0)
        {
            isPredBottom = 0;
        }

        tmpBlkSize.width  -= (paddingLeft + paddingRight);
        tmpBlkSize.height -= (paddingTop + paddingBottom);

        // all block out of plane
        if (tmpBlkSize.height <= 0)
        {
            tmpBlkSize.height = 0;
            if (paddingTop != 0)
            {
                // see paddingBottom
                paddingTop = (interpolateInfo->sizeBlock.height + 3);
                if ((interpolateInfo->oppositePadding)&(position.y % 2 != 0))
                {
                    TopFieldPOffsetTP = 1;
                    BottomFieldPOffsetTP = 0;
                    pTemp = pLUT[0];
                    pLUT[0] = pLUT[1];
                    pLUT[1] = pTemp;
                }
            }
            if (paddingBottom != 0)
            {
                paddingBottom = (interpolateInfo->sizeBlock.height + 3);
                if ((interpolateInfo->oppositePadding)&(position.y % 2 == 0))
                {
                    TopFieldPOffsetBP = 1;
                    BottomFieldPOffsetBP = 2;
                    pTemp = pLUT[0];
                    pLUT[0] = pLUT[1];
                    pLUT[1] = pTemp;
                }
            }
        }
        if (tmpBlkSize.width <= 0)
        {
                tmpBlkSize.width = 0;
            Ipp32s FieldPOffset[2] = {0,0};
            Ipp32s posy;

            if ((interpolateInfo->oppositePadding)&&(!interpolateInfo->fieldPrediction))
            {
                if (paddingTop)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 1;
                        FieldPOffset[1] = 0;
                    }
                    else
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = 1;
                    }
                }
                else if (paddingBottom)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = -1;
                    }
                    else
                    {
                        FieldPOffset[0] = -1;
                        FieldPOffset[1] = 0;
                    }
                }
            }
            // Compare with paddingRight case !!!!
            if (paddingLeft)
            {
                for (nfield = 0; nfield < 2; nfield ++)
                {
                    if(pLUT [nfield]) {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;

                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy > frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep + interpolateInfo->sizeBlock.width + 3] = 
                                pLUT [nfield][interpolateInfo->pSrc[RefBlockStep * posy- interpolateInfo->srcStep*OutOfBoundaryFlag]];
                        }
                    } else {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;

                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy > frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep + interpolateInfo->sizeBlock.width + 3] = 
                                interpolateInfo->pSrc[RefBlockStep * posy- interpolateInfo->srcStep*OutOfBoundaryFlag];
                        }
                    }
                }
                paddingLeft = (paddingLeft != 0)?(interpolateInfo->sizeBlock.width + 3):paddingLeft;
            }

            if (paddingRight)
            {
                for (nfield = 0; nfield < 2; nfield ++)
                {
                    if(pLUT [nfield]) {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;
                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy > frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep] = 
                                pLUT [nfield][interpolateInfo->pSrc[RefBlockStep*posy + interpolateInfo->sizeFrame.width - 1 - interpolateInfo->srcStep*OutOfBoundaryFlag]];
                        }
                    } else {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;
                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy > frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep] = 
                                interpolateInfo->pSrc[RefBlockStep*posy + interpolateInfo->sizeFrame.width - 1 - interpolateInfo->srcStep*OutOfBoundaryFlag];
                        }
                    }
                }
                paddingRight = (paddingRight != 0)?(interpolateInfo->sizeBlock.width + 2):paddingRight;
            }
        }

        // prepare dst
        pTmpBlkData += paddingTop * tmpBlkStep + paddingLeft;
        {
            // copy block
            for (nfield = 0; nfield < 2; nfield ++)
            {
                if(pLUT [nfield]) {
                    for (i = nfield; i < tmpBlkSize.height; i+=2) {
                        serv = i*tmpBlkStep;
                        for (j = 0; j < tmpBlkSize.width; j++) {
                            pTmpBlkData[serv+j] = pLUT[nfield][pRefBlock[i*RefBlockStep+j]];
                        }
                    }
                } else {
                    for (i = nfield; i < tmpBlkSize.height; i+=2) {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[i*tmpBlkStep]), &(pRefBlock[i*RefBlockStep]), tmpBlkSize.width);
                    }                
                }
            }
        }

        if ((!interpolateInfo->oppositePadding)||(interpolateInfo->fieldPrediction))
        {
            if (!interpolateInfo->oppositePadding)
            {
                // interlace padding. Field prediction
                // top
                for (i = -paddingTop; i < 0; i ++)
                {
                    serv = position.x + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(pLUT[0]) {
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[i*tmpBlkStep+j] = pLUT[0][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                // bottom
                for (i = 0; i < paddingBottom; i ++)
                {
                    serv = position.x + (frameHeightShifted - 1)*RefBlockStep + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(pLUT[1]) {                    
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = pLUT[1][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
            else
            {
                // progressive padding. Field prediction
                // top
                for (i = -paddingTop; i < 0; i ++)
                {
                    serv = position.x + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(interpolateInfo->pLUTTop) {
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[i*tmpBlkStep+j] = interpolateInfo->pLUTTop[interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                // bottom
                for (i = 0; i < paddingBottom; i ++)
                {
                    serv = position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(interpolateInfo->pLUTBottom){
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = interpolateInfo->pLUTBottom[interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
        }
        else
        {
            // Field  padding. Progressive prediction
            if (paddingTop)
            {
                // top
                for (i = 2; i <= paddingTop; i +=2)
                {
                    serv = position.x + paddingLeft + TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep;
                    if(pLUT[0]) {
                        for (j = 0; j < tmpBlkSize.width; j++) {
                            pTmpBlkData[-i*tmpBlkStep+j] = pLUT[0][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[-i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                for (i = 1; i <= paddingTop; i +=2)
                {
                    serv = position.x + paddingLeft + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep;
                    if(pLUT[1]){                    
                        for (j = 0; j < tmpBlkSize.width; j++) {
                            pTmpBlkData[-i*tmpBlkStep+j] = pLUT[1][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[-i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
            if (paddingBottom)
            {
                if (paddingBottom % 2)
                {
                    pTemp = pLUT[0];
                    pLUT[0] = pLUT[1];
                    pLUT[1] = pTemp;
                }
                // bottom
                for (i = 0; i < paddingBottom; i +=2)
                {
                    serv = position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft - left - isPredBottom*RefBlockStep;
                    if(pLUT[1]) {
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = pLUT[1][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                for (i = 1; i < paddingBottom; i +=2)
                {
                    serv = position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft - left - isPredBottom*RefBlockStep;
                    if(pLUT[0]){
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = pLUT[0][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
        }

        for (i = 0; i < paddingLeft; i ++)
        {
            for (j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
            {
                tmpBlk[i + j*tmpBlkStep] = tmpBlk[paddingLeft + j*tmpBlkStep];
            }
        }
        for (i = 1; i <=paddingRight; i ++)
        {
            serv = interpolateInfo->sizeBlock.width + 3 + step;
            for (j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
            {
                tmpBlk[serv - i + j*tmpBlkStep] =
                    tmpBlk[serv - paddingRight - 1 + j*tmpBlkStep];
            }
        }    
        inter_struct.srcStep = 19;
        inter_struct.pSrc = tmpBlk + 1 + inter_struct.srcStep;
    } else {
        inter_struct.srcStep = RefBlockStep;
        inter_struct.pSrc = pRefBlock + 1 + inter_struct.srcStep;
    }
    inter_struct.pDst = interpolateInfo->pDst;
    inter_struct.dstStep = interpolateInfo->dstStep;
    inter_struct.dx = interpolateInfo->pointVectorQuarterPix.x;
    inter_struct.dy = interpolateInfo->pointVectorQuarterPix.y;
    inter_struct.roundControl = interpolateInfo->roundControl;
    inter_struct.roiSize.width = interpolateInfo->sizeBlock.width;
    inter_struct.roiSize.height = interpolateInfo->sizeBlock.height;

    return ippiInterpolateQPBicubic_VC1_8u_C1R(&inter_struct);
}



IppStatus ippiPXInterpolatePXICBilinearBlock_VC1_8u_C1R(const IppVCInterpolateBlockIC_8u* interpolateInfo)

{

#if 0// ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T) || (_IPP_ARCH == _IPP_ARCH_LP64))
    Ipp64s i,j;
#else /* !((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T))) */
    Ipp32s i,j;
#endif /* ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T)) */

    Ipp32s   nfield;
    Ipp8u tmpBlk [19*19] = {0};
    IppiSize tmpBlkSize;
    Ipp32s   tmpBlkStep = 19;
    Ipp8u*   pTmpBlkData = tmpBlk;
    Ipp8u    shift;
    IppiPoint position;

    Ipp32u step = 0;

    Ipp32s TopFieldPOffsetTP = 0;
    Ipp32s BottomFieldPOffsetTP = 1;

    Ipp32s TopFieldPOffsetBP = 2;
    Ipp32s BottomFieldPOffsetBP = 1;

    Ipp32u SCoef;
    Ipp32s serv;

    // minimal distance from current point for interpolation.
    Ipp32s left = 1;
    Ipp32s right;
    Ipp32s top = 1; //<< shift;

    Ipp32s bottom;
    Ipp32s paddingLeft,paddingRight,paddingTop,paddingBottom;

    Ipp32s RefBlockStep;
    const Ipp8u* pRefBlock;
    const Ipp8u* pLUT[2];
    const Ipp8u* pTemp;
    IppVCInterpolate_8u inter_struct;
    Ipp32u isPredBottom;
    Ipp32s frameHeightShifted;
    Ipp32s OutOfBoundaryFlag = 0; //all block out of boundary flag

    shift = (interpolateInfo->fieldPrediction)? 1:0;
    SCoef = (interpolateInfo->fieldPrediction)? 1:0; // in case of interlace fields we should use half of frame height

    frameHeightShifted = interpolateInfo->sizeFrame.height >> SCoef;
    right = interpolateInfo->sizeFrame.width - (interpolateInfo->sizeBlock.width + 2);
    bottom = frameHeightShifted  - (interpolateInfo->sizeBlock.height + 2);
    RefBlockStep = interpolateInfo->srcStep << shift;
    isPredBottom = interpolateInfo->isPredBottom;

    tmpBlkSize.width = interpolateInfo->sizeBlock.width + 3;
    tmpBlkSize.height = interpolateInfo->sizeBlock.height + 3;

    position = interpolateInfo->pointRefBlockPos;

    paddingLeft    = (position.x < left) ?  left  - position.x: 0;
    paddingRight   = (position.x > right)?  position.x - right: 0;
    paddingTop     = (position.y < top)?    (top - position.y): 0;
    paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;

    pRefBlock = interpolateInfo->pSrc + (position.x + paddingLeft - 1) +
        (position.y + paddingTop  - 1)* RefBlockStep;

    if(interpolateInfo->pLUTBottom || interpolateInfo->pLUTTop || paddingLeft || paddingRight || paddingTop || paddingBottom)
    {
        if (isPredBottom)
        {
            pLUT[0] = (interpolateInfo->fieldPrediction)?interpolateInfo->pLUTBottom:interpolateInfo->pLUTTop;
            pLUT[1] = interpolateInfo->pLUTBottom;
        }
        else
        {
            pLUT[1] = (interpolateInfo->fieldPrediction)?interpolateInfo->pLUTTop:interpolateInfo->pLUTBottom;
            pLUT[0] = interpolateInfo->pLUTTop;
        }

        if ((position.y + paddingTop - 1) % 2)
        {
            pTemp = pLUT[0];
            pLUT[0] = pLUT[1];
            pLUT[1] = pTemp;
        }

        if (interpolateInfo->oppositePadding == 0)
        {
            isPredBottom = 0;
        }

        tmpBlkSize.width  -= (paddingLeft + paddingRight);
        tmpBlkSize.height -= (paddingTop + paddingBottom);

        // all block out of plane
        if (tmpBlkSize.height <= 0)
        {
            tmpBlkSize.height = 0;
            if (paddingTop != 0)
            {
                // see paddingBottom
                paddingTop = (interpolateInfo->sizeBlock.height + 3);
                if ((interpolateInfo->oppositePadding)&(position.y % 2 != 0))
                {
                    TopFieldPOffsetTP = 1;
                    BottomFieldPOffsetTP = 0;
                    pTemp = pLUT[0];
                    pLUT[0] = pLUT[1];
                    pLUT[1] = pTemp;
                }
            }
            if (paddingBottom != 0)
            {
                paddingBottom = (interpolateInfo->sizeBlock.height + 3);
                if ((interpolateInfo->oppositePadding)&(position.y % 2 == 0))
                {
                    TopFieldPOffsetBP = 1;
                    BottomFieldPOffsetBP = 2;
                    pTemp = pLUT[0];
                    pLUT[0] = pLUT[1];
                    pLUT[1] = pTemp;
                }
            }
        }
        if (tmpBlkSize.width <= 0)
        {
            Ipp32s FieldPOffset[2] = {0,0};
            Ipp32s posy;
            tmpBlkSize.width = 0;

            if ((interpolateInfo->oppositePadding)&&(!interpolateInfo->fieldPrediction))
            {
                if (paddingTop)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 1;
                        FieldPOffset[1] = 0;
                    }
                    else
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = 1;
                    }
                }
                else if (paddingBottom)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = -1;
                    }
                    else
                    {
                        FieldPOffset[0] = -1;
                        FieldPOffset[1] = 0;
                    }
                }
            }
            // Compare with paddingRight case !!!!
            if (paddingLeft)
            {
                 for (nfield = 0; nfield < 2; nfield ++)
                {
                    if(pLUT [nfield]) {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;

                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy >= frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep + interpolateInfo->sizeBlock.width + 3] = 
                                pLUT [nfield][interpolateInfo->pSrc[RefBlockStep * posy- interpolateInfo->srcStep*OutOfBoundaryFlag]];
                        }
                    } else {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;
                            
                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy >= frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep + interpolateInfo->sizeBlock.width + 3] = 
                                interpolateInfo->pSrc[RefBlockStep * posy- interpolateInfo->srcStep*OutOfBoundaryFlag];
                        }
                    }
                }
                paddingLeft = (paddingLeft != 0)?(interpolateInfo->sizeBlock.width + 3):paddingLeft;
            }

            if (paddingRight)
            {
                
                for (nfield = 0; nfield < 2; nfield ++)
                {
                    if(pLUT [nfield]) {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;
                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy >= frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep] = 
                                pLUT [nfield][interpolateInfo->pSrc[RefBlockStep*posy + interpolateInfo->sizeFrame.width - 1- interpolateInfo->srcStep*OutOfBoundaryFlag]];
                        }
                    } else {
                        for (i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                        {
                            posy = position.y + i - 1;
                            if((posy < 0) && (paddingTop != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&interpolateInfo->isPredBottom)
                                OutOfBoundaryFlag = 1; 
                            else
                                OutOfBoundaryFlag = 0;

                            if((posy >= frameHeightShifted) && (paddingBottom != 0) && interpolateInfo->oppositePadding
                                && interpolateInfo->fieldPrediction&&(!interpolateInfo->isPredBottom))
                                OutOfBoundaryFlag = -1; 

                            GetEdgeValue(frameHeightShifted,FieldPOffset[nfield],&posy);

                            tmpBlk[i*tmpBlkStep] = 
                                interpolateInfo->pSrc[RefBlockStep*posy + interpolateInfo->sizeFrame.width - 1- interpolateInfo->srcStep*OutOfBoundaryFlag];
                        }
                    }
                }
                paddingRight = (paddingRight != 0)?(interpolateInfo->sizeBlock.width + 2):paddingRight;
            }
        }

        // prepare dst
        pTmpBlkData += paddingTop * tmpBlkStep + paddingLeft;
        {
            // copy block
            for (nfield = 0; nfield < 2; nfield ++)
            {
                if(pLUT [nfield]) {
                    for (i = nfield; i < tmpBlkSize.height; i+=2) {
                        serv = i*tmpBlkStep;
                        for (j = 0; j < tmpBlkSize.width; j++) {
                            pTmpBlkData[serv+j] = pLUT[nfield][pRefBlock[i*RefBlockStep+j]];
                        }
                    }
                } else {
                    for (i = nfield; i < tmpBlkSize.height; i+=2) {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[i*tmpBlkStep]), &(pRefBlock[i*RefBlockStep]), tmpBlkSize.width);
                    }                
                }
            }
        }

        if ((!interpolateInfo->oppositePadding)||(interpolateInfo->fieldPrediction))
        {
            if (!interpolateInfo->oppositePadding)
            {
                // interlace padding. Field prediction
                // top
                for (i = -paddingTop; i < 0; i ++)
                {
                    serv = position.x + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(pLUT[0]) {
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[i*tmpBlkStep+j] = pLUT[0][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                // bottom
                for (i = 0; i < paddingBottom; i ++)
                {
                    serv = position.x + (frameHeightShifted - 1)*RefBlockStep + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(pLUT[1]) {                    
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = pLUT[1][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
            else
            {
                // progressive padding. Field prediction
                // top
                for (i = -paddingTop; i < 0; i ++)
                {
                    serv = position.x + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(interpolateInfo->pLUTTop) {
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[i*tmpBlkStep+j] = interpolateInfo->pLUTTop[interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                // bottom
                for (i = 0; i < paddingBottom; i ++)
                {
                    serv = position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft - left - isPredBottom*interpolateInfo->srcStep;
                    if(interpolateInfo->pLUTBottom){
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = interpolateInfo->pLUTBottom[interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
        }
        else
        {
            // Field  padding. Progressive prediction
            if (paddingTop)
            {
                // top
                for (i = 2; i <= paddingTop; i +=2)
                {
                    serv = position.x + paddingLeft + TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep;
                    if(pLUT[0]) {
                        for (j = 0; j < tmpBlkSize.width; j++) {
                            pTmpBlkData[-i*tmpBlkStep+j] = pLUT[0][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[-i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                for (i = 1; i <= paddingTop; i +=2)
                {
                    serv = position.x + paddingLeft + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep;
                    if(pLUT[1]){                    
                        for (j = 0; j < tmpBlkSize.width; j++) {
                            pTmpBlkData[-i*tmpBlkStep+j] = pLUT[1][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[-i*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
            if (paddingBottom)
            {
                if (paddingBottom % 2)
                {
                    pTemp = pLUT[0];
                    pLUT[0] = pLUT[1];
                    pLUT[1] = pTemp;
                }
                // bottom
                for (i = 0; i < paddingBottom; i +=2)
                {
                    serv = position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft - left - isPredBottom*RefBlockStep;
                    if(pLUT[1]) {
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = pLUT[1][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
                for (i = 1; i < paddingBottom; i +=2)
                {
                    serv = position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft - left - isPredBottom*RefBlockStep;
                    if(pLUT[0]){
                        for (j = 0; j <tmpBlkSize.width; j++) {
                            pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep+j] = pLUT[0][interpolateInfo->pSrc[serv+j]];
                        }
                    } else {
                        MFX_INTERNAL_CPY(&(pTmpBlkData[(tmpBlkSize.height+i)*tmpBlkStep]), &(interpolateInfo->pSrc[serv]), tmpBlkSize.width);
                    }
                }
            }
        }

        for (i = 0; i < paddingLeft; i ++)
        {
            for (j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
            {
                tmpBlk[i + j*tmpBlkStep] = tmpBlk[paddingLeft + j*tmpBlkStep];
            }
        }
        for (i = 1; i <=paddingRight; i ++)
        {
            serv = interpolateInfo->sizeBlock.width + 3 + step;
            for (j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
            {
                tmpBlk[serv - i + j*tmpBlkStep] =
                    tmpBlk[serv - paddingRight - 1 + j*tmpBlkStep];
            }
        }    
        inter_struct.srcStep = 19;
        inter_struct.pSrc = tmpBlk + 1 + inter_struct.srcStep;
    }
    else {
        inter_struct.srcStep = RefBlockStep;
        inter_struct.pSrc = pRefBlock + 1 + inter_struct.srcStep;
    }

    inter_struct.pDst = interpolateInfo->pDst;
    inter_struct.dstStep = interpolateInfo->dstStep;
    inter_struct.dx = interpolateInfo->pointVectorQuarterPix.x;
    inter_struct.dy = interpolateInfo->pointVectorQuarterPix.y;
    inter_struct.roundControl = interpolateInfo->roundControl;
    inter_struct.roiSize.width = interpolateInfo->sizeBlock.width;
    inter_struct.roiSize.height = interpolateInfo->sizeBlock.height;

    return ippiInterpolateQPBilinear_VC1_8u_C1R(&inter_struct);

}
void CalculateIntraFlag(VC1Context* pContext)
{
    Ipp8u i;
    pContext->m_pCurrMB->IntraFlag=0;

    for (i = 0; i < VC1_NUM_OF_BLOCKS; i++)
    {
        if(pContext->m_pCurrMB->m_pBlocks[i].blkType & VC1_BLK_INTRA)
        {
            pContext->m_pCurrMB->IntraFlag=(1<<i)|pContext->m_pCurrMB->IntraFlag;
        }
    }
}
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
