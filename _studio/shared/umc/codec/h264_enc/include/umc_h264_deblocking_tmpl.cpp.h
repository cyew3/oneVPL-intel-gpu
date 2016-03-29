/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define H264CoreEncoderType H264ENC_MAKE_NAME(H264CoreEncoder)
#define DeblockingParametersType H264ENC_MAKE_NAME(DeblockingParameters)
#define H264SliceType H264ENC_MAKE_NAME(H264Slice)
#define H264EncoderFrameType H264ENC_MAKE_NAME(H264EncoderFrame)
#define EncoderRefPicListStructType H264ENC_MAKE_NAME(EncoderRefPicListStruct)

// implement array of IPP optimized luma deblocking functions
#if defined(_WIN32_WCE) && defined(_M_IX86) && defined(__stdcall)
#define _IPP_STDCALL_CDECL
#undef __stdcall
#endif // defined(_WIN32_WCE) && defined(_M_IX86) && defined(__stdcall)

#if PIXBITS == 8

static IppStatus H264ENC_MAKE_NAME(FilterDeblockingLuma_VerEdge)(
    Ipp8u *pSrcDst,
    Ipp32s srcDstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBeta,
    Ipp8u* pThresholds,
    Ipp8u* pBS)
{
    return(ippiFilterDeblockingLuma_VerEdge_H264_8u_C1IR(pSrcDst, srcDstStep, pAlpha, pBeta, pThresholds, pBS));
}

static IppStatus H264ENC_MAKE_NAME(FilterDeblockingLuma_HorEdge)(
    Ipp8u *pSrcDst,
    Ipp32s srcDstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBeta,
    Ipp8u* pThresholds,
    Ipp8u* pBS)
{
    return(ippiFilterDeblockingLuma_HorEdge_H264_8u_C1IR(pSrcDst, srcDstStep, pAlpha, pBeta, pThresholds, pBS));
}

IppStatus (*(EncoderIppLumaDeblocking_8u[])) (Ipp8u *, Ipp32s, Ipp8u *, Ipp8u *, Ipp8u *, Ipp8u *) =
{
    &(H264ENC_MAKE_NAME(FilterDeblockingLuma_VerEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingLuma_HorEdge))
};

inline IppStatus H264ENC_MAKE_NAME(encoderIppLumaDeblocking)(
    Ipp32s dir,
    Ipp8u* pSrcDst,
    Ipp32s srcdstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBetha,
    Ipp8u* pThresholds,
    Ipp8u* pBs,
    Ipp32s bitdepth)
{
    H264ENC_UNREFERENCED_PARAMETER(bitdepth);
    return(EncoderIppLumaDeblocking_8u[dir](pSrcDst, srcdstStep, pAlpha, pBetha, pThresholds, pBs));
}

#ifdef USE_NV12
#define ABS(a) (((a) < 0) ? (-(a)) : (a))
#define IPP_BAD_PTR3_RET(arg0, arg1, arg2) \
    if ((0 == arg0) || (0 == arg1) || (0 == arg2)) \
{ \
    return ippStsNullPtrErr; \
}
#define IPP_BAD_PTR2_RET(arg0, arg1) \
    if ((0 == arg0) || (0 == arg1)) \
{ \
    return ippStsNullPtrErr; \
}
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define ClipDelta(x, limit) min(limit, max(x,-limit))
Ipp8u ClampVal(Ipp32s value)
{
    if (256 > (Ipp32u) value)
    {
        return (Ipp8u) value;
    }
    else if (0 > value)
    {
        return 0;
    }
    else
    {
        return 255;
    }

} /* Ipp8u ClampVal(Ipp32s value) */

#ifndef imp_ippiFilterDeblockingMergedChroma422_VerEdge_H264_8u_C1IR
#define imp_ippiFilterDeblockingMergedChroma422_VerEdge_H264_8u_C1IR

IppStatus __STDCALL ippiFilterDeblockingMergedChroma422_VerEdge_H264_8u_C1IR(Ipp8u *pSrcDst,
                                                                   Ipp32u srcdstStep,
                                                                   const Ipp8u *pAlpha,
                                                                   const Ipp8u *pBeta,
                                                                   const Ipp8u *pThresholds,
                                                                   const Ipp8u *pBs)
{
    Ipp32s i, j;
    Ipp8u *pEdge;
    const Ipp8u *pStrength = pBs;
    const Ipp8u *pClip = pThresholds;
    Ipp32s iFiltPel;

    /* check error(s) */
    IPP_BAD_PTR3_RET(pSrcDst, pAlpha, pBeta);
    IPP_BAD_PTR2_RET(pThresholds, pBs);

    /* cycle over edges */
    for (j = 0; j < 2; j += 1)
    {
        Ipp32u alpha, beta;

        /* get the alpha & beta */
        alpha = pAlpha[j];
        beta =  pBeta [j];

#if defined(_IPP_ARCH) && \
    (_IPP_ARCH == _IPP_ARCH_IA64 || _IPP_ARCH == _IPP_ARCH_XSC)
        if (pStrength[0] == 0 &&
            pStrength[1] == 0 &&
            pStrength[2] == 0 &&
            pStrength[3] == 0)
        {
            continue;
        }
#else
        if (0 == (*(Ipp32u *) pStrength))
        {
            continue;
        }
#endif

        /* cycle over pixels */
        for (i = 0; i < 16; i += 1)
        {
            Ipp32u strength;
            Ipp32u plane;

            /* set the edge. we set the edge for every iteration
            to avoid complex conditions */
            pEdge = pSrcDst + i * srcdstStep + j * (4 * 2);

            /* Filter this bit position? */
            strength = pStrength[i >> 2];
            if (0 == strength)
            {
                i += 3;
                continue;
            }

            /* filter is dependent upon pixel activity on each side of edge */
            for (plane = 0; plane <= 1; plane += 1)
            {
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
                    pEdge[-1 * 2 + plane] = (Ipp8u) ((Ipp32s) (2 * p1 + p0 + q1 + 2) >> 2);
                    pEdge[0 * 2 + plane] = (Ipp8u) ((Ipp32s) (2 * q1 + q0 + p1 + 2) >> 2);
                }
                /* do weak deblocking */
                else
                {
                    Ipp32s clip;
                    Ipp32s delta;

                    clip = pClip[i >> 1] + 1;

                    /* p0, q0 */
                    delta = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3;
                    if (delta)
                    {
                        delta = ClipDelta(delta, clip);
                        iFiltPel = p0 + delta;
                        pEdge[-1 * 2 + plane] = ClampVal(iFiltPel);
                        iFiltPel = q0 - delta;
                        pEdge[0 * 2 + plane] = ClampVal(iFiltPel);
                    }
                }
            }
        }

        /* advance pointers */
        pClip += 4;
        pStrength += 8;
    }

    return ippStsNoErr;

} /* IPPFUN(IppStatus, ippiFilterDeblockingMergedChroma422_VerEdge_H264_8u_C1IR, (Ipp8u *pSrcDst, */

#endif /* imp_ippiFilterDeblockingMergedChroma422_VerEdge_H264_8u_C1IR */

#ifndef imp_ippiFilterDeblockingMergedChroma422_HorEdge_H264_8u_C1I
#define imp_ippiFilterDeblockingMergedChroma422_HorEdge_H264_8u_C1I

IppStatus __STDCALL ippiFilterDeblockingMergedChroma422_HorEdge_H264_8u_C1IR(Ipp8u *pSrcDst,
                                                                   Ipp32u srcdstStep,
                                                                   const Ipp8u *pAlpha,
                                                                   const Ipp8u *pBeta,
                                                                   const Ipp8u *pThresholds,
                                                                   const Ipp8u *pBs)
{
    Ipp32s i, j;
    Ipp8u *pEdge;
    const Ipp8u *pStrength = pBs;
    const Ipp8u *pClip = pThresholds;
    Ipp32s iFiltPel;

    /* check error(s) */
    IPP_BAD_PTR3_RET(pSrcDst, pAlpha, pBeta);
    IPP_BAD_PTR2_RET(pThresholds, pBs);

    /* cycle over edges */
    for (j = 0; j < 4; j += 1)
    {
        Ipp32u alpha, beta;

        /* get the alpha & beta */
        alpha = pAlpha[(j + 3) / 4];
        beta =  pBeta [(j + 3) / 4];

#if defined(_IPP_ARCH) && \
    (_IPP_ARCH == _IPP_ARCH_IA64 || _IPP_ARCH == _IPP_ARCH_XSC)
        if (pStrength[0] == 0 &&
            pStrength[1] == 0 &&
            pStrength[2] == 0 &&
            pStrength[3] == 0)
        {
            continue;
        }
#else
        if (0 == (*(Ipp32u *) pStrength))
        {
            continue;
        }
#endif

        /* cycle over pixels */
        for (i = 0; i < 8; i += 1)
        {
            Ipp32u strength;
            Ipp32u plane;

            /* set the edge. we set the edge for every iteration
            to avoid complex conditions */
            pEdge = pSrcDst + i * 2 + j * (4 * srcdstStep);

            /* Filter this bit position? */
            strength = pStrength[i >> 1];
            if (0 == strength)
            {
                i += 1;
                continue;
            }

            /* filter is dependent upon pixel activity on each side of edge */
            for (plane = 0; plane <= 1; plane += 1)
            {
                Ipp32u absDiff;
                Ipp32s p0, p1;
                Ipp32s q0, q1;

                p0 = pEdge[-1 * srcdstStep + plane];
                q0 = pEdge[0 * srcdstStep + plane];
                absDiff = ABS(p0 - q0);
                if (absDiff >= alpha)
                {
                    continue;
                }

                p1  = pEdge[-2 * srcdstStep + plane];
                absDiff = ABS(p1 - p0);
                if (absDiff >= beta)
                {
                    continue;
                }

                q1  = pEdge[1 * srcdstStep + plane];
                absDiff = ABS(q1 - q0);
                if (absDiff >= beta)
                {
                    continue;
                }

                /* do strong deblocking */
                if (4 == strength)
                {
                    pEdge[-1 * srcdstStep + plane] = (Ipp8u) ((Ipp32s) (2 * p1 + p0 + q1 + 2) >> 2);
                    pEdge[0 * srcdstStep + plane] = (Ipp8u) ((Ipp32s) (2 * q1 + q0 + p1 + 2) >> 2);
                }
                /* do weak deblocking */
                else
                {
                    Ipp32s clip;
                    Ipp32s delta;

                    clip = pClip[i >> 1] + 1;

                    /* p0, q0 */
                    delta = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3;
                    if (delta)
                    {
                        delta = ClipDelta(delta, clip);
                        iFiltPel = p0 + delta;
                        pEdge[-1 * srcdstStep + plane] = ClampVal(iFiltPel);
                        iFiltPel = q0 - delta;
                        pEdge[0 * srcdstStep + plane] = ClampVal(iFiltPel);
                    }
                }
            }
        }

        /* advance pointers */
        pClip += 4;
        pStrength += 4;
    }

    return ippStsNoErr;

} /* IPPFUN(IppStatus, ippiFilterDeblockingMergedChroma422_HorEdge_H264_8u_C1IR, (Ipp8u *pSrcDst, */

