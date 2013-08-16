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
//    Created: Thu 12-Aug-2004 10:42
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccp.h"
#include "pcppma224.h"

#define BIGGER_HALF(x) (((x)>>1)+((x)&1))

/*
// Specific Modulo Arithmetic
//    P224 = 2^224 -2^96 +1
//    (reference secp224r1_p)
*/

/*
// Reduce modulo:
//
//  x = c13|c12|c11|c10|c09|c08|c07|c06|c05|c04|c03|c02|c01|c00 - 32-bits values
//
// s1 = c06|c05|c04|c03|c02|c01|c00
// s2 = c10|c09|c08|c07|000|000|000
// s3 = 000|c13|c12|c11|000|000|000
//
// s4 = c13|c12|c11|c10|c09|c08|c07
// s5 = 000|000|000|000|c13|c12|c11
//
// r = (s1+s2+s3-s4-s5) (mod P)
*/
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )
void Reduce_P224r1(Ipp32u* pR)
{
   Ipp32u carry;

   Ipp64u c7c11 = (Ipp64u)pR[ 7] + (Ipp64u)pR[11];
   Ipp64u c8c12 = (Ipp64u)pR[ 8] + (Ipp64u)pR[12];
   Ipp64u c9c13 = (Ipp64u)pR[ 9] + (Ipp64u)pR[13];

   Ipp64s
   sum   = (Ipp64u)pR[ 0] - c7c11;
   pR[0] = LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 1] - c8c12;
   pR[1] = LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 2] - c9c13;
   pR[2] = LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 3] + c7c11 - (Ipp64u)pR[10];
   pR[3] = LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 4] + c8c12 - (Ipp64u)pR[11];
   pR[4] = LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 5] + c9c13 - (Ipp64u)pR[12];
   pR[5] = LODWORD(sum);
   sum >>= 32;

   sum  += (Ipp64u)pR[ 6] + (Ipp64u)pR[10] - (Ipp64u)pR[13];
   pR[6] = LODWORD(sum);
   pR[7] = (Ipp32u)(sum>>32);

   while(pR[LEN_P224]&0x80000000)
      cpAdd_BNU(pR, secp224r1_p, pR, LEN_P224+1, &carry);

   while(0 <= cpCmp_BNU(pR, secp224r1_p, LEN_P224+1))
      cpSub_BNU(pR, secp224r1_p, pR, LEN_P224+1, &carry);
}
#endif

void cpAdde_224r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[LEN_P224+1];
#else
    Ipp64u  buffer[BIGGER_HALF(LEN_P224+1)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P224;

   FE_ADD(tmpR, aPtr, bPtr, LEN_P224);
   if(0 <= cpCmp_BNU(tmpR, secp224r1_p, LEN_P224+1)) {
      FE_SUB(tmpR, tmpR, secp224r1_p, LEN_P224+1);
   }
   FIX_BNU(tmpR, rSize);
   ZEXPAND_COPY_BNU(tmpR, rSize, rPtr, LEN_P224);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSube_224r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P224;

   if(0 <= cpCmp_BNU(aPtr, bPtr, LEN_P224)) {
      FE_SUB(rPtr, aPtr, bPtr, LEN_P224);
   }
   else {
      Ipp32u  tmpR[LEN_P224+1];
      FE_ADD(tmpR, aPtr, secp224r1_p, LEN_P224);
      FE_SUB(rPtr, tmpR, bPtr, LEN_P224);
   }
   FIX_BNU(rPtr, rSize);
   ZEXPAND_BNU(rPtr, rSize, LEN_P224);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSqre_224r1(IppsBigNumState* pA, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P224];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P224)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P224;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_SQR(tmpR, aPtr, LEN_P224);
   #else
   #if !defined(_USE_SQR_)
   cpMul_BNU_FullSize(aPtr,LEN_P224, aPtr,LEN_P224, tmpR);
   #else
   cpSqr_BNU(tmpR, aPtr,LEN_P224);
   #endif
   #endif

   Reduce_P224r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P224);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpMule_224r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P224];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P224)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P224;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_MUL(tmpR, aPtr,bPtr, LEN_P224);
   #else
   cpMul_BNU_FullSize(aPtr,LEN_P224, bPtr,LEN_P224, tmpR);
   #endif

   Reduce_P224r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P224);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

#endif /* _IPP_v50_ */
