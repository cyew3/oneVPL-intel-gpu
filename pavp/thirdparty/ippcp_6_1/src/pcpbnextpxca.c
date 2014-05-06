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
//    Internal Addition BN functionality
//
// Contents:
//    cpBN_copy()
//    BN_Zero()
//    BN_Power2()
//
//    Tst_BN()
//    Cmp_BN()
//    IsZero_BN()
//    IsOne_BN()
//    IsOdd_BN()
//
//
//    Created: Thu 09-Dec-2004 14:05
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"

/*
// Copy BN
*/
void cpBN_copy(const IppsBigNumState* pSrc, IppsBigNumState* pDst)
{
   BN_SIGN(pDst) = BN_SIGN(pSrc);
   BN_SIZE(pDst) = BN_SIZE(pSrc);
   ZEXPAND_COPY_BNU(BN_NUMBER(pSrc),(int)BN_SIZE(pSrc), BN_NUMBER(pDst),(int)BN_ROOM(pDst));
}

/*
// Zeros BN
*/
IppsBigNumState* BN_Zero(IppsBigNumState* pBN)
{
   BN_SIGN(pBN)   = IppsBigNumPOS;
   BN_SIZE(pBN)   = 1;
   ZEXPAND_BNU(BN_NUMBER(pBN),0, (int)BN_ROOM(pBN));
   return pBN;
}

IppsBigNumState* BN_Power2(IppsBigNumState* pBN, int m)
{
   int size = BITS2WORD32_SIZE(m+1);
   if(BN_ROOM(pBN) >= size) {
      BN_SIGN(pBN) = IppsBigNumPOS;
      BN_SIZE(pBN) = size;
      ZEXPAND_BNU(BN_NUMBER(pBN),0, (int)BN_ROOM(pBN));
      SET_BIT(BN_NUMBER(pBN), m);
      return pBN;
   }
   else return NULL;
}

/*
// Tst_BN
//
// Compare BN and zero
//
// Returns
//    -1, if A < 0
//     0, if A = 0
//    +1, if A > 0
*/
int Tst_BN(const IppsBigNumState* pA)
{
   Ipp32u result;
   ippsCmpZero_BN(pA, &result);
   return (result==GREATER_THAN_ZERO)? 1 : (result==IS_ZERO)? 0 : -1;
}

/*
// Cmp_BN
//
// Compare two arbitrary BNs
//
// Returns
//    -1, if A < B
//     0, if A = B
//    +1, if A > B
*/
int Cmp_BN(const IppsBigNumState* pA, const IppsBigNumState* pB)
{
   IppsBigNumSGN signA = BN_SIGN(pA);
   IppsBigNumSGN signB = BN_SIGN(pB);

   if(signA==signB) {
      int result = Cmp_BNU(BN_NUMBER(pA), BN_SIZE(pA), BN_NUMBER(pB), BN_SIZE(pB));
      return (IppsBigNumPOS==signA)? result : -result;
   }
   return (IppsBigNumPOS==signA)? 1 : -1;
}

/*
// IsZero_BN
// IsOne_BN
// IsOdd_BN
*/
int IsZero_BN(const IppsBigNumState* pA)
{
   return ( BN_SIZE(pA)==1 ) && ( BN_NUMBER(pA)[0]==0 );
}

int IsOne_BN(const IppsBigNumState* pA)
{
   return ( BN_SIZE(pA)==1 ) && ( BN_NUMBER(pA)[0]==1 );
}

int IsOdd_BN(const IppsBigNumState* pA)
{
   return BN_NUMBER(pA)[0] & 1;
}
