//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2017 Intel Corporation. All Rights Reserved.
//

#pragma warning( disable : 4702 )
#pragma warning( disable : 4100 )

#include "meforgen7_5.h"
#if (__HAAR_SAD_OPT == 1)
#include "emmintrin.h"
#ifdef __SSSE3_ENABLE
#include "tmmintrin.h"
#endif
#define __ALIGN8  VME_ALIGN_DECL(8)
#endif

#include "ipp.h"
#if (__HAAR_SAD_OPT == 1)
/*********************************************************************************/
void MEforGen75::GetSad4x4_par(U8 *src, U8 *blk, int blkw, int *sad)
/*********************************************************************************/
{                    

    sad[0] = 0;
    sad[1] = 0;

    // Load src values into 64-bit registers
    __m128i src_row0 = _mm_loadl_epi64((__m128i const*)src); src+=16;
    __m128i src_row1 = _mm_loadl_epi64((__m128i const*)src); src+=16;
    __m128i src_row2 = _mm_loadl_epi64((__m128i const*)src); src+=16;
    __m128i src_row3 = _mm_loadl_epi64((__m128i const*)src);

    src_row0 = _mm_unpacklo_epi32(src_row0,src_row1);
    src_row1 = _mm_unpacklo_epi32(src_row2,src_row3);

    // Load blk values into 64-bit registers
     __m128i blk_row0 = _mm_loadl_epi64((__m128i const*)blk); blk+=blkw;
     __m128i blk_row1 = _mm_loadl_epi64((__m128i const*)blk); blk+=blkw;
     __m128i blk_row2 = _mm_loadl_epi64((__m128i const*)blk); blk+=blkw;
     __m128i blk_row3 = _mm_loadl_epi64((__m128i const*)blk);

    blk_row0 = _mm_unpacklo_epi32(blk_row0,blk_row1);
    blk_row1 = _mm_unpacklo_epi32(blk_row2,blk_row3);

    // Create an all-zero 64-bit 
     __m128i zero_reg = _mm_setzero_si128();

    // Unpack src from 8-bit to 16-bit
     __m128i src_row0a_unpacked = _mm_unpacklo_epi8(src_row0, zero_reg);
     __m128i src_row1a_unpacked = _mm_unpacklo_epi8(src_row1, zero_reg);
     __m128i src_row0b_unpacked = _mm_unpackhi_epi8(src_row0, zero_reg);
     __m128i src_row1b_unpacked = _mm_unpackhi_epi8(src_row1, zero_reg);

    // Unpack blk from 8-bit to 16-bit
     __m128i blk_row0a_unpacked = _mm_unpacklo_epi8(blk_row0, zero_reg);
     __m128i blk_row1a_unpacked = _mm_unpacklo_epi8(blk_row1, zero_reg);
     __m128i blk_row0b_unpacked = _mm_unpackhi_epi8(blk_row0, zero_reg);
     __m128i blk_row1b_unpacked = _mm_unpackhi_epi8(blk_row1, zero_reg);

    // Calculate the difference image (difference betwween src and blk)
     __m128i diff_row0a = _mm_sub_epi16(src_row0a_unpacked, blk_row0a_unpacked);
     __m128i diff_row1a = _mm_sub_epi16(src_row1a_unpacked, blk_row1a_unpacked);
     __m128i diff_row0b = _mm_sub_epi16(src_row0b_unpacked, blk_row0b_unpacked);
     __m128i diff_row1b = _mm_sub_epi16(src_row1b_unpacked, blk_row1b_unpacked);

    // Transpose the 4x4 difference image block (Block A)
     __m128i tmp0 = _mm_unpacklo_epi16(diff_row0a, diff_row1a);
     __m128i tmp1 = _mm_unpackhi_epi16(diff_row0a, diff_row1a);

     __m128i tmp2 = _mm_unpacklo_epi16(tmp0, tmp1);
     __m128i tmp3 = _mm_unpackhi_epi16(tmp0, tmp1);     

    diff_row0a = _mm_unpacklo_epi16(tmp2, tmp3); // (d0,d4,d8,d12)
    diff_row1a = _mm_unpackhi_epi16(tmp2, tmp3); // (d1,d5,d9,d13)    

    // Haar transformation is defined as follows:
    // (H0,H1,H2,H3) = (d0-d1, d4-d5, d8-d9, d12-d13)>>1
    // (H4,H5,H6,H7) = (d2-d3, d6-d7, d10-d11, d14-d15)>>1

    // Calculate H_0_7
     __m128i H_0_7a = _mm_sub_epi16(diff_row0a, diff_row1a);    
    H_0_7a = _mm_srai_epi16(H_0_7a, 1);

    // Calculate the horizontal sums of two pixels:
    // (S0,S1,S2,S3) = (d0+d1, d4+d5, d8+d9, d12+d13)
    // (S4,S5,S6,S7) = (d2+d3, d6+d7, d10+d11, d14+d15)
     __m128i S_0_7a = _mm_add_epi16(diff_row0a, diff_row1a);

    // Transpose the 4x4 difference image block (Block B)
    tmp0 = _mm_unpacklo_epi16(diff_row0b, diff_row1b);
    tmp1 = _mm_unpackhi_epi16(diff_row0b, diff_row1b);

    tmp2 = _mm_unpacklo_epi16(tmp0, tmp1);
    tmp3 = _mm_unpackhi_epi16(tmp0, tmp1);     

    diff_row0b = _mm_unpacklo_epi16(tmp2, tmp3); // (d0,d4,d8,d12)
    diff_row1b = _mm_unpackhi_epi16(tmp2, tmp3); // (d1,d5,d9,d13)    

    // Haar transformation is defined as follows:
    // (H0,H1,H2,H3) = (d0-d1, d4-d5, d8-d9, d12-d13)>>1
    // (H4,H5,H6,H7) = (d2-d3, d6-d7, d10-d11, d14-d15)>>1

    // Calculate H_0_7
     __m128i H_0_7b = _mm_sub_epi16(diff_row0b, diff_row1b);    
    H_0_7b = _mm_srai_epi16(H_0_7b, 1);

    // Calculate the horizontal sums of two pixels:
    // (S0,S1,S2,S3) = (d0+d1, d4+d5, d8+d9, d12+d13)
    // (S4,S5,S6,S7) = (d2+d3, d6+d7, d10+d11, d14+d15)
     __m128i S_0_7b = _mm_add_epi16(diff_row0b, diff_row1b);

    tmp0 = _mm_unpacklo_epi32(S_0_7a, S_0_7b); // (d0,d4,d8,d12)
    tmp1 = _mm_unpackhi_epi32(S_0_7a, S_0_7b); // (d0,d4,d8,d12)

    tmp2 = _mm_unpacklo_epi32(tmp0, tmp1); // (d0,d4,d8,d12)
    tmp3 = _mm_unpackhi_epi32(tmp0, tmp1); // (d0,d4,d8,d12)

     __m128i H_8_11 = _mm_sub_epi16(tmp2, tmp3);
    H_8_11 = _mm_srai_epi16(H_8_11, 1);

     __m128i S_8_11 = _mm_add_epi16(tmp2, tmp3);

    __m128i tmp0b = _mm_unpackhi_epi64(S_8_11, zero_reg);
     tmp0 = _mm_unpacklo_epi16(S_8_11, tmp0b);

    tmp1 = _mm_unpacklo_epi16(tmp0, zero_reg);
    tmp2 = _mm_unpackhi_epi16(tmp0, zero_reg);
    tmp3 = _mm_unpacklo_epi16(tmp1, tmp2);
    __m128i tmp4 = _mm_unpackhi_epi16(tmp1, tmp2);

     __m128i H_12_13 = _mm_sub_epi16(tmp3, tmp4);
    H_12_13 = _mm_srai_epi16(H_12_13, 1);

     __m128i S_12_13 = _mm_add_epi16(tmp3, tmp4);

    // Calculate the absolute values of h0-h13
#ifdef __SSSE3_ENABLE
    H_0_7a = _mm_abs_epi16(H_0_7a);
#else
    tmp0 = _mm_cmpgt_epi16(H_0_7a, zero_reg);
    tmp1 = _mm_and_si128(tmp0, H_0_7a);
    tmp2 = _mm_andnot_si128(tmp0, H_0_7a);
    H_0_7a = _mm_sub_epi16(tmp1, tmp2);
#endif

#ifdef __SSSE3_ENABLE
    H_0_7b = _mm_abs_epi16(H_0_7b);
#else
    tmp0 = _mm_cmpgt_epi16(H_0_7b, zero_reg);
    tmp1 = _mm_and_si128(tmp0, H_0_7b);
    tmp2 = _mm_andnot_si128(tmp0, H_0_7b);
    H_0_7b = _mm_sub_epi16(tmp1, tmp2);
#endif

#ifdef __SSSE3_ENABLE
    H_8_11 = _mm_abs_epi16(H_8_11);
#else
    tmp0 = _mm_cmpgt_epi16(H_8_11, zero_reg);
    tmp1 = _mm_and_si128(tmp0, H_8_11);
    tmp2 = _mm_andnot_si128(tmp0, H_8_11);
    H_8_11 = _mm_sub_epi16(tmp1, tmp2);
#endif

#ifdef __SSSE3_ENABLE
    H_12_13 = _mm_abs_epi16(H_12_13);
#else
    tmp0 = _mm_cmpgt_epi16(H_12_13, zero_reg);
    tmp1 = _mm_and_si128(tmp0, H_12_13);
    tmp2 = _mm_andnot_si128(tmp0, H_12_13);
    H_12_13 = _mm_sub_epi16(tmp1, tmp2);
#endif    

#ifdef __SSSE3_ENABLE
    tmp0 = _mm_hadd_epi16(H_0_7a,H_0_7b); // [A01, A23, A45, A67, B01, B23, B45, B67]
    tmp1 = _mm_hadd_epi16(H_8_11,H_12_13); // [A89, A1011, B89, B1011, A1213, 0, B1213, 0]
    tmp2 = _mm_hadd_epi16(tmp0,tmp1); // [A03, A47, B03, B47, A811, B811, A1213, B1213]
#endif

#ifdef __SSSE3_ENABLE
    VME_ALIGN_DECL(16) short h_0_13[8];
#else
    VME_ALIGN_DECL(16) short h_0_7a[8];
    VME_ALIGN_DECL(16) short h_0_7b[8];
    VME_ALIGN_DECL(16) short h_8_11[8];
    VME_ALIGN_DECL(16) short h_12_13[8];
#endif
    VME_ALIGN_DECL(16) short s[8];

    // Extract 16-bit values from 128-bit MMX registers
#ifdef __SSSE3_ENABLE
    _mm_store_si128((__m128i*)&h_0_13[0], tmp2);
#else
    _mm_store_si128((__m128i*)&h_0_7a[0], H_0_7a);
    _mm_store_si128((__m128i*)&h_0_7b[0], H_0_7b);
    _mm_store_si128((__m128i*)&h_8_11[0], H_8_11);
    _mm_store_si128((__m128i*)&h_12_13[0], H_12_13);
#endif    
    _mm_store_si128((__m128i*)&s[0], S_12_13);        

#ifdef __SSSE3_ENABLE
    sad[0] += h_0_13[0];
    sad[0] += h_0_13[1];
    sad[0] += h_0_13[4];
    sad[0] += h_0_13[6];

    sad[1] += h_0_13[2];
    sad[1] += h_0_13[3];
    sad[1] += h_0_13[5];
    sad[1] += h_0_13[7];
#else
    sad[0] += h_0_7a[0];
    sad[0] += h_0_7a[1];
    sad[0] += h_0_7a[2];
    sad[0] += h_0_7a[3];
    sad[0] += h_0_7a[4];
    sad[0] += h_0_7a[5];
    sad[0] += h_0_7a[6];
    sad[0] += h_0_7a[7];

    sad[1] += h_0_7b[0];
    sad[1] += h_0_7b[1];
    sad[1] += h_0_7b[2];
    sad[1] += h_0_7b[3];
    sad[1] += h_0_7b[4];
    sad[1] += h_0_7b[5];
    sad[1] += h_0_7b[6];
    sad[1] += h_0_7b[7];

    sad[0] += h_8_11[0];
    sad[0] += h_8_11[1];
    sad[0] += h_8_11[2];
    sad[0] += h_8_11[3];

    sad[1] += h_8_11[4];
    sad[1] += h_8_11[5];
    sad[1] += h_8_11[6];
    sad[1] += h_8_11[7];

    sad[0] += h_12_13[0];
    sad[0] += h_12_13[1];

    sad[1] += h_12_13[4];
    sad[1] += h_12_13[5];    
#endif
    int h14[2], h15[2], h14_neg[2], h15_neg[2], sad_gt[2];
    h14[0] = (s[0]-s[1]) >> 1;
    h14[1] = (s[4]-s[5]) >> 1;

    h14_neg[0] = h14[0]<0;
    sad[0] += h14[0]*(1-(h14_neg[0]<<1));
    h14_neg[1] = h14[1]<0;
    sad[1] += h14[1]*(1-(h14_neg[1]<<1));
                        
    h15[0] = (s[0]+s[1]) >> 1;
    h15[1] = (s[4]+s[5]) >> 1;
    
    h15_neg[0] = h15[0]<0;
    sad[0] += h15[0]*(1-(h15_neg[0]<<1))*((int)(!(Vsta.MvFlags&AC_ONLY_HAAR)));
    h15_neg[1] = h15[1]<0;
    sad[1] += h15[1]*(1-(h15_neg[1]<<1))*((int)(!(Vsta.MvFlags&AC_ONLY_HAAR)));

    sad[0] >>= (11-DISTBIT4X4);
    sad[1] >>= (11-DISTBIT4X4);

    sad_gt[0] = (sad[0] > DISTMAX4X4);
    sad_gt[1] = (sad[1] > DISTMAX4X4);
    
    sad[0] = sad_gt[0]*DISTMAX4X4 + (1-sad_gt[0])*sad[0];
    sad[1] = sad_gt[1]*DISTMAX4X4 + (1-sad_gt[1])*sad[1];

}

