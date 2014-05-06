/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
*/

#if !defined(_CP_MONTGOMERY_H)
#define _CP_MONTGOMERY_H

/*
// Montgomery spec structure
*/
#if 0
typedef struct _cpMontgomery
{
   IppCtxId          idCtx;   /* Montgomery spec identifier                                   */
   IppsExpMethod     method;  /* gres: doesn't use in reality                                 */
   int               k;       /* R = b^k                                                      */
   IppsBigNumState*  n;       /* modulus                                      (k)             */
   IppsBigNumState*  wb;      /* working buffer for Montgomery reduction      (k*2)           */
   Ipp32u            n0;      /* the last significant DWORD of n1, where n*n1 = -1 mod R      */
   Ipp32u*           nHi;     /* the last significant 64-bit word of n1, where n*n1 = -1 mod R*/
   Ipp32u*           pBuffer; /* gres: any buffer */
} ippcpMontgomery;
#endif
struct _cpMontgomery
{
   IppCtxId          idCtx;                  /* Montgomery spec identifier             */
   IppsExpMethod     method;                 /* gres: doesn't use in reality           */
   int               k;                      /* R = b^k                                */
   Ipp32u            n0[BNUBASE_TYPE_SIZE];  /* LS DWORD of n1, where n*n1 = -1 mod R  */
   IppsBigNumState*  n;                      /* modulus (of k DWORD size)              */
   IppsBigNumState*  identity;               /* mont_form(1)                           */
   IppsBigNumState*  square;                 /* mont_form(R^2)                         */
   IppsBigNumState*  cube;                   /* mont_form(R^3)                         */
   IppsBigNumState*  wb;                     /* internal product (of 2*k DWORD size)   */
   Ipp32u*           pBuffer;                /* mul/sqr buffer (Karatsuba method used) */
};

/* default methos */
#define EXPONENT_METHOD    (IppsBinaryMethod)

/* alignment */
#define MONT_ALIGNMENT  ((int)(sizeof(void*)))
#define MONT_ALIGN_SIZE (ALIGN_VAL)

#define IPP_MAKE_ALIGNED_MONT(pCtx) ( (pCtx)=(IppsMontState*)(IPP_ALIGNED_PTR((pCtx), (MONT_ALIGN_SIZE))) )


/* accessory macros */
#define MNT_ID(eng)       ((eng)->idCtx)
#define MNT_METHOD(eng)   ((eng)->method)
#define MNT_K(eng)        ((eng)->k)
#define MNT_MODULO(eng)   ((eng)->n)
#define MNT_1(eng)        ((eng)->identity)
#define MNT_IDENT_R(eng)  (MNT_1((eng)))
#define MNT_SQUARE_R(eng) ((eng)->square)
#define MNT_CUBE_R(eng)   ((eng)->cube)
#define MNT_PRODUCT(eng)  ((eng)->wb)
#define MNT_HELPER(eng)   ((eng)->n0)
#define MNT_BUFFER(eng)   ((eng)->pBuffer)
#define MNT_VALID_ID(eng) (MNT_ID((eng))==idCtxMontgomery)


#if defined(_USE_NN_MONTMUL_)
void cpMontMul(const Ipp32u* pX, int  xSize,
               const Ipp32u* pY, int  ySize,
               const Ipp32u* pModulo, int  mSize,
                     Ipp32u* pR, int* prSsize,
               const Ipp32u* mHi, Ipp32u* pTmpProd);
#else
void cpMontMul(const Ipp32u* pX, int  xSize,
               const Ipp32u* pY, int  ySize,
               const Ipp32u* pModulo, int  mSize,
                     Ipp32u* pR, int* prSsize,
               const Ipp32u* mHi, Ipp32u* pTmpProd, Ipp32u* pBuffer);
#endif

#if !defined(_USE_SQR_)
#define cpMontSqr(px,xsize, pm,msize, pr,prsize, phlp,pProd,pBuff) \
   cpMontMul((px),(xsize), (px),(xsize), \
             (pm),(msize), \
             (pr),(prsize), \
             (phlp),(pProd),(pBuff))

