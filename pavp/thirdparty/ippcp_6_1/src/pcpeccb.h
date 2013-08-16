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
//    Internal ECC (binary) basic Definitions & Function Prototypes
//
//    Created: Sat 13-Sep-2003 14:54
//  Author(s): Sergey Kirillov
//
*/
#if defined( _IPP_v50_)

#if !defined(_PCP_ECCB_H)
#define _PCP_ECCB_H

#include "pcpelpoly.h"
#include "pcppolyresource.h"
#include "pcpbnresource.h"
#include "pcppma.h"

/*
// ECC over binary GF(2^m) Context
*/
struct _cpECCB {
   IppCtxId            idCtx;      /* prime EC identifier           */

   int                 gfeBitSize; /* size (bits) of field element  */
   int                 ordBitSize; /* size (bits) of BP order       */

   Ipp32u              eccStandard;/* generic/standard ecc          */

   EPOLY*              pPrime;     /* irredicible polynomia         */
   Ipp32s*             pPrimeArr;  /* irredicible in array's form   */
   EPOLY*              pA;         /* scecify A & B of EC equation: */
   EPOLY*              pB;         /* y^2 + x*y = x^3 + A*x^2 + B   */
   int                 is_a_0;     /* useful special cases          */
   int                 is_a_1;
   int                 is_b_1;

   IppsMontState*      pMontR;   /* montromery engine (modulo r)  */
   IppsBigNumState*    pCofactor;/* cofactor = #E/base_point_order*/

   IppsECCBPointState* pGenc; /* internal formatted Base Point */

   IppsBigNumState*    pPrivate;   /* private key                   */
   IppsECCBPointState* pPublic;    /* public key                    */
   IppsBigNumState*    pPrivateE;  /* ephemeral private key         */
   IppsECCBPointState* pPublicE;   /* ephemeral public key          */

   #if defined(_USE_NN_VERSION_)
   Ipp32u              randMask;   /* mask of high bits random      */
   IppsBigNumState*    pRandCnt;   /* random engine content         */
   IppsPRNGState*      pRandGen;   /* random generator engine       */
   #endif

   IppsPrimeState*     pPrimary;   /* prime engine                  */

   PolyNode*           pRcList;    /* list of temporary polynomials */
   BigNumNode*         pBnList;    /* list of big numbers           */
};

/* some useful constants */
#define PRIME_ARR_MAX     (5) /* most pentanomial irredicible  */
#define POLYLISTSIZE     (32) /* polynomial resource size (probably less) */
#define BNLISTSIZE        (5) /* list size (probably less) */


/*
// Contetx Access Macros
*/
#define ECB_ID(ctx)        ((ctx)->idCtx)

#define ECB_GFEBITS(ctx)   ((ctx)->gfeBitSize)
#define ECB_GFESIZE(ctx)   (BITS2WORD32_SIZE((ctx)->gfeBitSize))
#define ECB_ORDBITS(ctx)   ((ctx)->ordBitSize)
#define ECB_ORDSIZE(ctx)   (BITS2WORD32_SIZE((ctx)->ordBitSize))

#define ECB_TYPE(ctx)      ((ctx)->eccStandard)

#define ECB_PRIME(ctx)     ((ctx)->pPrime)
#define ECB_PRIMEARR(ctx)  ((ctx)->pPrimeArr)
#define ECB_A(ctx)         ((ctx)->pA)
#define ECB_B(ctx)         ((ctx)->pB)
#define ECB_A_IS_ZERO(ctx) ((ctx)->is_a_0)
#define ECB_A_IS_ONE(ctx)  ((ctx)->is_a_1)
#define ECB_B_IS_ONE(ctx)  ((ctx)->is_b_1)

#define ECB_RMONT(ctx)     ((ctx)->pMontR)
#define ECB_COFACTOR(ctx)  ((ctx)->pCofactor)

#define ECB_GENC(ctx)      ((ctx)->pGenc)

