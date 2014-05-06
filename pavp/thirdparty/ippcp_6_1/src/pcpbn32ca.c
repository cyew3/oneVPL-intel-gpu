/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Definitions and
//    Internal BN32 Function Prototypes
//
//    Created: Wed 08-Dec-2004 13:39
//  Author(s): Sergey Kirillov
//
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpbn32.h"


/*
// construct 32-bit BN
*/
IppsBigNumState* BN_Word(IppsBigNumState* pBN, Ipp32u w)
{
   BN_SIGN(pBN)   = IppsBigNumPOS;
   BN_SIZE(pBN)   = 1;
   ZEXPAND_BNU(BN_NUMBER(pBN),0, (int)BN_ROOM(pBN));
   *BN_NUMBER(pBN)= w;
   return pBN;
}

#if !defined(_IPP_TRS)
/*
// Define 1
*/
static IppsBigNumState32 one = {
   {
      idCtxBigNum,
      IppsBigNumPOS,
      1,1,
      &one.value,&one.temporary
   },
   1,0
};

/* returns reference on 1 */
IppsBigNumState* BN_OneRef(void)
{ return &one.bn; }


/*
// Define 2
*/
static IppsBigNumState32 two = {
   {
      idCtxBigNum,
      IppsBigNumPOS,
      1,1,
      &two.value,&two.temporary
   },
   2,0
};

/* returns reference on 2 */
IppsBigNumState* BN_TwoRef(void)
{ return &two.bn; }

#endif