void MEforGen75::GetFourSAD4x4(U8* pMB, int SrcStep, U8* pSrc, U32* dif)
{
    __ALIGN8 __m128i s0, s1, s2, s3, s4, s5, s6, s7; 
    __ALIGN8 __m128i m0, m1, m2, m3, m4, m5, m6, m7; 

    s0 = _mm_loadu_si128((__m128i*)pSrc); 
    s1 = _mm_loadu_si128((__m128i*)(pSrc + SrcStep)); 
    s2 = _mm_loadu_si128((__m128i*)(pSrc + 2 * SrcStep)); 
    s3 = _mm_loadu_si128((__m128i*)(pSrc + 3 * SrcStep)); 

    s4 = _mm_unpacklo_epi32(s0, s1);
    s5 = _mm_unpacklo_epi32(s2, s3);
    s6 = _mm_unpackhi_epi32(s0, s1);
    s7 = _mm_unpackhi_epi32(s2, s3);

    m0 = _mm_loadu_si128((__m128i*)pMB); 
    m1 = _mm_loadu_si128((__m128i*)(pMB + 16)); 
    m2 = _mm_loadu_si128((__m128i*)(pMB + 2 * 16)); 
    m3 = _mm_loadu_si128((__m128i*)(pMB + 3 * 16));
    
    m4 = _mm_unpacklo_epi32(m0, m1);
    m5 = _mm_unpacklo_epi32(m2, m3);
    m6 = _mm_unpackhi_epi32(m0, m1);
    m7 = _mm_unpackhi_epi32(m2, m3);

    s0 = _mm_sad_epu8(s4, m4);
    s1 = _mm_sad_epu8(s5, m5);

    s2 = _mm_sad_epu8(s6, m6);
    s3 = _mm_sad_epu8(s7, m7);

    s0 = _mm_adds_epu16(s0, s1);
    s2 = _mm_adds_epu16(s2, s3);

    //res = _mm_extract_epi16(s0, 4) + _mm_extract_epi16(s0, 0) + _mm_extract_epi16(s2, 4) + _mm_extract_epi16(s2, 0);
    dif[0] = _mm_extract_epi16(s0, 0);
    dif[1] = _mm_extract_epi16(s0, 4);
    dif[2] = _mm_extract_epi16(s2, 0);
    dif[3] = _mm_extract_epi16(s2, 4);

    if (dif[0] > DISTMAX4X4) dif[0] = DISTMAX4X4;
    if (dif[1] > DISTMAX4X4) dif[1] = DISTMAX4X4;
    if (dif[2] > DISTMAX4X4) dif[2] = DISTMAX4X4;
    if (dif[3] > DISTMAX4X4) dif[3] = DISTMAX4X4;

    _m_empty();
}
#endif

