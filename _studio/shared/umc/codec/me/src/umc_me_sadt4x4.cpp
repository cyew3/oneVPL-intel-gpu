//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2009 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>
//temporary here: the code is added from H264 encoder
#ifdef UMC_ENABLE_ME
#include "umc_me_sadt4x4.h"
Ipp32u SATD_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step, Ipp32s width, Ipp32s height)
{
#ifndef H264_SATD_OPT
    __ALIGN16 Ipp16s tmpBuff[4][4];
    __ALIGN16 Ipp16s diffBuff[4][4];
#endif
    Ipp32s x, y;
    Ipp32u satd = 0;

    for( y = 0; y < height; y += 4 ) {
        for( x = 0; x < width; x += 4 )  {
#ifndef H264_SATD_OPT
            Ipp32s b;
            ippiSub4x4_8u16s_C1R(pSrc1 + x, src1Step, pSrc2 + x, src2Step, &diffBuff[0][0], 8);
            for (b = 0; b < 4; b ++) {
                Ipp16s a01, a23, b01, b23;

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
                satd += ABS(a01 + a23) + ABS(a01 - a23) + ABS(b01 - b23) + ABS(b01 + b23);
            }
#else
            __ALIGN16 __m128i  _p_0, _p_1, _p_2, _p_3, _p_4, _p_5, _p_7, _p_zero;
            const Ipp8u *pS1, *pS2;
            Ipp32s  s;

            pS1 = pSrc1 + x;
            pS2 = pSrc2 + x;
            _p_zero = _mm_setzero_si128();
            _p_0 = _mm_cvtsi32_si128(*(int*)(pS1));
            _p_4 = _mm_cvtsi32_si128(*(int*)(pS2));
            _p_1 = _mm_cvtsi32_si128(*(int*)(pS1+src1Step));
            _p_5 = _mm_cvtsi32_si128(*(int*)(pS2+src2Step));
            _p_0 = _mm_unpacklo_epi8(_p_0, _p_zero);
            _p_4 = _mm_unpacklo_epi8(_p_4, _p_zero);
            _p_1 = _mm_unpacklo_epi8(_p_1, _p_zero);
            _p_5 = _mm_unpacklo_epi8(_p_5, _p_zero);
            _p_0 = _mm_sub_epi16(_p_0, _p_4);
            _p_1 = _mm_sub_epi16(_p_1, _p_5);
            pS1 += 2 * src1Step;
            pS2 += 2 * src2Step;
            _p_2 = _mm_cvtsi32_si128(*(int*)(pS1));
            _p_4 = _mm_cvtsi32_si128(*(int*)(pS2));
            _p_3 = _mm_cvtsi32_si128(*(int*)(pS1+src1Step));
            _p_5 = _mm_cvtsi32_si128(*(int*)(pS2+src2Step));
            _p_2 = _mm_unpacklo_epi8(_p_2, _p_zero);
            _p_4 = _mm_unpacklo_epi8(_p_4, _p_zero);
            _p_3 = _mm_unpacklo_epi8(_p_3, _p_zero);
            _p_5 = _mm_unpacklo_epi8(_p_5, _p_zero);
            _p_2 = _mm_sub_epi16(_p_2, _p_4);
            _p_3 = _mm_sub_epi16(_p_3, _p_5);
            _p_5 = _mm_subs_epi16(_p_0, _p_1);
            _p_0 = _mm_adds_epi16(_p_0, _p_1);
            _p_7 = _mm_subs_epi16(_p_2, _p_3);
            _p_2 = _mm_adds_epi16(_p_2, _p_3);
            _p_1 = _mm_subs_epi16(_p_0, _p_2);
            _p_0 = _mm_adds_epi16(_p_0, _p_2);
            _p_3 = _mm_adds_epi16(_p_5, _p_7);
            _p_5 = _mm_subs_epi16(_p_5, _p_7);
            _p_0 = _mm_unpacklo_epi16(_p_0, _p_1);
            _p_5 = _mm_unpacklo_epi16(_p_5, _p_3);
            _p_7 = _mm_unpackhi_epi32(_p_0, _p_5);
            _p_0 = _mm_unpacklo_epi32(_p_0, _p_5);
            _p_1 = _mm_srli_si128(_p_0, 8);
            _p_3 = _mm_srli_si128(_p_7, 8);
            _p_5 = _mm_subs_epi16(_p_0, _p_1);
            _p_0 = _mm_adds_epi16(_p_0, _p_1);
            _p_2 = _mm_subs_epi16(_p_7, _p_3);
            _p_7 = _mm_adds_epi16(_p_7, _p_3);
            _p_1 = _mm_subs_epi16(_p_0, _p_7);
            _p_0 = _mm_adds_epi16(_p_0, _p_7);
            _p_3 = _mm_adds_epi16(_p_5, _p_2);
            _p_5 = _mm_subs_epi16(_p_5, _p_2);
            _p_0 = _mm_unpacklo_epi16(_p_0, _p_1);
            _p_5 = _mm_unpacklo_epi16(_p_5, _p_3);
            _p_2 = _mm_unpackhi_epi32(_p_0, _p_5);
            _p_0 = _mm_unpacklo_epi32(_p_0, _p_5);
            _p_3 = _mm_srai_epi16(_p_2, 15);
            _p_1 = _mm_srai_epi16(_p_0, 15);
            _p_2 = _mm_xor_si128(_p_2, _p_3);
            _p_0 = _mm_xor_si128(_p_0, _p_1);
            _p_2 = _mm_sub_epi16(_p_2, _p_3);
            _p_0 = _mm_sub_epi16(_p_0, _p_1);
            _p_0 = _mm_add_epi16(_p_0, _p_2);
            _p_2 = _mm_srli_si128(_p_0, 8);
            _p_0 = _mm_add_epi16(_p_0, _p_2);
            _p_2 = _mm_srli_si128(_p_0, 4);
            _p_0 = _mm_add_epi16(_p_0, _p_2);
            s = _mm_cvtsi128_si32(_p_0);
            satd += (s >> 16) + (Ipp16s)s;
#endif
        }
        pSrc1 += 4 * src1Step;
        pSrc2 += 4 * src2Step;
    }
    return satd >> 1;
}

Ipp32u SATD_16u_C1R(const Ipp16u *pSrc1, Ipp32s src1Step, const Ipp16u *pSrc2, Ipp32s src2Step, Ipp32s width, Ipp32s height)
{
    __ALIGN16 Ipp32s tmpBuff[4][4];
    __ALIGN16 Ipp16s diffBuff[4][4];
    Ipp32s x, y;
    Ipp32u satd = 0;

    src1Step >>= 1;
    src2Step >>= 1;
    for( y = 0; y < height; y += 4 ) {
        for( x = 0; x < width; x += 4 )  {
            Ipp32s b;
            ippiSub4x4_16u16s_C1R(pSrc1 + x, src1Step, pSrc2 + x, src2Step, &diffBuff[0][0], 16);
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
                satd += ABS(a01 + a23) + ABS(a01 - a23) + ABS(b01 - b23) + ABS(b01 + b23);
            }
        }
        pSrc1 += 4 * src1Step;
        pSrc2 += 4 * src2Step;
    }
    return satd >> 1;
}
#endif
