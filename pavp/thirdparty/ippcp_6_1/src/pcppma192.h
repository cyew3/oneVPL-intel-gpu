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
//    Created: Sat 24-Jul-2004 10:30
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_PMA192_H)
#define _PCP_PMA192_H


#include "pcpbn.h"
#include "pcpbnu.h"
#include "pcppmafix.h"


#define  LEN_P192 (BITS2WORD32_SIZE(192))

/*
// Modular Arithmetic for secp192r1 ECC
*/
void Reduce_P192r1(Ipp32u* pR);
void cpAdde_192r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSube_192r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);
void cpSqre_192r1(IppsBigNumState* pA, IppsBigNumState* pR);
void cpMule_192r1(IppsBigNumState* pA, IppsBigNumState* pB, IppsBigNumState* pR);

#define PMA192_add(r,a,b) \
   cpAdde_192r1((a),(b), (r))

#define PMA192_sub(r,a,b) \
   cpSube_192r1((a),(b), (r))

#define PMA192_sqr(r,a) \
   cpSqre_192r1((a),(r))

#define PMA192_mul(r,a,b) \
   cpMule_192r1((a),(b), (r))

#define PMA192_div2(r,a) \
   if( IsOdd_BN((a)) ) { \
      FE_ADD(BN_NUMBER((a)), secp192r1_p, BN_NUMBER((a)), LEN_P192); \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), 1+LEN_P192, 1); \
   } \
   else \
      BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), LEN_P192, 1)

#define PMA192_inv(r,a,modulo) \
{ \
   ippsModInv_BN((a),(modulo),(r)); \
   ZEXPAND_BNU(BN_NUMBER((r)),BN_SIZE((r)), LEN_P192); \
}

#endif /* _PCP_PMA192_H */

#endif /* _IPP_v50_ */
