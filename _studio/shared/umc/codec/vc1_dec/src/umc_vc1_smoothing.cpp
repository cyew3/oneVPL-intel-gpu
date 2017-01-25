//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include <memory.h>

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_defs.h"
#include "assert.h"

#ifdef _OWN_FUNCTION
IppStatus _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R (Ipp16s* pSrcLeft, Ipp32s srcLeftStep,
                                                        Ipp16s* pSrcRight, Ipp32s srcRightStep,
                                                        Ipp8u* pDst, Ipp32s dstStep,
                                                        Ipp32u fieldNeighbourFlag,
                                                        Ipp32u edgeDisableFlag)
{
    Ipp32s i;
    Ipp8s r0, r1;
    Ipp16s *pSrcL = pSrcLeft;
    Ipp16s *pSrcR = pSrcRight;
    Ipp8u  *dst = pDst;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

    if(!edgeDisableFlag) return ippStsNoErr;

    srcLeftStep >>= 1;
    srcRightStep>>= 1;

    switch (fieldNeighbourFlag)
    {
    case 0:
        {
            //both blocks are not field
            r0 = 4; r1 = 3;
            if(edgeDisableFlag & IPPVC_EDGE_HALF_1)
                for(i=0; i < VC1_PIXEL_IN_BLOCK; i++)
                {
                    x0 = *(pSrcL);
                    x1 = *(pSrcL + 1);
                    x2 = *(pSrcR );
                    x3 = *(pSrcR + 1);

                    f0 = x3 - x0;
                    f1 = x2 + x3 - x0 - x1;

                    *(pSrcL)     = x0 + ((f0 + r0)>>3);
                    *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                    *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                    *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                    *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                    *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                    *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                    *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                    r0 = 7 - r0;
                    r1 = 7 - r1;
                    pSrcL += srcLeftStep;
                    pSrcR += srcRightStep;
                    dst+=dstStep;
                }
                pSrcL = pSrcLeft + VC1_PIXEL_IN_BLOCK*srcLeftStep;
                pSrcR = pSrcRight +VC1_PIXEL_IN_BLOCK*srcRightStep;
                dst = pDst + dstStep*VC1_PIXEL_IN_BLOCK;
                if(edgeDisableFlag & IPPVC_EDGE_HALF_2)
                    for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
                    {
                        x0 = *(pSrcL);
                        x1 = *(pSrcL + 1);
                        x2 = *(pSrcR );
                        x3 = *(pSrcR + 1);

                        f0 = x3 - x0;
                        f1 = x2 + x3 - x0 - x1;

                        *(pSrcL)     = x0 + ((f0 + r0)>>3);
                        *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                        *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                        *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                        *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                        *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                        *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                        *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                        r0 = 7 - r0;
                        r1 = 7 - r1;
                        pSrcL += srcLeftStep;
                        pSrcR += srcRightStep;
                        dst+=dstStep;
                    }
        }
        break;
    case 1:
        {
            //right - field,  left - not
            r0 = 4;
            r1 = 3;

            for(i=0; i < VC1_PIXEL_IN_BLOCK; i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcR += srcRightStep;
                pSrcL += 2*srcLeftStep;
                dst+=2*dstStep;
            }

            r0 = 3;
            r1 = 4;
            pSrcL = pSrcLeft + srcLeftStep;
            pSrcR = pSrcRight +VC1_PIXEL_IN_BLOCK*srcRightStep;
            dst = pDst + dstStep;

            for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcL += 2*srcLeftStep;
                pSrcR += srcRightStep;
                dst+=2*dstStep;
            }
        }
        break;
    case 2:
        {
            //left - field, right  - not
            r0 = 4;
            r1 = 3;

            for(i=0;i<4;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcR += srcRightStep;
                pSrcL = pSrcL + VC1_PIXEL_IN_BLOCK*srcLeftStep;

                dst+=dstStep;

                r0 = 7 - r0;
                r1 = 7 - r1;

                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcL -= 7*srcLeftStep;
                pSrcR += srcRightStep;

                r0 = 7 - r0;
                r1 = 7 - r1;
                dst+=dstStep;
            }
            pSrcL = pSrcLeft + 4*srcLeftStep;

            for(i=0;i<4;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcR += srcRightStep;
                pSrcL = pSrcL + VC1_PIXEL_IN_BLOCK*srcLeftStep;

                dst+=dstStep;

                r0 = 7 - r0;
                r1 = 7 - r1;

                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcL -= 7*srcLeftStep;
                pSrcR += srcRightStep;

                r0 = 7 - r0;
                r1 = 7 - r1;
                dst+=dstStep;
            }
        }
        break;
    case 3:
        {
            //both blocks are field
            r0 = 4; r1 = 3;
            for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcL += srcLeftStep;
                pSrcR += srcRightStep;
                dst+=2*dstStep;
            }

            r0 = 3; r1 = 4;
            pSrcL = pSrcLeft + VC1_PIXEL_IN_BLOCK*srcLeftStep;
            pSrcR = pSrcRight +VC1_PIXEL_IN_BLOCK*srcRightStep;
            dst = pDst + dstStep;
            for(i=0; i < VC1_PIXEL_IN_BLOCK; i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcL += srcLeftStep;
                pSrcR += srcRightStep;
                dst+=2*dstStep;
            }
        }
        break;
    }

    return ippStsNoErr;
}

