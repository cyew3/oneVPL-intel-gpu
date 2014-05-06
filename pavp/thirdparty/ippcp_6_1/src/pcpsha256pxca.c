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
//    Message block processing according to SHA256
//
// Contents:
//    UpdateSHA256()
//
//
//    Created: Sun 10-Mar-2002 14:14
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpshs.h"
#include "pcpciphertool.h"


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2))
void ProcessSHA256(Ipp8u* buffer, DigestSHA256 digest, const Ipp8u* pSrc, int len)
{
   if( IPP_UINT_PTR(pSrc) & 0x3) {
      while(len) {
         CopyBlock(pSrc, buffer, MBS_SHA256);
         UpdateSHA256(buffer, digest);
         pSrc += MBS_SHA256;
         len  -= MBS_SHA256;
      }
   }
   else {
      while(len) {
         UpdateSHA256(pSrc, digest);
         pSrc += MBS_SHA256;
         len  -= MBS_SHA256;
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
// SHA256 Specific Macros (reference proposal 256-384-512)
*/
#define CH(x,y,z)    (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z)   (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define SUM0(x)   (ROR32((x), 2) ^ ROR32((x),13) ^ ROR32((x),22))
#define SUM1(x)   (ROR32((x), 6) ^ ROR32((x),11) ^ ROR32((x),25))

#define SIG0(x)   (ROR32((x), 7) ^ ROR32((x),18) ^ LSR32((x), 3))
#define SIG1(x)   (ROR32((x),17) ^ ROR32((x),19) ^ LSR32((x),10))

#define SHA256_UPDATE(i) \
   wdat[i & 15] += SIG1(wdat[(i+14)&15]) + wdat[(i+9)&15] + SIG0(wdat[(i+1)&15])

#define SHA256_STEP(i,j)  \
   v[(7 - i) & 7] += (j ? SHA256_UPDATE(i) : wdat[i&15])    \
                  + K_SHA256[i + j]                         \
                  + SUM1(v[(4-i)&7])                        \
                  + CH(v[(4-i)&7], v[(5-i)&7], v[(6-i)&7]); \
   v[(3-i)&7] += v[(7-i)&7];                                \
   v[(7-i)&7] += SUM0(v[(0-i)&7]) + MAJ(v[(0-i)&7], v[(1-i)&7], v[(2-i)&7])


void UpdateSHA256(const Ipp8u* mblk, DigestSHA256 digest)
{
   #if defined(_IPP_TRS)
   const Ipp32u K_SHA256[] = FIPS_180_K_SHA256;
   #endif

   Ipp32u v[8];
   Ipp32u wdat[16];
   int j;

   /* initialize the first 16 words in the array W (remember about endian) */
   for(j=0; j<16; j++) {
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      wdat[j] = ((Ipp32u*)mblk)[j];
      #else
      wdat[j] = ENDIANNESS( ((Ipp32u*)mblk)[j] );
      #endif
   }

   /* copy digest */
   CopyBlock(digest, v, sizeof(DigestSHA256));

   for(j=0; j<64; j+=16) {
      SHA256_STEP( 0, j);
      SHA256_STEP( 1, j);
      SHA256_STEP( 2, j);
      SHA256_STEP( 3, j);
      SHA256_STEP( 4, j);
      SHA256_STEP( 5, j);
      SHA256_STEP( 6, j);
      SHA256_STEP( 7, j);
      SHA256_STEP( 8, j);
      SHA256_STEP( 9, j);
      SHA256_STEP(10, j);
      SHA256_STEP(11, j);
      SHA256_STEP(12, j);
      SHA256_STEP(13, j);
      SHA256_STEP(14, j);
      SHA256_STEP(15, j);
   }

   /* update digest */
   digest[0] += v[0];
   digest[1] += v[1];
   digest[2] += v[2];
   digest[3] += v[3];
   digest[4] += v[4];
   digest[5] += v[5];
   digest[6] += v[6];
   digest[7] += v[7];
}

#endif