/*********************************************************************************/
int MEforGen75::GetSad4x4(U8 *src, U8 *blk, int blkw)
/*********************************************************************************/
{
    int        d = 0;
    int        i;
    short    dif[24];
    short    *tmp = dif;

    switch(SadMethod){
    case 0: // SAD
#if (__SAD_OPT == 0)
        for(i=0; i<4; i++){
            d += ((src[0]>blk[0]) ? src[0]-blk[0] : blk[0]-src[0])
              +  ((src[1]>blk[1]) ? src[1]-blk[1] : blk[1]-src[1])
              +  ((src[2]>blk[2]) ? src[2]-blk[2] : blk[2]-src[2])
              +  ((src[3]>blk[3]) ? src[3]-blk[3] : blk[3]-src[3]);
            src += 16;
            blk += blkw;
        }
#else
        __ALIGN8  __m64  _p_0, _p_1, _p_2, _p_3;
        
        _p_0 = _mm_cvtsi32_si64(*(int*)(src));
        _p_1 = _mm_cvtsi32_si64(*(int*)(src + 16));
        
        _p_2 = _mm_cvtsi32_si64(*(int*)(src + 32));
        _p_3 = _mm_cvtsi32_si64(*(int*)(src + 16 + 32));
        
        _p_0 = _mm_sad_pu8(_p_0, _mm_cvtsi32_si64(*(int*)(blk)));
        _p_1 = _mm_sad_pu8(_p_1, _mm_cvtsi32_si64(*(int*)(blk + blkw)));
        
        _p_2 = _mm_sad_pu8(_p_2, _mm_cvtsi32_si64(*(int*)(blk + 2 * blkw)));
        _p_3 = _mm_sad_pu8(_p_3, _mm_cvtsi32_si64(*(int*)(blk + blkw + 2 * blkw)));
        
        _p_0 = _mm_add_pi16(_p_0, _p_1);
        _p_2 = _mm_add_pi16(_p_2, _p_3);
        _p_0 = _mm_add_pi16(_p_0, _p_2);
        d = _mm_cvtsi64_si32(_p_0);
        _m_empty();
#endif //if (__SAD_OPT == 0)
        d >>= (11-DISTBIT4X4); // Add this to improve quality for 10 bit case

         return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
//        return d;
#if 1
        // Newest Bspec
    case 2:    //HAAR
#if (__HAAR_SAD_OPT == 1)
        {        
        int sad = 0;

        // Load src values into 64-bit registers
        __m128i src_row0 = _mm_loadl_epi64((__m128i const*)src); src+=16;
        __m128i src_row1 = _mm_loadl_epi64((__m128i const*)src); src+=16;
        __m128i src_row2 = _mm_loadl_epi64((__m128i const*)src); src+=16;
        __m128i src_row3 = _mm_loadl_epi64((__m128i const*)src);

        src_row0 = _mm_unpacklo_epi32(src_row0,src_row1);
        src_row1 = _mm_unpacklo_epi32(src_row2,src_row3);
        
        // Load blk values into 64-bit registers
         __m128i blk_row0 = _mm_loadl_epi64((__m128i const*)blk); blk+=blkw;
         __m128i blk_row1 = _mm_loadl_epi64((__m128i const*)blk); blk+=blkw;
         __m128i blk_row2 = _mm_loadl_epi64((__m128i const*)blk); blk+=blkw;
         __m128i blk_row3 = _mm_loadl_epi64((__m128i const*)blk);

        blk_row0 = _mm_unpacklo_epi32(blk_row0,blk_row1);
        blk_row1 = _mm_unpacklo_epi32(blk_row2,blk_row3);
        
        // Create an all-zero 64-bit 
         __m128i zero_reg = _mm_setzero_si128();

        // Unpack src from 8-bit to 16-bit
         __m128i src_row0_unpacked = _mm_unpacklo_epi8(src_row0, zero_reg);
         __m128i src_row1_unpacked = _mm_unpacklo_epi8(src_row1, zero_reg);

        // Unpack blk from 8-bit to 16-bit
         __m128i blk_row0_unpacked = _mm_unpacklo_epi8(blk_row0, zero_reg);
         __m128i blk_row1_unpacked = _mm_unpacklo_epi8(blk_row1, zero_reg);    

        // Calculate the difference image (difference betwween src and blk)
         __m128i diff_row0 = _mm_sub_epi16(src_row0_unpacked, blk_row0_unpacked);
         __m128i diff_row1 = _mm_sub_epi16(src_row1_unpacked, blk_row1_unpacked);    

        // Transpose the 4x4 difference image block
         __m128i tmp0 = _mm_unpacklo_epi16(diff_row0, diff_row1);
         __m128i tmp1 = _mm_unpackhi_epi16(diff_row0, diff_row1);
        
         __m128i tmp2 = _mm_unpacklo_epi16(tmp0, tmp1);
         __m128i tmp3 = _mm_unpackhi_epi16(tmp0, tmp1);

         __m128i tmp4 = _mm_unpackhi_epi16(tmp0, tmp1);
         //__m128i tmp5 = _mm_unpacklo_epi16(tmp0, tmp1);

        diff_row0 = _mm_unpacklo_epi16(tmp2, tmp3); // (d0,d4,d8,d12)
        diff_row1 = _mm_unpackhi_epi16(tmp2, tmp3); // (d1,d5,d9,d13)    

        // Haar transformation is defined as follows:
        // (H0,H1,H2,H3) = (d0-d1, d4-d5, d8-d9, d12-d13)>>1
        // (H4,H5,H6,H7) = (d2-d3, d6-d7, d10-d11, d14-d15)>>1
        
        // Calculate H_0_7
         __m128i H_0_7 = _mm_sub_epi16(diff_row0, diff_row1);    
        H_0_7 = _mm_srai_epi16(H_0_7, 1);

        // Calculate the horizontal sums of two pixels:
        // (S0,S1,S2,S3) = (d0+d1, d4+d5, d8+d9, d12+d13)
        // (S4,S5,S6,S7) = (d2+d3, d6+d7, d10+d11, d14+d15)
         __m128i S_0_7 = _mm_add_epi16(diff_row0, diff_row1);    

        tmp0 = _mm_unpacklo_epi32(S_0_7, zero_reg); // (d0,d4,d8,d12)
        tmp1 = _mm_unpackhi_epi32(S_0_7, zero_reg); // (d0,d4,d8,d12)

        tmp2 = _mm_unpacklo_epi32(tmp0, tmp1); // (d0,d4,d8,d12)
        tmp3 = _mm_unpackhi_epi32(tmp0, tmp1); // (d0,d4,d8,d12)

         __m128i H_8_11 = _mm_sub_epi16(tmp2, tmp3);
        H_8_11 = _mm_srai_epi16(H_8_11, 1);

         __m128i S_8_11 = _mm_add_epi16(tmp2, tmp3);

        tmp0 = _mm_unpacklo_epi16(S_8_11, zero_reg);
        tmp1 = _mm_unpacklo_epi16(tmp0, zero_reg);
        tmp2 = _mm_unpackhi_epi16(tmp0, zero_reg);
        tmp3 = _mm_unpacklo_epi16(tmp1, tmp2);
        tmp4 = _mm_unpackhi_epi16(tmp1, tmp2);
        
         __m128i H_12_13 = _mm_sub_epi16(tmp3, tmp4);
        H_12_13 = _mm_srai_epi16(H_12_13, 1);
        
         __m128i S_12_13 = _mm_add_epi16(tmp3, tmp4);
        
        // Calculate the absolute values of h0-h13
        tmp0 = _mm_cmpgt_epi16(H_0_7, zero_reg);
        tmp1 = _mm_and_si128(tmp0, H_0_7);
        tmp2 = _mm_andnot_si128(tmp0, H_0_7);
        H_0_7 = _mm_sub_epi16(tmp1, tmp2);

        //H_0_7 = _mm_sad_epu8(H_0_7, zero_reg);

        tmp0 = _mm_cmpgt_epi16(H_8_11, zero_reg);
        tmp1 = _mm_and_si128(tmp0, H_8_11);
        tmp2 = _mm_andnot_si128(tmp0, H_8_11);
        H_8_11 = _mm_sub_epi16(tmp1, tmp2);    

        //H_8_11 = _mm_sad_epu8(H_8_11, zero_reg);
        
        tmp0 = _mm_cmpgt_epi16(H_12_13, zero_reg);
        tmp1 = _mm_and_si128(tmp0, H_12_13);
        tmp2 = _mm_andnot_si128(tmp0, H_12_13);
        H_12_13 = _mm_sub_epi16(tmp1, tmp2);
        
        //H_12_13 = _mm_sad_epu8(H_12_13, zero_reg);

        VME_ALIGN_DECL(16) short h[16];
        VME_ALIGN_DECL(16) short s[8];
        
        // Extract 16-bit values from 128-bit MMX registers
        _mm_store_si128((__m128i*)&h[0], H_0_7);
        //h[0] = _mm_extract_epi16(H_0_7, 0);
        //h[1] = _mm_extract_epi16(H_0_7, 1);
        //h[2] = _mm_extract_epi16(H_0_7, 2);
        //h[3] = _mm_extract_epi16(H_0_7, 3);
        //h[4] = _mm_extract_epi16(H_0_7, 4);
        //h[5] = _mm_extract_epi16(H_0_7, 5);
        //h[6] = _mm_extract_epi16(H_0_7, 6);
        //h[7] = _mm_extract_epi16(H_0_7, 7);
        
        _mm_storel_epi64((__m128i*)&h[8], H_8_11);
        //h[8] = _mm_extract_epi16(H_8_11, 0);
        //h[9] = _mm_extract_epi16(H_8_11, 1);
        //h[10] = _mm_extract_epi16(H_8_11, 2);
        //h[11] = _mm_extract_epi16(H_8_11, 3);

        _mm_storel_epi64((__m128i*)&h[12], H_12_13);
        //h[12] = _mm_extract_epi16(H_12_13, 0);
        //h[13] = _mm_extract_epi16(H_12_13, 1);

        _mm_storel_epi64((__m128i*)&s[0], S_12_13);
        //s[0] = _mm_extract_epi16(S_12_13, 0);
        //s[1] = _mm_extract_epi16(S_12_13, 1);

        h[14] = (s[0]-s[1]) >> 1;

        sad += h[0];
        sad += h[1];
        sad += h[2];
        sad += h[3];

        sad += h[4];
        sad += h[5];
        sad += h[6];
        sad += h[7];

        sad += h[8];
        sad += h[9];
        sad += h[10];
        sad += h[11];

        sad += h[12];
        sad += h[13];
        
        int h14_neg = h[14]<0;
        sad += h[14]*(1-(h14_neg<<1));
                            
        h[15] = (s[0]+s[1]) >> 1;
        int h15_neg = h[15]<0;
        sad += h[15]*(1-(h15_neg<<1))*((int)(!(Vsta.MvFlags&AC_ONLY_HAAR)));
        
        sad >>= (11-DISTBIT4X4);

        int sad_gt = (sad > DISTMAX4X4);
        //_mm_empty();            
        return (sad_gt*DISTMAX4X4 + (1-sad_gt)*sad);
        }
#else
        for(i=0;i<4;i++){
            tmp[ 0] = src[0]-blk[0]; tmp[ 1] = src[1]-blk[1];
            tmp[ 2] = src[2]-blk[2]; tmp[ 3] = src[3]-blk[3]; 
            src += 16; blk += blkw; tmp += 4;
        }
        tmp[0] = (dif[ 0]-dif[ 1])>>1;    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3])>>1;    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5])>>1;    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7])>>1;    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9])>>1;    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11])>>1;    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13])>>1;    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15])>>1;    dif[7] = (dif[14]+dif[15]);

        // keep (DISTBIT4X4+1) bits;
        for(i=0;i<8;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i]; 

        tmp[0] = (dif[ 0]-dif[ 2])>>1;    dif[0] = (dif[ 0]+dif[ 2]);
        tmp[1] = (dif[ 1]-dif[ 3])>>1;    dif[1] = (dif[ 1]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 6])>>1;    dif[2] = (dif[ 4]+dif[ 6]);
        tmp[3] = (dif[ 5]-dif[ 7])>>1;    dif[3] = (dif[ 5]+dif[ 7]);

        // keep (DISTBIT4X4+1) bits;
        for(i=0;i<4;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i];
        
        tmp[2] = (dif[ 0]-dif[ 1])>>1;    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[3] = (dif[ 2]-dif[ 3])>>1;    dif[1] = (dif[ 2]+dif[ 3]);
        d += ((tmp[2]<0)?-tmp[2]:tmp[2])+((tmp[3]<0)?-tmp[3]:tmp[3]);
        
        tmp[1] = (dif[ 0]-dif[ 1])>>1;    tmp[0] = (Vsta.MvFlags&AC_ONLY_HAAR)? 0:(dif[ 0]+dif[ 1])>>1;
        d += ((tmp[0]<0)?-tmp[0]:tmp[0])+((tmp[1]<0)?-tmp[1]:tmp[1]);

        d >>= (11-DISTBIT4X4);
        return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