IppStatus _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R (Ipp16s* pSrcUpper, Ipp32s srcUpperStep,
                                                        Ipp16s* pSrcBottom, Ipp32s srcBottomStep,
                                                        Ipp8u* pDst, Ipp32s dstStep,
                                                        Ipp32u edgeDisableFlag)
{
    Ipp32s i;
    Ipp8s r0=4, r1=3;
    Ipp16s *pSrcU = pSrcUpper;
    Ipp16s *pSrcB = pSrcBottom;
    Ipp8u  *dst = pDst;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;
    srcUpperStep/=2;
    srcBottomStep/=2;

    if(edgeDisableFlag & IPPVC_EDGE_HALF_1)
        for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
        {
            x0 = *(pSrcU);
            x1 = *(pSrcU+srcUpperStep);
            x2 = *(pSrcB);
            x3 = *(pSrcB+srcBottomStep);

            f0 = x3 - x0;
            f1 = x2 + x3 - x0 - x1;

            //*(pSrcU    ) = ((7 * x0 + 0 * x1 + 0 * x2 + 1 * x3) + r0)>>3;
            //*(pSrcU + srcUpperStep) = ((-1* x0 + 7 * x1 + 1 * x2 + 1 * x3) + r1)>>3;
            //*(pSrcB    ) = ((1 * x0 + 1 * x1 + 7 * x2 + -1* x3) + r0)>>3;
            //*(pSrcB + srcBottomStep) = ((1 * x0 + 0 * x1 + 0 * x2 + 7 * x3) + r1)>>3;

            *(dst-2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
            *(dst-1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
            *(dst+0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
            *(dst+1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

            dst++;
            pSrcU++;
            pSrcB++;

            r0 = 7 - r0;
            r1 = 7 - r1;
        }

        pSrcU = pSrcUpper + VC1_PIXEL_IN_BLOCK;
        pSrcB = pSrcBottom + VC1_PIXEL_IN_BLOCK;
        dst = pDst + 8;

        if(edgeDisableFlag & IPPVC_EDGE_HALF_2)
            for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
            {
                x0 = *(pSrcU);
                x1 = *(pSrcU+srcUpperStep);
                x2 = *(pSrcB);
                x3 = *(pSrcB+srcBottomStep);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                //*(pSrcU    ) = ((7 * x0 + 0 * x1 + 0 * x2 + 1 * x3) + r0)>>3;
                //*(pSrcU + srcUpperStep) = ((-1* x0 + 7 * x1 + 1 * x2 + 1 * x3) + r1)>>3;
                //*(pSrcB    ) = ((1 * x0 + 1 * x1 + 7 * x2 + -1* x3) + r0)>>3;
                //*(pSrcB + srcBottomStep) = ((1 * x0 + 0 * x1 + 0 * x2 + 7 * x3) + r1)>>3;

                *(dst-2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
                *(dst-1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
                *(dst+0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
                *(dst+1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

                dst++;
                pSrcU++;
                pSrcB++;

                r0 = 7 - r0;
                r1 = 7 - r1;
            }

            return ippStsNoErr;
}


IppStatus _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R (Ipp16s* pSrcUpper, Ipp32s srcUpperStep,
                                                          Ipp16s* pSrcBottom, Ipp32s srcBottomStep,
                                                          Ipp8u* pDst, Ipp32s dstStep)
{
    Ipp32s i;
    Ipp8s r0=4, r1=3;
    Ipp16s *pSrcU = pSrcUpper;
    Ipp16s *pSrcB = pSrcBottom;
    Ipp8u  *dst = pDst;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

    srcUpperStep/=2;
    srcBottomStep/=2;

    for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
    {
        x0 = *(pSrcU);
        x1 = *(pSrcU+srcUpperStep);
        x2 = *(pSrcB);
        x3 = *(pSrcB+srcBottomStep);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        //*(pSrcU    )            = x0 + (f0 + r0)>>3;
        //*(pSrcU + srcUpperStep) = x1 + (f1 + r1)>>3;
        //*(pSrcB    )            = x2 + (-f1 + r0)>>3;
        //*(pSrcB + srcBottomStep)= x3 + (-f0 + r1)>>3;

        *(dst-2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
        *(dst-1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
        *(dst+0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
        *(dst+1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

        dst++;
        pSrcU++;
        pSrcB++;

        r0 = 7 - r0;
        r1 = 7 - r1;
    }
    return ippStsNoErr;
}

IppStatus _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R (Ipp16s* pSrcLeft, Ipp32s srcLeftStep,
                                                          Ipp16s* pSrcRight, Ipp32s srcRightStep,
                                                          Ipp8u* pDst, Ipp32s dstStep)
{
    Ipp32s i;
    Ipp8s r0, r1;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

    r0 = 4; r1 = 3;
    srcLeftStep/=2;
    srcRightStep/=2;


    for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
    {
        x0 = *(pSrcLeft);
        x1 = *(pSrcLeft + 1);
        x2 = *(pSrcRight );
        x3 = *(pSrcRight + 1);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        *(pSrcLeft)     = x0 + ((f0 + r0)>>3);
        *(pSrcLeft + 1) = x1 + ((f1 + r1)>>3);
        *(pSrcRight )   = x2 + ((-f1 + r0)>>3);
        *(pSrcRight+1)  = x3 + ((-f0 + r1)>>3);

        *(pDst-2)=(Ipp8u)VC1_CLIP(*(pSrcLeft));
        *(pDst-1)=(Ipp8u)VC1_CLIP(*(pSrcLeft+1));
        *(pDst+0)=(Ipp8u)VC1_CLIP(*(pSrcRight));
        *(pDst+1)=(Ipp8u)VC1_CLIP(*(pSrcRight+1));

        r0 = 7 - r0;
        r1 = 7 - r1;
        pSrcLeft += srcLeftStep;
        pSrcRight += srcRightStep;
        pDst+=dstStep;
    }
    return ippStsNoErr;
}

#endif

void Smoothing_I(VC1Context* pContext, Ipp32s Height)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    if(pCurrMB->Overlap == 0)
        return;

    Ipp32s notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);
    Ipp32s Width = pContext->m_seqLayerHeader.widthMB;
    Ipp32s MaxWidth = pContext->m_seqLayerHeader.MaxWidthMB;
    Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

    Ipp16s* CurrBlock = pContext->m_pBlock;
    Ipp8u* YPlane = pCurrMB->currYPlane;
    Ipp8u* UPlane = pCurrMB->currUPlane;
    Ipp8u* VPlane = pCurrMB->currVPlane;
    Ipp32s YPitch = pCurrMB->currYPitch;
    Ipp32s UPitch = pCurrMB->currUPitch;
    Ipp32s VPitch = pCurrMB->currVPitch;

    Ipp32s i, j;
    //Ipp16s* UpYrow;
    //Ipp16s* UpUrow;
    //Ipp16s* UpVrow;

    for (j = 0; j< Height; j++)
    {
        notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);

        if(notTop)
        {
            YPlane = pCurrMB->currYPlane;
            UPlane = pCurrMB->currUPlane;
            VPlane = pCurrMB->currVPlane;

            //first MB in row
            //internal vertical smoothing
            _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6, VC1_PIXEL_IN_LUMA*2,
                                                            CurrBlock + 8, VC1_PIXEL_IN_LUMA*2,
                                                            YPlane + 8,    YPitch,
                                                            0,             EdgeDisabledFlag);
            for (i = 1; i < Width; i++)
            {
                CurrBlock  += 8*8*6;
                pCurrMB++;
                YPlane = pCurrMB->currYPlane;
                UPlane = pCurrMB->currUPlane;
                VPlane = pCurrMB->currVPlane;

                //LUMA
                //left boundary vertical smoothing
                _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6+14, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock,            VC1_PIXEL_IN_LUMA*2,
                                                                YPlane,               YPitch,
                                                                0,                    EdgeDisabledFlag);
                //internal vertical smoothing
                _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,        VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8,        VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8,           YPitch,
                                                                0,                    EdgeDisabledFlag);


                //left MB Upper horizontal edge
                _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA,              VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock - 8*8*6,   VC1_PIXEL_IN_LUMA*2,
                                                                YPlane - 16,         YPitch,
                                                                EdgeDisabledFlag);

                //left MB internal horizontal edge
                _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock - 8*8*4,      VC1_PIXEL_IN_LUMA*2,
                                                                YPlane - 16 + 8*YPitch, YPitch,
                                                                EdgeDisabledFlag);

                //CHROMA
                //U vertical smoothing
                _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                UPlane,                UPitch);

                //V vertical smoothing
                _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                CurrBlock + 8*8*5,   VC1_PIXEL_IN_CHROMA*2,
                                                                VPlane,              VPitch);

                //U top horizontal smoothing
                _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,           VC1_PIXEL_IN_CHROMA*2,
                                                                CurrBlock - 2*64, VC1_PIXEL_IN_CHROMA*2,
                                                                UPlane - 8,       UPitch);

                //V top horizontal smoothing
                _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,         VC1_PIXEL_IN_CHROMA*2,
                                                                CurrBlock - 64, VC1_PIXEL_IN_CHROMA*2,
                                                                VPlane - 8,     VPitch);
            }

            //MB Upper horizontal edge
            _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA,     VC1_PIXEL_IN_LUMA*2,
                                                            CurrBlock,  VC1_PIXEL_IN_LUMA*2,
                                                            YPlane,     YPitch,
                                                            EdgeDisabledFlag);

            //MB internal horizontal edge
            _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32, VC1_PIXEL_IN_LUMA*2,
                                                            CurrBlock + 8*8*2,      VC1_PIXEL_IN_LUMA*2,
                                                            YPlane + 8*YPitch, YPitch,
                                                            EdgeDisabledFlag);

            //U top horizontal smoothing
            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,            VC1_PIXEL_IN_CHROMA*2,
                                                            CurrBlock + 4*64,  VC1_PIXEL_IN_CHROMA*2,
                                                            UPlane,            UPitch);

            //V top horizontal smoothing
            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,           VC1_PIXEL_IN_CHROMA*2,
                                                            CurrBlock + 64*5, VC1_PIXEL_IN_CHROMA*2,
                                                            VPlane,           VPitch);

            CurrBlock  += 8*8*6;
            pCurrMB++;

            CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
            pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
    }
    else
    {
            YPlane = pCurrMB->currYPlane;
            UPlane = pCurrMB->currUPlane;
            VPlane = pCurrMB->currVPlane;

            //first MB in row
            //internal vertical smoothing
            _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,  VC1_PIXEL_IN_LUMA*2,
                                                            CurrBlock + 8,  VC1_PIXEL_IN_LUMA*2,
                                                            YPlane + 8,     YPitch,
                                                            0,              EdgeDisabledFlag);

            for (i = 1; i < Width; i++)
            {
                CurrBlock  += 8*8*6;
                pCurrMB++;

                YPlane = pCurrMB->currYPlane;
                UPlane = pCurrMB->currUPlane;
                VPlane = pCurrMB->currVPlane;

                //LUMA
                //left boundary vertical smoothing
                _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6 + 14,  VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock,               VC1_PIXEL_IN_LUMA*2,
                                                                YPlane,                  YPitch,
                                                                0,                       EdgeDisabledFlag);
                //internal vertical smoothing
                _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,  VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8,  VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8,     YPitch,
                                                                0,              EdgeDisabledFlag);

                //left MB internal horizontal edge
                _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32,   VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock - 8*8*4,        VC1_PIXEL_IN_LUMA*2,
                                                                YPlane - 16 + 8*YPitch,   YPitch,
                                                                EdgeDisabledFlag);

                //CHROMA
                //U vertical smoothing
                _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                UPlane,                UPitch);

                //V vertical smoothing
                _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                CurrBlock +  8*8*5,  VC1_PIXEL_IN_CHROMA*2,
                                                                VPlane,             VPitch);
            }

            //RIGHT MB
            //MB internal horizontal edge
            _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32,  VC1_PIXEL_IN_LUMA*2,
                                                            CurrBlock + 8*8*2,       VC1_PIXEL_IN_LUMA*2,
                                                            YPlane + 8*YPitch,       YPitch,
                                                            EdgeDisabledFlag);
            CurrBlock  += 8*8*6;
            pCurrMB++;

            CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
            pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
        }
    }
}

