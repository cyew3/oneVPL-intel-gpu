/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/

/* NOTE - this implementation requires SSE4.1 (PINSRD, PEXTRD, PMOVZWBW) */

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)


// Inverse HEVC Transform functions optimised by intrinsics
// Size: 32x32
#include <nmmintrin.h>
#include <immintrin.h>
#include "mfx_h265_transform_opt.h"

namespace MFX_HEVC_ENCODER
{

//#define _USE_SHORTCUT_ 1

// Load constants
//#define _mm_load_const _mm_load_si128  
#define _mm_load_const _mm_lddqu_si128   

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))
#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

// 32-bits constants
static signed int rounder_64[]   = {64, 64, 0, 0};
static signed int rounder_2048[] = {2048, 2048, 0, 0};

// 16-bits constants
static signed short koef0000[]  = {64, 64,  64,-64,  83, 36,  36,-83};
static signed short koef0001[]  = {89, 75,  75,-18,  50,-89,  18,-50};
static signed short koef0002[]  = {50, 18, -89,-50,  18, 75,  75,-89};
static signed short koef0003[]  = {90, 87,  87, 57,  80,  9,  70,-43};
static signed short koef0004[]  = {57,-80,  43,-90,  25,-70,   9,-25};
static signed short koef0005[]  = {80, 70,   9,-43, -70,-87, -87,  9};
static signed short koef0006[]  ={-25, 90,  57, 25,  90,-80,  43,-57};
static signed short koef0007[]  = {57, 43, -80,-90, -25, 57,  90, 25};
static signed short koef0008[]  = {-9,-87, -87, 70,  43,  9,  70,-80};
static signed short koef0009[]  = {25,  9, -70,-25,  90, 43, -80,-57};
static signed short koef0010[]  = {43, 70,   9,-80, -57, 87,  87,-90};
static signed short koef00010[] = {90, 90,  90, 82,  88, 67,  85, 46};
static signed short koef02030[] = {88, 85,  67, 46,  31,-13, -13,-67};
static signed short koef04050[] = {82, 78,  22, -4, -54,-82, -90,-73};
static signed short koef06070[] = {73, 67, -31,-54, -90,-78, -22, 38};
static signed short koef08090[] = {61, 54, -73,-85, -46, -4,  82, 88};
static signed short koef10110[] = {46, 38, -90,-88,  38, 73,  54, -4};
static signed short koef12130[] = {31, 22, -78,-61,  90, 85, -61,-90};
static signed short koef14150[] = {13,  4, -38,-13,  61, 22, -78,-31};
static signed short koef00011[] = {82, 22,  78, -4,  73,-31,  67,-54};
static signed short koef02031[] ={-54,-90, -82,-73, -90,-22, -78, 38};
static signed short koef04051[] ={-61, 13,  13, 85,  78, 67,  85,-22};
static signed short koef06071[] = {78, 85,  67,-22, -38,-90, -90,  4};
static signed short koef08091[] = {31,-46, -88,-61, -13, 82,  90, 13};
static signed short koef10111[] ={-90,-67,  31, 90,  61,-46, -88,-31};
static signed short koef12131[] = { 4, 73,  54,-38, -88, -4,  82, 46};
static signed short koef14151[] = {88, 38, -90,-46,  85, 54, -73,-61};
static signed short koef00012[] = {61,-73,  54,-85,  46,-90,  38,-88};
static signed short koef00013[] = {31,-78,  22,-61,  13,-38,   4,-13};
static signed short koef02032[] ={-46, 82,  -4, 88,  38, 54,  73, -4};
static signed short koef02033[] = {90,-61,  85,-90,  61,-78,  22,-31};
static signed short koef04052[] = {31,-88, -46,-61, -90, 31, -67, 90};
static signed short koef04053[] = { 4, 54,  73,-38,  88,-90,  38,-46};
static signed short koef06072[] ={-13, 90,  82, 13,  61,-88, -46,-31};
static signed short koef06073[] ={-88, 82,  -4, 46,  85,-73,  54,-61};
static signed short koef08092[] = {-4,-90, -90, 38,  22, 67,  85,-78};
static signed short koef08093[] ={-38,-22, -78, 90,  54,-31,  67,-73};
static signed short koef10112[] = {22, 85,  67,-78, -85, 13,  13, 61};
static signed short koef10113[] = {73,-90, -82, 54,   4, 22,  78,-82};
static signed short koef12132[] ={-38,-78, -22, 90,  73,-82, -90, 54}; 
static signed short koef12133[] = {67,-13, -13,-31, -46, 67,  85,-88};
static signed short koef14152[] = {54, 67, -31,-73,   4, 78,  22,-82};
static signed short koef14153[] ={-46, 85,  67,-88, -82, 90,  90,-90};

