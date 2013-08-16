/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.
//
// Purpose:
//    Intel(R) Integrated Performance Primitives
//    Cryptographic Primitives
//    Internal GF(p) basic Definitions & Function Prototypes
//
*/
#if !defined(__PCP_GFP__H__)
#define __PCP_GFP__H__

#include "pcpbn.h"
#include "pcpbnuext.h"


typedef Ipp32u cpFFchunk;

struct _cpGFpElement {
   IppCtxId idCtx;         /* GF(p) element ident */
   cpFFchunk* pData;
};

#define GFPE_ID(pCtx)      ((pCtx)->idCtx)
#define GFPE_DATA(pCtx)    ((pCtx)->pData)

#define GFPE_TEST_ID(pCtx) (GFPE_ID((pCtx))==idCtxGFPE)

/* GF(p) context */
struct _cpGFp {
   IppCtxId idCtx;         /* GFp spec ident    */
   Ipp32u   degree;        /* ==0               */
   Ipp32u   elementSize;   /* memory size (dwords) occupied by field element */
   Ipp32u   elementSize32; /* field element size (dwords) <= elementSize */
   cpFFchunk*  pModulus;   /* modulus           */
   void*       pGroundGF;  /* ground GF ==0     */
   cpFFchunk*  pHalfModulus;
   cpFFchunk*  pUnity;
   cpFFchunk*  pMontUnity;
   cpFFchunk*  pQnr;
   IppsMontState* pMontState;
   cpFFchunk*  pFFpool;    /* pool of temp vars */
};

/* Local definitions */
#define FF_MAX_BITSIZE   (4096)  /* max bitsize for FF element */
#define GF_POOL_SIZE       (10)  /* num of elements into the pool */
#define GF_RAND_ADD_BITS   (80)  /* parameter of random element generation */

#define GFP_ID(pCtx)          ((pCtx)->idCtx)
#define GFP_DEGREE(pCtx)      ((pCtx)->degree)
#define GFP_FESIZE(pCtx)      ((pCtx)->elementSize)
#define GFP_FESIZE32(pCtx)    ((pCtx)->elementSize32)
#define GFP_MODULUS(pCtx)     ((pCtx)->pModulus)
#define GFP_GROUNDGF(pCtx)    ((pCtx)->pGroundGF)
#define GFP_HMODULUS(pCtx)    ((pCtx)->pHalfModulus)
#define GFP_UNITY(pCtx)       ((pCtx)->pUnity)
#define GFP_MONTUNITY(pCtx)   ((pCtx)->pMontUnity)
#define GFP_QNR(pCtx)         ((pCtx)->pQnr)
#define GFP_MONT(pCtx)        ((pCtx)->pMontState)
#define GFP_POOL(pCtx)        ((pCtx)->pFFpool)

#define GFP_GROUNDFESIZE(pCtx)   (sizeof(cpFFchunk))
#define GFP_PESIZE(pCtx)         (GFP_FESIZE(pCtx)+BNUBASE_TYPE_SIZE)
#define GFP_FEBITSIZE(pGFpCtx)   (BNU_BITSIZE(GFP_MODULUS(pGFpCtx), GFP_FESIZE32(pGFpCtx)))
#define GFP_TEST_ID(pCtx)        (GFP_ID((pCtx))==idCtxGFP)


/*
// get/release n element from/to the pool
*/
__INLINE cpFFchunk* gfpGetPool(Ipp32u n, IppsGFPState* pGFpCtx)
{
   cpFFchunk* pPool = GFP_POOL(pGFpCtx);
   GFP_POOL(pGFpCtx) += n*GFP_PESIZE(pGFpCtx);
   return pPool;
}
__INLINE void gfpReleasePool(Ipp32u n, IppsGFPState* pGFpCtx)
{
   GFP_POOL(pGFpCtx) -= n*GFP_PESIZE(pGFpCtx);
}

Ipp32u gfpIsArrayOverGF(const Ipp32u* pA, Ipp32u lenA, const IppsGFPState* pGFpCtx);
cpFFchunk* gfpSetPolynomial(const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pPoly, Ipp32u deg, const IppsGFPState* pGFpCtx);
Ipp32u*    gfpGetPolynomial(const cpFFchunk* pPoly, Ipp32u deg, Ipp32u* pA, Ipp32u lenA, const IppsGFPState* pGFpCtx);