#endif

#else
    case 2:    //HAAR
        for(i=0;i<4;i++){
            tmp[ 0] = src[0]-blk[0]; tmp[ 1] = src[1]-blk[1];
            tmp[ 2] = src[2]-blk[2]; tmp[ 3] = src[3]-blk[3]; 
            src += 16; blk += blkw; tmp += 4;
        }
        tmp[0] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5]);    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7]);    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9]);    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11]);    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13]);    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15]);    dif[7] = (dif[14]+dif[15]);

        for(i=0;i<8;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i];

        tmp[0] = (dif[ 0]-dif[ 2]);    dif[0] = (dif[ 0]+dif[ 2]);
        tmp[1] = (dif[ 1]-dif[ 3]);    dif[1] = (dif[ 1]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 6]);    dif[2] = (dif[ 4]+dif[ 6]);
        tmp[3] = (dif[ 5]-dif[ 7]);    dif[3] = (dif[ 5]+dif[ 7]);

        for(i=0;i<4;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i];
        
        tmp[2] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[3] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        d += ((tmp[2]<0)?-tmp[2]:tmp[2])+((tmp[3]<0)?-tmp[3]:tmp[3]);
        
        tmp[1] = (dif[ 0]-dif[ 1]);    tmp[0] = (dif[ 0]+dif[ 1]);
        d += ((tmp[0]<0)?-tmp[0]:tmp[0])+((tmp[1]<0)?-tmp[1]:tmp[1]);

        d >>= (12-DISTBIT4X4);
        return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
