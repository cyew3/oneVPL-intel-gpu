/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Ciper-based Message Authentication Code (CMAC) see SP800-38B
//    Internal Definitions and Internal Functions Prototypes
//
//    Created: Mon 03-Jun-2002 20:15
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_CMAC_H)
#define _PCP_CMAC_H

#include "pcprij.h"


/*
// Rijndael128 based CMAC context
*/
struct _cpCMACRijndael128 {
   IppCtxId idCtx;              /* CMAC  identifier              */
   int      index;              /* internal buffer entry (free)  */
   int      dummy[2];           /* align-16                      */
   Ipp8u    k1[MBS_RIJ128];     /* k1 subkey                     */
   Ipp8u    k2[MBS_RIJ128];     /* k2 subkey                     */
   Ipp8u    mBuffer[MBS_RIJ128];/* buffer                        */
   Ipp8u    mMAC[MBS_RIJ128];   /* intermediate digest           */
   IppsRijndael128Spec mCipherCtx;
};

/* alignment */
#define CMACRIJ_ALIGNMENT (RIJ_ALIGNMENT)


/*
// Useful macros
*/
#define CMAC_ID(stt)      ((stt)->idCtx)
#define CMAC_INDX(stt)    ((stt)->index)
#define CMAC_K1(stt)      ((stt)->k1)
#define CMAC_K2(stt)      ((stt)->k2)
#define CMAC_BUFF(stt)    ((stt)->mBuffer)
#define CMAC_MAC(stt)     ((stt)->mMAC)
#define CMAC_CIPHER(stt)  ((stt)->mCipherCtx)
#define CMAC_CIPHER2(stt) ((stt)->mCipherCtx2)
#define CMAC_CIPHER3(stt) ((stt)->mCipherCtx3)

#endif /* _PCP_CMAC_H */
