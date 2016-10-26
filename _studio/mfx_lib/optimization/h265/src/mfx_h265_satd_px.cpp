//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

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

Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_4x4_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep)
{
  Ipp32s k, satd = 0, diff[16], m[16], d[16];

  for( k = 0; k < 16; k+=4 )
  {
    diff[k+0] = pSrcCur[0] - pSrcRef[0];
    diff[k+1] = pSrcCur[1] - pSrcRef[1];
    diff[k+2] = pSrcCur[2] - pSrcRef[2];
    diff[k+3] = pSrcCur[3] - pSrcRef[3];

    pSrcCur += srcCurStep;
    pSrcRef += srcRefStep;
  }

  m[ 0] = diff[ 0] + diff[12];
  m[ 1] = diff[ 1] + diff[13];
  m[ 2] = diff[ 2] + diff[14];
  m[ 3] = diff[ 3] + diff[15];
  m[ 4] = diff[ 4] + diff[ 8];
  m[ 5] = diff[ 5] + diff[ 9];
  m[ 6] = diff[ 6] + diff[10];
  m[ 7] = diff[ 7] + diff[11];
  m[ 8] = diff[ 4] - diff[ 8];
  m[ 9] = diff[ 5] - diff[ 9];
  m[10] = diff[ 6] - diff[10];
  m[11] = diff[ 7] - diff[11];
  m[12] = diff[ 0] - diff[12];
  m[13] = diff[ 1] - diff[13];
  m[14] = diff[ 2] - diff[14];
  m[15] = diff[ 3] - diff[15];

  d[ 0] = m[ 0] + m[ 4];
  d[ 1] = m[ 1] + m[ 5];
  d[ 2] = m[ 2] + m[ 6];
  d[ 3] = m[ 3] + m[ 7];
  d[ 4] = m[ 8] + m[12];
  d[ 5] = m[ 9] + m[13];
  d[ 6] = m[10] + m[14];
  d[ 7] = m[11] + m[15];
  d[ 8] = m[ 0] - m[ 4];
  d[ 9] = m[ 1] - m[ 5];
  d[10] = m[ 2] - m[ 6];
  d[11] = m[ 3] - m[ 7];
  d[12] = m[12] - m[ 8];
  d[13] = m[13] - m[ 9];
  d[14] = m[14] - m[10];
  d[15] = m[15] - m[11];

  m[ 0] = d[ 0] + d[ 3];
  m[ 1] = d[ 1] + d[ 2];
  m[ 2] = d[ 1] - d[ 2];
  m[ 3] = d[ 0] - d[ 3];
  m[ 4] = d[ 4] + d[ 7];
  m[ 5] = d[ 5] + d[ 6];
  m[ 6] = d[ 5] - d[ 6];
  m[ 7] = d[ 4] - d[ 7];
  m[ 8] = d[ 8] + d[11];
  m[ 9] = d[ 9] + d[10];
  m[10] = d[ 9] - d[10];
  m[11] = d[ 8] - d[11];
  m[12] = d[12] + d[15];
  m[13] = d[13] + d[14];
  m[14] = d[13] - d[14];
  m[15] = d[12] - d[15];

  d[ 0] = m[ 0] + m[ 1];
  d[ 1] = m[ 0] - m[ 1];
  d[ 2] = m[ 2] + m[ 3];
  d[ 3] = m[ 3] - m[ 2];
  d[ 4] = m[ 4] + m[ 5];
  d[ 5] = m[ 4] - m[ 5];
  d[ 6] = m[ 6] + m[ 7];
  d[ 7] = m[ 7] - m[ 6];
  d[ 8] = m[ 8] + m[ 9];
  d[ 9] = m[ 8] - m[ 9];
  d[10] = m[10] + m[11];
  d[11] = m[11] - m[10];
  d[12] = m[12] + m[13];
  d[13] = m[12] - m[13];
  d[14] = m[14] + m[15];
  d[15] = m[15] - m[14];

  for (k=0; k<16; ++k)
  {
    satd += abs(d[k]);
  }

  // NOTE - assume this scaling is done by caller (be consistent with 8-bit version)
  //satd = ((satd+1)>>1);

  return satd;
}

/* call twice for C version */
void H265_FASTCALL MAKE_NAME(h265_SATD_4x4_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_4x4_8u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_4x4_8u)(pSrcCur + 4, srcCurStep, pSrcRef + 4, srcRefStep);
}