#endif
    case 3:        //HADAMARD
        for(i=0;i<4;i++){
            tmp[ 0] = src[0]-blk[0]; tmp[ 1] = src[1]-blk[1];
            tmp[ 2] = src[2]-blk[2]; tmp[ 3] = src[3]-blk[3]; 
            src += 16; blk += blkw; tmp += 4;
        }
        tmp[0] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5]);    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7]);    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9]);    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11]);    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13]);    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15]);    dif[7] = (dif[14]+dif[15]);

        dif[ 8] = (dif[ 0]-dif[ 2]); dif[0] = (dif[ 0]+dif[ 2]);
        dif[ 9] = (dif[ 1]-dif[ 3]); dif[1] = (dif[ 1]+dif[ 3]);
        dif[10] = (dif[ 4]-dif[ 6]); dif[2] = (dif[ 4]+dif[ 6]);
        dif[11] = (dif[ 5]-dif[ 7]); dif[3] = (dif[ 5]+dif[ 7]);
        dif[12] = (tmp[ 0]-tmp[ 2]); dif[4] = (tmp[ 0]+tmp[ 2]);
        dif[13] = (tmp[ 1]-tmp[ 3]); dif[5] = (tmp[ 1]+tmp[ 3]);
        dif[14] = (tmp[ 4]-tmp[ 6]); dif[6] = (tmp[ 4]+tmp[ 6]);
        dif[15] = (tmp[ 5]-tmp[ 7]); dif[7] = (tmp[ 5]+tmp[ 7]);

        tmp[0] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5]);    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7]);    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9]);    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11]);    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13]);    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15]);    dif[7] = (dif[14]+dif[15]);

        dif[ 8] = (dif[ 0]-dif[ 2]); dif[0] = (dif[ 0]+dif[ 2]);
        dif[ 9] = (dif[ 1]-dif[ 3]); dif[1] = (dif[ 1]+dif[ 3]);
        dif[10] = (dif[ 4]-dif[ 6]); dif[2] = (dif[ 4]+dif[ 6]);
        dif[11] = (dif[ 5]-dif[ 7]); dif[3] = (dif[ 5]+dif[ 7]);
        dif[12] = (tmp[ 0]-tmp[ 2]); dif[4] = (tmp[ 0]+tmp[ 2]);
        dif[13] = (tmp[ 1]-tmp[ 3]); dif[5] = (tmp[ 1]+tmp[ 3]);
        dif[14] = (tmp[ 4]-tmp[ 6]); dif[6] = (tmp[ 4]+tmp[ 6]);
        dif[15] = (tmp[ 5]-tmp[ 7]); dif[7] = (tmp[ 5]+tmp[ 7]);

        for(i=0;i<16;i++) d += (dif[i]<0)?-dif[i]:dif[i];
        d >>= (12-DISTBIT4X4);
        return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
    default:    //MSE
        for(i=0; i<4; i++){
            d += (src[0]-blk[0])*(src[0]-blk[0]) 
               + (src[1]-blk[1])*(src[1]-blk[1]) 
               + (src[2]-blk[2])*(src[2]-blk[2]) 
               + (src[3]-blk[3])*(src[3]-blk[3]);
            src += 16;
            blk += blkw;
        }
        return d;
    }

    return d;
}