void Smoothing_P(VC1Context* pContext, Ipp32s Height)
{
    if(pContext->m_seqLayerHeader.OVERLAP == 0)
        return;

    {
        VC1MB* pCurrMB = pContext->m_pCurrMB;
        Ipp32s notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);
        Ipp32s Width = pContext->m_seqLayerHeader.widthMB;
        Ipp32s MaxWidth = pContext->m_seqLayerHeader.MaxWidthMB;
        Ipp32u EdgeDisabledFlag;
        Ipp16s* CurrBlock = pContext->m_pBlock;
        Ipp8u* YPlane = pCurrMB->currYPlane;
        Ipp8u* UPlane = pCurrMB->currUPlane;
        Ipp8u* VPlane = pCurrMB->currVPlane;

        Ipp32s YPitch = pCurrMB->currYPitch;
        Ipp32s UPitch = pCurrMB->currUPitch;
        Ipp32s VPitch = pCurrMB->currVPitch;
        Ipp32s LeftIntra;
        Ipp32s TopLeftIntra;
        Ipp32s TopIntra;
        Ipp32s CurrIntra = pCurrMB->IntraFlag*pCurrMB->Overlap;

        Ipp32s i, j;
 
        for (j = 0; j< Height; j++)
        {
            notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);

            if(notTop)
            {
                YPlane = pCurrMB->currYPlane;
                UPlane = pCurrMB->currUPlane;
                VPlane = pCurrMB->currVPlane;

                CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;
                TopIntra     = (pCurrMB - MaxWidth)->IntraFlag*(pCurrMB - MaxWidth)->Overlap;

                if(CurrIntra)
                {
                    //first block in row
                    //internal vertical smoothing
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                    |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));

                    _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8, VC1_PIXEL_IN_LUMA*2,
                                                                YPlane    + 8, YPitch,
                                                                0,             EdgeDisabledFlag);
                }

                for (i = 1; i < Width; i++)
                {

                    CurrBlock  += 8*8*6;
                    pCurrMB++;

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;
                    LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;
                    TopLeftIntra = (pCurrMB - MaxWidth - 1)->IntraFlag*(pCurrMB - MaxWidth - 1)->Overlap;

                    if(CurrIntra)
                    {

                        //////////////////////////////////
                        //LUMA left boundary vertical smoothing
                        EdgeDisabledFlag =(((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_0_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_1_INTRA)))
                            *(IPPVC_EDGE_HALF_1))|
                            (((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_3_INTRA)))
                            *(IPPVC_EDGE_HALF_2));

                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6+14, VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock,            VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane,               YPitch,
                                                                    0,                    EdgeDisabledFlag);
                        //////////////////////////////
                        //internal vertical smoothing
                        EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                        |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));

                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,        VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock + 8,        VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane+8,             YPitch,
                                                                    0,                    EdgeDisabledFlag);
                    }



                    if(LeftIntra)
                    {
                        ////////////////////////////////
                        //left MB Upper horizontal edge
                        EdgeDisabledFlag =(((VC1_EDGE_MB(TopLeftIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_0_INTRA)))
                            *(IPPVC_EDGE_HALF_1))|
                            (((VC1_EDGE_MB(TopLeftIntra,VC1_BLOCK_3_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_1_INTRA)))
                            *(IPPVC_EDGE_HALF_2));
                        _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA,              VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock - 8*8*6,   VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane - 16,         YPitch,
                                                                    EdgeDisabledFlag);
                        //////////////////////////////////
                        //left MB internal horizontal edge
                        EdgeDisabledFlag = (VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                        |(VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                        _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock - 8*8*4,      VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane - 16 + 8*YPitch, YPitch,
                                                                    EdgeDisabledFlag);
                         if(((LeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((CurrIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                        {
                            ///////////////////////
                            //U vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane,                UPitch);

                            ///////////////////////
                            //V vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*5,   VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane,              VPitch);
                        }

                        if(((TopLeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((LeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                        {
                            /////////////////////////////
                            //U top horizontal smoothing
                            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,           VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock - 2*64, VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane - 8,       UPitch);

                            ////////////////////////////
                            //V top horizontal smoothing
                            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,         VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock - 64, VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane - 8,     VPitch);
                        }

                    }
                }

                //RIGHT MB
                //LUMA
                TopIntra     = (pCurrMB - MaxWidth)->IntraFlag*(pCurrMB - MaxWidth)->Overlap;

                if(CurrIntra)
                {
                    ///////////////////////////
                    //MB Upper horizontal edge
                    EdgeDisabledFlag =(((VC1_EDGE_MB(TopIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(CurrIntra, VC1_BLOCK_0_INTRA)))
                            *(IPPVC_EDGE_HALF_1))|
                            (((VC1_EDGE_MB(TopIntra,VC1_BLOCK_3_INTRA)) && (VC1_EDGE_MB(CurrIntra, VC1_BLOCK_1_INTRA)))
                            *(IPPVC_EDGE_HALF_2));
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA,     VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock,   VC1_PIXEL_IN_LUMA*2,
                                                                YPlane,      YPitch,
                                                                EdgeDisabledFlag);
                    /////////////////////////////
                    //MB internal horizontal edge
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                    |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8*8*2,      VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8*YPitch,      YPitch,
                                                                EdgeDisabledFlag);
                    if(((TopIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((CurrIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                    {
                        /////////////////////////////
                        //U top horizontal smoothing
                        _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,            VC1_PIXEL_IN_CHROMA*2,
                                                                    CurrBlock + 4*64,  VC1_PIXEL_IN_CHROMA*2,
                                                                    UPlane,            UPitch);

                        /////////////////////////////
                        //V top horizontal smoothing
                        _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,           VC1_PIXEL_IN_CHROMA*2,
                                                                    CurrBlock + 64*5, VC1_PIXEL_IN_CHROMA*2,
                                                                    VPlane,           VPitch);
                    }

                    //copy last two srings of Left macroblock to SmoothUpperRows
                }

                CurrBlock  += 8*8*6;
                pCurrMB++;
                CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
                pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);

                LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;
                TopLeftIntra = (pCurrMB - MaxWidth - 1)->IntraFlag*(pCurrMB - MaxWidth - 1)->Overlap;
            }
            else
            {
                YPlane = pCurrMB->currYPlane;
                UPlane = pCurrMB->currUPlane;
                VPlane = pCurrMB->currVPlane;

                CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;

                if(CurrIntra)
                {
                    ////////////////////////////
                    //first block in row
                    //internal vertical smoothing
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                    |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));
                    _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8, VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8,    YPitch,
                                                                0,             EdgeDisabledFlag);
                }

                for (i = 1; i < Width; i++)
                {
                    CurrBlock  += 8*8*6;
                    pCurrMB++;

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;
                    LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;

                    //LUMA
                    if(CurrIntra)
                    {
                        ///////////////////////////////////
                        //left boundary vertical smoothing
                        EdgeDisabledFlag =(((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_0_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_1_INTRA)))
                                *(IPPVC_EDGE_HALF_1))|
                                (((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_3_INTRA)))
                                *(IPPVC_EDGE_HALF_2));

                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6+14, VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock,            VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane,               YPitch,
                                                                    0,                    EdgeDisabledFlag);
                        /////////////////////////////
                        //internal vertical smoothing
                        EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                        |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));
                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,        VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock + 8,        VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane + 8,           YPitch,
                                                                    0,                    EdgeDisabledFlag);
                    }

                    if(LeftIntra)
                    {
                        ///////////////////////////////////
                        //left MB internal horizontal edge
                        EdgeDisabledFlag = (VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                        |(VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                        _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock - 8*8*4,      VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane - 16 + 8*YPitch, YPitch,
                                                                    EdgeDisabledFlag);
                        //CHROMA
                        if(((LeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((CurrIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                        {
                            ///////////////////////
                            //U vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane,                UPitch);
                            //////////////////////
                            //V vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*5,    VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane,               VPitch);
                        }
                    }
                }

                //RIGHT MB
                //LUMA

                if(CurrIntra)
                {
                    /////////////////////////////
                    //MB internal horizontal edge
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                    |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8*8*2,      VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8*YPitch, YPitch,
                                                                EdgeDisabledFlag);
                }

                CurrBlock  += 8*8*6;
                pCurrMB++;
                CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
                pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
                LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;
            }
        }
    }
}
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