#endif /* imp_ippiFilterDeblockingMergedChroma422_HorEdge_H264_8u_C1IR */
#endif // USE_NV12

static IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma_VerEdge)(
    Ipp8u* pSrcDst,
    Ipp32s srcdstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBeta,
    Ipp8u* pThresholds,
    Ipp8u* pBS)
{
    return ippiFilterDeblockingChroma_VerEdge_H264_8u_C1IR(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS);
}

static IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma_HorEdge)(
    Ipp8u* pSrcDst,
    Ipp32s srcdstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBeta,
    Ipp8u* pThresholds,
    Ipp8u* pBS)
{
    return ippiFilterDeblockingChroma_HorEdge_H264_8u_C1IR(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS);
}

static IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma422_VerEdge)(
    Ipp8u* pSrcDst,
    Ipp32s srcdstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBeta,
    Ipp8u* pThresholds,
    Ipp8u* pBS)
{
    CONVERT_TO_8U(2, 8);
    return ippiFilterDeblockingChroma422VerEdge_H264_8u_C1IR(&info);
}

static IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma422_HorEdge)(
    Ipp8u* pSrcDst,
    Ipp32s srcdstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBeta,
    Ipp8u* pThresholds,
    Ipp8u* pBS)
{
    CONVERT_TO_8U(2, 16);
    return ippiFilterDeblockingChroma422HorEdge_H264_8u_C1IR(&info);
}

// implement array of IPP optimized chroma deblocking functions
IppStatus (*(EncoderIppChromaDeblocking_8u[])) (Ipp8u *, Ipp32s, Ipp8u *, Ipp8u *, Ipp8u *, Ipp8u *) =
{
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma_VerEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma_HorEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma422_VerEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma422_HorEdge))
};

#ifdef USE_NV12
// implement array of IPP optimized chroma deblocking functions
IppStatus (__STDCALL *(EncoderIppChromaDeblocking_NV12_8u[])) (Ipp8u *, Ipp32u, const Ipp8u *, const Ipp8u *, const Ipp8u *, const Ipp8u *) =
{
    &(ippiFilterDeblockingChroma_VerEdge_H264_8u_C2IR),
    &(ippiFilterDeblockingChroma_HorEdge_H264_8u_C2IR),
    &(ippiFilterDeblockingMergedChroma422_VerEdge_H264_8u_C1IR),
    &(ippiFilterDeblockingMergedChroma422_HorEdge_H264_8u_C1IR)
};

inline IppStatus H264ENC_MAKE_NAME(encoderIppChromaDeblocking)(
    Ipp32s dir,
    Ipp8u* pSrcDst,
    Ipp32s srcdstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBetha,
    Ipp8u* pThresholds,
    Ipp8u* pBs,
    Ipp32s bitdepth)
{
    H264ENC_UNREFERENCED_PARAMETER(bitdepth);
    return(EncoderIppChromaDeblocking_NV12_8u[dir](pSrcDst, srcdstStep, pAlpha, pBetha, pThresholds, pBs));
}
#else // USE_NV12
inline IppStatus H264ENC_MAKE_NAME(encoderIppChromaDeblocking)(
    Ipp32s dir,
    Ipp8u* pSrcDst,
    Ipp32s srcdstStep,
    Ipp8u* pAlpha,
    Ipp8u* pBetha,
    Ipp8u* pThresholds,
    Ipp8u* pBs,
    Ipp32s bitdepth)
{
    H264ENC_UNREFERENCED_PARAMETER(bitdepth);
    return(EncoderIppChromaDeblocking_8u[dir](pSrcDst, srcdstStep, pAlpha, pBetha, pThresholds, pBs));
}
#endif // USE_NV12

#elif PIXBITS == 16

IppStatus H264ENC_MAKE_NAME(FilterDeblockingLuma_VerEdge)(
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBeta,
    Ipp8u*  pThresholds,
    Ipp8u*  pBS,
    Ipp32s bit_depth)
{
    CONVERT_TO_16U(2, 16);
    return ippiFilterDeblockingLumaVerEdge_H264_16u_C1IR(&info);
}

IppStatus H264ENC_MAKE_NAME(FilterDeblockingLuma_HorEdge)(
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBeta,
    Ipp8u*  pThresholds,
    Ipp8u*  pBS,
    Ipp32s bit_depth)
{
    CONVERT_TO_16U(2, 16);
    return ippiFilterDeblockingLumaHorEdge_H264_16u_C1IR(&info);
}

IppStatus (*(EncoderIppLumaDeblocking_16s[])) (Ipp16u *, Ipp32s, Ipp8u *, Ipp8u *, Ipp8u *, Ipp8u *, Ipp32s bitdepth) =
{
    &(H264ENC_MAKE_NAME(FilterDeblockingLuma_VerEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingLuma_HorEdge))
};

inline IppStatus H264ENC_MAKE_NAME(encoderIppLumaDeblocking)(
    Ipp32s  dir,
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBetha,
    Ipp8u*  pThresholds,
    Ipp8u*  pBs,
    Ipp32s  bitdepth)
{
    return(EncoderIppLumaDeblocking_16s[dir](pSrcDst, srcdstStep, pAlpha, pBetha, pThresholds, pBs, bitdepth));
}

IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma_VerEdge)(
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBeta,
    Ipp8u*  pThresholds,
    Ipp8u*  pBS,
    Ipp32s  bit_depth)
{
    CONVERT_TO_16U(2, 8);
    return ippiFilterDeblockingChromaVerEdge_H264_16u_C1IR(&info);
}

IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma_HorEdge)(
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBeta,
    Ipp8u*  pThresholds,
    Ipp8u*  pBS,
    Ipp32s  bit_depth)
{
    CONVERT_TO_16U(2, 8);
    return ippiFilterDeblockingChromaHorEdge_H264_16u_C1IR(&info);
}

IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma422_VerEdge)(
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBeta,
    Ipp8u*  pThresholds,
    Ipp8u*  pBS,
    Ipp32s  bit_depth)
{
    CONVERT_TO_16U(2, 16);
    return ippiFilterDeblockingChroma422VerEdge_H264_16u_C1IR(&info);
}

IppStatus H264ENC_MAKE_NAME(FilterDeblockingChroma422_HorEdge)(
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBeta,
    Ipp8u*  pThresholds,
    Ipp8u*  pBS,
    Ipp32s  bit_depth)
{
    CONVERT_TO_16U(2, 16);
    return ippiFilterDeblockingChroma422HorEdge_H264_16u_C1IR(&info);//return DeblockChroma422_hor_16u(&info);
}

IppStatus (*(EncoderIppChromaDeblocking_16s[])) (Ipp16u *, Ipp32s, Ipp8u *, Ipp8u *, Ipp8u *, Ipp8u *, Ipp32s bitdepth) =
{
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma_VerEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma_HorEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma422_VerEdge)),
    &(H264ENC_MAKE_NAME(FilterDeblockingChroma422_HorEdge))
};

inline IppStatus H264ENC_MAKE_NAME(encoderIppChromaDeblocking)(
    Ipp32s  dir,
    Ipp16u* pSrcDst,
    Ipp32s  srcdstStep,
    Ipp8u*  pAlpha,
    Ipp8u*  pBetha,
    Ipp8u*  pThresholds,
    Ipp8u*  pBs,
    Ipp32s  bitdepth)
{
    return(EncoderIppChromaDeblocking_16s[dir](pSrcDst, srcdstStep, pAlpha, pBetha, pThresholds, pBs, bitdepth));
}

#endif //PIXBITS

