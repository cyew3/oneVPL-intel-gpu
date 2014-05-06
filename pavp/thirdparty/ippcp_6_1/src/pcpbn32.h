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
//    Created: Wed 08-Dec-2004 13:34
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_BN32_H)
#define _PCP_BN32_H

#include "pcpbn.h"


/*
// 32-bit value in BN format
*/
typedef struct _ippcpBigNum32 {
   IppsBigNumState   bn;
   Ipp32u            value;
   Ipp32u            temporary;
} IppsBigNumState32;


/* construct 1 word BN */
IppsBigNumState* BN_Word(IppsBigNumState* pBN, Ipp32u w);


#if defined(_IPP_TRS)

#  define DECLARE_BN_Word(X, Xvalue) \
   IppsBigNumState32 X = {{idCtxBigNum,IppsBigNumPOS,1,1,NULL,NULL},(Xvalue),0}

#  define INIT_BN_Word(X) \
   X.bn.number = &X.value; \
   X.bn.workBuffer = &X.temporary

#  define BN32_REFERENCE(X)  ((IppsBigNumState*)&(X))

#  define BN_ONE_REF()  BN32_REFERENCE(_trs_one32_)
#  define BN_WTO_REF()  BN32_REFERENCE(_trs_two32_)

#else
   /* reference to 1 */
   IppsBigNumState* BN_OneRef(void);
   /* reference to 1 */
   IppsBigNumState* BN_TwoRef(void);

#  define BN_ONE_REF()  BN_OneRef()
#  define BN_WTO_REF()  BN_TwoRef()
#endif

#endif /* _PCP_BN32_H */
