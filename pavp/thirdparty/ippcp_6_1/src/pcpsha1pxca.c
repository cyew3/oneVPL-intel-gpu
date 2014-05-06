/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Message block processing according to SHA1
//
// Contents:
//    ProcessSHA1()
//    UpdateSHA1()
//
//
//    Created: Thu 07-Mar-2002 18:52
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpshs.h"
#include "pcpciphertool.h"


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2))
void ProcessSHA1(Ipp8u* buffer, DigestSHA1 digest, const Ipp8u* pSrc, int len)
{
   if( IPP_UINT_PTR(pSrc) & 0x3) {
      while(len) {
         CopyBlock(pSrc, buffer, MBS_SHA1);
         UpdateSHA1(buffer, digest);
         pSrc += MBS_SHA1;
         len  -= MBS_SHA1;
      }
   }
   else {
      while(len) {
         UpdateSHA1(pSrc, digest);
         pSrc += MBS_SHA1;
         len  -= MBS_SHA1;
      }
   }
}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )
/*
// Magic functions defined in FIPS 180-1
//
*/
#define MAGIC_F0(B,C,D) (((B) & (C)) | ((~(B)) & (D)))
#define MAGIC_F1(B,C,D) ((B) ^ (C) ^ (D))
#define MAGIC_F2(B,C,D) (((B) & (C)) | ((B) & (D)) | ((C) & (D)))
#define MAGIC_F3(B,C,D) ((B) ^ (C) ^ (D))

#define SHA1_STEP(A,B,C,D,E, MAGIC_FUN, W,K) \
   (E)+= ROL32((A),5) + MAGIC_FUN((B),(C),(D)) + (W) + (K); \
   (B) = ROL32((B),30)


/*F*
//    Name: UpdateSHA1
//
// Purpose: Update internal digest according to message block.
//
// Parameters:
//    mblk        pointer to message block
//    digest      pointer to digest
//
*F*/
void UpdateSHA1(const Ipp8u* mblk, DigestSHA1 digest)
{
   #if defined(_IPP_TRS)
   const Ipp32u K_SHA1[] = FIPS_180_K_SHA1;
   #endif

   Ipp32u W[80];             /* reference FIPS 180-1 */
   Ipp32u A, B, C, D, E;     /* reference FIPS 180-1 */
   int    t;                 /* reference FIPS 180-1 */
   Ipp32u* data = (Ipp32u*)mblk;

   /*
   // expand message block
   */

   /* initialize the first 16 words in the array W (remember about endian) */
   for(t=0; t<16; t++) {
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      W[t] = data[t];
      #else
      W[t] = ENDIANNESS(data[t]);
      #endif
   }

   /* schedule another 80-16 words in the array W */
   for(; t<80; t++) {
      W[t] = ROL32(W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16], 1);
   }

   /* init A, B, C, D, E by the internal digest */
   A = digest[0];
   B = digest[1];
   C = digest[2];
   D = digest[3];
   E = digest[4];

   /* perform 0-19 steps */
   for(t=0; t<20; t+=5) {
      SHA1_STEP(A,B,C,D,E, MAGIC_F0, W[t  ],K_SHA1[0]);
      SHA1_STEP(E,A,B,C,D, MAGIC_F0, W[t+1],K_SHA1[0]);
      SHA1_STEP(D,E,A,B,C, MAGIC_F0, W[t+2],K_SHA1[0]);
      SHA1_STEP(C,D,E,A,B, MAGIC_F0, W[t+3],K_SHA1[0]);
      SHA1_STEP(B,C,D,E,A, MAGIC_F0, W[t+4],K_SHA1[0]);
   }

   /* perform 20-39 steps */
   for(; t<40; t+=5) {
      SHA1_STEP(A,B,C,D,E, MAGIC_F1, W[t  ],K_SHA1[1]);
      SHA1_STEP(E,A,B,C,D, MAGIC_F1, W[t+1],K_SHA1[1]);
      SHA1_STEP(D,E,A,B,C, MAGIC_F1, W[t+2],K_SHA1[1]);
      SHA1_STEP(C,D,E,A,B, MAGIC_F1, W[t+3],K_SHA1[1]);
      SHA1_STEP(B,C,D,E,A, MAGIC_F1, W[t+4],K_SHA1[1]);
   }

   /* perform 40-59 steps */
   for(; t<60; t+=5) {
      SHA1_STEP(A,B,C,D,E, MAGIC_F2, W[t  ],K_SHA1[2]);
      SHA1_STEP(E,A,B,C,D, MAGIC_F2, W[t+1],K_SHA1[2]);
      SHA1_STEP(D,E,A,B,C, MAGIC_F2, W[t+2],K_SHA1[2]);
      SHA1_STEP(C,D,E,A,B, MAGIC_F2, W[t+3],K_SHA1[2]);
      SHA1_STEP(B,C,D,E,A, MAGIC_F2, W[t+4],K_SHA1[2]);
   }

   /* perform 60-79 steps */
   for(; t<80; t+=5) {
      SHA1_STEP(A,B,C,D,E, MAGIC_F3, W[t  ],K_SHA1[3]);
      SHA1_STEP(E,A,B,C,D, MAGIC_F3, W[t+1],K_SHA1[3]);
      SHA1_STEP(D,E,A,B,C, MAGIC_F3, W[t+2],K_SHA1[3]);
      SHA1_STEP(C,D,E,A,B, MAGIC_F3, W[t+3],K_SHA1[3]);
      SHA1_STEP(B,C,D,E,A, MAGIC_F3, W[t+4],K_SHA1[3]);
   }

   /* update digest */
   digest[0] += A;
   digest[1] += B;
   digest[2] += C;
   digest[3] += D;
   digest[4] += E;
}

#endif