#if defined(_IPP_STDCALL_CDECL)
#undef _IPP_STDCALL_CDECL
#define __stdcall __cdecl
#endif // defined(_IPP_STDCALL_CDECL)

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockSlice)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u uFirstMB,
    Ipp32u unumMBs)
{
    const H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CoreEncoder_DeblockingFunction pDeblocking = NULL;

    // no filtering edges of this slice
    if (DEBLOCK_FILTER_OFF ==
        (core_enc->m_deblocking_IL ? core_enc->m_Slices[0].m_disable_inter_layer_deblocking_filter_idc :
        curr_slice->m_disable_deblocking_filter_idc))
        return;

    // MBAFF case
    if (core_enc->m_SliceHeader.MbaffFrameFlag)
    {
        // select optimized deblocking function
        switch (curr_slice->m_slice_type)
        {
        case INTRASLICE:
            pDeblocking = &H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockISliceMBAFF);
            break;

        case PREDSLICE:
            pDeblocking = &H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockPSliceMBAFF);
            break;

        case BPREDSLICE:
            pDeblocking = &H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockBSliceMBAFF);
            break;

        default:
            pDeblocking = NULL;
            break;
        }
    }
    // non-MBAFF case
    else
    {
        // select optimized deblocking function
        switch (curr_slice->m_slice_type)
        {
        case INTRASLICE:
            pDeblocking = &H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockISlice);
            break;

        case PREDSLICE:
            pDeblocking = &H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockPSlice);
            break;

        case BPREDSLICE:
            pDeblocking = &H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockBSlice);
            break;

        default:
            pDeblocking = NULL;
            break;
        }
    }

    if (NULL == pDeblocking)
        return;

    // perform deblocking process on every macroblock
    Ipp32u i;
    for (i = uFirstMB; i < uFirstMB + unumMBs; i++)
        (*pDeblocking)(state, i);
}

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockISlice)(
    void* state,
    Ipp32u MBAddr)
{
    __align(16)
    DeblockingParametersType params;

    // prepare deblocking parameters
    params.nMBAddr = MBAddr;
    H264ENC_MAKE_NAME(H264CoreEncoder_ResetDeblockingVariables)(state, &params);
    if (!params.isNeedToDeblock) return;
    H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersISlice)(state, &params);
    H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersStrengths)(state, &params);

    // perform deblocking
    //!!!Chroma must be called first because luma clears strength for 8x8 transform.
    if (params.deblockChroma)
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(state, VERTICAL_DEBLOCKING, &params);
        H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(state, HORIZONTAL_DEBLOCKING, &params);
    }
    H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(state, VERTICAL_DEBLOCKING, &params);
    H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(state, HORIZONTAL_DEBLOCKING, &params);
}

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockPSlice)(
    void* state,
    Ipp32u MBAddr)
{
    __align(16)
    DeblockingParametersType params;

    // prepare deblocking parameters
    params.nMBAddr = MBAddr;
    H264ENC_MAKE_NAME(H264CoreEncoder_ResetDeblockingVariables)(state, &params);
    if (!params.isNeedToDeblock) return;
    H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice)(state, &params);
    H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersStrengths)(state, &params);

    // perform deblocking
    //!!!Chroma must be called first because luma clears strength for 8x8 transform.
    if (params.deblockChroma)
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(state, VERTICAL_DEBLOCKING, &params);
        H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(state, HORIZONTAL_DEBLOCKING, &params);
    }
    H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(state, VERTICAL_DEBLOCKING, &params);
    H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(state, HORIZONTAL_DEBLOCKING, &params);

}

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockBSlice)(
    void* state,
    Ipp32u MBAddr)
{
    __align(16)
    DeblockingParametersType params;

    // prepare deblocking parameters
    params.nMBAddr = MBAddr;
    H264ENC_MAKE_NAME(H264CoreEncoder_ResetDeblockingVariables)(state, &params);
    if (!params.isNeedToDeblock) return;
    H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice)(state, &params);
    H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersStrengths)(state, &params);

    // perform deblocking
    //!!!Chroma must be called first because luma clears strength for 8x8 transform.
    if (params.deblockChroma)
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(state, VERTICAL_DEBLOCKING, &params);
        H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(state, HORIZONTAL_DEBLOCKING, &params);
    }
    H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(state, VERTICAL_DEBLOCKING, &params);
    H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(state, HORIZONTAL_DEBLOCKING, &params);
}

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;

    if (core_enc->m_deblocking_IL) core_enc = core_enc->ref_enc;

    PIXTYPE *pY = pParams->pY;
    Ipp32s pic_pitchPixels = pParams->pitchPixels;
    Ipp32s MBAddr = pParams->nMBAddr;
    Ipp8u Clipping[16];
    Ipp8u Alpha[2];
    Ipp8u Beta[2];
    Ipp32s AlphaC0Offset = pParams->nAlphaC0Offset;
    Ipp32s BetaOffset = pParams->nBetaOffset;
    Ipp32s pmq_QP = core_enc->m_mbinfo.mbs[MBAddr].QP_deblock;

    //
    // luma deblocking
    //

    if (pParams->DeblockingFlag[dir])
    {
        Ipp8u *pClipTab;
        Ipp32s QP;
        Ipp32s index;
        Ipp8u *pStrength = pParams->Strength[dir];

        //
        // correct strengths for high profile
        //
        if (pGetMB8x8TSPackFlag(core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr))
        {
            SetEdgeStrength(pStrength + 4, 0); // 16 bits????
            SetEdgeStrength(pStrength + 12, 0);
        }

        if (pParams->ExternalEdgeFlag[dir])
        {
            Ipp32s pmp_QP;

            // get neighbour block QP
            pmp_QP = core_enc->m_mbinfo.mbs[pParams->nNeighbour[dir]].QP_deblock;

            // luma variables
            QP = (pmp_QP + pmq_QP + 1) >> 1 ;

            // external edge variables
            index = IClip(0, 51, QP + BetaOffset);
            Beta[0] = H264ENC_MAKE_NAME(getEncoderBethaTable)(index);

            index = IClip(0, 51, QP + AlphaC0Offset);
            Alpha[0] = H264ENC_MAKE_NAME(getEncoderAlphaTable)(index);
            pClipTab = H264ENC_MAKE_NAME(getEncoderClipTab)(index);

            // create clipping values
            Clipping[0] = pClipTab[pStrength[0]];
            Clipping[1] = pClipTab[pStrength[1]];
            Clipping[2] = pClipTab[pStrength[2]];
            Clipping[3] = pClipTab[pStrength[3]];

        }
        else
            Alpha[0] = 0;

        // internal edge variables
        QP = pmq_QP;

        index = IClip(0, 51, QP + BetaOffset);
        Beta[1] = H264ENC_MAKE_NAME(getEncoderBethaTable)(index);

        index = IClip(0, 51, QP + AlphaC0Offset);
        Alpha[1] = H264ENC_MAKE_NAME(getEncoderAlphaTable)(index);
        pClipTab = H264ENC_MAKE_NAME(getEncoderClipTab)(index);

        // create clipping values
        {
            Ipp32u edge;

            for (edge = 1;edge < 4;edge += 1)
            {
                if (*((Ipp32u *) (pStrength + edge * 4)))
                {
                    // create clipping values
                    Clipping[edge * 4 + 0] = pClipTab[pStrength[edge * 4 + 0]];
                    Clipping[edge * 4 + 1] = pClipTab[pStrength[edge * 4 + 1]];
                    Clipping[edge * 4 + 2] = pClipTab[pStrength[edge * 4 + 2]];
                    Clipping[edge * 4 + 3] = pClipTab[pStrength[edge * 4 + 3]];
                }
            }
        }

        // perform deblocking
        H264ENC_MAKE_NAME(encoderIppLumaDeblocking)(
            dir,
            pY,
            pic_pitchPixels,
            Alpha,
            Beta,
            Clipping,
            pStrength,
            core_enc->m_PicParamSet->bit_depth_luma);
    }

}

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;

    if (core_enc->m_deblocking_IL) core_enc = core_enc->ref_enc;

    // do no deblocking of 4:0:0 format
    if (core_enc->m_PicParamSet->chroma_format_idc)
    {
        PIXTYPE *pU = pParams->pU;
#ifndef USE_NV12
        PIXTYPE *pV = pParams->pV;
#endif // !USE_NV12
        Ipp32s pic_pitchPixels = pParams->pitchPixels;
        Ipp32s MBAddr = pParams->nMBAddr;
        Ipp8u Clipping[16];
        Ipp8u Alpha[2];
        Ipp8u Beta[2];
        Ipp32s AlphaC0Offset = pParams->nAlphaC0Offset;
        Ipp32s BetaOffset = pParams->nBetaOffset;
        Ipp32s pmq_QP = core_enc->m_mbinfo.mbs[MBAddr].QP_deblock;

        //
        // chroma deblocking
        //

        if (pParams->DeblockingFlag[dir])

        {
            Ipp8u *pClipTab;
            Ipp32s QP;
            Ipp32s index;
            Ipp8u *pStrength = pParams->Strength[dir];
#ifndef USE_NV12
            Ipp32s nPlane;
            for (nPlane = 0; nPlane < 2; nPlane += 1)
            {
                if ((0 == nPlane) ||
                    (core_enc->m_PicParamSet->chroma_qp_index_offset !=
                     core_enc->m_PicParamSet->second_chroma_qp_index_offset))
                {
                    Ipp32s chroma_qp_offset = (0 == nPlane) ?
                        (core_enc->m_PicParamSet->chroma_qp_index_offset) :
                    (core_enc->m_PicParamSet->second_chroma_qp_index_offset);
#else // !USE_NV12
                    Ipp32s chroma_qp_offset = core_enc->m_PicParamSet->chroma_qp_index_offset;
#endif // !USE_NV12

                    if (pParams->ExternalEdgeFlag[dir])
                    {
                        Ipp32s pmp_QP;

                        // get left block QP
                        pmp_QP = core_enc->m_mbinfo.mbs[pParams->nNeighbour[dir]].QP_deblock;

                        // external edge variables
                        QP = (ENCODER_QP_SCALE_CR[IClip(0, 51, pmp_QP + chroma_qp_offset)] +
                            ENCODER_QP_SCALE_CR[IClip(0, 51, pmq_QP + chroma_qp_offset)] + 1) >> 1;

                        index = IClip(0, 51, QP + BetaOffset);
                        Beta[0] = H264ENC_MAKE_NAME(getEncoderBethaTable)(index);

                        index = IClip(0, 51, QP + AlphaC0Offset);
                        Alpha[0] = H264ENC_MAKE_NAME(getEncoderAlphaTable)(index);
                        pClipTab = H264ENC_MAKE_NAME(getEncoderClipTab)(index);

                        // create clipping values
                        Clipping[0] = pClipTab[pStrength[0]];
                        Clipping[1] = pClipTab[pStrength[1]];
                        Clipping[2] = pClipTab[pStrength[2]];
                        Clipping[3] = pClipTab[pStrength[3]];
                    }
                    else
                        Alpha[0] = 0;

                    // internal edge variables
                    QP = ENCODER_QP_SCALE_CR[IClip(0, 51, pmq_QP + chroma_qp_offset)];
                    //QP = ENCODER_QP_SCALE_CR[getChromaQP(pmq_QP, chroma_qp_offset, core_enc->m_SeqParamSet->bit_depth_chroma)];

                    index = IClip(0, 51, QP + BetaOffset);
                    Beta[1] = H264ENC_MAKE_NAME(getEncoderBethaTable)(index);

                    index = IClip(0, 51, QP + AlphaC0Offset);
                    Alpha[1] = H264ENC_MAKE_NAME(getEncoderAlphaTable)(index);
                    pClipTab = H264ENC_MAKE_NAME(getEncoderClipTab)(index);


                    // create clipping values
                    if (core_enc->m_PicParamSet->chroma_format_idc == 2 && HORIZONTAL_DEBLOCKING == dir)
                    {
                        // create clipping values

                        Clipping[4] = (Ipp8u) (pClipTab[pStrength[4]]);
                        Clipping[5] = (Ipp8u) (pClipTab[pStrength[5]]);
                        Clipping[6] = (Ipp8u) (pClipTab[pStrength[6]]);
                        Clipping[7] = (Ipp8u) (pClipTab[pStrength[7]]);
                        Clipping[8] = (Ipp8u) (pClipTab[pStrength[8]]);
                        Clipping[9] = (Ipp8u) (pClipTab[pStrength[9]]);
                        Clipping[10] = (Ipp8u) (pClipTab[pStrength[10]]);
                        Clipping[11] = (Ipp8u) (pClipTab[pStrength[11]]);
                        Clipping[12] = (Ipp8u) (pClipTab[pStrength[12]]);
                        Clipping[13] = (Ipp8u) (pClipTab[pStrength[13]]);
                        Clipping[14] = (Ipp8u) (pClipTab[pStrength[14]]);
                        Clipping[15] = (Ipp8u) (pClipTab[pStrength[15]]);
                    }
                    else
                    {
                        // create clipping values
                        Clipping[4] = (Ipp8u) (pClipTab[pStrength[8]]);
                        Clipping[5] = (Ipp8u) (pClipTab[pStrength[9]]);
                        Clipping[6] = (Ipp8u) (pClipTab[pStrength[10]]);
                        Clipping[7] = (Ipp8u) (pClipTab[pStrength[11]]);
                    }

                // perform deblocking chroma component
#ifndef USE_NV12
                }
                H264ENC_MAKE_NAME(encoderIppChromaDeblocking)(
                    dir + (core_enc->m_PicParamSet->chroma_format_idc & 0x2),
                    (0 == nPlane) ? (pU) : (pV),
                    pic_pitchPixels,
                    Alpha,
                    Beta,
                    Clipping,
                    pStrength,
                    core_enc->m_SeqParamSet->bit_depth_chroma);
            }
#else // !USE_NV12
                H264ENC_MAKE_NAME(encoderIppChromaDeblocking)(
                    dir + (core_enc->m_PicParamSet->chroma_format_idc & 0x2),
                    pU,
                    pic_pitchPixels,
                    Alpha,
                    Beta,
                    Clipping,
                    pStrength,
                    core_enc->m_SeqParamSet->bit_depth_chroma);
#endif // !USE_NV12
        }
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_ResetDeblockingVariables)(
    void* state,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    PIXTYPE *pY, *pU, *pV;
    Ipp32u offset;
    Ipp32s MBYAdjust = 0;
    Ipp32u mbXOffset, mbYOffset;
    Ipp32s pic_pitchPixels;
    Ipp32u MBAddr = pParams->nMBAddr;
    Ipp32u nCurrMB_X, nCurrMB_Y;
    bool deblocking_IL = core_enc->m_deblocking_IL;
    H264CoreEncoderType* core_enc_debl = deblocking_IL ? core_enc->ref_enc : core_enc;
    Ipp32s disable_deblocking_filter_idc;

    if (deblocking_IL && !IS_INTRA_MBTYPE(core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr].mbtype)) {
        pParams->isNeedToDeblock = 0;
        return;
    }

    pParams->is_profile_baseline_scalable = deblocking_IL ?
        core_enc->ref_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_BASELINE_PROFILE :
        core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_BASELINE_PROFILE;

    if (deblocking_IL) {
        H264SliceType *curr_slice = &core_enc->m_Slices[core_enc->m_pCurrentFrame->m_mbinfo.mbs[0].slice_id];
        disable_deblocking_filter_idc = curr_slice->m_disable_inter_layer_deblocking_filter_idc;
        // set slice's variables
        pParams->nAlphaC0Offset = curr_slice->m_inter_layer_slice_alpha_c0_offset_div2 << 1;
        pParams->nBetaOffset = curr_slice->m_inter_layer_slice_beta_offset_div2 << 1;
    } else {
        H264SliceType *curr_slice = &core_enc->m_Slices[core_enc->m_pCurrentFrame->m_mbinfo.mbs[MBAddr].slice_id];
        disable_deblocking_filter_idc = curr_slice->m_disable_deblocking_filter_idc;
        // set slice's variables
        pParams->nAlphaC0Offset = curr_slice->m_slice_alpha_c0_offset;
        pParams->nBetaOffset = curr_slice->m_slice_beta_offset;
    }

    // pParams->deblockEdgesStage2 will be equal to core_enc->m_deblocking_stage2 in this implementation
    pParams->deblockEdgesStage2 = core_enc->m_deblocking_stage2 ? 
        (disable_deblocking_filter_idc == DEBLOCK_FILTER_ON_2_PASS ||
        disable_deblocking_filter_idc == DEBLOCK_FILTER_ON_2_PASS_NO_CHROMA) : 0;

    pic_pitchPixels = core_enc_debl->m_pCurrentFrame->m_pitchPixels;

    // load planes
    if (deblocking_IL) {
        pY = core_enc->m_pYDeblockingILPlane;
        pU = core_enc->m_pUDeblockingILPlane;
        pV = core_enc->m_pVDeblockingILPlane;         
    } else {
        pY = core_enc->m_pReconstructFrame->m_pYPlane;
        pU = core_enc->m_pReconstructFrame->m_pUPlane;
        pV = core_enc->m_pReconstructFrame->m_pVPlane;
    }

    if (FRM_STRUCTURE > core_enc_debl->m_pCurrentFrame->m_PictureStructureForDec)
    {
        if(core_enc_debl->m_pCurrentFrame->m_bottom_field_flag[core_enc_debl->m_field_index])
        {
            pY += pic_pitchPixels;
            pU += pic_pitchPixels;
            pV += pic_pitchPixels;
        }
        if (core_enc_debl->m_field_index)
            MBYAdjust = core_enc_debl->m_HeightInMBs;
        pic_pitchPixels *= 2;
    }

    // prepare macroblock variables
    nCurrMB_X = (MBAddr % core_enc_debl->m_WidthInMBs);
    nCurrMB_Y = (MBAddr / core_enc_debl->m_WidthInMBs)- MBYAdjust;

    Ipp32s chromaShiftX;
    Ipp32s chromaShiftY;
    switch(core_enc_debl->m_PicParamSet->chroma_format_idc) {
        case 0: //MONOCHROME
            chromaShiftX = chromaShiftY = 0;
            break;
        case 2: //422
            chromaShiftX = 3;
            chromaShiftY = 4;
            break;
        case 3: //444
            chromaShiftX = chromaShiftY = 4;
            break;
        case 1: //420
        default:
#ifdef USE_NV12
            chromaShiftX = 4;
            chromaShiftY = 3;
#else
            chromaShiftX = chromaShiftY = 3;
#endif
    }
    mbXOffset = nCurrMB_X * 16;
    mbYOffset = nCurrMB_Y * 16;

    Ipp32s chromaXOffset = nCurrMB_X<<chromaShiftX;
    Ipp32s chromaYOffset = nCurrMB_Y<<chromaShiftY;
    // calc plane's offsets
    offset = mbXOffset + (mbYOffset * pic_pitchPixels);
    pY += offset;
    pU += chromaXOffset + chromaYOffset * pic_pitchPixels;
    pV += chromaXOffset + chromaYOffset * pic_pitchPixels;

    pParams->isNeedToDeblock = 1;
    pParams->deblockChroma = 1;

    if (disable_deblocking_filter_idc >= DEBLOCK_FILTER_ON_NO_CHROMA)
    {
        pParams->deblockChroma = 0;
    }

    // set external edge variables
    pParams->ExternalEdgeFlag[VERTICAL_DEBLOCKING] = (nCurrMB_X != 0);
    pParams->ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = (nCurrMB_Y != 0);

    if (pParams->deblockEdgesStage2)
    {
        pParams->isNeedToDeblock = 0;

        if (nCurrMB_X)
        {
            if (core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr].slice_id !=
                core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr - 1].slice_id) {
                    pParams->isNeedToDeblock = 1;
            } else {
                pParams->ExternalEdgeFlag[VERTICAL_DEBLOCKING] = 0;
            }
        }

        if (nCurrMB_Y)
        {
            if (core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr].slice_id !=
                core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr - core_enc_debl->m_WidthInMBs].slice_id) {
                    pParams->isNeedToDeblock = 1;
            } else {
                pParams->ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
            }
        }
    } else if (disable_deblocking_filter_idc >= DEBLOCK_FILTER_ON_NO_SLICE_EDGES &&
        disable_deblocking_filter_idc != DEBLOCK_FILTER_ON_NO_CHROMA) {
        if (core_enc->m_deblocking_stage2) {
            pParams->isNeedToDeblock = 0;
        } else {
            // don't filter at slice boundaries
            if (nCurrMB_X)
            {
                if (core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr].slice_id !=
                    core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr - 1].slice_id)
                    pParams->ExternalEdgeFlag[VERTICAL_DEBLOCKING] = 0;
            }

            if (nCurrMB_Y)
            {
                if (core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr].slice_id !=
                    core_enc_debl->m_pCurrentFrame->m_mbinfo.mbs[MBAddr - core_enc_debl->m_WidthInMBs].slice_id)
                    pParams->ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
            }
        }
    } else {
        if (core_enc->m_deblocking_stage2) {
            pParams->isNeedToDeblock = 0;
        }
    }

    // reset external edges strength
    SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING], 0);
    SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING], 0);

    // set neighbour addreses
    pParams->nNeighbour[VERTICAL_DEBLOCKING] = MBAddr - 1;
    pParams->nNeighbour[HORIZONTAL_DEBLOCKING] = MBAddr - core_enc_debl->m_WidthInMBs;

    // set deblocking flag(s)
    pParams->DeblockingFlag[VERTICAL_DEBLOCKING] = 0;
    pParams->DeblockingFlag[HORIZONTAL_DEBLOCKING] = 0;

    // save variables
    pParams->pY = pY;
    pParams->pU = pU;
    pParams->pV = pV;
    pParams->pitchPixels = pic_pitchPixels;
    pParams->nMaxMVector = (FRM_STRUCTURE > core_enc_debl->m_pCurrentFrame->m_PictureStructureForDec) ? (2) : (4);
    pParams->MBFieldCoded = (FRM_STRUCTURE > core_enc_debl->m_pCurrentFrame->m_PictureStructureForDec);
}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersStrengths)(
    void* state,
    DeblockingParametersType *pParams)
{
    H264ENC_UNREFERENCED_PARAMETER(state);

    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MBAddr = pParams->nMBAddr;

    bool m_spatial_resolution_change = core_enc->m_spatial_resolution_change;
    bool m_deblocking_IL = core_enc->m_deblocking_IL;

    if (m_deblocking_IL)
        m_spatial_resolution_change = core_enc->ref_enc->m_spatial_resolution_change;

    Ipp32s i, dir, mbtype, mbtype_n, cbp_coeffs, cbp_coeffs_n, cbp_residual, cbp_residual_n;
    Ipp32s field_cur;
    H264MacroblockGlobalInfo *mbs, *mbs_n;
    Ipp8u s;

    if (!m_spatial_resolution_change && !m_deblocking_IL) {
        H264ENC_MAKE_NAME(H264CoreEncoder_ResetInternalEdges)(pParams);
        return;
    }

    if (m_deblocking_IL) core_enc = core_enc->ref_enc;

    mbs = core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr;
    mbtype = mbs->mbtype;
    field_cur = pGetMBFieldDecodingFlag(core_enc->m_pCurrentFrame->m_mbinfo.mbs);

    for (dir = 0; dir < 2; dir ++) {
        cbp_coeffs = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_coeffs & 0xffff) << 1;
        cbp_residual = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_residual & 0xffff ) << 1;
        if (pParams->ExternalEdgeFlag[dir]) {
            mbs_n = core_enc->m_pCurrentFrame->m_mbinfo.mbs + pParams->nNeighbour[dir];
            mbtype_n = mbs_n->mbtype;
            cbp_coeffs_n = ((core_enc->m_mbinfo.mbs + pParams->nNeighbour[dir])->cbp_luma_coeffs & 0xffff) << 1;
            cbp_residual_n = ((core_enc->m_mbinfo.mbs + pParams->nNeighbour[dir])->cbp_luma_residual & 0xffff) << 1;
        } else {
            mbs_n = NULL;
            mbtype_n = cbp_coeffs_n = cbp_residual_n = 0;
        }

        for (i = 0; i < 16; i++) {
            s = 10;

            if (pGetMB8x8TSPackFlag(mbs) && (i & 4)) {
                pParams->Strength[dir][i] = 0;
                continue;
            }

            if (m_spatial_resolution_change) {
                if (i >= 4) {
                    if (pIS_INTRA_MBTYPE_BL(mbs)) {
                        s = (cbp_coeffs & ENCODER_INTERNAL_BLOCKS_MASK[dir][i-4]) ? 1 : 0;
                    }
                } else if (pParams->ExternalEdgeFlag[dir]) {
                    if (m_deblocking_IL && !IS_INTRA_MBTYPE(mbtype_n)) {
                        s = 0;
                    }
                    else if (mbs_n && pIS_INTRA_MBTYPE_BL(mbs) && pIS_INTRA_MBTYPE_BL(mbs_n)) {
                        s = ((cbp_coeffs & ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][i]) ||
                            (cbp_coeffs_n & ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][i]))
                            ? 1 : 0;
                    } else if (pIS_INTRA_MBTYPE_BL(mbs)) {
                        if (mbs_n && pIS_INTRA_MBTYPE_NOT_BL(mbs_n))
                            s = field_cur && dir ? 3 : 4;
                        else
                            s = (cbp_residual_n & ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][i])
                            ? 2 : 1;

                    } else if (mbs_n && pIS_INTRA_MBTYPE_BL(mbs_n)) {
                        if (pIS_INTRA_MBTYPE_NOT_BL(mbs))
                            s = field_cur && dir ? 3 : 4;
                        else
                            s = (cbp_residual & ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][i])
                            ? 2 : 1;
                    }
                }
            } else {
                if (m_deblocking_IL && pParams->ExternalEdgeFlag[dir] &&
                    !IS_INTRA_MBTYPE(mbtype_n) && i < 4 ) {
                        s = 0;
                }
            }
            if (s < 10)
                pParams->Strength[dir][i] = s;
        }
    }
    H264ENC_MAKE_NAME(H264CoreEncoder_ResetInternalEdges)(pParams);
}

