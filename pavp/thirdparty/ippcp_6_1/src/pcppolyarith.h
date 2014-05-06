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
//    Internal Definitions and
//    Internal Propotypes of Polynomial Arithmetic over GF(2^m)
//
//    Created: Thu 28-Aug-2003 08:53
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_CP_POLYARITH_H)
#define _CP_POLYARITH_H

#include "pcppoly.h"
#include "pcppolyresource.h"



/* field operations */
int CmpPolyMod(const EPOLY* pA, const EPOLY* pB, int bitSize);

int AddPolyMod(EPOLY* pC,
               const EPOLY* pA, const EPOLY* pB, int bitSize);

int ModPoly(EPOLY* pR,
            const EPOLY* pA, int bitsize,
            const int pM[],
            PolyNode* pResource);

int RemPoly(EPOLY* pR,
            const EPOLY* pA, int aBitSize,
            const EPOLY* pM, int mBitsize,
            PolyNode* pResource);

int MulPolyMod(EPOLY* pC,
               const EPOLY* pA, const EPOLY* pB, int bitSize,
               const int pM[],
               PolyNode* pResource);

int SqrPolyMod(EPOLY* pC,
               const EPOLY* pA, int bitSize,
               const int pM[],
               PolyNode* pResource);

int InvPolyMod(EPOLY* pR,
               const EPOLY* pA, int bitSize,
               const EPOLY* pM,
               PolyNode* pResource);

int GCDPoly(EPOLY* pGCD, int* gcdBitSize,
            const EPOLY* pA, int aBitSize,
            const EPOLY* pM, int mBitsize,
            PolyNode* pResource);

int IrreduciblePoly(const EPOLY* pA, int aBitSize,
            PolyNode* pResource);

#endif /* _CP_POLYARITH_H */


