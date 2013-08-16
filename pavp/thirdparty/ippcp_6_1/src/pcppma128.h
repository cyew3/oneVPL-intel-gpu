/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Definitions and
//    Internal Prime Modulo Arithmetic Function Prototypes
//
//    Created: Mon 16-Aug-2004 10:12
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_PMA128_H)
#define _PCP_PMA128_H


#include "pcpbn.h"
#include "pcpbnu.h"
#include "pcppmafix.h"


#define  LEN_P128 (BITS2WORD32_SIZE(128))

/*
// Modular Arithmetic for secp128r1 ECC
*/
void Reduce_P128r1(Ipp32u* pR);
void cpAdde_128r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSube_128r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSqre_128r1(IppsBigNumState* pA, IppsBigNumState* pR);
void cpMule_128r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);

#define PMA128_add(r,a,b) \
   cpAdde_128r1((a),(b), (r))

#define PMA128_sub(r,a,b) \
   cpSube_128r1((a),(b), (r))

#define PMA128_sqr(r,a) \
   cpSqre_128r1((a),(r))

#define PMA128_mul(r,a,b) \
   cpMule_128r1((a),(b), (r))

#define PMA128_div2(r,a) \
   if( IsOdd_BN((a)) ) { \
      FE_ADD(BN_NUMBER((a)), secp128r1_p, BN_NUMBER((a)), LEN_P128); \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), 1+LEN_P128, 1); \
   } \
   else \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), LEN_P128, 1)

#define PMA128_inv(r,a,modulo) \
{ \
   ippsModInv_BN((a),(modulo),(r)); \
   ZEXPAND_BNU(BN_NUMBER((r)),BN_SIZE((r)), LEN_P128); \
}

#endif /* _PCP_PMA128_H */

#endif /* _IPP_v50_ */
