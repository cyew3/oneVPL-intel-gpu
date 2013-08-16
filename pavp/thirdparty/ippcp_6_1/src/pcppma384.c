/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Prime Modulo Arithmetic Function
//
// Contents:
//
//
//    Created: Thu 15-Aug-2004 15:27
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccp.h"
#include "pcppma384.h"

#define BIGGER_HALF(x) (((x)>>1)+((x)&1))

/*
// Specific Modulo Arithmetic
//    P384 = 2^384 -2^128 -2^96 +2^32 -1
//    (reference secp384r1_p)
*/

/*
// Reduce modulo:
//
//  x = c23|c22|c21|c20|c19|c18|c17|c16|c15|c14|c13|c12|c11|c10|c09|c08|c07|c06|c05|c04|c03|c02|c01|c00 - 32-bits values
//
// s1 = c11|c10|c09|c08|c07|c06|c05|c04|c03|c02|c01|c00
// s2 = 000|000|000|000|000|c23|c22|c21|000|000|000|000
// s3 = c23|c22|c21|c20|c19|c18|c17|c16|c15|c14|c13|c12
// s4 = c20|c19|c18|c17|c16|c15|c14|c13|c12|c23|c22|c21
// s5 = c19|c18|c17|c16|c15|c14|c13|c12|c20|000|c23|000
// s6 = 000|000|000|000|c23|c22|c21|c20|000|000|000|000
// s7 = 000|000|000|000|000|000|c23|c22|c21|000|000|c20
//
// s8 = c22|c21|c20|c19|c18|c17|c16|c15|c14|c13|c12|c23
// s9 = 000|000|000|000|000|000|000|c23|c22|c21|c20|000
// s10= 000|000|000|000|000|000|000|c23|c23|000|000|000
//
// r = (s1+2*s2+s3+s4+s5+s6+s7-s8-s9-10) (mod P)
*/

static
void Reduce_P384r1(Ipp32u* pR)
{
   Ipp32u carry;

   Ipp64u c12c21 = (Ipp64u)pR[12] + (Ipp64u)pR[21];
   Ipp64u c13c22 = (Ipp64u)pR[13] + (Ipp64u)pR[22];
   Ipp64u c14c23 = (Ipp64u)pR[14] + (Ipp64u)pR[23];

   Ipp64s
   sum   = (Ipp64u)pR[ 0] + c12c21 + (Ipp64u)pR[20] - (Ipp64u)pR[23];
   pR[ 0]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 1] + c13c22 + (Ipp64u)pR[23] - (Ipp64u)pR[12] -  (Ipp64u)pR[20];
   pR[ 1]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 2] + c14c23 - (Ipp64u)pR[13] - (Ipp64u)pR[21];
   pR[ 2]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 3] + c12c21 + (Ipp64u)pR[15] + (Ipp64u)pR[20] - c14c23 - (Ipp64u)pR[22];
   pR[ 3]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 4] + (Ipp64u)pR[21] + c12c21 + c13c22 + (Ipp64u)pR[16] + (Ipp64u)pR[20] - (Ipp64u)pR[15] - (Ipp64u)pR[23] - (Ipp64u)pR[23];
   pR[ 4]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 5] + (Ipp64u)pR[22] + c13c22 + c14c23 + (Ipp64u)pR[17] + (Ipp64u)pR[21] - (Ipp64u)pR[16];
   pR[ 5]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 6] + (Ipp64u)pR[23] + c14c23 + (Ipp64u)pR[15] + (Ipp64u)pR[18] + (Ipp64u)pR[22] - (Ipp64u)pR[17];
   pR[ 6]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 7] + (Ipp64u)pR[15] + (Ipp64u)pR[16] + (Ipp64u)pR[19] + (Ipp64u)pR[23] - (Ipp64u)pR[18];
   pR[ 7]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 8] + (Ipp64u)pR[16] + (Ipp64u)pR[17] + (Ipp64u)pR[20] - (Ipp64u)pR[19];
   pR[ 8]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 9] + (Ipp64u)pR[17] + (Ipp64u)pR[18] + (Ipp64u)pR[21] - (Ipp64u)pR[20];
   pR[ 9]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[10] + (Ipp64u)pR[18] + (Ipp64u)pR[19] + (Ipp64u)pR[22] - (Ipp64u)pR[21];
   pR[10]= LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[11] + (Ipp64u)pR[19] + (Ipp64u)pR[20] + (Ipp64u)pR[23] - (Ipp64u)pR[22];
   pR[11]= LODWORD(sum);
   pR[12]= (Ipp32u)(sum>>32);

   while(pR[LEN_P384]&0x80000000)
      cpAdd_BNU(pR, secp384r1_p, pR, LEN_P384+1, &carry);

   while(0 <= cpCmp_BNU(pR, secp384r1_p, LEN_P384+1))
      cpSub_BNU(pR, secp384r1_p, pR, LEN_P384+1, &carry);
}

void cpAdde_384r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
    Ipp32u  tmpR[LEN_P384+1];
#else
    Ipp64u  buffer[BIGGER_HALF(LEN_P384+1)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P384;

   FE_ADD(tmpR, aPtr, bPtr, LEN_P384);
   if(0 <= cpCmp_BNU(tmpR, secp384r1_p, LEN_P384+1)) {
      FE_SUB(tmpR, tmpR, secp384r1_p, LEN_P384+1);
   }
   FIX_BNU(tmpR, rSize);
   ZEXPAND_COPY_BNU(tmpR, rSize, rPtr, LEN_P384);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSube_384r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P384;

   if(0 <= cpCmp_BNU(aPtr, bPtr, LEN_P384)) {
      FE_SUB(rPtr, aPtr, bPtr, LEN_P384);
   }
   else {
      Ipp32u  tmpR[LEN_P384+1];
      FE_ADD(tmpR, aPtr, secp384r1_p, LEN_P384);
      FE_SUB(rPtr, tmpR, bPtr, LEN_P384);
   }
   FIX_BNU(rPtr, rSize);
   ZEXPAND_BNU(rPtr, rSize, LEN_P384);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSqre_384r1(IppsBigNumState* pA, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P384];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P384)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P384;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_SQR(tmpR, aPtr, LEN_P384);
   #else
   #if !defined(_USE_SQR_)
   cpMul_BNU_FullSize(aPtr,LEN_P384, aPtr,LEN_P384, tmpR);
   #else
   cpSqr_BNU(tmpR, aPtr,LEN_P384);
   #endif
   #endif

   Reduce_P384r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P384);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpMule_384r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P384];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P384)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P384;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_MUL(tmpR, aPtr,bPtr, LEN_P384);
   #else
   cpMul_BNU_FullSize(aPtr,LEN_P384, bPtr,LEN_P384, tmpR);
   #endif

   Reduce_P384r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P384);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

#endif /* _IPP_v50_ */
