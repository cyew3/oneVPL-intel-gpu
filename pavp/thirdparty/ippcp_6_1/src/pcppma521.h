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
//    Created: Thu 12-Jul-2004 17:14
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_PMA521_H)
#define _PCP_PMA521_H


#include "pcpbn.h"
#include "pcpbnu.h"
#include "pcppmafix.h"


#define  LEN_P521 (BITS2WORD32_SIZE(521))

/*
// Modular Arithmetic for secp521r1 ECC
*/
void cpAdde_521r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSube_521r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSqre_521r1(IppsBigNumState* pA, IppsBigNumState* pR);
void cpMule_521r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);

#define PMA521_add(r,a,b) \
   cpAdde_521r1((a),(b), (r))

#define PMA521_sub(r,a,b) \
   cpSube_521r1((a),(b), (r))

#define PMA521_sqr(r,a) \
   cpSqre_521r1((a),(r))

#define PMA521_mul(r,a,b) \
   cpMule_521r1((a),(b), (r))

#define PMA521_div2(r,a) \
   if( IsOdd_BN((a)) ) { \
      FE_ADD(BN_NUMBER((a)), secp521r1_p, BN_NUMBER((a)), LEN_P521); \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), 1+LEN_P521, 1); \
   } \
   else \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), LEN_P521, 1)

#define PMA521_inv(r,a,modulo) \
{ \
   ippsModInv_BN((a),(modulo),(r)); \
   ZEXPAND_BNU(BN_NUMBER((r)),BN_SIZE((r)), LEN_P521); \
}

#endif /* _PCP_PMA521_H */

#endif /* _IPP_v50_ */
