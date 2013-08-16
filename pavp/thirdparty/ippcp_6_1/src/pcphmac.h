/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Hash Message Authentication Code
//    Internal Definitions and Internal Functions Prototypes
//
//    Created: Mon 03-Jun-2002 20:15
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_HMAC_H)
#define _PCP_HMAC_H

#include "pcpshs.h"


/*
// SHA1 based HMAC Declaration
*/
struct _cpHMACSHA1 {
   IppCtxId    idCtx;               /* HMAC identifier         */
   IppsSHA1State*   pShsState;      /* pointer to SHS state    */
   DigestSHA1  ipadKeyHash;         /* inner padding key Hash  */
   DigestSHA1  opadKeyHash;         /* outer padding key Hash  */
   Ipp8u       shsState[sizeof(IppsSHA1State)+SHA1_ALIGNMENT-1]; /* SHS state */
};

/*
// SHA256 based HMAC Declaration
*/
struct _cpHMACSHA256 {
   IppCtxId    idCtx;               /* HMAC identifier      */
   IppsSHA256State* pShsState;      /* pointer to SHS state */
   Ipp8u       ipadKey[MBS_SHA256]; /* inned padding        */
   Ipp8u       opadKey[MBS_SHA256]; /* outer padding        */
   Ipp8u       shsState[sizeof(IppsSHA256State)+SHA256_ALIGNMENT-1]; /* SHS state */
};

/*
// SHA224 based HMAC Declaration
*/
struct _cpHMACSHA224 {
   IppCtxId    idCtx;               /* HMAC identifier      */
   IppsSHA224State* pShsState;      /* pointer to SHS state */
   Ipp8u       ipadKey[MBS_SHA224]; /* inned padding        */
   Ipp8u       opadKey[MBS_SHA224]; /* outer padding        */
   Ipp8u       shsState[sizeof(IppsSHA224State)+SHA224_ALIGNMENT-1]; /* SHS state */
};

/*
// SHA384 based HMAC Declaration
*/
struct _cpHMACSHA384 {
   IppCtxId    idCtx;               /* HMAC identifier      */
   IppsSHA384State* pShsState;      /* pointer to SHS state */
   Ipp8u       ipadKey[MBS_SHA384]; /* inned padding        */
   Ipp8u       opadKey[MBS_SHA384]; /* outer padding        */
   Ipp8u       shsState[sizeof(IppsSHA384State)+SHA384_ALIGNMENT-1]; /* SHS state */
};

/*
// SHA512 based HMAC Declaration
*/
struct _cpHMACSHA512 {
   IppCtxId    idCtx;               /* HMAC identifier      */
   IppsSHA512State* pShsState;      /* pointer to SHS state */
   Ipp8u       ipadKey[MBS_SHA512]; /* inned padding        */
   Ipp8u       opadKey[MBS_SHA512]; /* outer padding        */
   Ipp8u       shsState[sizeof(IppsSHA512State)+SHA512_ALIGNMENT-1]; /* SHS state */
};

/*
// MD5 based HMAC Declaration
*/
struct _cpHMACMD5 {
   IppCtxId    idCtx;               /* HMAC identifier      */
   IppsMD5State*    pShsState;      /* pointer to SHS state */
   Ipp8u       ipadKey[MBS_MD5];    /* inned padding        */
   Ipp8u       opadKey[MBS_MD5];    /* outer padding        */
   Ipp8u       shsState[sizeof(IppsMD5State)+MD5_ALIGNMENT-1]; /* SHS state */
};

/*
// alignments
*/
#define   HMACSHA1_ALIGNMENT  ((int)(sizeof(void*)))
#define HMACSHA256_ALIGNMENT  ((int)(sizeof(void*)))
#define HMACSHA224_ALIGNMENT  ((int)(sizeof(void*)))
#define HMACSHA384_ALIGNMENT  ((int)(sizeof(void*)))
#define HMACSHA512_ALIGNMENT  ((int)(sizeof(void*)))
#define    HMACMD5_ALIGNMENT  ((int)(sizeof(void*)))


/*
// Useful macros
*/
#define IPAD            (0x36)   /* inner padding value */
#define OPAD            (0x5C)   /* outer padding value */
#define HMAC_ID(stt)    ((stt)->idCtx)
#define HASH_STATE(stt) ((stt)->pShsState)

#endif /* _PCP_HMAC_H */
