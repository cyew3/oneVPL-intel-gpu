/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Intel(R) Integrated Performance Primitives
//    Cryptography Primitive.
//    Internal EC over GF(p) basic Definitions & Function Prototypes
//
*/
#if !defined(__PCP_GFPXEC__H__)
#define __PCP_GFPXEC__H__

#include "pcpgfp.h"
#include "pcpgfpext.h"


/*
// EC over GF(p) Point context
*/
struct _cpGFPXECPoint {
   IppCtxId     idCtx; /* EC Point identifier     */
   Ipp32u       flags; /* flags: affine           */
   Ipp32u elementSize; /* size of each coordinate */
   cpFFchunk*   pData; /* coordinatex X, Y, Z     */
};

/*
// Contetx Access Macros
*/
#define ECPX_POINT_ID(ctx)       ((ctx)->idCtx)
#define ECPX_POINT_FLAGS(ctx)    ((ctx)->flags)
#define ECPX_POINT_FESIZE(ctx)   ((ctx)->elementSize)
#define ECPX_POINT_DATA(ctx)     ((ctx)->pData)
#define ECPX_POINT_X(ctx)        ((ctx)->pData)
#define ECPX_POINT_Y(ctx)        ((ctx)->pData+(ctx)->elementSize)
#define ECPX_POINT_Z(ctx)        ((ctx)->pData+(ctx)->elementSize*2)
#define ECPX_POINT_TEST_ID(ctx)  (ECPX_POINT_ID((ctx))==idCtxGFPXECPoint)

/* point flags */
#define ECPX_AFFINE_POINT   (1)
#define ECPX_FINITE_POINT   (2)

#define IS_ECPX_AFFINE_POINT(ctx)    (ECPX_POINT_FLAGS((ctx))&ECPX_AFFINE_POINT)
#define ON_ECPX_AFFINE_POINT(ctx)    (ECPX_POINT_FLAGS((ctx))|ECPX_AFFINE_POINT)
#define OFF_ECPX_AFFINE_POINT(ctx)   (ECPX_POINT_FLAGS((ctx))&~ECPX_AFFINE_POINT)
#define IS_ECPX_FINITE_POINT(ctx)    (ECPX_POINT_FLAGS((ctx))&ECPX_FINITE_POINT)
#define ON_ECPX_FINITE_POINT(ctx)    (ECPX_POINT_FLAGS((ctx))|ECPX_FINITE_POINT)
#define OFF_ECPX_FINITE_POINT(ctx)   (ECPX_POINT_FLAGS((ctx))&~ECPX_FINITE_POINT)

/* EC over GF(p) context */
struct _cpGFPXEC {
   IppCtxId     idCtx;  /* EC identifier */

   IppsGFPXState* pGF;  /* basic Finite Field */

   Ipp32u     cofactor;  /* cofactor = #E/base_point order */
   Ipp32u orderBitSize;  /* base_point order bitsize */
   Ipp32u  elementSize;  /* size of point    */
   cpFFchunk*       pA;  /*   EC parameter A */
   cpFFchunk*       pB;  /*                B */
   cpFFchunk*       pG;  /*       base_point */
   cpFFchunk*       pR;  /* base_point order */
   cpFFchunk*  pFFpool;  /* pool of points   */
// IppsMontState* pMontR;
};

/* Local definitions */
#define ECPX_POOL_SIZE        (8)  /* num of points into the pool */

#define ECPX_ID(pCtx)         ((pCtx)->idCtx)
#define ECPX_FIELD(pCtx)      ((pCtx)->pGF)
#define ECPX_COFACTOR(pCtx)   ((pCtx)->cofactor)
#define ECPX_ORDBITSIZE(pCtx) ((pCtx)->orderBitSize)
#define ECPX_FESIZE(pCtx)     ((pCtx)->elementSize)
#define ECPX_A(pCtx)          ((pCtx)->pA)
#define ECPX_B(pCtx)          ((pCtx)->pB)
#define ECPX_G(pCtx)          ((pCtx)->pG)
#define ECPX_R(pCtx)          ((pCtx)->pR)
#define ECPX_POOL(pCtx)       ((pCtx)->pFFpool)
//#define ECPX_MONTR(pCtx)      ((pCtx)->pMontR)

#define ECPX_TEST_ID(pCtx)    (ECPX_ID((pCtx))==idCtxGFPXEC)


