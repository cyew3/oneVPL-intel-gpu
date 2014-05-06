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
//    Created: Sat 24-Jul-2004 15:49
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccp.h"
#include "pcppma192.h"


#define BIGGER_HALF(x) (((x)>>1)+((x)&1))

/*
// Specific Modulo Arithmetic
//    P192 = 2^192 -2^64 -1
//    (reference secp192r1_p)
*/

/*
// Reduce modulo:
//
//  x = c11|c10|c9|c8|c7|c6|c5|c4|c3|c2|c1|c0
//
// s1 = c05|c04|c03|c02|c01|c00
// s2 = 000|000|c07|c06|c07|c06
// s3 = c09|c08|c09|c08|000|000
//
// r = (s1+s2+s3+s4) (mod P)
*/
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )
void Reduce_P192r1(Ipp32u* pR)
{
   Ipp64u
   sum   = (Ipp64u)pR[0*2+0]+(Ipp64u)pR[3*2+0]+(Ipp64u)pR[5*2+0];
   pR[0] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[0*2+1]+(Ipp64u)pR[3*2+1]+(Ipp64u)pR[5*2+1];
   pR[1] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[1*2+0]+(Ipp64u)pR[3*2+0]+(Ipp64u)pR[4*2+0]+(Ipp64u)pR[5*2+0];
   pR[2] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[1*2+1]+(Ipp64u)pR[3*2+1]+(Ipp64u)pR[4*2+1]+(Ipp64u)pR[5*2+1];
   pR[3] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[2*2+0]+(Ipp64u)pR[4*2+0]+(Ipp64u)pR[5*2+0];
   pR[4] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[2*2+1]+(Ipp64u)pR[4*2+1]+(Ipp64u)pR[5*2+1];
   pR[5] = LODWORD(sum);
   pR[6] = HIDWORD(sum);

   while(0 <= cpCmp_BNU(pR, secp192r1_p, LEN_P192+1))
      FE_SUB(pR, pR, secp192r1_p, LEN_P192+1);
}
#endif

void cpAdde_192r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[LEN_P192+1];
#else
    Ipp64u  buffer[BIGGER_HALF(LEN_P192+1)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P192;

   FE_ADD(tmpR, aPtr, bPtr, LEN_P192);
   if(0 <= cpCmp_BNU(tmpR, secp192r1_p, LEN_P192+1)) {
      FE_SUB(tmpR, tmpR, secp192r1_p, LEN_P192+1);
   }
   FIX_BNU(tmpR, rSize);
   ZEXPAND_COPY_BNU(tmpR, rSize, rPtr, LEN_P192);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSube_192r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P192;

   if(0 <= cpCmp_BNU(aPtr, bPtr, LEN_P192)) {
      FE_SUB(rPtr, aPtr, bPtr, LEN_P192);
   }
   else {
      Ipp32u  tmpR[LEN_P192+1];
      FE_ADD(tmpR, aPtr, secp192r1_p, LEN_P192);
      FE_SUB(rPtr, tmpR, bPtr, LEN_P192);
   }
   FIX_BNU(rPtr, rSize);
   ZEXPAND_BNU(rPtr, rSize, LEN_P192);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSqre_192r1(IppsBigNumState* pA, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P192];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P192)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P192;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_SQR(tmpR, aPtr, LEN_P192);
   #else
   #if !defined(_USE_SQR_)
   cpMul_BNU_FullSize(aPtr,LEN_P192, aPtr,LEN_P192, tmpR);
   #else
   cpSqr_BNU(tmpR, aPtr,LEN_P192);
   #endif
   #endif

   Reduce_P192r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P192);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpMule_192r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P192];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P192)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P192;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_MUL(tmpR, aPtr,bPtr, LEN_P192);
   #else
   cpMul_BNU_FullSize(aPtr,LEN_P192, bPtr,LEN_P192, tmpR);
   #endif

   Reduce_P192r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P192);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

#endif /* _IPP_v50_ */