void H264ENC_MAKE_NAME(H264CoreEncoder_ResetInternalEdges)(
    DeblockingParametersType *pParams)
{
    if (pParams->deblockEdgesStage2)
    {
        // skip all internal edges
        SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING] + 4, 0);
        SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING] + 8, 0);
        SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING] + 12, 0);
        SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 4, 0);
        SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 8, 0);
        SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 12, 0);
    }
} // void H264SegmentDecoder::ResetInternalEdges(DeblockingParameters *pParams)

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersISlice)(
    void* state,
    DeblockingParametersType *pParams)
{
    H264ENC_UNREFERENCED_PARAMETER(state);
    // set deblocking flag(s)
    pParams->DeblockingFlag[VERTICAL_DEBLOCKING] = 1;
    pParams->DeblockingFlag[HORIZONTAL_DEBLOCKING] = 1;

    // calculate strengths
    if (pParams->ExternalEdgeFlag[VERTICAL_DEBLOCKING])
    {
        // deblocking with strong deblocking of external edge
        SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING] + 0, 4);
    }

    SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING] + 4, 3);
    SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING] + 8, 3);
    SetEdgeStrength(pParams->Strength[VERTICAL_DEBLOCKING] + 12, 3);

    if (pParams->ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
    {
        if (pParams->MBFieldCoded)
        {
            // deblocking field macroblock with external edge
            SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 0, 3);
        }
        else
        {
            // deblocking with strong deblocking of external edge
            SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 0, 4);
        }
    }

    SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 4, 3);
    SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 8, 3);
    SetEdgeStrength(pParams->Strength[HORIZONTAL_DEBLOCKING] + 12, 3);
}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice)(
    void* state,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    if (core_enc->m_deblocking_IL) {
        core_enc = core_enc->ref_enc;
        state = (void*)core_enc;
    }

    Ipp32u MBAddr = pParams->nMBAddr;
    Ipp32u mbtype = (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->mbtype;

    // when this macroblock is intra coded
    if (IS_INTRA_MBTYPE(mbtype))
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersISlice)(state, pParams);
        return;
    }

    // try simplest function to prepare deblocking parameters
    switch (mbtype)
    {
        // when macroblock has type inter 16 on 16
    case MBTYPE_INTER:
    case MBTYPE_FORWARD:
    case MBTYPE_BACKWARD:
    case MBTYPE_BIDIR:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice16)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice16)(state, HORIZONTAL_DEBLOCKING, pParams);
        break;