// some addition macros for GFPX
#define GFPX_MONT_ONE(a,ctx) gfpFFelementCopyPadd(GFP_MONTUNITY(GFPX_GROUNDGF(ctx)), GFPX_GROUNDFESIZE(ctx), a, GFPX_FESIZE(ctx))
#define GFPX_ONE(a,ctx)      GFP_ONE((a), GFPX_FESIZE(ctx))


/*
// get/release n points from/to the pool
*/
__INLINE cpFFchunk* ecgfpxGetPool(Ipp32u n, IppsGFPXECState* pEC)
{
   cpFFchunk* pPool = ECPX_POOL(pEC);
   ECPX_POOL(pEC) += n*ECPX_FESIZE(pEC);
   return pPool;
}
__INLINE void ecgfpxReleasePool(Ipp32u n, IppsGFPXECState* pEC)
{
   ECPX_POOL(pEC) -= n*ECPX_FESIZE(pEC);
}

__INLINE IppsGFPXECPoint* ecgfpxNewPoint(IppsGFPXECPoint* pPoint, cpFFchunk* pData, Ipp32u flags, IppsGFPXECState* pEC)
{
   ECPX_POINT_ID(pPoint) = idCtxGFPXECPoint;
   ECPX_POINT_FLAGS(pPoint) = flags;
   ECPX_POINT_FESIZE(pPoint) = GFPX_FESIZE(ECPX_FIELD(pEC));
   ECPX_POINT_DATA(pPoint) = pData;
   return pPoint;
}

/*
// copy one point into another
*/
__INLINE IppsGFPXECPoint* ecgfpxCopyPoint(const IppsGFPXECPoint* pPointA, IppsGFPXECPoint* pPointR, Ipp32u elementSize)
{
   gfpFFelementCopy(ECPX_POINT_DATA(pPointA), ECPX_POINT_DATA(pPointR), 3*elementSize);
   ECPX_POINT_FLAGS(pPointR) = ECPX_POINT_FLAGS(pPointA);
   return pPointR;
}

/*
// set point (convert into inside representation)
//    SetProjectivePoint
//    SetProjectivePointAtInfinity
//    SetAffinePoint
*/
__INLINE IppsGFPXECPoint* ecgfpxSetProjectivePoint(const cpFFchunk* pX, const cpFFchunk* pY, const cpFFchunk* pZ,
                                                   IppsGFPXECPoint* pPoint,
                                                   IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   //Ipp32u elementSize = GFPX_FESIZE(pGF);
   Ipp32u pointFlag = 0;

   gfpxMontEnc(pX, ECPX_POINT_X(pPoint), pGF);
   gfpxMontEnc(pY, ECPX_POINT_X(pPoint), pGF);
   gfpxMontEnc(pZ, ECPX_POINT_Y(pPoint), pGF);

   if(!GFPX_IS_ZERO(pZ, pGF)) pointFlag |= ECPX_FINITE_POINT;
   if(GFPX_IS_MONT_ONE(pZ, pGF))  pointFlag |= ECPX_AFFINE_POINT;
   ECPX_POINT_FLAGS(pPoint) = pointFlag;
   return pPoint;
}
__INLINE IppsGFPXECPoint* ecgfpxSetProjectivePointAtInfinity(IppsGFPXECPoint* pPoint, Ipp32u elementSize)
{
   gfpFFelementPadd(0, ECPX_POINT_Z(pPoint), elementSize);
   ECPX_POINT_FLAGS(pPoint) = 0;
   return pPoint;
}
__INLINE IppsGFPXECPoint* ecgfpxSetAffinePoint(const cpFFchunk* pX, const cpFFchunk* pY,
                                               IppsGFPXECPoint* pPoint,
                                               IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   gfpxMontEnc(pX, ECPX_POINT_X(pPoint), pGF);
   gfpxMontEnc(pY, ECPX_POINT_Y(pPoint), pGF);
   GFPX_MONT_ONE(ECPX_POINT_Z(pPoint), pGF);
   ECPX_POINT_FLAGS(pPoint) = ECPX_AFFINE_POINT | ECPX_FINITE_POINT;
   return pPoint;
}