/*********************************************************************************/
int MEforGen75::GetSad4x4F(U8 *src, U8 *blk, int blkw)
/*********************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    int        d = 0;
    int        i;
    short    dif[24];
    short    *tmp = dif;

    switch(SadMethod){
    case 0: // SAD
        for(i=0; i<4; i++){
            d += ((src[0]>blk[0]) ? src[0]-blk[0] : blk[0]-src[0])
              +  ((src[1]>blk[1]) ? src[1]-blk[1] : blk[1]-src[1])
              +  ((src[2]>blk[2]) ? src[2]-blk[2] : blk[2]-src[2])
              +  ((src[3]>blk[3]) ? src[3]-blk[3] : blk[3]-src[3]);
            src += 32;
            blk += blkw;
        }
        d >>= (11-DISTBIT4X4); // Add this to improve quality for 10 bit case
         return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
//        return d;
#if 1
        // Newest Bspec
    case 2:    //HAAR
        for(i=0;i<4;i++){
            tmp[ 0] = src[0]-blk[0]; tmp[ 1] = src[1]-blk[1];
            tmp[ 2] = src[2]-blk[2]; tmp[ 3] = src[3]-blk[3]; 
            src += 32; blk += blkw; tmp += 4;
        }
        tmp[0] = (dif[ 0]-dif[ 1])>>1;    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3])>>1;    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5])>>1;    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7])>>1;    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9])>>1;    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11])>>1;    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13])>>1;    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15])>>1;    dif[7] = (dif[14]+dif[15]);

        // keep (DISTBIT4X4+1) bits;
        for(i=0;i<8;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i]; 

        tmp[0] = (dif[ 0]-dif[ 2])>>1;    dif[0] = (dif[ 0]+dif[ 2]);
        tmp[1] = (dif[ 1]-dif[ 3])>>1;    dif[1] = (dif[ 1]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 6])>>1;    dif[2] = (dif[ 4]+dif[ 6]);
        tmp[3] = (dif[ 5]-dif[ 7])>>1;    dif[3] = (dif[ 5]+dif[ 7]);

        // keep (DISTBIT4X4+1) bits;
        for(i=0;i<4;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i];
        
        tmp[2] = (dif[ 0]-dif[ 1])>>1;    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[3] = (dif[ 2]-dif[ 3])>>1;    dif[1] = (dif[ 2]+dif[ 3]);
        d += ((tmp[2]<0)?-tmp[2]:tmp[2])+((tmp[3]<0)?-tmp[3]:tmp[3]);
        
        tmp[1] = (dif[ 0]-dif[ 1])>>1;    tmp[0] = (dif[ 0]+dif[ 1])>>1;
        d += ((tmp[0]<0)?-tmp[0]:tmp[0])+((tmp[1]<0)?-tmp[1]:tmp[1]);

        d >>= (11-DISTBIT4X4);
        return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
#else
    case 2:    //HAAR
        for(i=0;i<4;i++){
            tmp[ 0] = src[0]-blk[0]; tmp[ 1] = src[1]-blk[1];
            tmp[ 2] = src[2]-blk[2]; tmp[ 3] = src[3]-blk[3]; 
            src += 32; blk += blkw; tmp += 4;
        }
        tmp[0] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5]);    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7]);    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9]);    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11]);    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13]);    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15]);    dif[7] = (dif[14]+dif[15]);

        for(i=0;i<8;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i];

        tmp[0] = (dif[ 0]-dif[ 2]);    dif[0] = (dif[ 0]+dif[ 2]);
        tmp[1] = (dif[ 1]-dif[ 3]);    dif[1] = (dif[ 1]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 6]);    dif[2] = (dif[ 4]+dif[ 6]);
        tmp[3] = (dif[ 5]-dif[ 7]);    dif[3] = (dif[ 5]+dif[ 7]);

        for(i=0;i<4;i++) d += (tmp[i]<0)?-tmp[i]:tmp[i];
        
        tmp[2] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[3] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        d += ((tmp[2]<0)?-tmp[2]:tmp[2])+((tmp[3]<0)?-tmp[3]:tmp[3]);
        
        tmp[1] = (dif[ 0]-dif[ 1]);    tmp[0] = (dif[ 0]+dif[ 1]);
        d += ((tmp[0]<0)?-tmp[0]:tmp[0])+((tmp[1]<0)?-tmp[1]:tmp[1]);

        d >>= (12-DISTBIT4X4);
        return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
#endif
    case 3:        //HADAMARD
        for(i=0;i<4;i++){
            tmp[ 0] = src[0]-blk[0]; tmp[ 1] = src[1]-blk[1];
            tmp[ 2] = src[2]-blk[2]; tmp[ 3] = src[3]-blk[3]; 
            src += 32; blk += blkw; tmp += 4;
        }
        tmp[0] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5]);    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7]);    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9]);    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11]);    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13]);    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15]);    dif[7] = (dif[14]+dif[15]);

        dif[ 8] = (dif[ 0]-dif[ 2]); dif[0] = (dif[ 0]+dif[ 2]);
        dif[ 9] = (dif[ 1]-dif[ 3]); dif[1] = (dif[ 1]+dif[ 3]);
        dif[10] = (dif[ 4]-dif[ 6]); dif[2] = (dif[ 4]+dif[ 6]);
        dif[11] = (dif[ 5]-dif[ 7]); dif[3] = (dif[ 5]+dif[ 7]);
        dif[12] = (tmp[ 0]-tmp[ 2]); dif[4] = (tmp[ 0]+tmp[ 2]);
        dif[13] = (tmp[ 1]-tmp[ 3]); dif[5] = (tmp[ 1]+tmp[ 3]);
        dif[14] = (tmp[ 4]-tmp[ 6]); dif[6] = (tmp[ 4]+tmp[ 6]);
        dif[15] = (tmp[ 5]-tmp[ 7]); dif[7] = (tmp[ 5]+tmp[ 7]);

        tmp[0] = (dif[ 0]-dif[ 1]);    dif[0] = (dif[ 0]+dif[ 1]);
        tmp[1] = (dif[ 2]-dif[ 3]);    dif[1] = (dif[ 2]+dif[ 3]);
        tmp[2] = (dif[ 4]-dif[ 5]);    dif[2] = (dif[ 4]+dif[ 5]);
        tmp[3] = (dif[ 6]-dif[ 7]);    dif[3] = (dif[ 6]+dif[ 7]);
        tmp[4] = (dif[ 8]-dif[ 9]);    dif[4] = (dif[ 8]+dif[ 9]);
        tmp[5] = (dif[10]-dif[11]);    dif[5] = (dif[10]+dif[11]);
        tmp[6] = (dif[12]-dif[13]);    dif[6] = (dif[12]+dif[13]);
        tmp[7] = (dif[14]-dif[15]);    dif[7] = (dif[14]+dif[15]);

        dif[ 8] = (dif[ 0]-dif[ 2]); dif[0] = (dif[ 0]+dif[ 2]);
        dif[ 9] = (dif[ 1]-dif[ 3]); dif[1] = (dif[ 1]+dif[ 3]);
        dif[10] = (dif[ 4]-dif[ 6]); dif[2] = (dif[ 4]+dif[ 6]);
        dif[11] = (dif[ 5]-dif[ 7]); dif[3] = (dif[ 5]+dif[ 7]);
        dif[12] = (tmp[ 0]-tmp[ 2]); dif[4] = (tmp[ 0]+tmp[ 2]);
        dif[13] = (tmp[ 1]-tmp[ 3]); dif[5] = (tmp[ 1]+tmp[ 3]);
        dif[14] = (tmp[ 4]-tmp[ 6]); dif[6] = (tmp[ 4]+tmp[ 6]);
        dif[15] = (tmp[ 5]-tmp[ 7]); dif[7] = (tmp[ 5]+tmp[ 7]);

        for(i=0;i<16;i++) d += (dif[i]<0)?-dif[i]:dif[i];
        d >>= (12-DISTBIT4X4);
        return ((d > DISTMAX4X4)? DISTMAX4X4 : d);
    default:    //MSE
        for(int i=0; i<4; i++){
            d += (src[0]-blk[0])*(src[0]-blk[0]) 
               + (src[1]-blk[1])*(src[1]-blk[1]) 
               + (src[2]-blk[2])*(src[2]-blk[2]) 
               + (src[3]-blk[3])*(src[3]-blk[3]);
            src += 32;
            blk += blkw;
        }
        return d;
    }

    return d;
    */
}

