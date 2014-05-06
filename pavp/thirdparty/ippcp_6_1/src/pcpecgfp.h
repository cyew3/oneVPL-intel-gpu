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
#if !defined(__PCP_ECGFP__H__)
#define __PCP_ECGFP__H__

#include "pcpgfp.h"


/*
// EC over GF(p) Point context
*/
struct _cpGFPECPoint {
   IppCtxId     idCtx;  /* EC Point identifier     */
   Ipp32u       flags;  /* flags: affine           */
   Ipp32u elementSize;  /* size of each coordinate */
   cpFFchunk*   pData;  /* coordinatex X, Y, Z     */
};

/*
// Contetx Access Macros
*/
#define ECP_POINT_ID(ctx)       ((ctx)->idCtx)
#define ECP_POINT_FLAGS(ctx)    ((ctx)->flags)
#define ECP_POINT_FESIZE(ctx)   ((ctx)->elementSize)
#define ECP_POINT_DATA(ctx)     ((ctx)->pData)
#define ECP_POINT_X(ctx)        ((ctx)->pData)
#define ECP_POINT_Y(ctx)        ((ctx)->pData+(ctx)->elementSize)
#define ECP_POINT_Z(ctx)        ((ctx)->pData+(ctx)->elementSize*2)
#define ECP_POINT_TEST_ID(ctx)  (ECP_POINT_ID((ctx))==idCtxGFPPoint)

/* point flags */
#define ECP_AFFINE_POINT   (1)
#define ECP_FINITE_POINT   (2)

#define IS_ECP_AFFINE_POINT(ctx)    (ECP_POINT_FLAGS((ctx))&ECP_AFFINE_POINT)
#define ON_ECP_AFFINE_POINT(ctx)    (ECP_POINT_FLAGS((ctx))|ECP_AFFINE_POINT)
#define OFF_ECP_AFFINE_POINT(ctx)   (ECP_POINT_FLAGS((ctx))&~ECP_AFFINE_POINT)
#define IS_ECP_FINITE_POINT(ctx)    (ECP_POINT_FLAGS((ctx))&ECP_FINITE_POINT)
#define ON_ECP_FINITE_POINT(ctx)    (ECP_POINT_FLAGS((ctx))|ECP_FINITE_POINT)
#define OFF_ECP_FINITE_POINT(ctx)   (ECP_POINT_FLAGS((ctx))&~ECP_FINITE_POINT)

/* EC over GF(p) context */
struct _cpGFPEC {
   IppCtxId     idCtx;  /* EC identifier */