/*
// test infinity:
//    IsProjectivePointAtInfinity
//    IsAffinePointAtInfinity0 (B==0)
//    IsAffinePointAtInfinity1 (B!=0)
//    IsProjectivePointAffine
*/
__INLINE int ecgfpxIsProjectivePointAtInfinity(const IppsGFPXECPoint* pPoint, Ipp32u elementSize)
{
   return GFP_IS_ZERO( ECPX_POINT_Z(pPoint), elementSize );
}
//__INLINE int ecgfpxIsAffinePointAtInfinity0(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
//{
//   return GFP_IS_ZERO(pX, elementSize) && !GFP_IS_ZERO(pY, elementSize);
//}
//__INLINE int ecgfpxIsAffinePointAtInfinity1(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
//{
//   return GFP_IS_ZERO(pX, elementSize) && GFP_IS_ZERO(pY, elementSize);
//}

/*
// get point (convert from inside representation)
//    GetProjectivePoint
//    GetAffinePointAtInfinity0 (B==0)
//    GetAffinePointAtInfinity1 (B!=0)
//    GetAffinePoint
*/
__INLINE void ecgfpxGetProjectivePoint(const IppsGFPXECPoint* pPoint,
                                      cpFFchunk* pX, cpFFchunk* pY, cpFFchunk* pZ,
                                      IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   gfpxMontDec(ECPX_POINT_X(pPoint), pX, pGF);
   gfpxMontDec(ECPX_POINT_Y(pPoint), pY, pGF);
   gfpxMontDec(ECPX_POINT_Z(pPoint), pZ, pGF);
}
//__INLINE void ecgfpxGetAffinePointAtInfinity0(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
//{
//   GFP_ZERO(pX, elementSize);
//   GFP_ONE(pY, elementSize);
//}
//__INLINE void ecgfpxGetAffinePointAtInfinity1(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
//{
//   GFP_ZERO(pX, elementSize);
//   GFP_ZERO(pY, elementSize);
//}

Ipp32u ecgfpxGetAffinePoint(const IppsGFPXECPoint* pPoint, cpFFchunk* pX, cpFFchunk* pY, IppsGFPXECState* pEC);


/*
// other point operations
*/
int ecgfpxIsPointEquial(const IppsGFPXECPoint* pP, const IppsGFPXECPoint* pQ, IppsGFPXECState* pEC);
int ecgfpxIsPointOnCurve(const IppsGFPXECPoint* pP, IppsGFPXECState* pEC);
int ecgfpxIsPointInGroup(const IppsGFPXECPoint* pP, IppsGFPXECState* pEC);

IppsGFPXECPoint* ecgfpxNegPoint(const IppsGFPXECPoint* pP,
                            IppsGFPXECPoint* pR, IppsGFPXECState* pEC);
IppsGFPXECPoint* ecgfpxDblPoint(const IppsGFPXECPoint* pP,
                            IppsGFPXECPoint* pR, IppsGFPXECState* pEC);
IppsGFPXECPoint* ecgfpxAddPoint(const IppsGFPXECPoint* pP, const IppsGFPXECPoint* pQ,
                            IppsGFPXECPoint* pR, IppsGFPXECState* pEC);
IppsGFPXECPoint* ecgfpxMulPoint(const IppsGFPXECPoint* pP, const cpFFchunk* pN, Ipp32u nSize,
                            IppsGFPXECPoint* pR, IppsGFPXECState* pEC);
/*
IppsGFPXECPoint* ecgfpxMulPoint2(const IppsGFPECPoint* pP1, const cpFFchunk* pN1, Ipp32u nSize1,
                             const IppsGFPECPoint* pP2, const cpFFchunk* pN2, Ipp32u nSize2,
                             IppsGFPECPoint* pR, IppsGFPXECState* pEC);
IppsGFPXECPoint* ecgfpxMulPoint3(const IppsGFPECPoint* pP1, const cpFFchunk* pN1, Ipp32u nSize1,
                             const IppsGFPECPoint* pP2, const cpFFchunk* pN2, Ipp32u nSize2,
                             const IppsGFPECPoint* pP3, const cpFFchunk* pN3, Ipp32u nSize3,
                             IppsGFPECPoint* pR, IppsGFPXECState* pEC);
IppsGFPXECPoint* ecgfpxMulPoint4(const IppsGFPECPoint* pP1, const cpFFchunk* pN1, Ipp32u nSize1,
                             const IppsGFPECPoint* pP2, const cpFFchunk* pN2, Ipp32u nSize2,
                             const IppsGFPECPoint* pP3, const cpFFchunk* pN3, Ipp32u nSize3,
                             const IppsGFPECPoint* pP4, const cpFFchunk* pN4, Ipp32u nSize4,
                             IppsGFPECPoint* pR, IppsGFPXECState* pEC);
*/

#endif /* __PCP_GFPXEC__H__ */
