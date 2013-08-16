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
//    Addition BN Definitions & Function Prototypes
//
//    Created: Thu 09-Dec-2004 14:31
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_BNEXT_H)
#define _PCP_BNEXT_H

int Tst_BN(const IppsBigNumState* pA);
int Cmp_BN(const IppsBigNumState* pA, const IppsBigNumState* pB);
int IsZero_BN(const IppsBigNumState* pA);
int IsOne_BN(const IppsBigNumState* pA);
int IsOdd_BN(const IppsBigNumState* pA);

IppsBigNumState* BN_Zero(IppsBigNumState* pBN);
IppsBigNumState* BN_Power2(IppsBigNumState* pBN, int m);
void cpBN_copy(const IppsBigNumState* pSrc, IppsBigNumState* pDst);

#endif /* _PCP_BNEXT_H*/