/*
        // when macroblock has type inter 16 on 8
    case MBTYPE_INTER_16x8:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice8x16)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice16x8)(state, HORIZONTAL_DEBLOCKING, pParams);
        return;

        // when macroblock has type inter 8 on 16
    case MBTYPE_INTER_8x16:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice16x8)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice8x16)(state, HORIZONTAL_DEBLOCKING, pParams);
        return;
*/
    default:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4)(state, HORIZONTAL_DEBLOCKING, pParams);
        break;
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MBAddr = pParams->nMBAddr;
//fc    Ipp32u cbp_luma = (core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma;
    Ipp32u cbp_luma = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_residual & 0xffff) << 1;
    Ipp8u *pStrength = pParams->Strength[dir];
    Ipp32u *pDeblockingFlag = &(pParams->DeblockingFlag[dir]);

    if (pParams->is_profile_baseline_scalable)
        cbp_luma |= ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_coeffs & 0xffff) << 1;

    //
    // external edge
    //

    if (pParams->ExternalEdgeFlag[dir])
    {
        Ipp32u nNeighbour;

        // select neighbour addres
        nNeighbour = pParams->nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->mbtype))
        {
            Ipp32u idx;

            // select neighbour
//fc            H264MacroblockLocalInfo *pNeighbour = core_enc->m_mbinfo.mbs + nNeighbour;
            Ipp32u cbp_luma_nb = ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_residual & 0xffff) << 1;

            if (pParams->is_profile_baseline_scalable)
                cbp_luma_nb |= ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_coeffs & 0xffff) << 1;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32u blkQ, blkP;

                blkQ = ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp_luma & blkQ) ||
//fc                    (pNeighbour->cbp_luma & blkP))
                    (cbp_luma_nb & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32u nBlock, nNeighbourBlock;
                    Ipp32s iRefQ, iRefP;
                    Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                    {
                        nBlock = idx * 4;
                        nNeighbourBlock = idx * 4 + 3;
                    }
                    else
                    {
                        nBlock = idx;
                        nNeighbourBlock = idx + 12;
                    }

                    // field coded image
                    if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;
                        Ipp8s *pFields;

                        // select reference index for current block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                0)->m_Prediction;
                            iRefQ = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefQ = -1;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_Prediction;
                            iRefP = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefP = -1;
                    }
                    // frame coded image
                    else
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;

                        // select reference index for current block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                0)->m_RefPicList;
                            iRefQ = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefQ = -1;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            iRefP = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefP = -1;
                    }

                    VM_ASSERT((iRefP != -1) || (iRefQ != -1));

                    // when reference indexes are equal
                    if (iRefQ == iRefP)
                    {
                        H264MotionVector *pVectorQ, *pVectorP;

                        pVectorQ = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nBlock;
                        pVectorP = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;

                        // compare motion vectors
                        if ((4 <= abs(pVectorQ->mvx - pVectorP->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQ->mvy - pVectorP->mvy)))
                        {
                            pStrength[idx] = 1;
                            *pDeblockingFlag = 1;
                        }
                        else
                            pStrength[idx] = 0;
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (pParams->MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32u idx;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            Ipp32u blkQ;

            blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp_luma & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // compare motion vectors & reference indexes
            else
            {
                Ipp32u nBlock, nNeighbourBlock;
                Ipp32s iRefQ, iRefP;
                Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                // calc block and neighbour block number
                if (VERTICAL_DEBLOCKING == dir)
                {
                    nBlock = (idx & 3) * 4 + (idx >> 2);
                    nNeighbourBlock = nBlock - 1;
                }
                else
                {
                    nBlock = idx;
                    nNeighbourBlock = idx - 4;
                }

//                VM_ASSERT(-1 == core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nBlock]);
//                VM_ASSERT(-1 == core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nNeighbourBlock]);

                // field coded image
                if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;
                    Ipp8s *pFields;

                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_RefPicList;

                    // select reference fields number array
                    pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_Prediction;

                    // select reference field for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                    iRefQ = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));

                    // select reference field for previous block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nNeighbourBlock];
                    iRefP = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                }
                // frame coded image
                else
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;

                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_RefPicList;

                    // select reference index for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                    iRefQ = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);

                    // select reference index for previous block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nNeighbourBlock];
                    iRefP = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                }

                VM_ASSERT((iRefP != -1) || (iRefQ != -1));

                // when reference indexes are equal
                if (iRefQ == iRefP)
                {
                    H264MotionVector *pVectorQ, *pVectorP;

                    pVectorQ = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nBlock;
                    pVectorP = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nNeighbourBlock;

                    // compare motion vectors
                    if ((4 <= abs(pVectorQ->mvx - pVectorP->mvx)) ||
                        (nVectorDiffLimit <= abs(pVectorQ->mvy - pVectorP->mvy)))
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                    else
                        pStrength[idx] = 0;
                }
                // when reference indexes are different
                else
                {
                    pStrength[idx] = 1;
                    *pDeblockingFlag = 1;
                }
            }
        }
    }

}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice16)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MBAddr = pParams->nMBAddr;
//fc    Ipp32u cbp_luma = (core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma;
    Ipp32u cbp_luma = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_residual & 0xffff) << 1;
    Ipp8u *pStrength = pParams->Strength[dir];
    Ipp32u *pDeblockingFlag = &(pParams->DeblockingFlag[dir]);

    if (pParams->is_profile_baseline_scalable)
        cbp_luma |= ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_coeffs & 0xffff) << 1;

    //
    // external edge
    //
    if (pParams->ExternalEdgeFlag[dir])
    {
        Ipp32u nNeighbour;

        // select neighbour addres
        nNeighbour = pParams->nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->mbtype))
        {
            Ipp32u idx;
            Ipp32s iRefQ;
            H264MotionVector *pVectorQ;

            // load reference index & motion vector for current block
            {
                // field coded image
                if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;
                    Ipp8s *pFields;

                    // select reference index for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[0];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_RefPicList;
                        // select reference fields number array
                        pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_Prediction;
                        iRefQ = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                pRefPicList[index],
                                pFields[index]));
                    }
                    else
                        iRefQ = -1;
                }
                // frame coded image
                else
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;

                    // select reference index for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[0];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_RefPicList;
                        iRefQ = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            0);
                    }
                    else
                        iRefQ = -1;
                }

                pVectorQ = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors;
            }

            // select neighbour
//fc            H264MacroblockLocalInfo *pNeighbour = core_enc->m_mbinfo.mbs + nNeighbour;
            Ipp32u cbp_luma_nb = ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_residual & 0xffff) << 1;

            if (pParams->is_profile_baseline_scalable)
                cbp_luma_nb |= ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_coeffs & 0xffff) << 1;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32u blkQ, blkP;

                blkQ = ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp_luma & blkQ) ||