void H265_FASTCALL MAKE_NAME(h265_SATD_4x4_Pair_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_4x4_16u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_4x4_16u)(pSrcCur + 4, srcCurStep, pSrcRef + 4, srcRefStep);
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

Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_8x8_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep)
{
  Ipp32s k, i, j, jj, sad=0;
  Ipp32s diff[64], m1[8][8], m2[8][8], m3[8][8];

  for( k = 0; k < 64; k += 8 )
  {
    diff[k+0] = pSrcCur[0] - pSrcRef[0];
    diff[k+1] = pSrcCur[1] - pSrcRef[1];
    diff[k+2] = pSrcCur[2] - pSrcRef[2];
    diff[k+3] = pSrcCur[3] - pSrcRef[3];
    diff[k+4] = pSrcCur[4] - pSrcRef[4];
    diff[k+5] = pSrcCur[5] - pSrcRef[5];
    diff[k+6] = pSrcCur[6] - pSrcRef[6];
    diff[k+7] = pSrcCur[7] - pSrcRef[7];

    pSrcCur += srcCurStep;
    pSrcRef += srcRefStep;
  }

  for (j=0; j < 8; j++)
  {
    jj = j << 3;
    m2[j][0] = diff[jj  ] + diff[jj+4];
    m2[j][1] = diff[jj+1] + diff[jj+5];
    m2[j][2] = diff[jj+2] + diff[jj+6];
    m2[j][3] = diff[jj+3] + diff[jj+7];
    m2[j][4] = diff[jj  ] - diff[jj+4];
    m2[j][5] = diff[jj+1] - diff[jj+5];
    m2[j][6] = diff[jj+2] - diff[jj+6];
    m2[j][7] = diff[jj+3] - diff[jj+7];

    m1[j][0] = m2[j][0] + m2[j][2];
    m1[j][1] = m2[j][1] + m2[j][3];
    m1[j][2] = m2[j][0] - m2[j][2];
    m1[j][3] = m2[j][1] - m2[j][3];
    m1[j][4] = m2[j][4] + m2[j][6];
    m1[j][5] = m2[j][5] + m2[j][7];
    m1[j][6] = m2[j][4] - m2[j][6];
    m1[j][7] = m2[j][5] - m2[j][7];

    m2[j][0] = m1[j][0] + m1[j][1];
    m2[j][1] = m1[j][0] - m1[j][1];
    m2[j][2] = m1[j][2] + m1[j][3];
    m2[j][3] = m1[j][2] - m1[j][3];
    m2[j][4] = m1[j][4] + m1[j][5];
    m2[j][5] = m1[j][4] - m1[j][5];
    m2[j][6] = m1[j][6] + m1[j][7];
    m2[j][7] = m1[j][6] - m1[j][7];
  }

  for (i=0; i < 8; i++)
  {
    m3[0][i] = m2[0][i] + m2[4][i];
    m3[1][i] = m2[1][i] + m2[5][i];
    m3[2][i] = m2[2][i] + m2[6][i];
    m3[3][i] = m2[3][i] + m2[7][i];
    m3[4][i] = m2[0][i] - m2[4][i];
    m3[5][i] = m2[1][i] - m2[5][i];
    m3[6][i] = m2[2][i] - m2[6][i];
    m3[7][i] = m2[3][i] - m2[7][i];

    m1[0][i] = m3[0][i] + m3[2][i];
    m1[1][i] = m3[1][i] + m3[3][i];
    m1[2][i] = m3[0][i] - m3[2][i];
    m1[3][i] = m3[1][i] - m3[3][i];
    m1[4][i] = m3[4][i] + m3[6][i];
    m1[5][i] = m3[5][i] + m3[7][i];
    m1[6][i] = m3[4][i] - m3[6][i];
    m1[7][i] = m3[5][i] - m3[7][i];

    m2[0][i] = m1[0][i] + m1[1][i];
    m2[1][i] = m1[0][i] - m1[1][i];
    m2[2][i] = m1[2][i] + m1[3][i];
    m2[3][i] = m1[2][i] - m1[3][i];
    m2[4][i] = m1[4][i] + m1[5][i];
    m2[5][i] = m1[4][i] - m1[5][i];
    m2[6][i] = m1[6][i] + m1[7][i];
    m2[7][i] = m1[6][i] - m1[7][i];
  }

  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 8; j++)
    {
      sad += abs(m2[i][j]);
    }
  }

  // NOTE - assume this scaling is done by caller (be consistent with 8-bit version)
  //sad=((sad+2)>>2);

  return sad;
}

/* call twice for C version */
void H265_FASTCALL MAKE_NAME(h265_SATD_8x8_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_8x8_8u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_8x8_8u)(pSrcCur + 8, srcCurStep, pSrcRef + 8, srcRefStep);
}

void H265_FASTCALL MAKE_NAME(h265_SATD_8x8_Pair_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_8x8_16u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_8x8_16u)(pSrcCur + 8, srcCurStep, pSrcRef + 8, srcRefStep);
}

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_SSSE3)

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
