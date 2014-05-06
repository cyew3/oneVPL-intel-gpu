/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Definitions and
//    Internal Prime Modulo Arithmetic Function Prototypes
//
//    Created: Fri 07-Mar-2003 20:10
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_PMA_H)
#define _PCP_PMA_H


#include "pcpbn.h"
#include "pcpmontgomery.h"


/*
// unsigned BN set/get
*/
#define SET_BN(pBN,bnu,len) \
   BN_SIGN((pBN)) = IppsBigNumPOS; \
   BN_SIZE((pBN)) = ((len)); \
   Cpy_BNU((bnu), BN_NUMBER((pBN)), (len))

#define GET_BN(pBN,bnu,len) \
   Set_BNU(0, (bnu), (len)); \
   Cpy_BNU(BN_NUMBER((pBN)), (bnu), BN_SIZE((pBN)))


/*
// Prime Modulo Arithmetic
*/
#define PMA_set(r,a) \
   BN_SIGN((r)) = BN_SIGN((a)); \
   BN_SIZE((r)) = BN_SIZE((a)); \
   ZEXPAND_COPY_BNU(BN_NUMBER((a)),(int)BN_SIZE((a)), BN_NUMBER((r)),(int)BN_ROOM((a))) \

#define PMA_mod(r,a,modulo) \
   ippsMod_BN((a),(modulo),(r))

#define PMA_inv(r,a,modulo) \
   ippsModInv_BN((a),(modulo),(r))

#define PMA_neg(r,a,modulo) \
   ippsSub_BN((modulo),(a),(r))

#define PMA_lsr(r,a,modulo) \
   BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), (int)BN_SIZE((a)), 1)
#define PMA_div2(r,a,modulo) \
   if( IsOdd_BN((a)) ) { \
      ippsAdd_BN((a), (modulo), (a)); \
   } \
   BN_SIZE((r)) = LSR_BNU(BN_NUMBER((a)), BN_NUMBER((r)), (int)BN_SIZE((a)), 1)

#define PMA_sqr(r,a,modulo) \
   PMA_mul(r,a,a,modulo)

#define PMA_add(r,a,b,modulo) \
   ippsAdd_BN((a),(b),(r));   \
   if( Cmp_BNU(BN_NUMBER((r)),(int)BN_SIZE((r)),BN_NUMBER((modulo)),(int)BN_SIZE(modulo)) >= 0 ) \
      ippsSub_BN((r),(modulo),(r))

#define PMA_sub(r,a,b,modulo) \
   ippsSub_BN((a),(b),(r));   \
   if( BN_NEGATIVE((r)) )     \
      ippsAdd_BN((r),(modulo),(r))

#define PMA_mul(r,a,b,modulo) \
   ippsMul_BN((a),(b),(r));   \
   if( Cmp_BNU(BN_NUMBER((r)),(int)BN_SIZE((r)),BN_NUMBER((modulo)),(int)BN_SIZE(modulo)) >= 0 ) \
      ippsMod_BN((r),(modulo),(r))

#define PMA_enc(r,a,mont) \
   ippsMontForm((a), (mont),(r))

#define PMA_dec(r,a,mont) \
   ippsMontMul((a), BN_ONE_REF(), (mont),(r))

#if !defined(_USE_SQR_)
#define PMA_sqre(r,a,mont) \
   ippsMontMul((a),(a), (mont),(r))
#else
#define PMA_sqre(r,a,mont) \
   cpMontSqr(BN_NUMBER((a)), \
             BN_SIZE((a)),   \
             BN_NUMBER(MNT_MODULO((mont))), \
             BN_SIZE(MNT_MODULO((mont))),   \
             BN_NUMBER((r)), \
            &BN_SIZE((r)),   \
             MNT_HELPER(MONT_CTX((mont))), \
             BN_NUMBER(MNT_PRODUCT((mont)))); \
   BN_SIGN((r)) = IppsBigNumPOS
#endif

#define PMA_mule(r,a,b,mont) \
   ippsMontMul((a),(b), (mont),(r))

#endif /* _PCP_PMA_H */

#endif /* _IPP_v50_ */