//fc                    (pNeighbour->cbp_luma & blkP))
                    (cbp_luma_nb & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32u nNeighbourBlock;
                    Ipp32s iRefP;
                    Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                        nNeighbourBlock = idx * 4 + 3;
                    else
                        nNeighbourBlock = idx + 12;

                    // field coded image
                    if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;
                        Ipp8s *pFields;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_Prediction;
                            iRefP = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefP = -1;
                    }
                    // frame coded image
                    else
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            iRefP = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefP = -1;
                    }

                    VM_ASSERT((iRefP != -1) || (iRefQ != -1));

                    // when reference indexes are equal
                    if (iRefQ == iRefP)
                    {
                        H264MotionVector *pVectorP;

                        pVectorP = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;

                        // compare motion vectors
                        if ((4 <= abs(pVectorQ->mvx - pVectorP->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQ->mvy - pVectorP->mvy)))
                        {
                            pStrength[idx] = 1;
                            *pDeblockingFlag = 1;
                        }
                        else
                            pStrength[idx] = 0;
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (pParams->MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32u idx;

        // reset all strengths
        SetEdgeStrength(pStrength + 4, 0);
        SetEdgeStrength(pStrength + 8, 0);
        SetEdgeStrength(pStrength + 12, 0);

        // set deblocking flag
        if (cbp_luma & 0x1fffe)
            *pDeblockingFlag = 1;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            Ipp32u blkQ;

            blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp_luma & blkQ)
                pStrength[idx] = 2;
        }
    }

}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice)(
    void* state,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    if (core_enc->m_deblocking_IL) {
        core_enc = core_enc->ref_enc;
        state = (void*)core_enc;
    }
    Ipp32u MBAddr = pParams->nMBAddr;
    Ipp32u mbtype = (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->mbtype;

    // when this macroblock is intra coded
    if (IS_INTRA_MBTYPE(mbtype))
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersISlice)(state, pParams);
        return;
    }

    // try simplest function to prepare deblocking parameters
    switch (mbtype)
    {
        // when macroblock has type inter 16 on 16
    case MBTYPE_INTER:
    case MBTYPE_FORWARD:
    case MBTYPE_BACKWARD:
    case MBTYPE_BIDIR:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16)(state, HORIZONTAL_DEBLOCKING, pParams);
        break;

        // when macroblock has type inter 16 on 8
    case MBTYPE_INTER_16x8:
    case MBTYPE_FWD_FWD_16x8:
    case MBTYPE_BWD_BWD_16x8:
    case MBTYPE_FWD_BWD_16x8:
    case MBTYPE_BWD_FWD_16x8:
    case MBTYPE_BIDIR_FWD_16x8:
    case MBTYPE_BIDIR_BWD_16x8:
    case MBTYPE_FWD_BIDIR_16x8:
    case MBTYPE_BWD_BIDIR_16x8:
    case MBTYPE_BIDIR_BIDIR_16x8:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice8x16)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16x8)(state, HORIZONTAL_DEBLOCKING, pParams);
        return;

        // when macroblock has type inter 8 on 16
    case MBTYPE_INTER_8x16:
    case MBTYPE_FWD_FWD_8x16:
    case MBTYPE_BWD_BWD_8x16:
    case MBTYPE_FWD_BWD_8x16:
    case MBTYPE_BWD_FWD_8x16:
    case MBTYPE_BIDIR_FWD_8x16:
    case MBTYPE_BIDIR_BWD_8x16:
    case MBTYPE_FWD_BIDIR_8x16:
    case MBTYPE_BWD_BIDIR_8x16:
    case MBTYPE_BIDIR_BIDIR_8x16:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16x8)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice8x16)(state, HORIZONTAL_DEBLOCKING, pParams);
        return;

    default:
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice4)(state, VERTICAL_DEBLOCKING, pParams);
        H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice4)(state, HORIZONTAL_DEBLOCKING, pParams);
        break;
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice4)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MBAddr = pParams->nMBAddr;
//fc    Ipp32u cbp_luma = (core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma;
    Ipp32u cbp_luma = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_residual & 0xffff) << 1;
    Ipp8u *pStrength = pParams->Strength[dir];
    Ipp32u *pDeblockingFlag = &(pParams->DeblockingFlag[dir]);

    if (pParams->is_profile_baseline_scalable)
        cbp_luma |= ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_coeffs & 0xffff) << 1;

    //
    // external edge
    //
    if (pParams->ExternalEdgeFlag[dir])
    {
        Ipp32u nNeighbour;

        // select neighbour addres
        nNeighbour = pParams->nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->mbtype))
        {
            Ipp32u idx;

            // select neighbour
//fc            H264MacroblockLocalInfo *pNeighbour = core_enc->m_mbinfo.mbs + nNeighbour;
            Ipp32u cbp_luma_nb = ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_residual & 0xffff) << 1;

            if (pParams->is_profile_baseline_scalable)
                cbp_luma_nb |= ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_coeffs & 0xffff) << 1;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32u blkQ, blkP;

                blkQ = ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp_luma & blkQ) ||
//fc                    (pNeighbour->cbp_luma & blkP))
                    (cbp_luma_nb & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32u nBlock, nNeighbourBlock;
                    Ipp32s iRefQFrw, iRefPFrw, iRefQBck, iRefPBck;
                    Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                    {
                        nBlock = idx * 4;
                        nNeighbourBlock = nBlock + 3;
                    }
                    else
                    {
                        nBlock = idx;
                        nNeighbourBlock = idx + 12;
                    }

                    // field coded image
                    if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;
                        Ipp8s *pFields;

                        // select reference index for current block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                0)->m_Prediction;
                            iRefQFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefQFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                1)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                1)->m_Prediction;
                            iRefQBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefQBck = -1;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_Prediction;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_Prediction;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPBck = -1;
                    }
                    // frame coded image
                    else
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;

                        // select reference index for current block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                0)->m_RefPicList;
                            iRefQFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefQFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                                1)->m_RefPicList;
                            iRefQBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefQBck = -1;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPBck = -1;
                    }

                    // when reference indexes are equal
                    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
                        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
                    {
                        // set initial value of strength
                        pStrength[idx] = 0;

                        // when forward and backward reference pictures of previous block are different
                        if (iRefPFrw != iRefPBck)
                        {
                            const H264MotionVector *pVectorQFrw, *pVectorQBck;
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select current block motion vectors
                            pVectorQFrw = (iRefQFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nBlock;
                            pVectorQBck = (iRefQBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + nBlock;

                            // select previous block motion vectors
                            if (iRefQFrw == iRefPFrw)
                            {
                                pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                            }
                            else
                            {
                                pVectorPFrw = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            }

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                        // when forward and backward reference pictures of previous block are equal
                        else
                        {
                            const H264MotionVector *pVectorQFrw, *pVectorQBck;
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select current block motion vectors
                            pVectorQFrw = (iRefQFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nBlock;
                            pVectorQBck = (iRefQBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + nBlock;

                            // select previous block motion vectors
                            pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                            {
                                if ((4 <= abs(pVectorQFrw->mvx - pVectorPBck->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPBck->mvy)) ||
                                    (4 <= abs(pVectorQBck->mvx - pVectorPFrw->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPFrw->mvy)))
                                {
                                    pStrength[idx] = 1;
                                    *pDeblockingFlag = 1;
                                }
                            }
                        }
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (pParams->MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32u idx;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            Ipp32u blkQ;

            blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp_luma & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // compare motion vectors & reference indexes
            else
            {
                Ipp32u nBlock, nNeighbourBlock;
                Ipp32s iRefQFrw, iRefQBck, iRefPFrw, iRefPBck;
                Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                // calc block and neighbour block number
                if (VERTICAL_DEBLOCKING == dir)
                {
                    nBlock = (idx & 3) * 4 + (idx >> 2);
                    nNeighbourBlock = nBlock - 1;
                }
                else
                {
                    nBlock = idx;
                    nNeighbourBlock = idx - 4;
                }

                // field coded image
                if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;
                    Ipp8s *pFields;

                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_RefPicList;
                    // select reference fields number array
                    pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_Prediction;

                    // select forward reference index for blocks
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                    iRefQFrw = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nNeighbourBlock];
                    iRefPFrw = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));

                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        1)->m_RefPicList;
                    // select reference fields number array
                    pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        1)->m_Prediction;

                    // select backward reference index for blocks
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nBlock];
                    iRefQBck = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nNeighbourBlock];
                    iRefPBck = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));

                }
                // frame coded image
                else
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;

                    // select forward reference pictures list
                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_RefPicList;
                    // select forward reference index for block(s)
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nBlock];
                    iRefQFrw = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[nNeighbourBlock];
                    iRefPFrw = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);

                    // select backward reference pictures list
                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        1)->m_RefPicList;
                    // select backward reference index for block(s)
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nBlock];
                    iRefQBck = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[nNeighbourBlock];
                    iRefPBck = (index < 0) ?
                            (-1) :
                            H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                }

                // when reference indexes are equal
                if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
                    ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
                {
                    // set initial value of strength
                    pStrength[idx] = 0;

                    // when forward and backward reference pictures of previous block are different
                    if (iRefPFrw != iRefPBck)
                    {
                        const H264MotionVector *pVectorQFrw, *pVectorQBck;
                        const H264MotionVector *pVectorPFrw, *pVectorPBck;

                        // select current block motion vectors
                        pVectorQFrw = (iRefQFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nBlock;
                        pVectorQBck = (iRefQBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + nBlock;

                        // select previous block motion vectors
                        if (iRefQFrw == iRefPFrw)
                        {
                            pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nNeighbourBlock;
                            pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + nNeighbourBlock;
                        }
                        else
                        {
                            pVectorPFrw = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + nNeighbourBlock;
                            pVectorPBck = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nNeighbourBlock;
                        }

                        // compare motion vectors
                        if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                            (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                        {
                            pStrength[idx] = 1;
                            *pDeblockingFlag = 1;
                        }
                    }
                    // when forward and backward reference pictures of previous block are equal
                    else
                    {
                        const H264MotionVector *pVectorQFrw, *pVectorQBck;
                        const H264MotionVector *pVectorPFrw, *pVectorPBck;

                        // select current block motion vectors
                        pVectorQFrw = (iRefQFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nBlock;
                        pVectorQBck = (iRefQBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + nBlock;

                        // select previous block motion vectors
                        pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + nNeighbourBlock;
                        pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + nNeighbourBlock;

                        // compare motion vectors
                        if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                            (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                        {
                            if ((4 <= abs(pVectorQFrw->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPBck->mvy)) ||
                                (4 <= abs(pVectorQBck->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPFrw->mvy)))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                    }
                }
                // when reference indexes are different
                else
                {
                    pStrength[idx] = 1;
                    *pDeblockingFlag = 1;
                }
            }
        }
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MBAddr = pParams->nMBAddr;
//fc    Ipp32u cbp_luma = (core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma;
    Ipp32u cbp_luma = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_residual & 0xffff) << 1;
    Ipp8u *pStrength = pParams->Strength[dir];
    Ipp32u *pDeblockingFlag = &(pParams->DeblockingFlag[dir]);

    if (pParams->is_profile_baseline_scalable)
        cbp_luma |= ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_coeffs & 0xffff) << 1;

    //
    // external edge
    //
    if (pParams->ExternalEdgeFlag[dir])
    {
        Ipp32u nNeighbour;

        // select neighbour addres
        nNeighbour = pParams->nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->mbtype))
        {
            Ipp32u idx;
            Ipp32s iRefQFrw, iRefQBck;
            const H264MotionVector *pVectorQFrw, *pVectorQBck;

            // load reference indexes for current block
            {
                // field coded image
                if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;
                    Ipp8s *pFields;

                    // select reference index for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[0];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_RefPicList;
                        // select reference fields number array
                        pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_Prediction;
                        iRefQFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                pRefPicList[index],
                                pFields[index]));
                    }
                    else
                        iRefQFrw = -1;
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[0];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            1)->m_RefPicList;
                        // select reference fields number array
                        pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            1)->m_Prediction;
                        iRefQBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                pRefPicList[index],
                                pFields[index]));
                    }
                    else
                        iRefQBck = -1;
                }
                // frame coded image
                else
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;

                    // select reference index for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[0];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_RefPicList;
                        iRefQFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            0);
                    }
                    else
                        iRefQFrw = -1;
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[0];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            1)->m_RefPicList;
                        iRefQBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            0);
                    }
                    else
                        iRefQBck = -1;
                }

                // select current block motion vectors
                pVectorQFrw = (iRefQFrw < 0)? &null_mv : core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors;
                pVectorQBck = (iRefQBck < 0)? &null_mv : core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors;
            }

            // select neighbour
