/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 Intel Corporation. All Rights Reserved.
//
// Purpose:
//    Intel(R) Integrated Performance Primitives
//    Cryptographic Primitives
//    Internal Tate Pairing Definitions & Function Prototypes
//
*/
#if !defined(__PCP_TATE_H__)
#define __PCP_TATE_H__


#include "pcpgfp.h"

/* Tate Pairing context */
struct _cpPairing {
   IppCtxId    idCtx;         /* GFp spec ident    */
   IppsGFPECState*   pECP;    /* EC over prime field */
   IppsGFPXECState*  pECF;    /* EC over prime field extension */
   IppsGFPXQState*   pPXQX;   /* quadratic extension of prime field extension */

   Ipp32u        expLen;
   cpFFchunk*    pExponent;   /* the value (q^2 - q + 1)/order */
   cpFFchunk*    pAlphaq;     /* representation of alpha^(q*i) */
};

#define PAIRING_ID(pCtx)      ((pCtx)->idCtx)
#define PAIRING_G1(pCtx)      ((pCtx)->pECP)
#define PAIRING_G2(pCtx)      ((pCtx)->pECF)
#define PAIRING_GT(pCtx)      ((pCtx)->pPXQX)
#define PAIRING_EXPLEN(pCtx)  ((pCtx)->expLen)
#define PAIRING_EXP(pCtx)     ((pCtx)->pExponent)
#define PAIRING_ALPHATO(pCtx) ((pCtx)->pAlphaq)

#define PAIRING_TEST_ID(pCtx) (PAIRING_ID((pCtx))==idCtxPairing)

void cpTatePairing(const cpFFchunk* pPoint,
                   const cpFFchunk* pQx, const cpFFchunk* pQy,
                         cpFFchunk* pR,
                   IppsTatePairingDE3State* pCtx);

#endif /* __PCP_TATE_H__ */

