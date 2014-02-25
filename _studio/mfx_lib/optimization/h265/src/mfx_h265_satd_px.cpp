/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_PX)

namespace MFX_HEVC_PP
{

/* h265_SATD_4x4_8u() - 4x4 SATD from IPP source code */
Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_4x4_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep)
{
    ALIGN_DECL(16) Ipp16s tmpBuff[4][4];
    ALIGN_DECL(16) Ipp16s diffBuff[4][4];
    Ipp32u satd = 0;
    Ipp32s b;

    int i,j;

    //ippiSub4x4_8u16s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, &diffBuff[0][0], 8);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++)
            diffBuff[i][j] = (Ipp16s)(pSrcCur[j] - pSrcRef[j]);
        pSrcCur += srcCurStep;
        pSrcRef += srcRefStep;
    }

    for (b = 0; b < 4; b ++) {
        Ipp32s a01, a23, b01, b23;

        a01 = diffBuff[b][0] + diffBuff[b][1];
        a23 = diffBuff[b][2] + diffBuff[b][3];
        b01 = diffBuff[b][0] - diffBuff[b][1];
        b23 = diffBuff[b][2] - diffBuff[b][3];
        tmpBuff[b][0] = a01 + a23;
        tmpBuff[b][1] = a01 - a23;
        tmpBuff[b][2] = b01 - b23;
        tmpBuff[b][3] = b01 + b23;
    }
    for (b = 0; b < 4; b ++) {
        Ipp32s a01, a23, b01, b23;

        a01 = tmpBuff[0][b] + tmpBuff[1][b];
        a23 = tmpBuff[2][b] + tmpBuff[3][b];
        b01 = tmpBuff[0][b] - tmpBuff[1][b];
        b23 = tmpBuff[2][b] - tmpBuff[3][b];
        satd += IPP_ABS(a01 + a23) + IPP_ABS(a01 - a23) + IPP_ABS(b01 - b23) + IPP_ABS(b01 + b23);
    }

    return satd;
}

/* call twice for C version */
void H265_FASTCALL MAKE_NAME(h265_SATD_4x4_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_4x4_8u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_4x4_8u)(pSrcCur + 4, srcCurStep, pSrcRef + 4, srcRefStep);
}

/* h265_SATD_8x8_8u() - 8x8 SATD  from IPP source code */
Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_8x8_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep)
{
    Ipp32s i, j;
    Ipp32u satd = 0;
    ALIGN_DECL(16) Ipp16s diff[8][8];

    //ippiSub8x8_8u16s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, &diff[0][0], 16);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            diff[i][j] = (Ipp16s)(pSrcCur[j] - pSrcRef[j]);
        pSrcCur += srcCurStep;
        pSrcRef += srcRefStep;
    }

    for (i = 0; i < 8; i++) {
        Ipp32s t0 = diff[i][0] + diff[i][4];
        Ipp32s t4 = diff[i][0] - diff[i][4];
        Ipp32s t1 = diff[i][1] + diff[i][5];
        Ipp32s t5 = diff[i][1] - diff[i][5];
        Ipp32s t2 = diff[i][2] + diff[i][6];
        Ipp32s t6 = diff[i][2] - diff[i][6];
        Ipp32s t3 = diff[i][3] + diff[i][7];
        Ipp32s t7 = diff[i][3] - diff[i][7];
        Ipp32s s0 = t0 + t2;
        Ipp32s s2 = t0 - t2;
        Ipp32s s1 = t1 + t3;
        Ipp32s s3 = t1 - t3;
        Ipp32s s4 = t4 + t6;
        Ipp32s s6 = t4 - t6;
        Ipp32s s5 = t5 + t7;
        Ipp32s s7 = t5 - t7;
        diff[i][0] = s0 + s1;
        diff[i][1] = s0 - s1;
        diff[i][2] = s2 + s3;
        diff[i][3] = s2 - s3;
        diff[i][4] = s4 + s5;
        diff[i][5] = s4 - s5;
        diff[i][6] = s6 + s7;
        diff[i][7] = s6 - s7;
    }
    for (i = 0; i < 8; i++) {
        Ipp32s t0 = diff[0][i] + diff[4][i];
        Ipp32s t4 = diff[0][i] - diff[4][i];
        Ipp32s t1 = diff[1][i] + diff[5][i];
        Ipp32s t5 = diff[1][i] - diff[5][i];
        Ipp32s t2 = diff[2][i] + diff[6][i];
        Ipp32s t6 = diff[2][i] - diff[6][i];
        Ipp32s t3 = diff[3][i] + diff[7][i];
        Ipp32s t7 = diff[3][i] - diff[7][i];
        Ipp32s s0 = t0 + t2;
        Ipp32s s2 = t0 - t2;
        Ipp32s s1 = t1 + t3;
        Ipp32s s3 = t1 - t3;
        Ipp32s s4 = t4 + t6;
        Ipp32s s6 = t4 - t6;
        Ipp32s s5 = t5 + t7;
        Ipp32s s7 = t5 - t7;
        satd += IPP_ABS(s0 + s1);
        satd += IPP_ABS(s0 - s1);
        satd += IPP_ABS(s2 + s3);
        satd += IPP_ABS(s2 - s3);
        satd += IPP_ABS(s4 + s5);
        satd += IPP_ABS(s4 - s5);
        satd += IPP_ABS(s6 + s7);
        satd += IPP_ABS(s6 - s7);
    }

    return satd;
}

/* call twice for C version */
void H265_FASTCALL MAKE_NAME(h265_SATD_8x8_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_8x8_8u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_8x8_8u)(pSrcCur + 8, srcCurStep, pSrcRef + 8, srcRefStep);
}

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_SSSE3)

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