//fc            H264MacroblockLocalInfo *pNeighbour = core_enc->m_mbinfo.mbs + nNeighbour;
            Ipp32u cbp_luma_nb = ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_residual & 0xffff) << 1;

            if (pParams->is_profile_baseline_scalable)
                cbp_luma_nb |= ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_coeffs & 0xffff) << 1;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32u blkQ, blkP;

                blkQ = ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp_luma & blkQ) ||
//fc                    (pNeighbour->cbp_luma & blkP))
                    (cbp_luma_nb & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32u nNeighbourBlock;
                    Ipp32s iRefPFrw, iRefPBck;
                    Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                        nNeighbourBlock = idx * 4 + 3;
                    else
                        nNeighbourBlock = idx + 12;

                    // field coded image
                    if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;
                        Ipp8s *pFields;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_Prediction;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_Prediction;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPBck = -1;
                    }
                    // frame coded image
                    else
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPBck = -1;
                    }

                    // when reference indexes are equal
                    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
                        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
                    {
                        // set initial value of strength
                        pStrength[idx] = 0;

                        // when forward and backward reference pictures of previous block are different
                        if (iRefPFrw != iRefPBck)
                        {
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select previous block motion vectors
                            if (iRefQFrw == iRefPFrw)
                            {
                                pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                            }
                            else
                            {
                                pVectorPFrw = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            }

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                        // when forward and backward reference pictures of previous block are equal
                        else
                        {
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select previous block motion vectors
                            pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                            {
                                if ((4 <= abs(pVectorQFrw->mvx - pVectorPBck->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPBck->mvy)) ||
                                    (4 <= abs(pVectorQBck->mvx - pVectorPFrw->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPFrw->mvy)))
                                {
                                    pStrength[idx] = 1;
                                    *pDeblockingFlag = 1;
                                }
                            }
                        }
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (pParams->MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32u idx;

        // reset all strengths
        SetEdgeStrength(pStrength + 4, 0);
        SetEdgeStrength(pStrength + 8, 0);
        SetEdgeStrength(pStrength + 12, 0);

        // set deblocking flag
        if (cbp_luma & 0x1fffe)
            *pDeblockingFlag = 1;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            Ipp32u blkQ;

            blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp_luma & blkQ)
                pStrength[idx] = 2;
        }
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16x8)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MBAddr = pParams->nMBAddr;
//fc    Ipp32u cbp_luma = (core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma;
    Ipp32u cbp_luma = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_residual & 0xffff) << 1;
    Ipp8u *pStrength = pParams->Strength[dir];
    Ipp32u *pDeblockingFlag = &(pParams->DeblockingFlag[dir]);
    Ipp32s iRefQFrw, iRefQBck;
    const H264MotionVector *pVectorQFrw, *pVectorQBck;

    if (pParams->is_profile_baseline_scalable)
        cbp_luma |= ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_coeffs & 0xffff) << 1;

    //
    // external edge
    //

    // load reference indexes & motion vector for first half of current block
    {
        // field coded image
        if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
        {
            H264EncoderFrameType **pRefPicList;
            Ipp32s index;
            Ipp8s *pFields;

            // select reference index for current block
            index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[0];
            if (0 <= index)
            {
                pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                    core_enc->m_pCurrentFrame,
                    (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                    0)->m_RefPicList;
                // select reference fields number array
                pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                    core_enc->m_pCurrentFrame,
                    (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                    0)->m_Prediction;
                iRefQFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                    pRefPicList[index],
                    H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                        pRefPicList[index],
                        pFields[index]));
            }
            else
                iRefQFrw = -1;
            index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[0];
            if (0 <= index)
            {
                pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                    core_enc->m_pCurrentFrame,
                    (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                    1)->m_RefPicList;
                // select reference fields number array
                pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                    core_enc->m_pCurrentFrame,
                    (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                    1)->m_Prediction;
                iRefQBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                    pRefPicList[index],
                    H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                        pRefPicList[index],
                        pFields[index]));
            }
            else
                iRefQBck = -1;
        }
        // frame coded image
        else
        {
            H264EncoderFrameType **pRefPicList;
            Ipp32s index;

            // select reference index for current block
            index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[0];
            if (0 <= index)
            {
                pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                    core_enc->m_pCurrentFrame,
                    (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                    0)->m_RefPicList;
                iRefQFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                    pRefPicList[index],
                    0);
            }
            else
                iRefQFrw = -1;
            index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[0];
            if (0 <= index)
            {
                pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                    core_enc->m_pCurrentFrame,
                    (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                    1)->m_RefPicList;
                iRefQBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                    pRefPicList[index],
                    0);
            }
            else
                iRefQBck = -1;
        }

        // select current block motion vectors
        pVectorQFrw = (iRefQFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors;
        pVectorQBck = (iRefQBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors;
    }

    // prepare deblocking parameter for external edge
    if (pParams->ExternalEdgeFlag[dir])
    {
        Ipp32u nNeighbour;

        // select neighbour addres
        nNeighbour = pParams->nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->mbtype))
        {
            Ipp32u idx;

            // select neighbour
//fc            H264MacroblockLocalInfo *pNeighbour = core_enc->m_mbinfo.mbs + nNeighbour;
            Ipp32u cbp_luma_nb = ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_residual & 0xffff) << 1;

            if (pParams->is_profile_baseline_scalable)
                cbp_luma_nb |= ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_coeffs & 0xffff) << 1;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32u blkQ, blkP;

                blkQ = ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp_luma & blkQ) ||
