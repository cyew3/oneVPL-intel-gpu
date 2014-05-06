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
//    Created: Thu 15-Jul-2004 15:32
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_PMA384_H)
#define _PCP_PMA384_H


#include "pcpbn.h"
#include "pcpbnu.h"
#include "pcppmafix.h"


#define  LEN_P384 (BITS2WORD32_SIZE(384))

/*
// Modular Arithmetic for secp384r1 ECC
*/
void cpAdde_384r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSube_384r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSqre_384r1(IppsBigNumState* pA, IppsBigNumState* pR);
void cpMule_384r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);

#define PMA384_add(r,a,b) \
   cpAdde_384r1((a),(b), (r))

#define PMA384_sub(r,a,b) \
   cpSube_384r1((a),(b), (r))

#define PMA384_sqr(r,a) \
   cpSqre_384r1((a),(r))

#define PMA384_mul(r,a,b) \
   cpMule_384r1((a),(b), (r))

#define PMA384_div2(r,a) \
   if( IsOdd_BN((a)) ) { \
      FE_ADD(BN_NUMBER((a)), secp384r1_p, BN_NUMBER((a)), LEN_P384); \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), 1+LEN_P384, 1); \
   } \
   else \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), LEN_P384, 1)

#define PMA384_inv(r,a,modulo) \
{ \
   ippsModInv_BN((a),(modulo),(r)); \
   ZEXPAND_BNU(BN_NUMBER((r)),BN_SIZE((r)), LEN_P384); \
}

#endif /* _PCP_PMA384_H */

#endif /* _IPP_v50_ */
