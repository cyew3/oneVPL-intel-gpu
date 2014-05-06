/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal ECC (prime) basic Definitions & Function Prototypes
//
//    Created: Fri 07-Mar-2003 20:10
//  Author(s): Sergey Kirillov
//
*/
#if defined( _IPP_v50_)

#if !defined(_PCP_ECCP_H)
#define _PCP_ECCP_H

#include "pcpbnresource.h"
#include "pcppma.h"
#include "pcpeccppoint.h"


#if !defined(_IPP_TRS)
typedef struct eccp_method_st ECCP_METHOD;
#endif

/*
// ECC over prime GF(p) Context
*/
struct _cpECCP {
   IppCtxId            idCtx;      /* prime EC identifier           */

   IppsBigNumState*    pPrime;     /* specify finite field GF(p)    */
   IppsBigNumState*    pA;         /* scecify A & B of EC equation: */
   IppsBigNumState*    pB;         /* y^2 = x^3 + A*x + B (mod)p    */

   IppsBigNumState*    pGX;        /* Base Point (X coordinate)     */
   IppsBigNumState*    pGY;        /* Base Point (Y coordinate)     */
   IppsBigNumState*    pR;         /* order (r) of Base Point       */
   /*    fields above mainly for ippsECCPSet()/ippsECCPGet()        */

   Ipp32u              eccStandard;/* generic/standard ecc          */

   #if !defined(_IPP_TRS)
   ECCP_METHOD*        pMethod;
   #endif

   int                 gfeBitSize; /* size (bits) of field element  */
   int                 ordBitSize; /* size (bits) of BP order       */

   int                 a_3;        /* ==1 if A==-3 or A==P-3        */
   IppsBigNumState*    pAenc;      /* internal formatted pA  value  */
   IppsBigNumState*    pBenc;      /* internal formatted pB  value  */
   IppsMontState*      pMontP;     /* montromery engine (modulo p)  */

   IppsECCPPointState* pGenc;      /* internal formatted Base Point */
   IppsBigNumState*    pCofactor;  /* cofactor = #E/base_point_order*/
   IppsMontState*      pMontR;     /* montromery engine (modulo r)  */

   IppsBigNumState*    pPrivate;   /* private key                   */
   IppsECCPPointState* pPublic;    /* public key (affine)           */
   IppsBigNumState*    pPrivateE;  /* ephemeral private key         */
   IppsECCPPointState* pPublicE;   /* ephemeral public key (affine) */

   #if defined(_USE_NN_VERSION_)
   Ipp32u              randMask;   /* mask of high bits random      */
   IppsBigNumState*    pRandCnt;   /* random engine content         */
   IppsPRNGState*      pRandGen;   /* random generator engine       */
   #endif

   IppsPrimeState*     pPrimary;   /* prime engine                  */
   BigNumNode*         pBnList;    /* list of big numbers           */
 /*BigNumNode*        pBnListExt;*//* list of big numbers           */
};

/* some useful constants */
#define BNLISTSIZE      (32)  /* list size (probably less) */

/*
// Contetx Access Macros
*/
#define ECP_ID(ctx)        ((ctx)->idCtx)

#define ECP_PRIME(ctx)     ((ctx)->pPrime)
#define ECP_A(ctx)         ((ctx)->pA)
#define ECP_B(ctx)         ((ctx)->pB)

#define ECP_GX(ctx)        ((ctx)->pGX)
#define ECP_GY(ctx)        ((ctx)->pGY)
#define ECP_ORDER(ctx)     ((ctx)->pR)

#define ECP_TYPE(ctx)      ((ctx)->eccStandard)

#if !defined(_IPP_TRS)
#define ECP_METHOD(ctx)    ((ctx)->pMethod)
#endif

#define ECP_GFEBITS(ctx)   ((ctx)->gfeBitSize)
#define ECP_GFESIZE(ctx)   (BITS2WORD32_SIZE((ctx)->gfeBitSize))
#define ECP_ORDBITS(ctx)   ((ctx)->ordBitSize)
#define ECP_ORDSIZE(ctx)   (BITS2WORD32_SIZE((ctx)->ordBitSize))

#define ECP_AMI3(ctx)      ((ctx)->a_3)
#define ECP_AENC(ctx)      ((ctx)->pAenc)
#define ECP_BENC(ctx)      ((ctx)->pBenc)
#define ECP_PMONT(ctx)     ((ctx)->pMontP)

#define ECP_GENC(ctx)      ((ctx)->pGenc)
#define ECP_COFACTOR(ctx)  ((ctx)->pCofactor)
#define ECP_RMONT(ctx)     ((ctx)->pMontR)

#define ECP_PRIVATE(ctx)   ((ctx)->pPrivate)
#define ECP_PUBLIC(ctx)    ((ctx)->pPublic)
#define ECP_PRIVATE_E(ctx) ((ctx)->pPrivateE)
#define ECP_PUBLIC_E(ctx)  ((ctx)->pPublicE)

