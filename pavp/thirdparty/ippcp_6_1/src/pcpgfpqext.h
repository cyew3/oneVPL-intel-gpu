
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
//              GF(p) quadratic extension internal
*/
#if !defined(__PCP_GFPQEXT__H__)


struct _cpGFpQExtElement {
    IppCtxId idCtx;
    cpFFchunk* pData;
};

#define GFPXQXE_ID(pCtx) ((pCtx)->idCtx)
#define GFPXQXE_DATA(pCtx) ((pCtx)->pData)
#define GFPXQXE_TEST_ID(pCtx) (GFPXE_ID((pCtx))==idCtxGFPXQXE)

/* GF(P) quadratic extension context */
struct _cpGFpQExt {
    IppCtxId            idCtx;
    Ipp32u              degreeQx;             /* Degree of extension, currently 2 */
    Ipp32u              elementSizeQx;        /* length in dwords of GF(p^k) element */
    cpFFchunk*          pModulusQx;           /* Quadratic extension modulus */
    IppsGFPXState*      pGroundGFQx;         /* Corresponding GFPX structure */
    cpFFchunk*          pFFpoolQx;            /* Pool of temporary variables */
};
/* Limits */
#define GFPXQX_SAFEID_DEGREE (2)
#define GFPXQX_MAX_DEGREE   (GFPXQX_SAFEID_DEGREE)
#define GFPXQX_POOL_SIZE    (10)

/* Useful definitions */
#define GFPXQX_ID(pCtx)             ((pCtx)->idCtx)
#define GFPXQX_DEGREE(pCtx)         ((pCtx)->degreeQx)
#define GFPXQX_FESIZE(pCtx)         ((pCtx)->elementSizeQx)
#define GFPXQX_MODULUS(pCtx)        ((pCtx)->pModulusQx)
#define GFPXQX_GROUNDGF(pCtx)       ((pCtx)->pGroundGFQx)
#define GFPXQX_GROUNDFESIZE(pCtx)   (GFPX_FESIZE(GFPXQX_GROUNDGF((pCtx))))
#define GFPXQX_POOL(pCtx)           ((pCtx)->pFFpoolQx)
#define GFPXQX_IDX_ELEMENT(ptr, idx, eleSize) \
                                    ((ptr)+(eleSize)*(idx))
#define GFPXQX_SET_MONT_ONE(a,ctx)  gfpFFelementCopyPadd(GFP_MONTUNITY(GFPX_GROUNDGF(GFPXQX_GROUNDGF(ctx))), \
                                        GFPX_GROUNDFESIZE(GFPXQX_GROUNDGF(ctx)), a, GFPXQX_FESIZE(ctx))
#define GFPXQX_ZERO(a,ctx)          GFP_ZERO(a, GFPXQX_FESIZE(ctx))
#define GFPXQX_IS_ZERO(a,ctx)       GFP_IS_ZERO(a, GFPXQX_FESIZE(ctx))
#define GFPXQX_IS_ONE(a,ctx)        GFP_IS_ONE(a, GFPXQX_FESIZE(ctx))
#define GFPXQX_TEST_ID(pCtx)        (GFPXQX_ID((pCtx))==idCtxGFPXQX)
#define GFPXQX_PESIZE(pCtx)         (GFPXQX_FESIZE(pCtx))

/* Internal function prototypes */
Ipp32u gfpxqxIsUserModulusValid(const Ipp32u* pA, Ipp32u lenA, const IppsGFPXQState* pGFPExtCtx);

cpFFchunk* gfpxqxReduce(const cpFFchunk* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxAdd(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxSub(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxNeg(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxMontMul(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxMontMul_GFP(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxMontExp(const cpFFchunk* pA, const cpFFchunk* pB, Ipp32u bSize, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxMontInv(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxFFelementCopy(const cpFFchunk* pA, cpFFchunk* pB, const IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxMontEnc(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxMontDec(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXQState* pGFPQExtCtx);
cpFFchunk* gfpxqxRand(cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx, IppBitSupplier rndFunc, void *pRndParam);

__INLINE Ipp32s gfpxqxFFelementCmp(const cpFFchunk* pA, const cpFFchunk* pX, const IppsGFPXQState* pGFPQExtCtx)
{
   return gfpFFelementCmp(pA, pX, GFPXQX_FESIZE(pGFPQExtCtx));
}
__INLINE cpFFchunk* gfpxqxGetPool(Ipp32u n, IppsGFPXQState* pGFPQExtCtx)
{
    cpFFchunk* pPool = GFPXQX_POOL(pGFPQExtCtx);
    GFPXQX_POOL(pGFPQExtCtx) += n*GFPXQX_PESIZE(pGFPQExtCtx);
    return pPool;
}
__INLINE void gfpxqxReleasePool(Ipp32u n, IppsGFPXQState* pGFPQExtCtx)
{
   GFPXQX_POOL(pGFPQExtCtx) -= n*GFPXQX_PESIZE(pGFPQExtCtx);
}

#endif /* !defined(__PCP_GFPQEXT__H__) */

