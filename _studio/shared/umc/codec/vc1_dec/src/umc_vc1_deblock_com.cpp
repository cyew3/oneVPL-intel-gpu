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
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_debug.h"

#ifdef _OWN_FUNCTION
IppStatus _own_FilterDeblockingLuma_VerEdge_VC1(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

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
                a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                    pRPixel[-1] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
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
                    a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                        pRPixel[-1] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingChroma_VerEdge_VC1(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    static int32_t EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    IppStatus ret = ippStsNoErr;
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

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
                a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                    pRPixel[-1] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
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
                    a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                        pRPixel[-1] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingLuma_HorEdge_VC1(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

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
                a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                    pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
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
                    a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                        pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}


IppStatus _own_FilterDeblockingChroma_HorEdge_VC1(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    static int32_t EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

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
                a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                    pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
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
                    a3 = MFX_MIN(vc1_abs_16s(a1), vc1_abs_16s(a2));
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
                        pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

#endif

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