#if defined(_USE_NN_VERSION_)
#define ECP_RANDMASK(ctx)  ((ctx)->randMask)
#define ECP_RANDCNT(ctx)   ((ctx)->pRandCnt)
#define ECP_RAND(ctx)      ((ctx)->pRandGen)
#endif

#define ECP_PRIMARY(ctx)   ((ctx)->pPrimary)
#define ECP_BNCTX(ctx)     ((ctx)->pBnList)
//#define ECP_BNCTX_EXT(ctx) ((ctx)->pBnListExt)

#define ECP_VALID_ID(ctx)  (ECP_ID((ctx))==idCtxECCP)

/*
// Recommended (Standard) Domain Parameters
*/
extern const Ipp32u secp112r1_p[]; // (2^128 -3)/76439
extern const Ipp32u secp112r1_a[];
extern const Ipp32u secp112r1_b[];
extern const Ipp32u secp112r1_gx[];
extern const Ipp32u secp112r1_gy[];
extern const Ipp32u secp112r1_r[];
extern       Ipp32u secp112r1_h;

extern const Ipp32u secp112r2_p[]; // (2^128 -3)/76439
extern const Ipp32u secp112r2_a[];
extern const Ipp32u secp112r2_b[];
extern const Ipp32u secp112r2_gx[];
extern const Ipp32u secp112r2_gy[];
extern const Ipp32u secp112r2_r[];
extern       Ipp32u secp112r2_h;

extern const Ipp32u secp128r1_p[]; // 2^128 -2^97 -1
extern const Ipp32u secp128r1_a[];
extern const Ipp32u secp128r1_b[];
extern const Ipp32u secp128r1_gx[];
extern const Ipp32u secp128r1_gy[];
extern const Ipp32u secp128r1_r[];
extern       Ipp32u secp128r1_h;

extern const Ipp32u* secp128_mx[];

extern const Ipp32u secp128r2_p[]; // 2^128 -2^97 -1
extern const Ipp32u secp128r2_a[];
extern const Ipp32u secp128r2_b[];
extern const Ipp32u secp128r2_gx[];
extern const Ipp32u secp128r2_gy[];
extern const Ipp32u secp128r2_r[];
extern       Ipp32u secp128r2_h;

extern const Ipp32u secp160r1_p[]; // 2^160 -2^31 -1
extern const Ipp32u secp160r1_a[];
extern const Ipp32u secp160r1_b[];
extern const Ipp32u secp160r1_gx[];
extern const Ipp32u secp160r1_gy[];
extern const Ipp32u secp160r1_r[];
extern       Ipp32u secp160r1_h;

extern const Ipp32u secp160r2_p[]; // 2^160 -2^32 -2^14 -2^12 -2^9 -2^8 -2^7 -2^2 -1
extern const Ipp32u secp160r2_a[];
extern const Ipp32u secp160r2_b[];
extern const Ipp32u secp160r2_gx[];
extern const Ipp32u secp160r2_gy[];
extern const Ipp32u secp160r2_r[];
extern       Ipp32u secp160r2_h;

extern const Ipp32u secp192r1_p[]; // 2^192 -2^64 -1
extern const Ipp32u secp192r1_a[];
extern const Ipp32u secp192r1_b[];
extern const Ipp32u secp192r1_gx[];
extern const Ipp32u secp192r1_gy[];
extern const Ipp32u secp192r1_r[];
extern       Ipp32u secp192r1_h;

extern const Ipp32u secp224r1_p[]; // 2^224 -2^96 +1
extern const Ipp32u secp224r1_a[];
extern const Ipp32u secp224r1_b[];
extern const Ipp32u secp224r1_gx[];
extern const Ipp32u secp224r1_gy[];
extern const Ipp32u secp224r1_r[];
extern       Ipp32u secp224r1_h;

extern const Ipp32u secp256r1_p[]; // 2^256 -2^224 +2^192 +2^96 -1
extern const Ipp32u secp256r1_a[];
extern const Ipp32u secp256r1_b[];
extern const Ipp32u secp256r1_gx[];
extern const Ipp32u secp256r1_gy[];
extern const Ipp32u secp256r1_r[];
extern       Ipp32u secp256r1_h;

extern const Ipp32u secp384r1_p[]; // 2^384 -2^128 -2^96 +2^32 -1
extern const Ipp32u secp384r1_a[];
extern const Ipp32u secp384r1_b[];
extern const Ipp32u secp384r1_gx[];
extern const Ipp32u secp384r1_gy[];
extern const Ipp32u secp384r1_r[];
extern       Ipp32u secp384r1_h;

extern const Ipp32u secp521r1_p[]; // 2^521 -1
extern const Ipp32u secp521r1_a[];
extern const Ipp32u secp521r1_b[];
extern const Ipp32u secp521r1_gx[];
extern const Ipp32u secp521r1_gy[];
extern const Ipp32u secp521r1_r[];
extern       Ipp32u secp521r1_h;

#endif /* _PCP_ECCP_H */

#endif /* _IPP_v50_ */
