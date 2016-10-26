//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

// ML: OPT: significant overhead if not inlined (ICC does not honor implied 'inline' with -Qipo)
// ML: OPT: TODO: Make sure compiler recognizes saturation idiom for vectorization when used
#if 1
#define Clip3( m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? \
                                                  (m_Min) : \
                                                ( (m_Value) > (m_Max) ? \
                                                              (m_Max) : \
                                                              (m_Value) ) )
#endif

namespace MFX_HEVC_PP
{
    enum FilterType
    {
        VERT_FILT = 0,
        HOR_FILT = 1,
    };

    static const Ipp32s tcTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
        3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
        14, 16, 18, 20, 22, 24
    };

    static const Ipp32s betaTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
        26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
        58, 60, 62, 64
    };

    template <int bitDepth, typename PixType>
    static Ipp32s h265_FilterEdgeLuma_Kernel(H265EdgeData *edge, PixType *srcDst, Ipp32s srcDstStride, Ipp32s dir)
    {
        Ipp32s tcIdx, bIdx, tc, beta, sideThreshhold;
        Ipp32s i;
        Ipp32s dp0, dp3, dq0, dq3, d0, d3, dq, dp, d;
        Ipp32s ds0, ds3, dm0, dm3, delta;
        Ipp32s p0, p1, p2, p3, q0, q1, q2, q3, tmp;
        Ipp32s strongFiltering = 2; /* 2 = no filtering, only for counting statistics */
        PixType *r[8];

        Ipp32s max_value = (1 << bitDepth) - 1;
        Ipp32s bitDepthScale = 1 << (bitDepth - 8);

        tcIdx = Clip3(0, 53, edge->qp + 2 * (edge->strength - 1) + edge->tcOffset);
        bIdx = Clip3(0, 51, edge->qp + edge->betaOffset);
        tc =  tcTable[tcIdx]*bitDepthScale;
        beta = betaTable[bIdx]*bitDepthScale;
        sideThreshhold = (beta + (beta >> 1)) >> 3;

        if (dir == VERT_FILT) {
            r[0] = srcDst + 0*srcDstStride - 4;
            r[1] = r[0] + srcDstStride;
            r[2] = r[1] + srcDstStride;
            r[3] = r[2] + srcDstStride;

            dp0 = abs(r[0][1] - 2*r[0][2] + r[0][3]);
            dq0 = abs(r[0][4] - 2*r[0][5] + r[0][6]);

            dp3 = abs(r[3][1] - 2*r[3][2] + r[3][3]);
            dq3 = abs(r[3][4] - 2*r[3][5] + r[3][6]);

            d0 = dp0 + dq0;
            d3 = dp3 + dq3;
            d = d0 + d3;

            if (d >= beta)
                return strongFiltering;

            dq = dq0 + dq3;
            dp = dp0 + dp3;

            /* since this is abs, can reverse the subtraction */
            ds0  = abs(r[0][0] - r[0][3]);
            ds0 += abs(r[0][4] - r[0][7]);

            ds3  = abs(r[3][0] - r[3][3]);
            ds3 += abs(r[3][4] - r[3][7]);

            dm0  = abs(r[0][4] - r[0][3]);
            dm3  = abs(r[3][4] - r[3][3]);

            strongFiltering = 0;
            if ((ds0 < (beta >> 3)) && (2*d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) && (ds3 < (beta >> 3)) && (2*d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
                strongFiltering = 1;

            if (strongFiltering) {
                for (i = 0; i < 4; i++) {
                    p0 = srcDst[-1];
                    p1 = srcDst[-2];
                    p2 = srcDst[-3];
                    q0 = srcDst[0];
                    q1 = srcDst[1];
                    q2 = srcDst[2];

                    if (edge->deblockP)
                    {
                        p3 = srcDst[-4];
                        tmp = p1 + p0 + q0;
                        srcDst[-1] = (PixType)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                        srcDst[-2] = (PixType)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                        srcDst[-3] = (PixType)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                    }

                    if (edge->deblockQ)
                    {
                        q3 = srcDst[3];

                        tmp = q1 + q0 + p0;
                        srcDst[0] = (PixType)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                        srcDst[1] = (PixType)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                        srcDst[2] = (PixType)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                    }
                    srcDst += srcDstStride;
                }
            } else {
                for (i = 0; i < 4; i++) {
                    p0 = srcDst[-1];
                    p1 = srcDst[-2];
                    p2 = srcDst[-3];
                    q0 = srcDst[0];
                    q1 = srcDst[1];
                    q2 = srcDst[2];

                    delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                    if (abs(delta) < tc * 10)
                    {
                        delta = Clip3(-tc, tc, delta);

                        if (edge->deblockP)
                        {
                            srcDst[-1] = (PixType)(Clip3(0, max_value, (p0 + delta)));

                            if (dp < sideThreshhold)
                            {
                                tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                                srcDst[-2] = (PixType)(Clip3(0, max_value, (p1 + tmp)));
                            }
                        }

                        if (edge->deblockQ)
                        {
                            srcDst[0] = (PixType)(Clip3(0, max_value, (q0 - delta)));

                            if (dq < sideThreshhold)
                            {
                                tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                                srcDst[1] = (PixType)(Clip3(0, max_value, (q1 + tmp)));
                            }
                        }
                    }

                    srcDst += srcDstStride;
                }
            }
        } else {
            r[0] = srcDst - 4*srcDstStride;
            r[1] = r[0] + srcDstStride;
            r[2] = r[1] + srcDstStride;
            r[3] = r[2] + srcDstStride;
            r[4] = r[3] + srcDstStride;
            r[5] = r[4] + srcDstStride;
            r[6] = r[5] + srcDstStride;
            r[7] = r[6] + srcDstStride;

            dp0 = abs(r[1][0] - 2*r[2][0] + r[3][0]);
            dq0 = abs(r[4][0] - 2*r[5][0] + r[6][0]);

            dp3 = abs(r[1][3] - 2*r[2][3] + r[3][3]);
            dq3 = abs(r[4][3] - 2*r[5][3] + r[6][3]);

            d0 = dp0 + dq0;
            d3 = dp3 + dq3;

            dq = dq0 + dq3;
            dp = dp0 + dp3;

            d = d0 + d3;

            if (d >= beta)
                return strongFiltering;

            /* since this is abs, can reverse the subtraction */
            ds0  = abs(r[0][0] - r[3][0]);
            ds0 += abs(r[4][0] - r[7][0]);

            ds3  = abs(r[0][3] - r[3][3]);
            ds3 += abs(r[4][3] - r[7][3]);

            dm0  = abs(r[4][0] - r[3][0]);
            dm3  = abs(r[4][3] - r[3][3]);

            strongFiltering = 0;
            if ((ds0 < (beta >> 3)) && (2 * d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) && (ds3 < (beta >> 3)) && (2 * d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
                strongFiltering = 1;

            if (strongFiltering) {
                for (i = 0; i < 4; i++)
                {
                    p0 = srcDst[-1*srcDstStride];
                    p1 = srcDst[-2*srcDstStride];
                    p2 = srcDst[-3*srcDstStride];
                    q0 = srcDst[0*srcDstStride];
                    q1 = srcDst[1*srcDstStride];
                    q2 = srcDst[2*srcDstStride];

                    if (edge->deblockP)
                    {
                        p3 = srcDst[-4*srcDstStride];
                        tmp = p1 + p0 + q0;
                        srcDst[-1*srcDstStride] = (PixType)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                        srcDst[-2*srcDstStride] = (PixType)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                        srcDst[-3*srcDstStride] = (PixType)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                    }

                    if (edge->deblockQ)
                    {
                        q3 = srcDst[3*srcDstStride];

                        tmp = q1 + q0 + p0;
                        srcDst[0*srcDstStride] = (PixType)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                        srcDst[1*srcDstStride] = (PixType)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                        srcDst[2*srcDstStride] = (PixType)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                    }
                    srcDst += 1;
                }
            } else {
                for (i = 0; i < 4; i++)
                {
                    p0 = srcDst[-1*srcDstStride];
                    p1 = srcDst[-2*srcDstStride];
                    p2 = srcDst[-3*srcDstStride];
                    q0 = srcDst[0*srcDstStride];
                    q1 = srcDst[1*srcDstStride];
                    q2 = srcDst[2*srcDstStride];
                    delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                    if (abs(delta) < tc * 10)
                    {
                        delta = Clip3(-tc, tc, delta);

                        if (edge->deblockP)
                        {
                            srcDst[-1*srcDstStride] = (PixType)(Clip3(0, max_value, (p0 + delta)));

                            if (dp < sideThreshhold)
                            {
                                tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                                srcDst[-2*srcDstStride] = (PixType)(Clip3(0, max_value, (p1 + tmp)));
                            }
                        }

                        if (edge->deblockQ)
                        {
                            srcDst[0] = (PixType)(Clip3(0, max_value, (q0 - delta)));

                            if (dq < sideThreshhold)
                            {
                                tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                                srcDst[1*srcDstStride] = (PixType)(Clip3(0, max_value, (q1 + tmp)));
                            }
                        }
                    }
                    srcDst += 1;
                }
            }
        }

        return strongFiltering;

    } // Ipp32s h265_FilterEdgeLuma_8u_I_C(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir)

    template <int bitDepth, typename PixType>
    static void h265_FilterEdgeChroma_Plane_Kernel(H265EdgeData *edge, PixType *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp)
    {
        Ipp32s max_value = (1 << bitDepth) - 1;
        Ipp32s qp = chromaQp;
        Ipp32s tcIdx = Clip3(0, 53, qp + 2 * (edge->strength - 1) + edge->tcOffset);
        Ipp32s tc =  tcTable[tcIdx] * (1 << (bitDepth - 8));
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

        for (i = 0; i < 4; i++)
        {
            Ipp32s p0 = srcDst[-1*offset];
            Ipp32s p1 = srcDst[-2*offset];
            Ipp32s q0 = srcDst[0*offset];
            Ipp32s q1 = srcDst[1*offset];
            Ipp32s delta = ((((q0 - p0) << 2) + p1 - q1 + 4) >> 3);

            delta = Clip3(-tc, tc, delta);

            if (edge->deblockP)
            {
                srcDst[-offset] = (PixType)(Clip3(0, max_value, (p0 + delta)));
            }

            if (edge->deblockQ)
            {
                srcDst[0] = (PixType)(Clip3(0, max_value, (q0 - delta)));
            }

            srcDst += strDstStep;
        }

    } // void h265_FilterEdgeChroma_Plane_8u_I(H265EdgeData *edge, PixType *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp)

    template <int bitDepth, typename PixType>
    static void h265_FilterEdgeChroma_Interleaved_Kernel(H265EdgeData *edge, PixType *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr)
    {
        //Ipp32s qpCb = QpUV(edge->qp + chromaCbQpOffset);
        Ipp32s qpCb = chromaQpCb;

        //Ipp32s qpCr = QpUV(edge->qp + chromaCrQpOffset);
        Ipp32s qpCr = chromaQpCr;

        Ipp32s tcIdxCb = Clip3(0, 53, qpCb + 2 * (edge->strength - 1) + edge->tcOffset);
        Ipp32s tcIdxCr = Clip3(0, 53, qpCr + 2 * (edge->strength - 1) + edge->tcOffset);
        Ipp32s tcCb =  tcTable[tcIdxCb] * (1 << (bitDepth - 8));
        Ipp32s tcCr =  tcTable[tcIdxCr] * (1 << (bitDepth - 8));
        Ipp32s offset, strDstStep;
        Ipp32s i;

        Ipp32s max_value = (1 << bitDepth) - 1;

        if (dir == VERT_FILT)
        {
            offset = 2;
            strDstStep = srcDstStride;
        }
        else
        {
            offset = srcDstStride;
            strDstStep = 2;
        }

        for (i = 0; i < 4; i++)
        {
            Ipp32s p0Cb = srcDst[-1*offset];
            Ipp32s p1Cb = srcDst[-2*offset];
            Ipp32s q0Cb = srcDst[0*offset];
            Ipp32s q1Cb = srcDst[1*offset];
            Ipp32s deltaCb = ((((q0Cb - p0Cb) << 2) + p1Cb - q1Cb + 4) >> 3);
            deltaCb = Clip3(-tcCb, tcCb, deltaCb);

            Ipp32s p0Cr = srcDst[-1*offset + 1];
            Ipp32s p1Cr = srcDst[-2*offset + 1];
            Ipp32s q0Cr = srcDst[0*offset + 1];
            Ipp32s q1Cr = srcDst[1*offset + 1];
            Ipp32s deltaCr = ((((q0Cr - p0Cr) << 2) + p1Cr - q1Cr + 4) >> 3);
            deltaCr = Clip3(-tcCr, tcCr, deltaCr);

            if (edge->deblockP)
            {
                srcDst[-offset] = (PixType)(Clip3(0, max_value, (p0Cb + deltaCb)));
                srcDst[-offset + 1] = (PixType)(Clip3(0, max_value, (p0Cr + deltaCr)));
            }

            if (edge->deblockQ)
            {
                srcDst[0] = (PixType)(Clip3(0, max_value, (q0Cb - deltaCb)));
                srcDst[1] = (PixType)(Clip3(0, max_value, (q0Cr - deltaCr)));
            }

            srcDst += strDstStep;
        }
    }

    Ipp32s MAKE_NAME(h265_FilterEdgeLuma_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir)
    {
        return h265_FilterEdgeLuma_Kernel<8, Ipp8u>(edge, srcDst, srcDstStride, dir);
    }

    Ipp32s MAKE_NAME(h265_FilterEdgeLuma_16u_I)(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u bit_depth)
    {
        if (bit_depth == 9)
            return h265_FilterEdgeLuma_Kernel< 9, Ipp16u>(edge, srcDst, srcDstStride, dir);
        else if (bit_depth == 10)
            return h265_FilterEdgeLuma_Kernel<10, Ipp16u>(edge, srcDst, srcDstStride, dir);
        else if (bit_depth == 8)
            return h265_FilterEdgeLuma_Kernel<8, Ipp16u>(edge, srcDst, srcDstStride, dir);
        else if (bit_depth == 11)
            return h265_FilterEdgeLuma_Kernel<11, Ipp16u>(edge, srcDst, srcDstStride, dir);
        else if (bit_depth == 12)
            return h265_FilterEdgeLuma_Kernel<12, Ipp16u>(edge, srcDst, srcDstStride, dir);
        else
            return -1;    /* unsupported */
    }

    void MAKE_NAME(h265_FilterEdgeChroma_Interleaved_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr)
    {
        h265_FilterEdgeChroma_Interleaved_Kernel<8, Ipp8u>(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
    }

    void MAKE_NAME(h265_FilterEdgeChroma_Interleaved_16u_I)(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u bit_depth)
    {
        if (bit_depth == 9)
            h265_FilterEdgeChroma_Interleaved_Kernel< 9, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
        else if (bit_depth == 10)
            h265_FilterEdgeChroma_Interleaved_Kernel<10, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
        else if (bit_depth == 8)
            h265_FilterEdgeChroma_Interleaved_Kernel<8, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
        else if (bit_depth == 11)
            h265_FilterEdgeChroma_Interleaved_Kernel<11, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
        else if (bit_depth == 12)
            h265_FilterEdgeChroma_Interleaved_Kernel<12, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
    }

    void MAKE_NAME(h265_FilterEdgeChroma_Plane_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp)
    {
        h265_FilterEdgeChroma_Plane_Kernel<8, Ipp8u>(edge, srcDst, srcDstStride, dir, chromaQp);
    }

    void MAKE_NAME(h265_FilterEdgeChroma_Plane_16u_I)(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u bit_depth)
    {
        if (bit_depth == 9)
            h265_FilterEdgeChroma_Plane_Kernel< 9, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQp);
        else if (bit_depth == 10)
            h265_FilterEdgeChroma_Plane_Kernel<10, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQp);
        else if (bit_depth == 8)
            h265_FilterEdgeChroma_Plane_Kernel<8, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQp);
        else if (bit_depth == 11)
            h265_FilterEdgeChroma_Plane_Kernel<11, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQp);
        else if (bit_depth == 12)
            h265_FilterEdgeChroma_Plane_Kernel<12, Ipp16u>(edge, srcDst, srcDstStride, dir, chromaQp);
    }

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
