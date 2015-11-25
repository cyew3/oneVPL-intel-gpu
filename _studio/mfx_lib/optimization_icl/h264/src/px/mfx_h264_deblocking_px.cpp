
#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_dispatcher.h"

namespace MFX_PP
{

#define ABS(a)          (((a) < 0) ? (-(a)) : (a))
#define clipd1(x, limit) ((limit < (x)) ? (limit) : (((-limit) > (x)) ? (-limit) : (x)))

template<typename Plane>
inline Plane ClampVal_16U(mfxI32 val, mfxU32 bitDepth)
{
    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane maxValue = ((1 << bitDepth) - 1);
    return (Plane) ((maxValue < (val)) ? (maxValue) : ((0 > (val)) ? (0) : (val)));
}

template<typename Plane>
void ippiFilterDeblockingLumaVerEdge_H264_kernel(Plane* srcDst, Ipp32s  srcDstStep,
                                    Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                    Ipp8u*  pThresholds, Ipp8u*  pBS,
                                    Ipp32s  bitDepth)
{
    mfxI32 i,j;
    Plane *pEdge;
    mfxU32 Aedge, Ap1, Ap2, Aq1, Aq2; /* pixel activity measures */
    mfxI32 p0, p1, p2, p3;
    mfxI32 q0, q1, q2, q3;
    mfxU32 alpha, beta;
    mfxU8 *pStrong;
    mfxU8 *pClip;
    Plane  uClip;
    Ipp8u  uStrong;
    Plane  uClip1;
    mfxI32 iDelta;
    mfxI32 iFiltPel;
    mfxI32 bFullStrong;
    mfxI32 tmp;

    Ipp32s bitDepthScale = sizeof(Plane) == 1 ? 1 : 1 << (bitDepth - 8);

    for (j = 0; j < 4; j++)
    {
        alpha   = pAlpha[j>0]*bitDepthScale;
        beta    = pBeta [j>0]*bitDepthScale;
        pStrong = pBS             + 4*j;
        pEdge   = (Plane*)srcDst    + 4*j;
        pClip   = pThresholds + 4*j;

        if (0 == *((int *) pStrong))
            continue;

        for (i=0; i<16; i++, pEdge += srcDstStep)
        {
            /* Filter this bit position?*/
            uStrong = pStrong[i>>2];
            if (!uStrong)
            {
                i += 3;
                pEdge += 3*srcDstStep;
                continue;
            }

            /* filter used is dependent upon pixel activity on each side of edge*/
            p0 = *(pEdge-1);
            q0 = *(pEdge);
            Aedge = ABS(p0 - q0);
            if (Aedge >= alpha)
                continue;       /* filter edge only when abs(p0-q0) < alpha*/

            p1  = *(pEdge-2);
            Ap1 = ABS(p1 - p0);
            if (Ap1 >= beta)
                continue;               /* filter edge only when abs(p1-p0) < beta*/

            q1  = *(pEdge+1);
            Aq1 = ABS(q1 - q0);
            if (Aq1 >= beta)
                continue;               /* filter edge only when abs(q1-q0) < beta*/


            p2 = *(pEdge-3);
            q2 = *(pEdge+2);
            Ap2 = ABS(p2 - p0);
            Aq2 = ABS(q2 - q0);
            if (uStrong == 4) /*Strong*/
            {
                bFullStrong = Aedge < ((alpha>>2)+2);

                if (bFullStrong && (Ap2 < beta))
                {
                    tmp = p1 + p0 + q0;
                    p3 = *(pEdge-4);
                    *(pEdge-1) = (Plane)((p2 + 2*tmp + q1 + 4) >> 3);
                    *(pEdge-2) = (Plane)((2 * p2 + 2 * tmp + 4) >> 3);
                    *(pEdge-3) = (Plane)((2*p3 + 3*p2 + tmp + 4) >> 3);
                }
                else
                {
                     *(pEdge-1) = (Plane)((2*p1 + p0 + q1 + 2) >> 2);
                }
                if (bFullStrong && (Aq2 < beta))
                {
                    tmp = q1 + q0 + p0;
                    q3 = *(pEdge+3);
                    *(pEdge)   = (Plane)((q2 + 2*tmp + p1 + 4) >> 3);
                    *(pEdge+1) = (Plane)((q2 + tmp + 2) >> 2);
                    *(pEdge+2) = (Plane)((2*q3 + 3*q2 + tmp + 4) >> 3);
                }
                else
                {
                    *(pEdge) = (Plane)((2*q1 + q0 + p1 + 2) >> 2);
                }
            }       /* strong*/
            else
            {       /* weak*/

                uClip = (Plane)(pClip[i>>2]*bitDepthScale);
                uClip1 = uClip;
                if (Ap2 < beta) uClip1++;
                if (Aq2 < beta) uClip1++;

                /* p0, q0*/
                iDelta = (((q0-p0)<<2) + (p1-q1) + 4)>>3;
                if (iDelta)
                {
                    iDelta = clipd1(iDelta, uClip1);
                    iFiltPel = p0 + iDelta;
                    *(pEdge-1) = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                    iFiltPel = q0 - iDelta;
                    *(pEdge) = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                }

                /* p1*/
                if (Ap2 < beta)
                {
                    iDelta = (p2 + ((p0 + q0 + 1) >> 1) - 2*p1) >> 1;
                    /*++++++++++++++++++*/
                    iDelta = clipd1(iDelta, uClip);
                    *(pEdge - 2) = (Plane) (*(pEdge - 2) + iDelta);
                    /*++++++++++++++++++*/
                }       /* p1*/

                /* q1*/
                if (Aq2 < beta)
                {
                    iDelta = (q2 + ((p0 + q0 + 1) >> 1) - 2*q1) >> 1;
                    /*++++++++++++++++++*/
                    iDelta = clipd1(iDelta, uClip);
                    *(pEdge + 1) = (Plane) (*(pEdge + 1) + iDelta);
                    /*++++++++++++++++++*/
                }       /* q1*/
            }       /* weak*/
        }       /* for i*/
    }       /*for j*/

} /* ippiFilterDeblockingLumaVerEdge_H264_16u_C1IR*/


template<typename Plane>
void ippiFilterDeblockingLumaHorEdge_H264_kernel(Plane* srcDst, Ipp32s  srcDstStep,
                                    Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                    Ipp8u*  pThresholds, Ipp8u*  pBS,
                                    Ipp32s  bitDepth)
{
    Plane* pSrcDst;
    mfxI32 i,j;
    Plane *pEdge;
    mfxU32 Aedge, Ap1, Ap2, Aq1, Aq2; /* pixel activity measures*/
    mfxI32 p0, p1, p2, p3;
    mfxI32 q0, q1, q2, q3;
    mfxU32 alpha, beta;
    mfxU8 *pStrong;
    mfxU8 *pClip;
    Plane uClip;
    mfxU8 uStrong;
    Plane uClip1;
    mfxI32 iDelta;
    mfxI32 iFiltPel;
    mfxI32 bFullStrong;
    mfxI32 tmp;
    mfxI32 negStep;

    /* check error(s) */
    Ipp32s bitDepthScale = sizeof(Plane) == 1 ? 1 : 1 << (bitDepth - 8);

    negStep = -srcDstStep;
    pSrcDst = (Plane*)srcDst;

    for (j = 0; j < 4; j++)
    {
        alpha = pAlpha[j>0]*bitDepthScale;
        beta =  pBeta [j>0]*bitDepthScale;
        pStrong = pBS + 4*j;
        pEdge = pSrcDst + 4*j*srcDstStep;
        pClip = pThresholds + 4*j;

        if (0 == *((int *) pStrong))
            continue;

        for (i=0; i < 16; i++, pEdge++)
        {
            /* Filter this bit position?*/
            uStrong = pStrong[i>>2];
            if (!uStrong)
            {
                i += 3;
                pEdge += 3;
                continue;
            }

            /* filter used is dependent upon pixel activity on each side of edge*/
            p0 = *(pEdge+1*negStep);
            q0 = *(pEdge);
            Aedge = ABS(p0 - q0);
            if (Aedge >= alpha)
                continue;       /* filter edge only when abs(p0-q0) < alpha*/

            p1  = *(pEdge+2*negStep);
            Ap1 = ABS(p1 - p0);
            if (Ap1 >= beta)
                continue;               /* filter edge only when abs(p1-p0) < beta*/

            q1  = *(pEdge+1*srcDstStep);
            Aq1 = ABS(q1 - q0);
            if (Aq1 >= beta)
                continue;               /* filter edge only when abs(q1-q0) < beta*/


            p2 = *(pEdge+3*negStep);
            q2 = *(pEdge+2*srcDstStep);
            Ap2 = ABS(p2 - p0);
            Aq2 = ABS(q2 - q0);

            if (uStrong == 4)
            {
                bFullStrong = Aedge < ((alpha>>2)+2);

                if (bFullStrong && (Ap2 < beta))
                {
                    tmp = p1 + p0 + q0;
                    p3 = *(pEdge+4*negStep);
                    *(pEdge+1*negStep) = (Plane)((p2 + 2*tmp + q1 + 4) >> 3);  /*p0*/
                    *(pEdge+2*negStep) = (Plane)((p2 + tmp + 2) >> 2);         /*p1*/
                    *(pEdge+3*negStep) = (Plane)((2*p3 + 3*p2 + tmp + 4) >> 3);/*p2*/
                }
                else
                {
                    *(pEdge+1*negStep) = (Plane)((2*p1 + p0 + q1 + 2) >> 2);/*p0*/
                }
                if (bFullStrong && (Aq2 < beta))
                {
                    tmp = q1 + q0 + p0;
                    q3 = *(pEdge+3*srcDstStep);
                    *(pEdge)              = (Plane)((q2 + 2*tmp + p1 + 4) >> 3); /*q0*/
                    *(pEdge+1*srcDstStep) = (Plane)((q2 + tmp + 2) >> 2);                   /*q1*/
                    *(pEdge+2*srcDstStep) = (Plane)((2*q3 + 3*q2 + tmp + 4) >> 3);  /*q2*/
                }
                else
                {
                    *(pEdge) = (Plane)((2*q1 + q0 + p1 + 2) >> 2);/*q0*/
                }
            }       /* strong*/
            else
            {       /* weak*/

                uClip = (Plane)(pClip[i>>2]*bitDepthScale);
                uClip1 = uClip;
                if (Ap2 < beta) uClip1++;
                if (Aq2 < beta) uClip1++;

                /* p0, q0*/
                iDelta = (((q0-p0)<<2) + (p1-q1) + 4)>>3;
                if (iDelta)
                {
                    iDelta = clipd1(iDelta, uClip1);
                    iFiltPel = p0 + iDelta;
                    *(pEdge+1*negStep) = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                    iFiltPel = q0 - iDelta;
                    *(pEdge) = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                }

                /* p1*/
                if (Ap2 < beta)
                {
                    iDelta = (p2 + ((p0 + q0 + 1) >> 1) - 2 * p1) >> 1;
                    /*++++++++++++++++++*/
                    iDelta = clipd1(iDelta, uClip);
                    *(pEdge + 2 * negStep) = (Plane) (*(pEdge + 2 * negStep) + iDelta);
                    /*++++++++++++++++++*/
                }       /* p1*/

                /* q1*/
                if (Aq2 < beta)
                {
                    iDelta = (q2 + ((p0 + q0 + 1) >> 1) - 2 * q1) >> 1;
                    /*++++++++++++++++++*/
                    iDelta = clipd1(iDelta, uClip);
                    *(pEdge + 1 * srcDstStep) = (Plane) (*(pEdge + 1 * srcDstStep) + iDelta);
                    /*++++++++++++++++++*/
                }       /* q1*/
            }       /* weak*/
        }       /* for i*/
    }       /*for j*/
}

template<typename Plane, int chroma_format_idc>
void ippiFilterDeblockingChroma_VerEdge_H264_kernel(Plane* pSrcDstPlane,
                                                Ipp32s  srcDstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBs,
                                                Ipp32s  bitDepth)
{
    Ipp32s i, j;
    Plane *pEdge;
    const Ipp8u *pStrength;
    const Ipp8u *pClip;
    Ipp32s iFiltPel;

    Ipp32s bitDepthScale = sizeof(Plane) == 1 ? 1 : 1 << (bitDepth - 8);

    for (Ipp32s plane = 0; plane <= 1; plane += 1)
    {
        pStrength = pBs;
        pClip = pThresholds + 8*plane;

        /* cycle over edges */
        for (j = 0; j < 2; j += 1)
        {
            Ipp32u alpha, beta;

            /* get the alpha & beta */
            alpha = pAlpha[(j > 0) + plane*2]*bitDepthScale;
            beta =  pBeta[(j > 0) + plane*2]*bitDepthScale;

            if (0 == (*(Ipp32u *) pStrength))
            {
                pClip += 4;
                pStrength += 8;
                continue;
            }

            /* cycle over pixels */
            for (i = 0; i < 8 * (chroma_format_idc == 2 ? 2: 1); i += 1)
            {
                Ipp32u strength;

                Ipp32s index =  (chroma_format_idc == 2) ? (i >> 2) : (i >> 1);

                /* set the edge. we set the edge for every iteration
                   to avoid complex conditions */
                pEdge = (Plane*)pSrcDstPlane + i * srcDstStep + j * (4 * 2);

                /* Filter this bit position? */

                strength = pStrength[index];
                if (0 == strength)
                {
                    i += (chroma_format_idc == 2) ? 3 : 1;
                    continue;
                }

                /* filter is dependent upon pixel activity on each side of edge */
                Ipp32u absDiff;
                Ipp32s p0, p1;
                Ipp32s q0, q1;

                p0 = pEdge[-1 * 2 + plane];
                q0 = pEdge[0 * 2 + plane];
                absDiff = ABS(p0 - q0);
                if (absDiff >= alpha)
                {
                    continue;
                }

                p1  = pEdge[-2 * 2 + plane];
                absDiff = ABS(p1 - p0);
                if (absDiff >= beta)
                {
                    continue;
                }

                q1  = pEdge[1 * 2 + plane];
                absDiff = ABS(q1 - q0);
                if (absDiff >= beta)
                {
                    continue;
                }

                /* do strong deblocking */
                if (4 == strength)
                {
                    pEdge[-1 * 2 + plane] = (Plane) ((Ipp32s) (2 * p1 + p0 + q1 + 2) >> 2);
                    pEdge[0 * 2 + plane] = (Plane) ((Ipp32s) (2 * q1 + q0 + p1 + 2) >> 2);
                }
                /* do weak deblocking */
                else
                {
                    Ipp32s clip;
                    Ipp32s delta;

                    clip = pClip[index]*bitDepthScale + 1;
                    /* p0, q0 */
                    delta = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3;
                    if (delta)
                    {
                        delta = clipd1(delta, clip);
                        iFiltPel = p0 + delta;
                        pEdge[-1 * 2 + plane] = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                        iFiltPel = q0 - delta;
                        pEdge[0 * 2 + plane] = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                    }
                }
            }

            /* advance pointers */
            pClip += 4;
            pStrength += 8;
        }
    }
}

template<typename Plane, int chroma_format_idc>
void ippiFilterDeblockingChroma_HorEdge_H264_kernel(Plane* pSrcDstPlane,
                                                Ipp32s  srcDstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bitDepth)
{
    Ipp32s i, j;
    Plane *pEdge;
    const Ipp8u *pStrength;
    const Ipp8u *pClip;
    Ipp32s iFiltPel;
    Ipp32s negStep = -((Ipp32s)srcDstStep);

    Ipp32s bitDepthScale = sizeof(Plane) == 1 ? 1 : 1 << (bitDepth - 8);

    for (Ipp32s plane = 0; plane <= 1; plane += 1)
    {
        pStrength = pBS;
        pClip = pThresholds + ((chroma_format_idc == 2) ? 16*plane : 8*plane);

        /* cycle over edges */
        for (j = 0; j < 2 * (chroma_format_idc == 2 ? 2: 1); j += 1)
        {
            Ipp32u alpha, beta;

            /* get the alpha & beta */
            alpha = pAlpha[(j > 0) + plane*2]*bitDepthScale;
            beta =  pBeta [(j > 0) + plane*2]*bitDepthScale;

            if (0 == (*(Ipp32u *) pStrength))
            {
                pClip += 4;
                pStrength += (chroma_format_idc == 2 ? 4 : 8);
                continue;
            }

            /* cycle over pixels */
            for (i = 0; i < 8; i += 1)
            {
                Ipp32u strength;

                /* set the edge. we set the edge for every iteration
                   to avoid complex conditions */
                pEdge = (Plane*)pSrcDstPlane + i * 2 + j * (4 * srcDstStep);

                /* Filter this bit position? */
                strength = pStrength[i >> 1];
                if (0 == strength)
                {
                    i += 1;
                    continue;
                }

                /* filter is dependent upon pixel activity on each side of edge */
                Ipp32u absDiff;
                Ipp32s p0, p1;
                Ipp32s q0, q1;

                p0 = pEdge[plane + negStep];
                q0 = pEdge[plane];
                absDiff = ABS(p0 - q0);
                if (absDiff >= alpha)
                {
                    continue;
                }

                p1  = pEdge[plane + 2 * negStep];
                absDiff = ABS(p1 - p0);
                if (absDiff >= beta)
                {
                    continue;
                }

                q1  = pEdge[1 * srcDstStep + plane];
                absDiff = ABS(q1 - q0);
                if (absDiff >= beta)
                {
                    continue;
                }

                /* do strong deblocking */
                if (4 == strength)
                {
                    pEdge[plane + negStep] = (Plane) ((Ipp32s) (2 * p1 + p0 + q1 + 2) >> 2);
                    pEdge[plane] = (Plane) ((Ipp32s) (2 * q1 + q0 + p1 + 2) >> 2);
                }
                /* do weak deblocking */
                else
                {
                    Ipp32s clip;
                    Ipp32s delta;

                    clip = pClip[i >> 1]*bitDepthScale + 1;

                    /* p0, q0 */
                    delta = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3;
                    if (delta)
                    {
                        delta = clipd1(delta, clip);
                        iFiltPel = p0 + delta;
                        pEdge[plane + negStep] = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                        iFiltPel = q0 - delta;
                        pEdge[plane] = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                    }
                }
            }

            /* advance pointers */
            pClip += 4;
            pStrength += (chroma_format_idc == 2 ? 4 : 8);
        }
    }
}

template<typename Plane>
void ippiFilterDeblockingChroma_NV12_VerEdge_MBAFF_H264_kernel(Plane* pSrcDstPlane,
                                                Ipp32s  srcDstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  thresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bitDepth,
                                                Ipp32u  chroma_format_idc)
{
    Ipp32s i;
    Ipp32u Aedge, Ap1, Aq1; /* pixel activity measures */
    mfxI32 p0, p1;
    mfxI32 q0, q1;
    Ipp8u uStrong;
    Plane uClip1;
    Ipp32s iDelta;
    Ipp32s iFiltPel;

    if (0 == *((Ipp32u *) pBS))
        return;

    Ipp32s idx = chroma_format_idc == 2 ? 1 : 0;

    Ipp32s bitDepthScale = sizeof(Plane) == 1 ? 1 : 1 << (bitDepth - 8);
    Ipp32s plane;
    for (plane = 0; plane <= 1; plane += 1)
    {
        Plane *pSrcDst = (Plane*)pSrcDstPlane + plane;
        const Ipp8u *pThresholds = thresholds + 8*plane;
        Ipp32u nAlpha = pAlpha[2*plane]*bitDepthScale;
        Ipp32u nBeta = pBeta[2*plane]*bitDepthScale;

        if (nAlpha == 0) continue;

        for (i = 0; i < 4; i += 1, pSrcDst += srcDstStep)
        {
            /* Filter this bit position? */
            uStrong = pBS[i >> idx];
            if (uStrong)
            {
                /* filter used is dependent upon pixel activity on each side of edge */
                p0 = *(pSrcDst - 1 * 2);
                q0 = *(pSrcDst);
                Aedge = ABS(p0 - q0);
                /* filter edge only when abs(p0-q0) < nAlpha */
                if (Aedge >= nAlpha)
                    continue;

                p1  = *(pSrcDst - 2 * 2);
                Ap1 = ABS(p1 - p0);
                /* filter edge only when abs(p1-p0) < nBeta */
                if (Ap1 >= nBeta)
                    continue;

                q1  = *(pSrcDst + 1 * 2);
                Aq1 = ABS(q1 - q0);
                /* filter edge only when abs(q1-q0) < nBeta */
                if (Aq1 >= nBeta)
                    continue;

                /* strong filtering */
                if (4 == uStrong)
                {
                    *(pSrcDst - 1 * 2) = (Plane)((Ipp32s)(2 * p1 + p0 + q1 + 2) >> 2);
                    *(pSrcDst) = (Plane)((Ipp32s)(2 * q1 + q0 + p1 + 2) >> 2);
                }
                /* weak filtering */
                else
                {
                    uClip1 = (Plane) (pThresholds[i >> idx]*bitDepthScale + 1);
                    /* p0, q0 */
                    iDelta = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3;
                    if (iDelta)
                    {
                        iDelta = clipd1(iDelta, uClip1);
                        iFiltPel = p0 + iDelta;
                        *(pSrcDst - 1 * 2) = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                        iFiltPel = q0 - iDelta;
                        *(pSrcDst) = ClampVal_16U<Plane>(iFiltPel, bitDepth);
                    }
                }
            }
        }
    }
}

void MFX_Dispatcher::FilterDeblockingLumaEdge(Ipp8u* pSrcDst, Ipp32s  srcdstStep,
                                    Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                    Ipp8u*  pThresholds, Ipp8u*  pBS,
                                    Ipp32s  ,//bit_depth,
                                    Ipp32u  isDeblHor)
{
    if (isDeblHor)
    {
        //ippiFilterDeblockingLumaHorEdge_H264_kernel<mfxU8>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        ippiFilterDeblockingLuma_HorEdge_H264_8u_C1IR(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS);
    }
    else
    {
        //ippiFilterDeblockingLumaVerEdge_H264_kernel<mfxU8>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        ippiFilterDeblockingLuma_VerEdge_H264_8u_C1IR(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS);
    }
}

void MFX_Dispatcher::FilterDeblockingLumaEdge(Ipp16u* pSrcDst, Ipp32s  srcdstStep,
                                    Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                    Ipp8u*  pThresholds, Ipp8u*  pBS,
                                    Ipp32s  bit_depth,
                                    Ipp32u  isDeblHor)
{
    if (isDeblHor)
    {
        ippiFilterDeblockingLumaHorEdge_H264_kernel<mfxU16>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
    else
    {
        ippiFilterDeblockingLumaVerEdge_H264_kernel<mfxU16>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
}

void MFX_Dispatcher::FilterDeblockingChromaEdge(Ipp8u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc,
                                                Ipp32u  isDeblHor)
{
    if (isDeblHor)
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_HorEdge_H264_kernel<mfxU8, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_HorEdge_H264_kernel<mfxU8, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
    else
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_VerEdge_H264_kernel<mfxU8, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_VerEdge_H264_kernel<mfxU8, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
}

void MFX_Dispatcher::FilterDeblockingChromaEdge(Ipp16u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc,
                                                Ipp32u  isDeblHor)
{
    if (isDeblHor)
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_HorEdge_H264_kernel<mfxU16, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_HorEdge_H264_kernel<mfxU16, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
    else
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_VerEdge_H264_kernel<mfxU16, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_VerEdge_H264_kernel<mfxU16, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
}

void MFX_Dispatcher::FilterDeblockingChromaVerEdge_MBAFF(Ipp16u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc)
{
    ippiFilterDeblockingChroma_NV12_VerEdge_MBAFF_H264_kernel<mfxU16>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth, chroma_format_idc);
}

void MFX_Dispatcher::FilterDeblockingChromaVerEdge_MBAFF(Ipp8u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc)
{
    ippiFilterDeblockingChroma_NV12_VerEdge_MBAFF_H264_kernel<mfxU8>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth, chroma_format_idc);
}

} // namespace MFX_PP