/*********************************************************************************/
U32 MEforGen75::GetFtq4x4(U8 *src, U8 *blk, int blkw)
/*********************************************************************************/
{
    //int        d = 0;
    int        i;
    //short    dif[24];
    //short    *tmp = dif;

    I16 psrcrow[4][4];
    I16 pdstrow[4][4];
    I16 a[4];

    for(i=0; i<4; i++){
        psrcrow[i][0] = src[0]-blk[0];
        psrcrow[i][1] = src[1]-blk[1];
        psrcrow[i][2] = src[2]-blk[2];
        psrcrow[i][3] = src[3]-blk[3];
          
        src += 16;
        blk += blkw;
    }

    /* horizontal */
    for (i=0; i<4; i++)
    {
        a[0] = (I16) (psrcrow[i][0] + psrcrow[i][3]);
        a[3] = (I16) (psrcrow[i][0] - psrcrow[i][3]);
        a[1] = (I16) (psrcrow[i][1] + psrcrow[i][2]);
        a[2] = (I16) (psrcrow[i][1] - psrcrow[i][2]);

        pdstrow[i][0] = (I16) (a[0] + a[1]);
        pdstrow[i][1] = (I16) (2 * a[3] + a[2]);
        pdstrow[i][2] = (I16) (a[0] - a[1]);
        pdstrow[i][3] = (I16) (a[3] - 2 * a[2]);
    }

    /* vertical */
    for (i=0; i<4; i++)
    {
        a[0] = (I16) (pdstrow[0][i] + pdstrow[3][i]);
        a[3] = (I16) (pdstrow[0][i] - pdstrow[3][i]);
        a[1] = (I16) (pdstrow[1][i] + pdstrow[2][i]);
        a[2] = (I16) (pdstrow[1][i] - pdstrow[2][i]);

        pdstrow[0][i] = (I16) (a[0] + a[1]);
        pdstrow[1][i] = (I16) (2 * a[3] + a[2]);
        pdstrow[2][i] = (I16) (a[0] - a[1]);
        pdstrow[3][i] = (I16) (a[3] - 2 * a[2]);
    }
    
    U32 threshold[4][4];
    threshold[0][0] =                                                        Vsta.FTXCoeffThresh_DC;
    threshold[0][1] = threshold[1][0] =                                        Vsta.FTXCoeffThresh[0];
    threshold[2][0] = threshold[1][1] = threshold[0][2] =                    Vsta.FTXCoeffThresh[1];
    threshold[3][0] = threshold[2][1] = threshold[1][2] = threshold[0][3] = Vsta.FTXCoeffThresh[2];
    threshold[3][1] = threshold[2][2] = threshold[1][3] =                    Vsta.FTXCoeffThresh[3];
    threshold[3][2] = threshold[2][3] =                                        Vsta.FTXCoeffThresh[4];
    threshold[3][3] =                                                        Vsta.FTXCoeffThresh[5];

    // Threshold as proxy for quantization
    U8 countNZC = 0;
    U32 diff=0;
    I32 temp = 0;
    for (i=0; i<4; i++)
    {    
        pdstrow[i][0] = ((pdstrow[i][0] < 0) ? -pdstrow[i][0] : pdstrow[i][0]);
        temp = pdstrow[i][0]-threshold[i][0]; if(temp > 0){ countNZC++; diff += temp; }
        pdstrow[i][1] = ((pdstrow[i][1] < 0) ? -pdstrow[i][1] : pdstrow[i][1]);
        temp = pdstrow[i][1]-threshold[i][1]; if(temp > 0){ countNZC++; diff += temp; }
        pdstrow[i][2] = ((pdstrow[i][2] < 0) ? -pdstrow[i][2] : pdstrow[i][2]);
        temp = pdstrow[i][2]-threshold[i][2]; if(temp > 0){ countNZC++; diff += temp; }
        pdstrow[i][3] = ((pdstrow[i][3] < 0) ? -pdstrow[i][3] : pdstrow[i][3]);
        temp = pdstrow[i][3]-threshold[i][3]; if(temp > 0){ countNZC++; diff += temp; }
    }

    U32 out = diff;
    out |= (countNZC << 16);
    return out;
}