unsigned int repos[32] = 
  {0, 16,  8, 17, 4, 18,  9, 19, 
   2, 20, 10, 21, 5, 22, 11, 23, 
   1, 24, 12, 25, 6, 26, 13, 27,
   3, 28, 14, 29, 7, 30, 15, 31};

#define src_stride 32  // strade for temp[]

// 9200 CPU clocks. 
void DCTInverse32x32_sse_update(const short* __restrict src, short* __restrict dst, int dstStride, int shift0)
{
   __m128i __declspec(align(16)) buff[32*32];
   short __declspec(align(16)) temp[32*32];
   __m128i s0, s1, s2, s3, s4, s5, s6, s7;
   s1 = _mm_setzero_si128();
   s2 = _mm_setzero_si128();
   s3 = _mm_setzero_si128();
   s4 = _mm_setzero_si128();
   s5 = _mm_setzero_si128();

#ifdef _USE_SHORTCUT_
   // Prepare shortcut for empty columns. Calculate bitmask
   char zero[32]; // mask buffer
   __m128i z0, z1, z2, z3;
   char * buff = (char *)src;
   z0 = _mm_lddqu_si128((const __m128i *)(buff + 16*0));
   z1 = _mm_lddqu_si128((const __m128i *)(buff + 16*1));
   z2 = _mm_lddqu_si128((const __m128i *)(buff + 16*2));
   z3 = _mm_lddqu_si128((const __m128i *)(buff + 16*3));
   for (int i=0; i<31; i++) {
      buff += 16*4;
      z0 = _mm_or_si128(z0, _mm_lddqu_si128((const __m128i *)(buff + 16*0)));
      z1 = _mm_or_si128(z1, _mm_lddqu_si128((const __m128i *)(buff + 16*1)));
      z2 = _mm_or_si128(z2, _mm_lddqu_si128((const __m128i *)(buff + 16*2)));
      z3 = _mm_or_si128(z3, _mm_lddqu_si128((const __m128i *)(buff + 16*3)));
   }
   _mm_storel_epi64((__m128i *)&zero[ 0], _mm_packs_epi16(z0, z0));
   _mm_storel_epi64((__m128i *)&zero[ 8], _mm_packs_epi16(z1, z1));
   _mm_storel_epi64((__m128i *)&zero[16], _mm_packs_epi16(z2, z2));
   _mm_storel_epi64((__m128i *)&zero[24], _mm_packs_epi16(z3, z3));
#endif

   // Vertical 1-D inverse transform
   {
      #define shift       7
      #define rounder     rounder_64
      #define dest_stride 32

      signed short * dest  = temp;

      for (int i=0; i<32; i++) 
      {
#ifdef _USE_SHORTCUT_
         if (zero[i] == 0) { // check shortcut: is input colum empty?
            __m128i null = _mm_setzero_si128(); // _mm_xor_si128(null, null);
            _mm_storeu_si128((__m128i *)(dest + 16*0), null);
            _mm_storeu_si128((__m128i *)(dest + 16*1), null);
            _mm_storeu_si128((__m128i *)(dest + 16*2), null);
            _mm_storeu_si128((__m128i *)(dest + 16*3), null);
            continue;
         }
#endif
         int num = 4 * repos[i];

         s1 = _mm_insert_epi16(s1, (int)src[0*src_stride], 0);  // 0 0 0 0 0 0 0 a
         s1 = _mm_insert_epi16(s1, (int)src[16*src_stride], 1); // 0 0 0 0 0 0 b a
         s1 = _mm_insert_epi16(s1, (int)src[8*src_stride], 2);  // 0 0 0 0 0 c b a
         s1 = _mm_insert_epi16(s1, (int)src[24*src_stride], 3); // 0 0 0 0 d c b a
         s1 = _mm_unpacklo_epi32(s1, s1);                       // d c d c b a b a

         s1 = _mm_madd_epi16(s1, _mm_lddqu_si128((const __m128i *)koef0000));
         s1 = _mm_add_epi32(s1,  _mm_lddqu_si128((const __m128i *)rounder));

         s0 = _mm_shuffle_epi32(s1, 0x4E); 
         s1 = _mm_sub_epi32(s1, s0);
         s0 = _mm_add_epi32(s0, s0);
         s0 = _mm_add_epi32(s0, s1);
         s1 = _mm_shuffle_epi32(s1, 1); 
         s0 = _mm_unpacklo_epi64(s0, s1);

         s2 = _mm_insert_epi16(s2, (int)src[ 4*src_stride], 0); // 0 0 0 0 0 0 0 a
         s2 = _mm_insert_epi16(s2, (int)src[12*src_stride], 1); // 0 0 0 0 0 0 b a
         s2 = _mm_insert_epi16(s2, (int)src[20*src_stride], 2); // 0 0 0 0 0 c b a
         s2 = _mm_insert_epi16(s2, (int)src[28*src_stride], 3); // 0 0 0 0 d c b a
         s4 = _mm_shuffle_epi32(s2, 0x00);                      // b a b a b a b a
         s5 = _mm_shuffle_epi32(s2, 0x55);                      // d c d c d c d c 

         s4 = _mm_madd_epi16(s4, _mm_lddqu_si128((const __m128i *)koef0001));
         s5 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef0002));
         s4 = _mm_add_epi32(s4,  s5);

         s1 = _mm_shuffle_epi32(s0, 0x1B); 
         s7 = _mm_shuffle_epi32(s4, 0x1B); 
         s0 = _mm_add_epi32(s0,  s4);
         s1 = _mm_sub_epi32(s1,  s7);

         s3 = _mm_insert_epi16(s3, (int)src[2*src_stride], 0);
         s3 = _mm_insert_epi16(s3, (int)src[6*src_stride], 1);
         s3 = _mm_insert_epi16(s3, (int)src[10*src_stride], 2);
         s3 = _mm_insert_epi16(s3, (int)src[14*src_stride], 3);
         s3 = _mm_insert_epi16(s3, (int)src[18*src_stride], 4);
         s3 = _mm_insert_epi16(s3, (int)src[22*src_stride], 5);
         s3 = _mm_insert_epi16(s3, (int)src[26*src_stride], 6);
         s3 = _mm_insert_epi16(s3, (int)src[30*src_stride], 7);

         s5 = _mm_shuffle_epi32(s3, 0); 
         s4 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef0003));
         s5 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef0004));

         s6 = _mm_shuffle_epi32(s3, 0x55); 
         s7 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0005));
         s4 = _mm_add_epi32(s4,  s7);

         s6 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0006));
         s5 = _mm_add_epi32(s5,  s6);

         s6 = _mm_shuffle_epi32(s3, 0xAA); 
         s7 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0007));
         s4 = _mm_add_epi32(s4,  s7);
         s6 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0008));
         s5 = _mm_add_epi32(s5,  s6);

         s6 = _mm_shuffle_epi32(s3, 0xFF); 
         s7 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0009));
         s4 = _mm_add_epi32(s4,  s7);
         s6 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0010));
         s5 = _mm_add_epi32(s5,  s6);

         s2 = _mm_shuffle_epi32(s0, 0x1B); 
         s3 = _mm_shuffle_epi32(s1, 0x1B); 
         s6 = _mm_shuffle_epi32(s4, 0x1B); 
         s7 = _mm_shuffle_epi32(s5, 0x1B); 

         s0 = _mm_add_epi32(s0,  s4);
         s1 = _mm_add_epi32(s1,  s5);
         s2 = _mm_sub_epi32(s2,  s6);
         s3 = _mm_sub_epi32(s3,  s7);

         s4 = _mm_insert_epi16(s4, (int)src[1*src_stride], 0);
         s4 = _mm_insert_epi16(s4, (int)src[3*src_stride], 1);
         s4 = _mm_insert_epi16(s4, (int)src[5*src_stride], 2);
         s4 = _mm_insert_epi16(s4, (int)src[7*src_stride], 3);
         s4 = _mm_insert_epi16(s4, (int)src[9*src_stride], 4);
         s4 = _mm_insert_epi16(s4, (int)src[11*src_stride], 5);
         s4 = _mm_insert_epi16(s4, (int)src[13*src_stride], 6);
         s4 = _mm_insert_epi16(s4, (int)src[15*src_stride], 7);

         s5 = _mm_shuffle_epi32(s4, 0); 
         s6 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef00010));
         s5 = _mm_shuffle_epi32(s4, 0x55); 
         s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef02030));
         s6 = _mm_add_epi32(s6,  s7);

         s5 = _mm_shuffle_epi32(s4, 0xAA); 
         s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef04050));
         s6 = _mm_add_epi32(s6,  s7);
         s5 = _mm_shuffle_epi32(s4, 0xFF); 
         s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef06070));
         s6 = _mm_add_epi32(s6,  s7);

         s5 = _mm_insert_epi16(s5, (int)src[17*src_stride], 0);
         s5 = _mm_insert_epi16(s5, (int)src[19*src_stride], 1);
         s5 = _mm_shuffle_epi32(s5, 0); 
         s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef08090));
         s6 = _mm_add_epi32(s6,  s7);

         s5 = _mm_insert_epi16(s5, (int)src[21*src_stride], 2);
         s5 = _mm_insert_epi16(s5, (int)src[23*src_stride], 3);
         s5 = _mm_insert_epi16(s5, (int)src[25*src_stride], 4);
         s5 = _mm_insert_epi16(s5, (int)src[27*src_stride], 5);
         s5 = _mm_insert_epi16(s5, (int)src[29*src_stride], 6);
         s5 = _mm_insert_epi16(s5, (int)src[31*src_stride], 7);

         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0x55), _mm_lddqu_si128((const __m128i *)koef10110)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xAA), _mm_lddqu_si128((const __m128i *)koef12130)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xFF), _mm_lddqu_si128((const __m128i *)koef14150)));
         s7 = _mm_shuffle_epi32(s0, 0x1B); 
         s0 = _mm_add_epi32(s0,  s6);
         s6 = _mm_shuffle_epi32(s6, 0x1B); 
         s7 = _mm_sub_epi32(s7,  s6);

         s0 = _mm_srai_epi32(s0,  shift);            // (r03 r02 r01 r00) >>= shift
         s7 = _mm_srai_epi32(s7,  shift);            // (r31 r30 r29 r28) >>= shift
         s0 = _mm_packs_epi32(s0, s7);               // clip(-32768, 32767)

         // Store 1
         buff[num++] = s0;

         s6 = _mm_madd_epi16(_mm_shuffle_epi32(s4, 0), _mm_lddqu_si128((const __m128i *)koef00011));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s4, 0x55), _mm_lddqu_si128((const __m128i *)koef02031)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s4, 0xAA), _mm_lddqu_si128((const __m128i *)koef04051)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s4, 0xFF), _mm_lddqu_si128((const __m128i *)koef06071)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0x00), _mm_lddqu_si128((const __m128i *)koef08091)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0x55), _mm_lddqu_si128((const __m128i *)koef10111)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xAA), _mm_lddqu_si128((const __m128i *)koef12131)));
         s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xFF), _mm_lddqu_si128((const __m128i *)koef14151)));

         s7 = _mm_shuffle_epi32(s1, 0x1B);
         s1 = _mm_add_epi32(s1,  s6);
         s6 = _mm_shuffle_epi32(s6, 0x1B);
         s7 = _mm_sub_epi32(s7,  s6);

         s1 = _mm_packs_epi32(_mm_srai_epi32(s1,  shift), _mm_srai_epi32(s7,  shift));

         // Store 2
         buff[num++] = s1;

         s1 = _mm_shuffle_epi32(s4, 0x00);
         s0 = _mm_madd_epi16(s1, _mm_lddqu_si128((const __m128i *)koef00012));
         s1 = _mm_madd_epi16(s1, _mm_lddqu_si128((const __m128i *)koef00013));

         s7 = _mm_shuffle_epi32(s4, 0x55);
         s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef02032)));
         s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef02033)));

         s7 = _mm_shuffle_epi32(s4, 0xAA);
         s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef04052)));
         s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef04053)));

         s7 = _mm_shuffle_epi32(s4, 0xFF);
         s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef06072)));
         s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef06073)));

         s7 = _mm_shuffle_epi32(s5, 0x00);
         s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef08092)));
         s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef08093)));

         s7 = _mm_shuffle_epi32(s5, 0x55);
         s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef10112)));
         s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef10113)));

         s7 = _mm_shuffle_epi32(s5, 0xAA);
         s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef12132)));
         s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef12133)));

         s7 = _mm_shuffle_epi32(s5, 0xFF);
         s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef14152)));
         s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef14153)));

         s6 = _mm_shuffle_epi32(s3, 0x1B);
         s3 = _mm_add_epi32(s3,  s0);
         s0 = _mm_shuffle_epi32(s0, 0x1B);
         s6 = _mm_sub_epi32(s6,  s0);
         s3 = _mm_packs_epi32(_mm_srai_epi32(s3,  shift), _mm_srai_epi32(s6,  shift));

         // Store 3
         buff[num++] = s3;

         s7 = _mm_shuffle_epi32(s2, 0x1B);
         s2 = _mm_add_epi32(s2,  s1);
         s1 = _mm_shuffle_epi32(s1, 0x1B);
         s7 = _mm_sub_epi32(s7,  s1);
         s2 = _mm_packs_epi32(_mm_srai_epi32(s2,  shift), _mm_srai_epi32(s7,  shift));

         // Store 4
         buff[num++] = s2;
         src++;
      }
      #undef  shift
      #undef  rounder
      #undef  dest_stride
   }

   signed short * dest  = temp;
   __m128i * in = buff;
   #define dest_stride 32

   for (int i=0; i<8; i++)
   {
      // load buffer short[8x4], rotate 2x4x4, save short[4x8]. Repeat 4 times
      s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4)); // 07 06 05 04 03 02 01 00
      s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4)); // 17 16 15 14 13 12 11 10
      s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4)); // 27 26 25 24 23 22 21 20
      s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4)); // 37 36 35 34 33 32 31 30
      s6 = s0;
      s0 = _mm_unpacklo_epi16(s0, s1);  // 13 03 12 02 11 01 10 00
      s6 = _mm_unpackhi_epi16(s6, s1);  // 17 07 16 06 15 05 14 04
      s5 = s2;
      s4 = s0;
      s2 = _mm_unpacklo_epi16(s2, s3);  // 33 23 32 22 31 21 30 20
      s5 = _mm_unpackhi_epi16(s5, s3);  // 37 27 36 26 35 25 34 24
      s0 = _mm_unpacklo_epi32(s0, s2);  // 31 21 11 01 30 20 10 00
      _mm_storel_epi64((__m128i*)(&dest[ 0*dest_stride]), s0);
      _mm_storeh_epi64((__m128i*)(&dest[ 1*dest_stride]), s0);
      s7 = s6;
      s4 = _mm_unpackhi_epi32(s4, s2);  // 33 23 13 03 32 22 12 02
      _mm_storel_epi64((__m128i*)(&dest[ 2*dest_stride]), s4);
      _mm_storeh_epi64((__m128i*)(&dest[ 3*dest_stride]), s4);
      s6 = _mm_unpacklo_epi32(s6, s5);  // 35 25 15 05 34 24 14 04
      _mm_storel_epi64((__m128i*)(&dest[28*dest_stride]), s6);
      _mm_storeh_epi64((__m128i*)(&dest[29*dest_stride]), s6);
      s7 = _mm_unpackhi_epi32(s7, s5);  // 37 27 17 07 36 26 16 06
      _mm_storel_epi64((__m128i*)(&dest[30*dest_stride]), s7);
      _mm_storeh_epi64((__m128i*)(&dest[31*dest_stride]), s7);

      // 1th
      s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4 + 1));
      s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4 + 1));
      s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4 + 1));
      s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4 + 1));
      s6 = s0;
      s0 = _mm_unpacklo_epi16(s0, s1);
      s6 = _mm_unpackhi_epi16(s6, s1);
      s5 = s2;
      s4 = s0;
      s2 = _mm_unpacklo_epi16(s2, s3);
      s5 = _mm_unpackhi_epi16(s5, s3);
      s0 = _mm_unpacklo_epi32(s0, s2);
      _mm_storel_epi64((__m128i*)(&dest[ 4*dest_stride]), s0);
      _mm_storeh_epi64((__m128i*)(&dest[ 5*dest_stride]), s0);
      s7 = s6;
      s4 = _mm_unpackhi_epi32(s4, s2);
      _mm_storel_epi64((__m128i*)(&dest[ 6*dest_stride]), s4);
      _mm_storeh_epi64((__m128i*)(&dest[ 7*dest_stride]), s4);
      s6 = _mm_unpacklo_epi32(s6, s5);
      _mm_storel_epi64((__m128i*)(&dest[24*dest_stride]), s6);
      _mm_storeh_epi64((__m128i*)(&dest[25*dest_stride]), s6);
      s7 = _mm_unpackhi_epi32(s7, s5);
      _mm_storel_epi64((__m128i*)(&dest[26*dest_stride]), s7);
      _mm_storeh_epi64((__m128i*)(&dest[27*dest_stride]), s7);

      // 2th
      s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4 + 2));
      s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4 + 2));
      s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4 + 2));
      s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4 + 2));
      s6 = s0;
      s0 = _mm_unpacklo_epi16(s0, s1);
      s6 = _mm_unpackhi_epi16(s6, s1);
      s5 = s2;
      s4 = s0;
      s2 = _mm_unpacklo_epi16(s2, s3);
      s5 = _mm_unpackhi_epi16(s5, s3);
      s0 = _mm_unpacklo_epi32(s0, s2);
      _mm_storel_epi64((__m128i*)(&dest[ 8*dest_stride]), s0);
      _mm_storeh_epi64((__m128i*)(&dest[ 9*dest_stride]), s0);
      s7 = s6;
      s4 = _mm_unpackhi_epi32(s4, s2);
      _mm_storel_epi64((__m128i*)(&dest[10*dest_stride]), s4);
      _mm_storeh_epi64((__m128i*)(&dest[11*dest_stride]), s4);
      s6 = _mm_unpacklo_epi32(s6, s5);
      _mm_storel_epi64((__m128i*)(&dest[20*dest_stride]), s6);
      _mm_storeh_epi64((__m128i*)(&dest[21*dest_stride]), s6);
      s7 = _mm_unpackhi_epi32(s7, s5);
      _mm_storel_epi64((__m128i*)(&dest[22*dest_stride]), s7);
      _mm_storeh_epi64((__m128i*)(&dest[23*dest_stride]), s7);

      // 3th
      s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4 + 3));
      s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4 + 3));
      s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4 + 3));
      s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4 + 3));
      s6 = s0;
      s0 = _mm_unpacklo_epi16(s0, s1);
      s6 = _mm_unpackhi_epi16(s6, s1);
      s5 = s2;
      s4 = s0;
      s2 = _mm_unpacklo_epi16(s2, s3);
      s5 = _mm_unpackhi_epi16(s5, s3);
      s0 = _mm_unpacklo_epi32(s0, s2);
      _mm_storel_epi64((__m128i*)(&dest[12*dest_stride]), s0);
      _mm_storeh_epi64((__m128i*)(&dest[13*dest_stride]), s0);
      s7 = s6;
      s4 = _mm_unpackhi_epi32(s4, s2);
      _mm_storel_epi64((__m128i*)(&dest[14*dest_stride]), s4);
      _mm_storeh_epi64((__m128i*)(&dest[15*dest_stride]), s4);
      s6 = _mm_unpacklo_epi32(s6, s5);
      _mm_storel_epi64((__m128i*)(&dest[16*dest_stride]), s6);
      _mm_storeh_epi64((__m128i*)(&dest[17*dest_stride]), s6);
      s7 = _mm_unpackhi_epi32(s7, s5);
      _mm_storel_epi64((__m128i*)(&dest[18*dest_stride]), s7);
      _mm_storeh_epi64((__m128i*)(&dest[19*dest_stride]), s7);
      dest += 4;
      in += 16;
   }

   // Horizontal 1-D inverse transform
   {
      #define shift       12
      #define rounder     rounder_2048
      #define dest_stride dstStride

      __m128i s0_, s1_, s6_;

      const signed short * dest = dst;
      src  = temp;

      for (int i=0; i<32; i++) {
         // load first 8 words
         s7 = _mm_load_si128((const __m128i *)(src)); src += 8;
         s1 = _mm_unpacklo_epi32(s7, s7);
         s1 = _mm_madd_epi16(s1, _mm_load_const((const __m128i *)koef0000));
         s1 = _mm_add_epi32(s1,  _mm_load_const((const __m128i *)rounder));

         s0 = _mm_shuffle_epi32(s1, 0x4E); 
         s1 = _mm_sub_epi32(s1, s0);
         s0 = _mm_add_epi32(s0, s0);
         s0 = _mm_add_epi32(s0, s1);
         s1 = _mm_shuffle_epi32(s1, 1); 
         s0 = _mm_unpacklo_epi64(s0, s1);

         s4 = _mm_shuffle_epi32(s7, 0xAA); 
         s5 = _mm_shuffle_epi32(s7, 0xFF); 

         s4 = _mm_madd_epi16(s4, _mm_load_const((const __m128i *)koef0001));
         s5 = _mm_madd_epi16(s5, _mm_load_const((const __m128i *)koef0002));
         s4 = _mm_add_epi32(s4,  s5);

         s1 = _mm_shuffle_epi32(s0, 0x1B); 
         s7 = _mm_shuffle_epi32(s4, 0x1B); 
         s0 = _mm_add_epi32(s0,  s4);
         s1 = _mm_sub_epi32(s1,  s7);

         // load next 8 words
         s3 = _mm_load_si128((const __m128i *)(src)); src += 8;
         s5 = _mm_shuffle_epi32(s3, 0); 
         s4 = _mm_madd_epi16(s5, _mm_load_const((const __m128i *)koef0003));
         s5 = _mm_madd_epi16(s5, _mm_load_const((const __m128i *)koef0004));

         s6 = _mm_shuffle_epi32(s3, 0x55); 
         s7 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0005));
         s4 = _mm_add_epi32(s4,  s7);
         s6 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0006));
         s5 = _mm_add_epi32(s5,  s6);

         s6 = _mm_shuffle_epi32(s3, 0xAA); 
         s7 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0007));
         s4 = _mm_add_epi32(s4,  s7);
         s6 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0008));
         s5 = _mm_add_epi32(s5,  s6);

         s6 = _mm_shuffle_epi32(s3, 0xFF); 
         s7 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0009));
         s4 = _mm_add_epi32(s4,  s7);
         s6 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0010));
         s5 = _mm_add_epi32(s5,  s6);

         s2 = _mm_shuffle_epi32(s0, 0x1B); 
         s3 = _mm_shuffle_epi32(s1, 0x1B); 
         s6 = _mm_shuffle_epi32(s4, 0x1B); 
         s7 = _mm_shuffle_epi32(s5, 0x1B); 

         s0 = _mm_add_epi32(s0,  s4);
         s1 = _mm_add_epi32(s1,  s5);
         s2 = _mm_sub_epi32(s2,  s6);
         s3 = _mm_sub_epi32(s3,  s7);

         // load 3th 8 words
         s4  = _mm_load_si128((const __m128i *)(src)); src += 8;
         s7  = _mm_shuffle_epi32(s4, 0);
         s6  = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00010));
         s6_ = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00011));
         s0_ = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00012));
         s1_ = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00013));

         s7  = _mm_shuffle_epi32(s4, 0x55);
         s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02030)));
         s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02031)));
         s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02032)));
         s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02033)));

         s7  = _mm_shuffle_epi32(s4, 0xAA);
         s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04050)));
         s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04051)));
         s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04052)));
         s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04053)));

         s7  = _mm_shuffle_epi32(s4, 0xFF);
         s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06070)));
         s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06071)));
         s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06072)));
         s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06073)));

         // load last 8 words
         s5  = _mm_load_si128((const __m128i *)(src)); src += 8;
         s7  = _mm_shuffle_epi32(s5, 0);
         s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08090)));
         s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08091)));
         s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08092)));
         s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08093)));

         s7  = _mm_shuffle_epi32(s5, 0x55);
         s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10110)));
         s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10111)));
         s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10112)));
         s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10113)));

         s7  = _mm_shuffle_epi32(s5, 0xAA);
         s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12130)));
         s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12131)));
         s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12132)));
         s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12133)));

         s7  = _mm_shuffle_epi32(s5, 0xFF);
         s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14150)));
         s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14151)));
         s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14152)));
         s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14153)));

         s7 = _mm_shuffle_epi32(s0, 0x1B); 
         s0 = _mm_add_epi32(s0,  s6);
         s6 = _mm_shuffle_epi32(s6, 0x1B); 
         s7 = _mm_sub_epi32(s7,  s6);

         s0 = _mm_srai_epi32(s0,  shift);           // (r03 r02 r01 r00) >>= shift
         s7 = _mm_srai_epi32(s7,  shift);           // (r31 r30 r29 r28) >>= shift
         s0 = _mm_packs_epi32(s0, s7);              // clip(-32768, 32767)
         _mm_storel_epi64((__m128i *)&dest[0], s0);  // Store r03 r02 r01 r00
         _mm_storeh_epi64((__m128i *)&dest[28], s0); // Store r31 r30 r29 r28

         s7 = _mm_shuffle_epi32(s1, 0x1B);
         s1 = _mm_add_epi32(s1,  s6_);
         s6 = _mm_shuffle_epi32(s6_, 0x1B);
         s7 = _mm_sub_epi32(s7,  s6);

         s1 = _mm_packs_epi32(_mm_srai_epi32(s1,  shift), _mm_srai_epi32(s7,  shift));
         _mm_storel_epi64((__m128i *)&dest[4], s1);
         _mm_storeh_epi64((__m128i *)&dest[24], s1);

         s6 = _mm_shuffle_epi32(s3, 0x1B);
         s3 = _mm_add_epi32(s3,  s0_);
         s0 = _mm_shuffle_epi32(s0_, 0x1B);
         s6 = _mm_sub_epi32(s6,  s0);
         s3 = _mm_packs_epi32(_mm_srai_epi32(s3,  shift), _mm_srai_epi32(s6,  shift));
         _mm_storel_epi64((__m128i *)&dest[8], s3);
         _mm_storeh_epi64((__m128i *)&dest[20], s3);

         s7 = _mm_shuffle_epi32(s2, 0x1B);
         s2 = _mm_add_epi32(s2,  s1_);
         s1 = _mm_shuffle_epi32(s1_, 0x1B);
         s7 = _mm_sub_epi32(s7,  s1);
         s2 = _mm_packs_epi32(_mm_srai_epi32(s2,  shift), _mm_srai_epi32(s7,  shift));
         _mm_storeu_si128((__m128i *)&dest[12], s2);
         dest += dest_stride;
      }
      #undef  shift
      #undef  rounder
      #undef  dest_stride
   }
}

} // end namespace MFX_HEVC_ENCODER

#endif // (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
