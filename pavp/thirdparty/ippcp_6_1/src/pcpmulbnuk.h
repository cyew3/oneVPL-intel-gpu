/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    BN Multiplication (Karatsuba method) Definitions & Function Prototypes
//
// Contents:
//    cpKaratsubaBufferSize()
//    cpKaratsubaMul_BNU()
//    cpKAdd_BNU()
//    cpKSub_BNU()
//
//
//    Created: Thu 21-Apr-2005 20:04
//  Author(s): Sergey Kirillov
*/
#if !defined(_KARATSUBA_MUL_)
#define _KARATSUBA_MUL_

#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))

#undef _USE_KARATSUBA_
#endif

#if defined(_USE_KARATSUBA_)

#if ((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2))
   #define IPP_KARATSUBA_BOUND 16
#elif ((_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
       (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
       (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9))
   #define IPP_KARATSUBA_BOUND 17
#elif ((_IPP32E==_IPP32E_M7) || \
       (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
       (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
   #define IPP_KARATSUBA_BOUND 16
#endif


int cpKaratsubaBufferSize(int len, int karatsuba_bound);

#if ((_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
     (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
     (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9))
int cpKAdd_BNU(Ipp32u* pR,
         const Ipp32u* pA, int aSize,
         const Ipp32u* pB, int bSize);

int cpKSub_BNU(Ipp32u* pR,
         const Ipp32u* pA, int aSize,
         const Ipp32u* pB, int bSize);
#endif

#if ((_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
     (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9))
int cpKAdd_BNU_I(Ipp32u* pR, int rSize,
           const Ipp32u* pB, int bSize);
int cpKAdd_BNU_IC(Ipp32u* pR, int rSize, Ipp32u v);

int cpKSub_BNU_I(Ipp32u* pR, int rSize,
           const Ipp32u* pB, int bSize);
#else
#  define cpKSub_BNU_I(R,rL,B,bL)   cpKSub_BNU((R), (R),(rL),(B),(bL))
#  define cpKAdd_BNU_I(R,rL,B,bL)   cpKAdd_BNU((R), (R),(rL),(B),(bL))
#  define cpKAdd_BNU_IC(R,rL,val)   cpKAdd_BNU((R), (R),(rL),&(val),1)
#endif

void cpKaratsubaMul_BNU(Ipp32u* pR,
                  const Ipp32u* pX, const Ipp32u* pY, int size,
                        Ipp32u* pBuffer);
void cpKaratsubaSqr_BNU(Ipp32u* pR,
                  const Ipp32u* pX, int size,
                        Ipp32u* pBuffer);

#endif /* _USE_KARATSUBA_ */
#endif /* _KARATSUBA_MUL_ */
