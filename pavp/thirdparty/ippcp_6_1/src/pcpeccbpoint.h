/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal EC Point Definitions & Function Prototypes
//
//    Created: Mon 15-Sep-2003 15:50
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_ECCBPOINT_H)
#define _PCP_ECCBPOINT_H

#include "pcpeccb.h"


/*
// There are several type of projective coordinates:
//
//    projective type                     projective equation
//    ---------------                     ------------------------------------
//    standard (X:Y:Z) => (X/Z,Y/X)       Y^2*Z + X*Y*Z = X^3   + a*X^2*Z   +b*Z^3
//    Jacobian (X:Y:Z) => (X/Z^2,Y/Z^3)   Y^2   + X*Y*Z = X^3   + a*X^2*Z^2 +b*Z^6
//    Lopez    (X:Y:Z) => (X/Z,Y/Z^2)     Y^2   + X*Y*Z = X^3*Z + a*X^2*Z^2 +b*Z^4
//
//
// EC Point context
*/
struct _cpECCBPoint {
   IppCtxId idCtx;  /* EC Point ID        */

   EPOLY*   pX;     /* Lopez projective X */
   EPOLY*   pY;     /*                  Y */
   EPOLY*   pZ;     /*                  Z */
   int  affine;     /* impotrant case Z=1 */
};


/*
// Contetx Access Macros
*/
#define ECB_POINT_ID(ctx)       ((ctx)->idCtx)
#define ECB_POINT_X(ctx)        ((ctx)->pX)
#define ECB_POINT_Y(ctx)        ((ctx)->pY)
#define ECB_POINT_Z(ctx)        ((ctx)->pZ)
#define ECB_POINT_AFFINE(ctx)   ((ctx)->affine)
#define ECB_POINT_VALID_ID(ctx) (ECB_POINT_ID((ctx))==idCtxECCBPoint)

/*
// Copy
*/
void ECCB_CopyPoint(const IppsECCBPointState* pSrc, IppsECCBPointState* pDst,
                    const IppsECCBState* pECC);

/*
// Point Set. These operations implies
// transformation of regular coordinates into internal format
*/
void ECCB_SetPointProjective(const EPOLY* pX,
                             const EPOLY* pY,
                             const EPOLY* pZ,
                             IppsECCBPointState* pPoint,
                             const IppsECCBState* pECC);

void ECCB_SetPointAffine(const EPOLY* pX,
                         const EPOLY* pY,
                         IppsECCBPointState* pPoint,
                         const IppsECCBState* pECC);

/*
// Get Point. These operations implies
// transformation of internal format coordinates into regular
*/
void ECCB_GetPointProjective(EPOLY* pX,
                             EPOLY* pY,
                             EPOLY* pZ,
                             const IppsECCBPointState* pPoint,
                             const IppsECCBState* pECC);

void ECCB_GetPointAffine(EPOLY* pX,
                         EPOLY* pY,
                         const IppsECCBPointState* pPoint,
                         const IppsECCBState* pECC,
                         PolyNode* pResource);

/*
// Set To Infinity
*/
void ECCB_SetPointToInfinity(IppsECCBPointState* pPoint, const IppsECCBState* pECC);
void ECCB_SetPointToAffineInfinity(EPOLY* pX, EPOLY* pY, int feBitSize);

/*
// Test Is At Infinity
// Test is On EC
*/
int ECCB_IsPointAtInfinity(const IppsECCBPointState* pPoint, const IppsECCBState* pECC);
int ECCB_IsPointAtAffineInfinity(const EPOLY* pX, const EPOLY* pY, int feBitSize);
int ECCB_IsPointOnCurve(const IppsECCBPointState* pPoint,
                        const IppsECCBState* pECC,
                        PolyNode* pResource);

/*
// Operations
*/
int ECCB_ComparePoint(const IppsECCBPointState* pP,
                      const IppsECCBPointState* pQ,
                      const IppsECCBState* pECC,
                      PolyNode* pResource);

void ECCB_NegPoint(const IppsECCBPointState* pP,
                   IppsECCBPointState* pR,
                   const IppsECCBState* pECC,
                   PolyNode* pResource);

void ECCB_DblPoint(const IppsECCBPointState* pP,
                   IppsECCBPointState* pR,
                   const IppsECCBState* pECC,
                   PolyNode* pResource);

void ECCB_AddPoint(const IppsECCBPointState* pP,
                   const IppsECCBPointState* pQ,
                   IppsECCBPointState* pR,
                   const IppsECCBState* pECC,
                   PolyNode* pResource);

void ECCB_Montgomery_MulPoint(const IppsECCBPointState* pP,
                              const IppsBigNumState* pK,
                              IppsECCBPointState* pR,
                              const IppsECCBState* pECC,
                              PolyNode* pResource);

#endif /* _PCP_ECCBPOINT_H */
