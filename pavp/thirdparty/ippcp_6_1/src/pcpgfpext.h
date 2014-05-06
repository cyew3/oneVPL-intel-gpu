/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//              Cryptographic Primitives (ippCP)
//              GF(p) extension internal
*/
#if !defined(__PCP_GFPEXT__H__)
#define __PCP_GFPEXT__H__


struct _cpGFpExtElement {
    IppCtxId idCtx;
    cpFFchunk* pData;
};

#define GFPXE_ID(pCtx)      ((pCtx)->idCtx)
#define GFPXE_DATA(pCtx)    ((pCtx)->pData)

#define GFPXE_TEST_ID(pCtx) (GFPXE_ID((pCtx))==idCtxGFPXE)

/* GF(P) extension context */
struct _cpGFpExt {
   IppCtxId       idCtx;
   Ipp32u         degree;        /* GF(p^k) element degree, i.e. basic polynomial degree-1  */
   Ipp32u         elementSize;   /* memory size (dwords) occupied by field element */
   cpFFchunk*     pModulus;      /* coeffs of irreducible poly without hi-order 1 */
   IppsGFPState*  pGroundGF;     /* basic GFP structure */
   cpFFchunk*     pFFpool;       /* Pool of temporary variables of GF(p) extension type*/
};
/* Limits */
#define GFPX_MAX_DEGREE   (9)   /* Max. polynom degree */
#define GFPX_POOL_SIZE    (14)   /* Number of temporary variables in pool */

/* Mark specific to SafeID degree of polynom */
#define GFPX_SAFEID_CASE  (3)   /* Degree of specific SafeID polynomial */

/* Useful definitions */
/* Structure element access macros */
#define GFPX_ID(pCtx)           ((pCtx)->idCtx)
#define GFPX_TEST_ID(pCtx)      (GFPX_ID((pCtx))==idCtxGFPX)
#define GFPX_DEGREE(pCtx)       ((pCtx)->degree)
#define GFPX_FESIZE(pCtx)       ((pCtx)->elementSize)
#define GFPX_MODULUS(pCtx)      ((pCtx)->pModulus)
#define GFPX_GROUNDGF(pCtx)     ((pCtx)->pGroundGF)
#define GFPX_GROUNDFESIZE(pCtx) (GFP_FESIZE(GFPX_GROUNDGF((pCtx))))
#define GFPX_POOL(pCtx)         ((pCtx)->pFFpool)
#define GFPX_IDX_ELEMENT(ptr, idx, eleSize) \
                                ((ptr)+(eleSize)*(idx))
/* Pool element size eq to element size */
#define GFPX_PESIZE(pCtx)       (GFPX_FESIZE(pCtx))

#define GFPX_IS_MONT_ONE(a,ctx)  (GFP_EQ(GFP_MONTUNITY(GFPX_GROUNDGF((ctx))), (a), GFPX_GROUNDFESIZE((ctx))) \
                               && GFP_IS_ZERO((a)+GFPX_GROUNDFESIZE((ctx)), GFPX_FESIZE((ctx))-GFPX_GROUNDFESIZE((ctx))))

/* Internal function prototypes */
Ipp32u gfpxIsUserModulusValid(const Ipp32u* pA, Ipp32u lenA, const IppsGFPXState* pGFPExtCtx);

cpFFchunk* gfpxReduce      (const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxCopyMod     (const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pR, const IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxMontEnc     (const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXState* pGFpExtCtx);
cpFFchunk* gfpxMontDec     (const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXState* pGFpExtCtx);
cpFFchunk* gfpxAdd         (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxAdd_GFP     (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxSub         (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxSub_GFP     (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxMontMul     (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxMontMul_GFP (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxNeg         (const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxMontInv     (const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxDiv         (const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pQ, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
cpFFchunk* gfpxMontExp     (const cpFFchunk* pA, const Ipp32u* pB, Ipp32u bSize, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx);
//cpFFchunk* gfpxGetElement  (const cpFFchunk* pA, Ipp32u* pR, Ipp32u rLen, const IppsGFPXState* pGFPExtCtx);

cpFFchunk* gfpxRand(cpFFchunk* pR, IppsGFPXState* pGFPExtCtx, IppBitSupplier rndFunc, void* pRndParam);

__INLINE Ipp32s gfpxFFelementCmp(const cpFFchunk* pA, const cpFFchunk* pX, const IppsGFPXState* pGFPExtCtx)
{
   return gfpFFelementCmp(pA, pX, GFPX_FESIZE(pGFPExtCtx));
}

__INLINE cpFFchunk* gfpxGetPool(Ipp32u n, IppsGFPXState* pGFPExtCtx)
{
    cpFFchunk* pPool = GFPX_POOL(pGFPExtCtx);
    GFPX_POOL(pGFPExtCtx) += n*GFPX_PESIZE(pGFPExtCtx);
    return pPool;
}
__INLINE void gfpxReleasePool(Ipp32u n, IppsGFPXState* pGFPExtCtx)
{
   GFPX_POOL(pGFPExtCtx) -= n*GFPX_PESIZE(pGFPExtCtx);
}
__INLINE int degree(const cpFFchunk* pA, const IppsGFPXState* pGFPExtCtx)
{
    int i;
    Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
    for(i=(int)GFPX_DEGREE(pGFPExtCtx); i>=0; i-- ) {
        if(!GFP_IS_ZERO(pA+elementSize*i, elementSize)) break;
    }
    return i;
}
__INLINE cpFFchunk* gfpxFFelementCopy(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXState* pGFPExtCtx)
{
    gfpFFelementCopy(pA, pR, GFPX_FESIZE(pGFPExtCtx));
    return pR;
}
__INLINE Ipp32u gfpxBit(const cpFFchunk* pA, Ipp32u bitIdx)
{
    return pA[bitIdx>>5] & (1<<(bitIdx & 0x1F));
}

#define GFPX_SET_MONT_ONE(a,ctx) gfpFFelementCopyPadd(GFP_MONTUNITY(GFPX_GROUNDGF(ctx)), GFPX_GROUNDFESIZE(ctx), a, GFPX_FESIZE(ctx))
#define GFPX_ZERO(a,ctx)         GFP_ZERO(a, GFPX_FESIZE(ctx))
#define GFPX_IS_ZERO(a,ctx)      GFP_IS_ZERO(a, GFPX_FESIZE(ctx))
#define GFPX_IS_ONE(a,ctx)       GFP_IS_ONE(a, GFPX_FESIZE(ctx))
#define GFPX_IS_MONT_ONE(a,ctx)  (GFP_EQ(GFP_MONTUNITY(GFPX_GROUNDGF((ctx))), (a), GFPX_GROUNDFESIZE((ctx))) \
                               && GFP_IS_ZERO((a)+GFPX_GROUNDFESIZE((ctx)), GFPX_FESIZE((ctx))-GFPX_GROUNDFESIZE((ctx))))


#endif /* __PCP_GFPEXT__H__ */

