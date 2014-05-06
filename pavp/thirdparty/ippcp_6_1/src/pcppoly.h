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
//    Internal Definitions of Polynomial over GF(2)
//
//    Created: Thu 16-Aug-2003 14:45
//  Author(s): Sergey Kirillov
//
*/
#if 0
#if !defined(_CP_POLY_H)
#define _CP_POLY_H

#include "pcpelpoly.h"

/*
// Polynomial Structure
*/
typedef struct _ippcPoly
{
   IppCtxId   idCtx;    /* ID                            */
   int        bitsize;  /* size of polynomial (bits)     */
   int        bitroom;  /* max size of polynomial (bits) */
   EPOLY*     pPoly;    /* polynomial                    */
} IppcPoly;

/*
// Accessory macros
*/
#define POLY_ID(ctx)       ((ctx)->idCtx)
#define POLY_SIZE(ctx)     ((ctx)->bitsize)
#define POLY_ROOM(ctx)     ((ctx)->bitroom)
#define POLY_DATA(ctx)     ((ctx)->pPoly)
#define POLY_VALID_ID(ctx) (POLY_ID((ctx))==idCtxUnknown)

/*
// Init polynomial
*/
IPPAPI(int,       ippcPolyGetSize, (int bitsize))
IPPAPI(IppStatus, ippcInitPoly, (IppcPoly* pA, int bitsize))
IPPAPI(IppStatus, ippcSetPoly, (IppcPoly* pA, const EPOLY* data, int bitsize))
IPPAPI(IppStatus, ippcSetPolyEPOLY,(IppcPoly* pA, EPOLY data))
IPPAPI(IppStatus, ippcCopyPoly,(IppcPoly* pR, const IppcPoly* pA))

/*
// Retrieve polynomial
*/
IPPAPI(IppStatus, ippcRefPoly, (EPOLY** pData, int* pBitsize, int* pBitroom, const IppcPoly* pA))

/*
// Test polynomial
*/
IPPAPI(int, IsZeroPoly,(const IppcPoly* pA))
IPPAPI(int, IsOnePoly, (const IppcPoly* pA))
IPPAPI(int, IsOddPoly, (const IppcPoly* pA))

#endif /* _CP_POLY_H */
#endif