#else
void cpMontSqr(const Ipp32u* pX, int  xSize,
               const Ipp32u* pModulo, int  mSize,
                     Ipp32u* pR, int* prSsize,
               const Ipp32u* mHi, Ipp32u* pTmpProd, Ipp32u* pBuffer);
#endif

#if !((_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpMontReduction(Ipp32u* pR, Ipp32u* pTmpProd,
               const Ipp32u* pModulo, int mSize, Ipp32u m0);
#else
void cpMontReduction64(Ipp32u* pR, Ipp32u* pTmpProd,
                 const Ipp32u* pModulo, int mSize, Ipp64u m0);
#endif


#include "pcpmontext.h"
#include "pcpmulbnuk.h"


#if !defined(_USE_KARATSUBA_)
#define MUL_BNU(R,A,na,B,nb,buffer) \
   cpMul_BNU_FullSize((A),(na), (B),(nb), (R))
#endif


/*
// depend on situation
// either cpMul_BNU_FullSize() or cpKaratsubaMul_BNU()
// actually will used
*/
#if defined(_USE_KARATSUBA_)

/*
// no CPU specific: use cpMul_BNU_FullSize()
*/
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2)  || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8)  || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9)  || \
      (_IPP64==_IPP64_I7))
#define MUL_BNU(R,A,na,B,nb,buffer) \
   cpMul_BNU_FullSize((A),(na), (B),(nb), (R))
#endif

/*
// IA32 specific
*/
#if ((_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
     (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
     (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9))
#define MUL_BNU(R,A,na,B,nb,buffer) \
   if((A)==(B)) { \
      ((na)<=IPP_KARATSUBA_BOUND)? \
         cpMul_BNU_FullSize((A),(na), (A),(na), (R)) : \
         cpKaratsubaSqr_BNU((R), (A),(na), (buffer)); \
   } \
   else { \
      (((na)!=(nb)) || ((na)<=IPP_KARATSUBA_BOUND))? \
         cpMul_BNU_FullSize((A),(na), (B),(nb), (R)) : \
         cpKaratsubaMul_BNU((R), (A),(B),(na), (buffer)); \
   }
#endif

/*
// EM64T specific
*/
#if ((_IPP32E==_IPP32E_M7) || \
     (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
     (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
#define MUL_BNU(R,A,na,B,nb,buffer) \
   if((A)==(B)) { \
      cpMul_BNU_FullSize((A),(na), (B),(nb), (R)); \
   } \
   else { \
      if(((na)!=(nb)) || ((na)<=IPP_KARATSUBA_BOUND)) { \
         cpMul_BNU_FullSize((A),(na), (B),(nb), (R)); \
      } \
      else { \
         if( (23<=(na))&&((na)<=33)|| \
             (47<=(na))&&((na)<=66)|| (75<=(na)) ) \
            cpKaratsubaMul_BNU((R), (A),(B),(na), (buffer)); \
         else \
            cpMul_BNU_FullSize((A),(na), (B),(na), (R)); \
      } \
   }
#endif

/*
// PCA and IXP specific
*/
#if ((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2))
#define MUL_BNU(R,A,na,B,nb,buffer) \
   if((A)==(B)) { \
      cpMul_BNU_FullSize((A),(na), (B),(nb), (R)); \
   } \
   else { \
      if(((na)!=(nb)) || ((na)<=IPP_KARATSUBA_BOUND)) { \
         cpMul_BNU_FullSize((A),(na), (B),(nb), (R)); \
      } \
      else { \
         cpKaratsubaMul_BNU((R), (A),(B),(na), (buffer)); \
      } \
   }
#endif

/*
// IA64 specific
*/
#if (_IPP64==_IPP64_I7)
#define MUL_BNU(R,A,na,B,nb,buffer) \
   cpMul_BNU_FullSize((A),(na), (B),(nb), (R))
#endif

#endif /* _USE_KARATSUBA_ */

#endif /* _CP_MONTGOMERY_H */