cpFFchunk* gfpReduce(const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpNeg (const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpInv (const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpHalve(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpAdd (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpSub (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpMul (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpExp (const cpFFchunk* pA, const cpFFchunk* pE, Ipp32u eSize, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpExp2(const cpFFchunk* pA, Ipp32u e, cpFFchunk* pR, IppsGFPState* pGFpCtx);

cpFFchunk* gfpMontEnc(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpMontDec(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
       int gfpMontSqrt(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
       int gfpSqrt(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpMontInv(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpMontMul(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpMontExp(const cpFFchunk* pA, const cpFFchunk* pE, Ipp32u eSize, cpFFchunk* pR, IppsGFPState* pGFpCtx);
cpFFchunk* gfpMontExp2(const cpFFchunk* pA, Ipp32u e, cpFFchunk* pR, IppsGFPState* pGFpCtx);

__INLINE Ipp32u gfpElementActualSize(const cpFFchunk* pA, Ipp32u sizeA)
{
   for(; sizeA>1 && 0==pA[sizeA-1]; sizeA--) ;
   return sizeA;
}
__INLINE cpFFchunk* gfpFFelementCopy(const cpFFchunk* pA, cpFFchunk* pR, Ipp32u size)
{
   Ipp32u n;
   for(n=0; n<size; n++) pR[n] = pA[n];
   return pR;
}
__INLINE cpFFchunk* gfpFFelementPadd(Ipp32u filler, cpFFchunk* pA, Ipp32u size)
{
   Ipp32u n;
   for(n=0; n<size; n++) pA[n] = filler;
   return pA;
}
__INLINE cpFFchunk* gfpFFelementCopyPadd(const cpFFchunk* pA, Ipp32u sizeA, cpFFchunk* pR, Ipp32u sizeR)
{
   Ipp32u n;
   for(n=0; n<sizeA; n++) pR[n] = pA[n];
   for(; n<sizeR; n++) pR[n] = 0;
   return pR;
}
__INLINE int gfpFFelementCmp(const cpFFchunk* pA, const cpFFchunk* pX, Ipp32u size)
{
   for(; size>1 && pA[size-1]==pX[size-1]; size--)
      ;
   return pA[size-1]==pX[size-1]? 0 : pA[size-1]>pX[size-1]? 1:-1;
}
__INLINE int gfpFFelementIsEqu32u(const cpFFchunk* pA, Ipp32u sizeA, Ipp32u x)
{
   int isEqu = (pA[0] == x);
   return isEqu && (1==gfpElementActualSize(pA, sizeA));
}
__INLINE cpFFchunk* gfpFFelementSet32u(Ipp32u x, cpFFchunk* pR, Ipp32u sizeR)
{
   return gfpFFelementCopyPadd(&x, 1, pR, sizeR);
}

#define GFP_LT(a,b,size)  (-1==gfpFFelementCmp((a),(b),(size)))
#define GFP_EQ(a,b,size)  ( 0==gfpFFelementCmp((a),(b),(size)))
#define GFP_GT(a,b,size)  ( 1==gfpFFelementCmp((a),(b),(size)))

#define GFP_IS_ZERO(a,size)  gfpFFelementIsEqu32u((a),(size), 0)
#define GFP_IS_ONE(a,size)   gfpFFelementIsEqu32u((a),(size), 1)

#define GFP_ZERO(a,size)      gfpFFelementSet32u(0, (a),(size))
#define GFP_ONE(a,size)       gfpFFelementSet32u(1, (a),(size))
#define GFP_MONT_ONE(a,gf)    gfpFFelementCopy(GFP_MONTUNITY(gf), (a), GFP_FESIZE(gf))

#define GFP_IS_EVEN(a)  (0==((a)[0]&1))
#define GFP_IS_ODD(a)   (1==((a)[0]&1))

cpFFchunk* gfpRand(cpFFchunk* pR, IppsGFPState* pGFpCtx, IppBitSupplier rndFunc, void* pRndParam);

IppsBigNumState* gfpInitBigNum(IppsBigNumState* pBN, Ipp32u size, Ipp32u* pNumBuffer, Ipp32u* pTmpBuffer);
IppsBigNumState* gfpSetBigNum(IppsBigNumState* pBN, Ipp32u size, const Ipp32u* pBNU, Ipp32u* pTmpBuffer);

#endif /* __PCP_GFP__H__ */
