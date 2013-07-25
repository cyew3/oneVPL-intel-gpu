/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)

// ML: OPT: significant overhead if not inlined (ICC does not honor implied 'inline' with -Qipo)
// ML: OPT: TODO: Make sure compiler recognizes saturation idiom for vectorization when used
#if 1
#define Clip3( m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? \
                                                  (m_Min) : \
                                                ( (m_Value) > (m_Max) ? \
                                                              (m_Max) : \
                                                              (m_Value) ) )
#endif

namespace MFX_HEVC_COMMON
{
    enum FilterType
    {
        VERT_FILT = 0,
        HOR_FILT = 1,
    };

    static Ipp32s tcTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
        3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
        14, 16, 18, 20, 22, 24
    };

    static Ipp32s betaTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
        26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
        58, 60, 62, 64
    };

    void h265_FilterEdgeLuma_8u_I(
        H265EdgeData *edge, 
        Ipp8u *srcDst,
        Ipp32s srcDstStride,
        Ipp32s dir)
    {
        Ipp32s bitDepthLuma = 8;
        Ipp32s tcIdx = Clip3(0, 53, edge->qp + 2 * (edge->strength - 1) + edge->tcOffset);
        Ipp32s bIdx = Clip3(0, 51, edge->qp + edge->betaOffset);
        Ipp32s tc =  tcTable[tcIdx] * (1 << (bitDepthLuma - 8));
        Ipp32s beta = betaTable[bIdx] * (1 << (bitDepthLuma - 8));
        Ipp32s sideThreshhold = (beta + (beta >> 1)) >> 3;
        Ipp32s offset, strDstStep;
        Ipp32s i;

        if (dir == VERT_FILT)
        {
            offset = 1;
            strDstStep = srcDstStride;
        }
        else
        {
            offset = srcDstStride;
            strDstStep = 1;
        }

        {
            Ipp32s dp0 = abs(srcDst[0*strDstStep-3*offset] - 2 * srcDst[0*strDstStep-2*offset] + srcDst[0*strDstStep-1*offset]);
            Ipp32s dp3 = abs(srcDst[3*strDstStep-3*offset] - 2 * srcDst[3*strDstStep-2*offset] + srcDst[3*strDstStep-1*offset]);
            Ipp32s dq0 = abs(srcDst[0*strDstStep+0*offset] - 2 * srcDst[0*strDstStep+1*offset] + srcDst[0*strDstStep+2*offset]);
            Ipp32s dq3 = abs(srcDst[3*strDstStep+0*offset] - 2 * srcDst[3*strDstStep+1*offset] + srcDst[3*strDstStep+2*offset]);
            Ipp32s d0 = dp0 + dq0;
            Ipp32s d3 = dp3 + dq3;
            Ipp32s dq = dq0 + dq3;
            Ipp32s dp = dp0 + dp3;
            Ipp32s d = d0 + d3;

            if (d < beta)
            {
                Ipp32s ds0 = abs(srcDst[0*strDstStep-4*offset] - srcDst[0*strDstStep-1*offset]) +
                    abs(srcDst[0*strDstStep+3*offset] - srcDst[0*strDstStep+0*offset]);
                Ipp32s ds3 = abs(srcDst[3*strDstStep-4*offset] - srcDst[3*strDstStep-1*offset]) +
                    abs(srcDst[3*strDstStep+3*offset] - srcDst[3*strDstStep+0*offset]);
                Ipp32s dm0 = abs(srcDst[0*strDstStep-1*offset] - srcDst[0*strDstStep+0*offset]);
                Ipp32s dm3 = abs(srcDst[3*strDstStep-1*offset] - srcDst[3*strDstStep+0*offset]);
                bool strongFiltering = false;

                if ((ds0 < (beta >> 3)) && (2 * d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) &&
                    (ds3 < (beta >> 3)) && (2 * d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
                {
                    strongFiltering = true;
                }

                for (i = 0; i < 4; i++)
                {
                    Ipp32s p0 = srcDst[-1*offset];
                    Ipp32s p1 = srcDst[-2*offset];
                    Ipp32s p2 = srcDst[-3*offset];
                    Ipp32s q0 = srcDst[0*offset];
                    Ipp32s q1 = srcDst[1*offset];
                    Ipp32s q2 = srcDst[2*offset];
                    Ipp32s tmp;

                    if (strongFiltering)
                    {
                        if (edge->deblockP)
                        {
                            Ipp32s p3 = srcDst[-4*offset];
                            tmp = p1 + p0 + q0;
                            srcDst[-1*offset] = (Ipp8u)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                            srcDst[-2*offset] = (Ipp8u)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                            srcDst[-3*offset] = (Ipp8u)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                        }

                        if (edge->deblockQ)
                        {
                            Ipp32s q3 = srcDst[3*offset];

                            tmp = q1 + q0 + p0;
                            srcDst[0*offset] = (Ipp8u)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                            srcDst[1*offset] = (Ipp8u)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                            srcDst[2*offset] = (Ipp8u)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                        }
                    }
                    else
                    {
                        Ipp32s delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                        if (abs(delta) < tc * 10)
                        {
                            delta = Clip3(-tc, tc, delta);

                            if (edge->deblockP)
                            {
                                srcDst[-1*offset] = (Ipp8u)(Clip3(0, 255, (p0 + delta)));

                                if (dp < sideThreshhold)
                                {
                                    tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                                    srcDst[-2*offset] = (Ipp8u)(Clip3(0, 255, (p1 + tmp)));
                                }
                            }

                            if (edge->deblockQ)
                            {
                                srcDst[0] = (Ipp8u)(Clip3(0, 255, (q0 - delta)));

                                if (dq < sideThreshhold)
                                {
                                    tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                                    srcDst[1*offset] = (Ipp8u)(Clip3(0, 255, (q1 + tmp)));
                                }
                            }
                        }
                    }

                    srcDst += strDstStep;
                }
            }
        }

    } // void h265_FilterEdgeLuma_8u_I(...)

}; // namespace MFX_HEVC_COMMON

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