/* ****************************************************************************** *\
   FUNCTION: DoFTChroma2x2
   DESCRIPTION: 
\* ****************************************************************************** */
/* // Chroma to use same transform as luma => low-cost
void AvcPakMfdVft::DoFTChroma2x2(
      const VftIf &in_VftIf,
      CoeffsType *out_pTransformResult)
{
      bool bNeedTransform = 1;
      CoeffsType pTBuf[4];
      const CoeffsType* pDiffBuf = in_VftIf.pDiffBuf;

    if (bNeedTransform)
    {
        pTBuf[0] = (I16) ((pDiffBuf[0] + pDiffBuf[2]) + (pDiffBuf[1] + pDiffBuf[3]));
        pTBuf[1] = (I16) ((pDiffBuf[0] + pDiffBuf[2]) - (pDiffBuf[1] + pDiffBuf[3]));
        pTBuf[2] = (I16) ((pDiffBuf[0] - pDiffBuf[2]) + (pDiffBuf[1] - pDiffBuf[3]));
        pTBuf[3] = (I16) ((pDiffBuf[0] - pDiffBuf[2]) - (pDiffBuf[1] - pDiffBuf[3]));
    }

      MFX_INTERNAL_CPY(out_pTransformResult, pTBuf, sizeof(pTBuf));
}
*/