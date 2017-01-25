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

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_debug.h"

#ifdef _OWN_FUNCTION
IppStatus _own_FilterDeblockingLuma_VerEdge_VC1(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

    for (count = 0; count < 4;count++)
    {
        pRPixel = pSrcDst + 2 * srcdstStep + 4*count*srcdstStep;

        if (!(EdgeDisabledFlag & (1 << count) ))
        {

            p4 = pRPixel[-1];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4];
            p2 = pRPixel[-3];
            p3 = pRPixel[-2];
            p6 = pRPixel[1];
            p7 = pRPixel[2];
            p8 = pRPixel[3];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (vc1_abs_16s(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                if (a3 < vc1_abs_16s(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2 * srcdstStep;
            for (i=4; i--; pRPixel += srcdstStep)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4];
                p2 = pRPixel[-3];
                p3 = pRPixel[-2];
                p6 = pRPixel[1];
                p7 = pRPixel[2];
                p8 = pRPixel[3];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (vc1_abs_16s(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                    if (a3 < vc1_abs_16s(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingChroma_VerEdge_VC1(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    static Ipp32s EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    IppStatus ret = ippStsNoErr;
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

    for (count = 0; count < 2;count++)
    {

        pRPixel = pSrcDst + 2 * srcdstStep + 4*count*srcdstStep;

        if (!(EdgeDisabledFlag & (EdgeTable[count]) ))
        {

            p4 = pRPixel[-1];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4];
            p2 = pRPixel[-3];
            p3 = pRPixel[-2];
            p6 = pRPixel[1];
            p7 = pRPixel[2];
            p8 = pRPixel[3];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (vc1_abs_16s(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                if (a3 < vc1_abs_16s(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2 * srcdstStep;
            for (i=4; i--; pRPixel += srcdstStep)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4];
                p2 = pRPixel[-3];
                p3 = pRPixel[-2];
                p6 = pRPixel[1];
                p7 = pRPixel[2];
                p8 = pRPixel[3];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (vc1_abs_16s(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                    if (a3 < vc1_abs_16s(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingLuma_HorEdge_VC1(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

    for (count = 0; count < 4;count++)
    {

        pRPixel = pSrcDst + 2 + 4*count;

        if (!(EdgeDisabledFlag & (1 << count) ))
        {

            p4 = pRPixel[-1*srcdstStep];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4*srcdstStep];
            p2 = pRPixel[-3*srcdstStep];
            p3 = pRPixel[-2*srcdstStep];
            p6 = pRPixel[1*srcdstStep];
            p7 = pRPixel[2*srcdstStep];
            p8 = pRPixel[3*srcdstStep];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (vc1_abs_16s(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                if (a3 < vc1_abs_16s(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2;
            for (i=4; i--; pRPixel += 1)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1*srcdstStep];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4*srcdstStep];
                p2 = pRPixel[-3*srcdstStep];
                p3 = pRPixel[-2*srcdstStep];
                p6 = pRPixel[1*srcdstStep];
                p7 = pRPixel[2*srcdstStep];
                p8 = pRPixel[3*srcdstStep];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (vc1_abs_16s(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                    if (a3 < vc1_abs_16s(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}


IppStatus _own_FilterDeblockingChroma_HorEdge_VC1(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    static Ipp32s EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

    for (count = 0; count < 2;count++)
    {
        pRPixel = pSrcDst + 2 + 4*count;

        if (!(EdgeDisabledFlag & (EdgeTable[count]) ))
        {

            p4 = pRPixel[-1*srcdstStep];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4*srcdstStep];
            p2 = pRPixel[-3*srcdstStep];
            p3 = pRPixel[-2*srcdstStep];
            p6 = pRPixel[1*srcdstStep];
            p7 = pRPixel[2*srcdstStep];
            p8 = pRPixel[3*srcdstStep];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (vc1_abs_16s(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                if (a3 < vc1_abs_16s(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2;
            for (i=4; i--; pRPixel += 1)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1*srcdstStep];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4*srcdstStep];
                p2 = pRPixel[-3*srcdstStep];
                p3 = pRPixel[-2*srcdstStep];
                p6 = pRPixel[1*srcdstStep];
                p7 = pRPixel[2*srcdstStep];
                p8 = pRPixel[3*srcdstStep];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (vc1_abs_16s(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = IPP_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
                    if (a3 < vc1_abs_16s(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (vc1_abs_16s(d) > vc1_abs_16s(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

#endif

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
