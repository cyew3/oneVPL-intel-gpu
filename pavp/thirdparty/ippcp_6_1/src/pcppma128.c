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
//    Created: Mon 16-Aug-2004 10:14
//  Author(s): Sergey Kirillov
*/

#if defined(_IPP_v50_)


#include "precomp.h"
#include "owncp.h"
#include "pcpeccp.h"
#include "pcppma128.h"

#define BIGGER_HALF(x) (((x)>>1)+((x)&1))

/*
// Specific Modulo Arithmetic
//    P128 = 2^128 -2^97 -1
//    (reference secp128r1_p)
*/

/*
// Reduce modulo:
//
//  x = c7|c6|c5|c4|c3|c2|c1|c0
//
// s1 =  c3| c2| c1| c0
// s2 = 2c4| 00| 00| c4
// s3 = 4c5| 00| c5|2c5
// s4 = 8c6| c6|2c6|4c6
// s5 =17c7|2c7|4c7|8c7
//
// r = (s1+s2+s3+s4+s5) (mod P)
*/
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )
void Reduce_P128r1(Ipp32u* pR)
{
   Ipp32u borrow;

   Ipp64u c7x2 = (Ipp64u)pR[7] + (Ipp64u)pR[7];
   Ipp64u c7x4 = c7x2 + c7x2;
   Ipp64u c7x8 = c7x4 + c7x4;

   Ipp64u c6x2 = (Ipp64u)pR[6] + (Ipp64u)pR[6];
   Ipp64u c6x4 = c6x2 + c6x2;
   Ipp64u c6x8 = c6x4 + c6x4;

   Ipp64u c5x2 = (Ipp64u)pR[5] + (Ipp64u)pR[5];
   Ipp64u c5x4 = c5x2 + c5x2;

   Ipp64u c4x2 = (Ipp64u)pR[4] + (Ipp64u)pR[4];

   Ipp64u
   sum   = (Ipp64u)pR[0] +  (Ipp64u)pR[4] + c5x2 + c6x4 + c7x8;
   pR[0] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[1] + (Ipp64u)pR[5] + c6x2 + c7x4;
   pR[1] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[2] + (Ipp64u)pR[6] + c7x2;
   pR[2] = LODWORD(sum);
   sum   = HIDWORD(sum);

   sum  += (Ipp64u)pR[3] + c4x2 + c5x4 + c6x8 + c7x8+c7x8+(Ipp64u)pR[7];
   pR[3] = LODWORD(sum);
   pR[4] = HIDWORD(sum);

   if(pR[4])
      cpSub_BNU(pR, secp128_mx[pR[4]], pR, LEN_P128+1, &borrow);

   while(pR[LEN_P128]&0x80000000)
      cpAdd_BNU(pR, secp128r1_p, pR, LEN_P128+1, &borrow);

   while(0 <= cpCmp_BNU(pR, secp128r1_p, LEN_P128+1)) {
      cpSub_BNU(pR, secp128r1_p, pR, LEN_P128+1, &borrow);
   }
}
#endif

void cpAdde_128r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[LEN_P128+1];
#else
    Ipp64u  buffer[BIGGER_HALF(LEN_P128+1)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P128;

   FE_ADD(tmpR, aPtr, bPtr, LEN_P128);
   if(0 <= cpCmp_BNU(tmpR, secp128r1_p, LEN_P128+1)) {
      FE_SUB(tmpR, tmpR, secp128r1_p, LEN_P128+1);
   }
   FIX_BNU(tmpR, rSize);
   ZEXPAND_COPY_BNU(tmpR, rSize, rPtr, LEN_P128);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSube_128r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P128;

   if(0 <= cpCmp_BNU(aPtr, bPtr, LEN_P128)) {
      FE_SUB(rPtr, aPtr, bPtr, LEN_P128);
   }
   else {
      Ipp32u  tmpR[LEN_P128+1];
      FE_ADD(tmpR, aPtr, secp128r1_p, LEN_P128);
      FE_SUB(rPtr, tmpR, bPtr, LEN_P128);
   }
   FIX_BNU(rPtr, rSize);
   ZEXPAND_BNU(rPtr, rSize, LEN_P128);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpSqre_128r1(IppsBigNumState* pA, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P128];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P128)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P128;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_SQR(tmpR, aPtr, LEN_P128);
   #else
   #if !defined(_USE_SQR_)
   cpMul_BNU_FullSize(aPtr,LEN_P128, aPtr,LEN_P128, tmpR);
   #else
   cpSqr_BNU(tmpR, aPtr,LEN_P128);
   #endif
   #endif

   Reduce_P128r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P128);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

void cpMule_128r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR)
{
#if !((_IPP64==_IPP64_I7) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
    Ipp32u  tmpR[2*LEN_P128];
#else
    Ipp64u  buffer[2*BIGGER_HALF(LEN_P128)];
    Ipp32u* tmpR = (Ipp32u *)buffer;
#endif

   Ipp32u* aPtr = BN_NUMBER(pA);
   Ipp32u* bPtr = BN_NUMBER(pB);
   Ipp32u* rPtr = BN_NUMBER(pR);
   int rSize = LEN_P128;

   #if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
         (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
         (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
         (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
         (_IPP32E==_IPP32E_M7) || \
         (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
         (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
         (_IPP64==_IPP64_I7) )
   FE_MUL(tmpR, aPtr,bPtr, LEN_P128);
   #else
   cpMul_BNU_FullSize(aPtr,LEN_P128, bPtr,LEN_P128, tmpR);
   #endif

   Reduce_P128r1(tmpR);
   COPY_BNU(tmpR, rPtr, LEN_P128);
   FIX_BNU(tmpR, rSize);

   BN_SIGN(pR) = IppsBigNumPOS;
   BN_SIZE(pR) = rSize;
}

#endif /* _IPP_v50_ */