   IppsGFPState*  pGF;  /* basic Finite Field */

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
#define EC_POOL_SIZE       (8)  /* num of points into the pool */

#define ECP_ID(pCtx)          ((pCtx)->idCtx)
#define ECP_FIELD(pCtx)       ((pCtx)->pGF)
#define ECP_COFACTOR(pCtx)    ((pCtx)->cofactor)
#define ECP_ORDBITSIZE(pCtx)  ((pCtx)->orderBitSize)
#define ECP_FESIZE(pCtx)      ((pCtx)->elementSize)
#define ECP_A(pCtx)           ((pCtx)->pA)
#define ECP_B(pCtx)           ((pCtx)->pB)
#define ECP_G(pCtx)           ((pCtx)->pG)
#define ECP_R(pCtx)           ((pCtx)->pR)
#define ECP_POOL(pCtx)        ((pCtx)->pFFpool)
//#define ECP_MONTR(pCtx)       ((pCtx)->pMontR)

#define ECP_TEST_ID(pCtx)     (ECP_ID((pCtx))==idCtxGFPEC)

/*
// get/release n points from/to the pool
*/
__INLINE cpFFchunk* ecgfpGetPool(Ipp32u n, IppsGFPECState* pEC)
{
   cpFFchunk* pPool = ECP_POOL(pEC);
   ECP_POOL(pEC) += n*GFP_FESIZE(ECP_FIELD(pEC))*3;
   return pPool;
}
__INLINE void ecgfpReleasePool(Ipp32u n, IppsGFPECState* pEC)
{
   ECP_POOL(pEC) -= n*GFP_FESIZE(ECP_FIELD(pEC))*3;
}

__INLINE IppsGFPECPoint* ecgfpNewPoint(IppsGFPECPoint* pPoint, cpFFchunk* pData, Ipp32u flags, IppsGFPECState* pEC)
{
   ECP_POINT_ID(pPoint) = idCtxGFPPoint;
   ECP_POINT_FLAGS(pPoint) = flags;
   ECP_POINT_FESIZE(pPoint) = GFP_FESIZE(ECP_FIELD(pEC));
   ECP_POINT_DATA(pPoint) = pData;
   return pPoint;
}

/*
// copy one point into another
*/
__INLINE IppsGFPECPoint* ecgfpCopyPoint(const IppsGFPECPoint* pPointA, IppsGFPECPoint* pPointR, Ipp32u elementSize)
{
   gfpFFelementCopy(ECP_POINT_DATA(pPointA), ECP_POINT_DATA(pPointR), 3*elementSize);
   ECP_POINT_FLAGS(pPointR) = ECP_POINT_FLAGS(pPointA);
   return pPointR;
}

/*
// set point (convert into inside representation)
//    SetProjectivePoint
//    SetProjectivePointAtInfinity
//    SetAffinePoint
*/
__INLINE IppsGFPECPoint* ecgfpSetProjectivePoint(const cpFFchunk* pX, const cpFFchunk* pY, const cpFFchunk* pZ,
                                               IppsGFPECPoint* pPoint,
                                               IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   Ipp32u elementSize = GFP_FESIZE(pGF);
   Ipp32u pointFlag = 0;

   gfpMontEnc(pX, ECP_POINT_X(pPoint), pGF);
   gfpMontEnc(pY, ECP_POINT_Y(pPoint), pGF);
   gfpMontEnc(pZ, ECP_POINT_Z(pPoint), pGF);

   if(!GFP_IS_ZERO(pZ, elementSize)) pointFlag |= ECP_FINITE_POINT;
   if(GFP_IS_ONE(pZ, elementSize))  pointFlag |= ECP_AFFINE_POINT;
   ECP_POINT_FLAGS(pPoint) = pointFlag;
   return pPoint;
}
__INLINE IppsGFPECPoint* ecgfpSetProjectivePointAtInfinity(IppsGFPECPoint* pPoint, Ipp32u elementSize)
{
   gfpFFelementPadd(0, ECP_POINT_Z(pPoint), elementSize);
   ECP_POINT_FLAGS(pPoint) = 0;
   return pPoint;
}
__INLINE IppsGFPECPoint* ecgfpSetAffinePoint(const cpFFchunk* pX, const cpFFchunk* pY,
                                           IppsGFPECPoint* pPoint,
                                           IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   gfpMontEnc(pX, ECP_POINT_X(pPoint), pGF);
   gfpMontEnc(pY, ECP_POINT_Y(pPoint), pGF);
   gfpFFelementCopy(GFP_MONTUNITY(pGF), ECP_POINT_Z(pPoint), GFP_FESIZE(pGF));
   ECP_POINT_FLAGS(pPoint) = ECP_AFFINE_POINT | ECP_FINITE_POINT;
   return pPoint;
}

/*
// test infinity:
//    IsProjectivePointAtInfinity
//    IsAffinePointAtInfinity0 (B==0)
//    IsAffinePointAtInfinity1 (B!=0)
//    IsProjectivePointAffine
*/
__INLINE int ecgfpIsProjectivePointAtInfinity(const IppsGFPECPoint* pPoint, Ipp32u elementSize)
{
   return GFP_IS_ZERO( ECP_POINT_Z(pPoint), elementSize );
}
//__INLINE int ecgfpIsAffinePointAtInfinity0(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
//{
//   return GFP_IS_ZERO(pX, elementSize) && !GFP_IS_ZERO(pY, elementSize);
//}
//__INLINE int ecgfpIsAffinePointAtInfinity1(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
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
__INLINE void ecgfpGetProjectivePoint(const IppsGFPECPoint* pPoint,
                                      cpFFchunk* pX, cpFFchunk* pY, cpFFchunk* pZ,
                                      IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   gfpMontDec(ECP_POINT_X(pPoint), pX, pGF);
   gfpMontDec(ECP_POINT_Y(pPoint), pY, pGF);
   gfpMontDec(ECP_POINT_Z(pPoint), pZ, pGF);
}
__INLINE void ecgfpGetAffinePointAtInfinity0(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
{
   GFP_ZERO(pX, elementSize);
   GFP_ONE(pY, elementSize);
}
__INLINE void ecgfpGetAffinePointAtInfinity1(cpFFchunk* pX, cpFFchunk* pY, Ipp32u elementSize)
{
   GFP_ZERO(pX, elementSize);
   GFP_ZERO(pY, elementSize);
}

Ipp32u ecgfpGetAffinePoint(const IppsGFPECPoint* pPoint, cpFFchunk* pX, cpFFchunk* pY, IppsGFPECState* pEC);


/*
// other point operations
*/
int ecgfpIsPointEquial(const IppsGFPECPoint* pP, const IppsGFPECPoint* pQ, IppsGFPECState* pEC);
int ecgfpIsPointOnCurve(const IppsGFPECPoint* pP, IppsGFPECState* pEC);
int ecgfpIsPointInGroup(const IppsGFPECPoint* pP, IppsGFPECState* pEC);

IppsGFPECPoint* ecgfpNegPoint(const IppsGFPECPoint* pP,
                            IppsGFPECPoint* pR, IppsGFPECState* pEC);
IppsGFPECPoint* ecgfpDblPoint(const IppsGFPECPoint* pP,
                            IppsGFPECPoint* pR, IppsGFPECState* pEC);
IppsGFPECPoint* ecgfpAddPoint(const IppsGFPECPoint* pP, const IppsGFPECPoint* pQ,
                            IppsGFPECPoint* pR, IppsGFPECState* pEC);
IppsGFPECPoint* ecgfpMulPoint(const IppsGFPECPoint* pP, const cpFFchunk* pN, Ipp32u nSize,
                            IppsGFPECPoint* pR, IppsGFPECState* pEC);
/*
IppsGFPECPoint* ecgfpMulPoint2(const IppsGFPECPoint* pP1, const cpFFchunk* pN1, Ipp32u nSize1,
                             const IppsGFPECPoint* pP2, const cpFFchunk* pN2, Ipp32u nSize2,
                             IppsGFPECPoint* pR, IppsGFPECState* pEC);
IppsGFPECPoint* ecgfpMulPoint3(const IppsGFPECPoint* pP1, const cpFFchunk* pN1, Ipp32u nSize1,
                             const IppsGFPECPoint* pP2, const cpFFchunk* pN2, Ipp32u nSize2,
                             const IppsGFPECPoint* pP3, const cpFFchunk* pN3, Ipp32u nSize3,
                             IppsGFPECPoint* pR, IppsGFPECState* pEC);
IppsGFPECPoint* ecgfpMulPoint4(const IppsGFPECPoint* pP1, const cpFFchunk* pN1, Ipp32u nSize1,
                             const IppsGFPECPoint* pP2, const cpFFchunk* pN2, Ipp32u nSize2,
                             const IppsGFPECPoint* pP3, const cpFFchunk* pN3, Ipp32u nSize3,
                             const IppsGFPECPoint* pP4, const cpFFchunk* pN4, Ipp32u nSize4,
                             IppsGFPECPoint* pR, IppsGFPECState* pEC);
*/

#endif /* __PCP_ECGFP__H__ */
