/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Definitions and
//    Internal Prime Modulo Arithmetic Function Prototypes
//
//    Created: Thu 12-Jul-2004 10:40
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_PMA224_H)
#define _PCP_PMA224_H


#include "pcpbn.h"
#include "pcpbnu.h"
#include "pcppmafix.h"


#define  LEN_P224 (BITS2WORD32_SIZE(224))

/*
// Modular Arithmetic for secp224r1 ECC
*/
void Reduce_P224r1(Ipp32u* pR);
void cpAdde_224r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSube_224r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSqre_224r1(IppsBigNumState* pA, IppsBigNumState* pR);
void cpMule_224r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);

#define PMA224_add(r,a,b) \
   cpAdde_224r1((a),(b), (r))

#define PMA224_sub(r,a,b) \
   cpSube_224r1((a),(b), (r))

#define PMA224_sqr(r,a) \
   cpSqre_224r1((a),(r))

#define PMA224_mul(r,a,b) \
   cpMule_224r1((a),(b), (r))

#define PMA224_div2(r,a) \
   if( IsOdd_BN((a)) ) { \
      FE_ADD(BN_NUMBER((a)), secp224r1_p, BN_NUMBER((a)), LEN_P224); \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), 1+LEN_P224, 1); \
   } \
   else \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), LEN_P224, 1)

#define PMA224_inv(r,a,modulo) \
{ \
   ippsModInv_BN((a),(modulo),(r)); \
   ZEXPAND_BNU(BN_NUMBER((r)),BN_SIZE((r)), LEN_P224); \
}

#endif /* _PCP_PMA224_H */

#endif /* _IPP_v50_ */