#define ECB_PRIVATE(ctx)   ((ctx)->pPrivate)
#define ECB_PUBLIC(ctx)    ((ctx)->pPublic)
#define ECB_PRIVATE_E(ctx) ((ctx)->pPrivateE)
#define ECB_PUBLIC_E(ctx)  ((ctx)->pPublicE)

#if defined(_USE_NN_VERSION_)
#define ECB_RANDMASK(ctx)  ((ctx)->randMask)
#define ECB_RANDCNT(ctx)   ((ctx)->pRandCnt)
#define ECB_RAND(ctx)      ((ctx)->pRandGen)
#endif

#define ECB_PRIMARY(ctx)   ((ctx)->pPrimary)
#define ECB_RCCTX(ctx)     ((ctx)->pRcList)
#define ECB_BNCTX(ctx)     ((ctx)->pBnList)

#define ECB_VALID_ID(ctx)  (ECB_ID((ctx))==idCtxECCB)

/*
// Recommended (Standard) Domain Parameters
*/
extern const Ipp32u* sect113r1_p; /* x^113 +x^9 +1 */
extern const Ipp32u  sect113r1_a[];
extern const Ipp32u  sect113r1_b[];
extern const Ipp32u sect113r1_gx[];
extern const Ipp32u sect113r1_gy[];
extern const Ipp32u sect113r1_r[];
extern       Ipp32u sect113r1_h;

extern const Ipp32u* sect113r2_p; /* x^113 +x^9 +1 */
extern const Ipp32u  sect113r2_a[];
extern const Ipp32u  sect113r2_b[];
extern const Ipp32u sect113r2_gx[];
extern const Ipp32u sect113r2_gy[];
extern const Ipp32u sect113r2_r[];
extern       Ipp32u sect113r2_h;

extern const Ipp32u* sect131r1_p; /* x^131 +x^8 +x^3 +x^2 +1 */
extern const Ipp32u  sect131r1_a[];
extern const Ipp32u  sect131r1_b[];
extern const Ipp32u sect131r1_gx[];
extern const Ipp32u sect131r1_gy[];
extern const Ipp32u sect131r1_r[];
extern       Ipp32u sect131r1_h;

extern const Ipp32u* sect131r2_p; /* x^131 +x^8 +x^3 +x^2 +1 */
extern const Ipp32u  sect131r2_a[];
extern const Ipp32u  sect131r2_b[];
extern const Ipp32u sect131r2_gx[];
extern const Ipp32u sect131r2_gy[];
extern const Ipp32u sect131r2_r[];
extern       Ipp32u sect131r2_h;

extern const Ipp32u* sect163r1_p; /* x^163 +x^7 +x^6 +x^3 +1 */
extern const Ipp32u  sect163r1_a[];
extern const Ipp32u  sect163r1_b[];
extern const Ipp32u sect163r1_gx[];
extern const Ipp32u sect163r1_gy[];
extern const Ipp32u sect163r1_r[];
extern       Ipp32u sect163r1_h;

extern const Ipp32u* sect163r2_p; /* x^163 +x^7 +x^6 +x^3 +1 */
extern const Ipp32u  sect163r2_a[];
extern const Ipp32u  sect163r2_b[];
extern const Ipp32u sect163r2_gx[];
extern const Ipp32u sect163r2_gy[];
extern const Ipp32u sect163r2_r[];
extern       Ipp32u sect163r2_h;

extern const Ipp32u* sect193r1_p; /* x^193 +x^15 +1 */
extern const Ipp32u  sect193r1_a[];
extern const Ipp32u  sect193r1_b[];
extern const Ipp32u sect193r1_gx[];
extern const Ipp32u sect193r1_gy[];
extern const Ipp32u sect193r1_r[];
extern       Ipp32u sect193r1_h;

extern const Ipp32u* sect193r2_p; /* x^193 +x^15 +1 */
extern const Ipp32u  sect193r2_a[];
extern const Ipp32u  sect193r2_b[];
extern const Ipp32u sect193r2_gx[];
extern const Ipp32u sect193r2_gy[];
extern const Ipp32u sect193r2_r[];
extern       Ipp32u sect193r2_h;