//fc                    (pNeighbour->cbp_luma & blkP))
                    (cbp_luma_nb & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32u nNeighbourBlock;
                    Ipp32s iRefPFrw, iRefPBck;
                    Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                        nNeighbourBlock = idx * 4 + 3;
                    else
                        nNeighbourBlock = idx + 12;

                    // field coded image
                    if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;
                        Ipp8s *pFields;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_Prediction;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_Prediction;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPBck = -1;
                    }
                    // frame coded image
                    else
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPBck = -1;
                    }

                    // when reference indexes are equal
                    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
                        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
                    {
                        // set initial value of strength
                        pStrength[idx] = 0;

                        // when forward and backward reference pictures of previous block are different
                        if (iRefPFrw != iRefPBck)
                        {
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select previous block motion vectors
                            if (iRefQFrw == iRefPFrw)
                            {
                                pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                            }
                            else
                            {
                                pVectorPFrw = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            }

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                        // when forward and backward reference pictures of previous block are equal
                        else
                        {
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select previous block motion vectors
                            pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPBck->mvy)))
                            {
                                if ((4 <= abs(pVectorQFrw->mvx - pVectorPBck->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorPBck->mvy)) ||
                                    (4 <= abs(pVectorQBck->mvx - pVectorPFrw->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorPFrw->mvy)))
                                {
                                    pStrength[idx] = 1;
                                    *pDeblockingFlag = 1;
                                }
                            }
                        }
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (pParams->MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32u idx;

        // cicle of edge(s)
        for (idx = 4;idx < 8;idx += 1)
        {
            Ipp32u blkQ;

            blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp_luma & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // we haven't to compare motion vectors  - they are equal
            else
                pStrength[idx] = 0;
        }

        // load reference indexes & motion vector for second half of current block
        {
            Ipp32s iRefQFrw2, iRefQBck2;
            Ipp32u nStrength;
            Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

            // load reference indexes for current block

            // field coded image
            if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
            {
                H264EncoderFrameType **pRefPicList;
                Ipp32s index;
                Ipp8s *pFields;

                // select reference index for current block
                index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[15];
                if (0 <= index)
                {
                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_RefPicList;
                    // select reference fields number array
                    pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_Prediction;
                    iRefQFrw2 = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                        pRefPicList[index],
                        H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                            pRefPicList[index],
                            pFields[index]));
                }
                else
                    iRefQFrw2 = -1;
                index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[15];
                if (0 <= index)
                {
                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        1)->m_RefPicList;
                    // select reference fields number array
                    pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        1)->m_Prediction;
                    iRefQBck2 = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                        pRefPicList[index],
                        H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                            pRefPicList[index],
                            pFields[index]));
                }
                else
                    iRefQBck2 = -1;
            }
            // frame coded image
            else
            {
                H264EncoderFrameType **pRefPicList;
                Ipp32s index;

                // select reference index for current block
                index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[15];
                if (0 <= index)
                {
                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        0)->m_RefPicList;
                    iRefQFrw2 = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                        pRefPicList[index],
                        0);
                }
                else
                    iRefQFrw2 = -1;
                index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[15];
                if (0 <= index)
                {
                    pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                        core_enc->m_pCurrentFrame,
                        (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                        1)->m_RefPicList;
                    iRefQBck2 = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                        pRefPicList[index],
                        0);
                }
                else
                    iRefQBck2 = -1;
            }

            // when reference indexes are equal
            if (((iRefQFrw == iRefQFrw2) && (iRefQBck == iRefQBck2)) ||
                ((iRefQFrw == iRefQBck2) && (iRefQBck == iRefQFrw2)))
            {
                // set initial value of strength
                nStrength = 0;

                // when forward and backward reference pictures of previous block are different
                if (iRefQFrw2 != iRefQBck2)
                {
                    const H264MotionVector *pVectorQFrw2, *pVectorQBck2;

                    // select previous block motion vectors
                    if (iRefQFrw == iRefQFrw2)
                    {
                        pVectorQFrw2 = (iRefQFrw2 < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + 15;
                        pVectorQBck2 = (iRefQBck2 < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + 15;
                    }
                    else
                    {
                        pVectorQFrw2 = (iRefQBck2 < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + 15;
                        pVectorQBck2 = (iRefQFrw2 < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + 15;
                    }

                    // compare motion vectors
                    if ((4 <= abs(pVectorQFrw->mvx - pVectorQFrw2->mvx)) ||
                        (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorQFrw2->mvy)) ||
                        (4 <= abs(pVectorQBck->mvx - pVectorQBck2->mvx)) ||
                        (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorQBck2->mvy)))
                    {
                        nStrength = 1;
                        *pDeblockingFlag = 1;
                    }
                }
                // when forward and backward reference pictures of previous block are equal
                else
                {
                    const H264MotionVector *pVectorQFrw2, *pVectorQBck2;

                    // select block second motion vectors
                    pVectorQFrw2 = (iRefQFrw2 < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + 15;
                    pVectorQBck2 = (iRefQBck2 < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + 15;

                    // compare motion vectors
                    if ((4 <= abs(pVectorQFrw->mvx - pVectorQFrw2->mvx)) ||
                        (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorQFrw2->mvy)) ||
                        (4 <= abs(pVectorQBck->mvx - pVectorQBck2->mvx)) ||
                        (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorQBck2->mvy)))
                    {
                        if ((4 <= abs(pVectorQFrw->mvx - pVectorQBck2->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQFrw->mvy - pVectorQBck2->mvy)) ||
                            (4 <= abs(pVectorQBck->mvx - pVectorQFrw2->mvx)) ||
                            (nVectorDiffLimit <= abs(pVectorQBck->mvy - pVectorQFrw2->mvy)))
                        {
                            nStrength = 1;
                            *pDeblockingFlag = 1;
                        }
                    }
                }
            }
            // when reference indexes are different
            else
            {
                nStrength = 1;
                *pDeblockingFlag = 1;
            }

            // cicle of edge(s)
            for (idx = 8;idx < 12;idx += 1)
            {
                Ipp32u blkQ;

                blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

                if (cbp_luma & blkQ)
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // we have compared motion vectors
                else
                    pStrength[idx] = (Ipp8u) nStrength;
            }
        }

        // cicle of edge(s)
        for (idx = 12;idx < 16;idx += 1)
        {
            Ipp32u blkQ;

            blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp_luma & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // we haven't to compare motion vectors  - they are equal
            else
                pStrength[idx] = 0;
        }
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice8x16)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType *pParams)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MBAddr = pParams->nMBAddr;
//fc    Ipp32u cbp_luma = (core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma;
    Ipp32u cbp_luma = ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_residual & 0xffff) << 1;
    Ipp8u *pStrength = pParams->Strength[dir];
    Ipp32u *pDeblockingFlag = &(pParams->DeblockingFlag[dir]);

    if (pParams->is_profile_baseline_scalable)
        cbp_luma |= ((core_enc->m_mbinfo.mbs + MBAddr)->cbp_luma_coeffs & 0xffff) << 1;

    //
    // external edge
    //
    if (pParams->ExternalEdgeFlag[dir])
    {
        Ipp32u nNeighbour;

        // select neighbour addres
        nNeighbour = pParams->nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->mbtype))
        {
            Ipp32u idx;
            Ipp32s iRefQFrw[2], iRefQBck[2];
            const H264MotionVector *(pVectorQFrw[2]), *(pVectorQBck[2]);

            // in following calculations we avoided multiplication on 15
            // by using formulae a * 15 = a * 16 - a

            // load reference indexes for current block
            for (idx = 0;idx < 2;idx += 1)
            {
                // field coded image
                if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;
                    Ipp8s *pFields;

                    // select reference index for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[idx * 16 - idx];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_RefPicList;
                        // select reference fields number array
                        pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_Prediction;
                        iRefQFrw[idx] = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                pRefPicList[index],
                                pFields[index]));
                    }
                    else
                        iRefQFrw[idx] = -1;
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[idx * 16 - idx];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            1)->m_RefPicList;
                        // select reference fields number array
                        pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            1)->m_Prediction;
                        iRefQBck[idx] = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                pRefPicList[index],
                                pFields[index]));
                    }
                    else
                        iRefQBck[idx] = -1;
                }
                // frame coded image
                else
                {
                    H264EncoderFrameType **pRefPicList;
                    Ipp32s index;

                    // select reference index for current block
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][MBAddr].RefIdxs[idx * 16 - idx];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            0)->m_RefPicList;
                        iRefQFrw[idx] = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            0);
                    }
                    else
                        iRefQFrw[idx] = -1;
                    index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][MBAddr].RefIdxs[idx * 16 - idx];
                    if (0 <= index)
                    {
                        pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                            core_enc->m_pCurrentFrame,
                            (core_enc->m_pCurrentFrame->m_mbinfo.mbs + MBAddr)->slice_id,
                            1)->m_RefPicList;
                        iRefQBck[idx] = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                            pRefPicList[index],
                            0);
                    }
                    else
                        iRefQBck[idx] = -1;
                }

                // select current block motion vectors
                pVectorQFrw[idx] = (iRefQFrw[idx] < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][MBAddr].MotionVectors + (idx * 16 - idx);
                pVectorQBck[idx] = (iRefQBck[idx] < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][MBAddr].MotionVectors + (idx * 16 - idx);
            }

            // select neighbour
//fc            H264MacroblockLocalInfo *pNeighbour = core_enc->m_mbinfo.mbs + nNeighbour;
            Ipp32u cbp_luma_nb = ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_residual & 0xffff) << 1;

            if (pParams->is_profile_baseline_scalable)
                cbp_luma_nb |= ((core_enc->m_mbinfo.mbs + nNeighbour)->cbp_luma_coeffs & 0xffff) << 1;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32u blkQ, blkP;

                blkQ = ENCODER_EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = ENCODER_EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp_luma & blkQ) ||
//fc                    (pNeighbour->cbp_luma & blkP))
                    (cbp_luma_nb & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32u nNeighbourBlock;
                    Ipp32s iRefPFrw, iRefPBck;
                    Ipp32s nVectorDiffLimit = pParams->nMaxMVector;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                        nNeighbourBlock = idx * 4 + 3;
                    else
                        nNeighbourBlock = idx + 12;

                    // field coded image
                    if (FRM_STRUCTURE > core_enc->m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;
                        Ipp8s *pFields;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_Prediction;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            // select reference fields number array
                            pFields = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_Prediction;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                                    pRefPicList[index],
                                    pFields[index]));
                        }
                        else
                            iRefPBck = -1;
                    }
                    // frame coded image
                    else
                    {
                        H264EncoderFrameType **pRefPicList;
                        Ipp32s index;

                        // select reference index for previous block
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                0)->m_RefPicList;
                            iRefPFrw = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPFrw = -1;
                        index = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][nNeighbour].RefIdxs[nNeighbourBlock];
                        if (0 <= index)
                        {
                            pRefPicList = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                                core_enc->m_pCurrentFrame,
                                (core_enc->m_pCurrentFrame->m_mbinfo.mbs + nNeighbour)->slice_id,
                                1)->m_RefPicList;
                            iRefPBck = H264ENC_MAKE_NAME(H264EncoderFrame_DeblockPicID)(
                                pRefPicList[index],
                                0);
                        }
                        else
                            iRefPBck = -1;
                    }

                    // when reference indexes are equal
                    if (((iRefQFrw[idx / 2] == iRefPFrw) && (iRefQBck[idx / 2] == iRefPBck)) ||
                        ((iRefQFrw[idx / 2] == iRefPBck) && (iRefQBck[idx / 2] == iRefPFrw)))
                    {
                        // set initial value of strength
                        pStrength[idx] = 0;

                        // when forward and backward reference pictures of previous block are different
                        if (iRefPFrw != iRefPBck)
                        {
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select previous block motion vectors
                            if (iRefQFrw[idx / 2] == iRefPFrw)
                            {
                                pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                            }
                            else
                            {
                                pVectorPFrw = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;
                                pVectorPBck = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            }

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw[idx / 2]->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw[idx / 2]->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck[idx / 2]->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck[idx / 2]->mvy - pVectorPBck->mvy)))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                        // when forward and backward reference pictures of previous block are equal
                        else
                        {
                            const H264MotionVector *pVectorPFrw, *pVectorPBck;

                            // select previous block motion vectors
                            pVectorPFrw = (iRefPFrw < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[0][nNeighbour].MotionVectors + nNeighbourBlock;
                            pVectorPBck = (iRefPBck < 0)? &null_mv: core_enc->m_pCurrentFrame->m_mbinfo.MV[1][nNeighbour].MotionVectors + nNeighbourBlock;

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw[idx / 2]->mvx - pVectorPFrw->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQFrw[idx / 2]->mvy - pVectorPFrw->mvy)) ||
                                (4 <= abs(pVectorQBck[idx / 2]->mvx - pVectorPBck->mvx)) ||
                                (nVectorDiffLimit <= abs(pVectorQBck[idx / 2]->mvy - pVectorPBck->mvy)))
                            {
                                if ((4 <= abs(pVectorQFrw[idx / 2]->mvx - pVectorPBck->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQFrw[idx / 2]->mvy - pVectorPBck->mvy)) ||
                                    (4 <= abs(pVectorQBck[idx / 2]->mvx - pVectorPFrw->mvx)) ||
                                    (nVectorDiffLimit <= abs(pVectorQBck[idx / 2]->mvy - pVectorPFrw->mvy)))
                                {
                                    pStrength[idx] = 1;
                                    *pDeblockingFlag = 1;
                                }
                            }
                        }
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (pParams->MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32u idx;

        // reset all strengths
        SetEdgeStrength(pStrength + 4, 0);
        SetEdgeStrength(pStrength + 8, 0);
        SetEdgeStrength(pStrength + 12, 0);

        // set deblocking flag
        if (cbp_luma & 0x1fffe)
            *pDeblockingFlag = 1;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            Ipp32u blkQ;

            blkQ = ENCODER_INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp_luma & blkQ)
                pStrength[idx] = 2;
        }
    }
}

#undef EncoderRefPicListStructType
#undef H264EncoderFrameType
#undef H264SliceType
#undef H264CoreEncoderType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