extern const Ipp32u* sect233r1_p; /* x^233 +x^74 +1 */
extern const Ipp32u  sect233r1_a[];
extern const Ipp32u  sect233r1_b[];
extern const Ipp32u sect233r1_gx[];
extern const Ipp32u sect233r1_gy[];
extern const Ipp32u sect233r1_r[];
extern       Ipp32u sect233r1_h;

extern const Ipp32u* sect283r1_p; /* x^283 +x^12 +x^7 +x^5 +1 */
extern const Ipp32u  sect283r1_a[];
extern const Ipp32u  sect283r1_b[];
extern const Ipp32u sect283r1_gx[];
extern const Ipp32u sect283r1_gy[];
extern const Ipp32u sect283r1_r[];
extern       Ipp32u sect283r1_h;

extern const Ipp32u* sect409r1_p; /* x^409 +x^87 +1 */
extern const Ipp32u  sect409r1_a[];
extern const Ipp32u  sect409r1_b[];
extern const Ipp32u sect409r1_gx[];
extern const Ipp32u sect409r1_gy[];
extern const Ipp32u sect409r1_r[];
extern       Ipp32u sect409r1_h;

extern const Ipp32u* sect571r1_p; /* x^571 +x^10 +x^5 +x^2 +1 */
extern const Ipp32u  sect571r1_a[];
extern const Ipp32u  sect571r1_b[];
extern const Ipp32u sect571r1_gx[];
extern const Ipp32u sect571r1_gy[];
extern const Ipp32u sect571r1_r[];
extern       Ipp32u sect571r1_h;


extern const Ipp32u* sect163k1_p; /* x^163 +x^7 +x^6 +x^3 +1 */
extern const Ipp32u  sect163k1_a[];
extern const Ipp32u  sect163k1_b[];
extern const Ipp32u sect163k1_gx[];
extern const Ipp32u sect163k1_gy[];
extern const Ipp32u sect163k1_r[];
extern       Ipp32u sect163k1_h;

extern const Ipp32u* sect233k1_p; /* x^233 +x^74 +1 */
extern const Ipp32u  sect233k1_a[];
extern const Ipp32u  sect233k1_b[];
extern const Ipp32u sect233k1_gx[];
extern const Ipp32u sect233k1_gy[];
extern const Ipp32u sect233k1_r[];
extern       Ipp32u sect233k1_h;

extern const Ipp32u* sect239k1_p; /* x^239 +x^158 +1 */
extern const Ipp32u  sect239k1_a[];
extern const Ipp32u  sect239k1_b[];
extern const Ipp32u sect239k1_gx[];
extern const Ipp32u sect239k1_gy[];
extern const Ipp32u sect239k1_r[];
extern       Ipp32u sect239k1_h;

extern const Ipp32u* sect283k1_p; /* x^283 +x^12 +x^7 +x^5 +1 */
extern const Ipp32u  sect283k1_a[];
extern const Ipp32u  sect283k1_b[];
extern const Ipp32u sect283k1_gx[];
extern const Ipp32u sect283k1_gy[];
extern const Ipp32u sect283k1_r[];
extern       Ipp32u sect283k1_h;

extern const Ipp32u* sect409k1_p; /* x^409 +x^87 +1 */
extern const Ipp32u  sect409k1_a[];
extern const Ipp32u  sect409k1_b[];
extern const Ipp32u sect409k1_gx[];
extern const Ipp32u sect409k1_gy[];
extern const Ipp32u sect409k1_r[];
extern       Ipp32u sect409k1_h;

extern const Ipp32u* sect571k1_p; /* x^571 +x^10 +x^5 +x^2 +1 */
extern const Ipp32u  sect571k1_a[];
extern const Ipp32u  sect571k1_b[];
extern const Ipp32u sect571k1_gx[];
extern const Ipp32u sect571k1_gy[];
extern const Ipp32u sect571k1_r[];
extern       Ipp32u sect571k1_h;

#endif /* _PCP_ECCB_H */

#endif /* _IPP_v50_ */
