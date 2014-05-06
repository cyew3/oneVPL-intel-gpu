/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Security Hash Standard
//    Internal Definitions and Internal Functions Prototypes
//
//    Created: Thu 07-Mar-2002 18:26
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_SHS_H)
#define _PCP_SHS_H


/*
// SHS definitions
*/
#define MBS_SHA1   (64)    /* SHA1   mesage block size (byes) */
#define MBS_SHA256 (64)    /* SHA256 mesage block size (byes) */
#define MBS_SHA224 (64)    /* SHA224 mesage block size (byes) */
#define MBS_SHA512 (128)   /* SHA512 mesage block size (byes) */
#define MBS_SHA384 (128)   /* SHA384 mesage block size (byes) */
#define MBS_MD5    (64)    /* MD5    mesage block size (byes) */

typedef Ipp32u DigestSHA1[5];   /* SHA1 digest   */
typedef Ipp32u DigestSHA224[7]; /* SHA224 digest */
typedef Ipp32u DigestSHA256[8]; /* SHA256 digest */
typedef Ipp64u DigestSHA384[6]; /* SHA384 digest */
typedef Ipp64u DigestSHA512[8]; /* SHA512 digest */
typedef Ipp32u DigestMD5[4];    /* MD5 digest */


struct _cpSHA1 {
   IppCtxId    idCtx;   /* SHA1 identifier               */
   int         index;   /* internal buffer entry (free)  */
   Ipp64u      mlenLo;  /* message length (bits)         */
   Ipp8u       mBuffer[MBS_SHA1]; /* buffer              */
   DigestSHA1  mDigest; /* intermediate digest           */
};

struct _cpSHA224 {
   IppCtxId     idCtx;   /* SHA224 identifier            */
   int          index;   /* internal buffer entry (free) */
   Ipp64u       mlenLo;  /* message length (bits)        */
   Ipp8u        mBuffer[MBS_SHA224]; /* buffer           */
   DigestSHA256 mDigest; /* intermediate digest          */
};

struct _cpSHA256 {
   IppCtxId     idCtx;   /* SHA256 identifier            */
   int          index;   /* internal buffer entry (free) */
   Ipp64u       mlenLo;  /* message length (bits)        */
   Ipp8u        mBuffer[MBS_SHA256]; /* buffer           */
   DigestSHA256 mDigest; /* intermediate digest          */
};

struct _cpSHA384 {
   IppCtxId     idCtx;   /* SHA384 identifier            */
   int          index;   /* internal buffer entry (free) */
   Ipp64u       mlenLo;  /* message length (bits)        */
   Ipp64u       mlenHi;  /* message length (bits)        */
   Ipp64u       dummy;   /* for alignment puprose only   */
   Ipp8u        mBuffer[MBS_SHA384]; /* buffer           */
   DigestSHA512 mDigest; /* intermediate digest          */
};

struct _cpSHA512 {
   IppCtxId     idCtx;   /* SHA512 identifier            */
   int          index;   /* internal buffer entry (free) */
   Ipp64u       mlenLo;  /* message length (bits)        */
   Ipp64u       mlenHi;  /* message length (bits)        */
   Ipp64u       dummy;   /* for alignment puprose only   */
   Ipp8u        mBuffer[MBS_SHA512]; /* buffer           */
   DigestSHA512 mDigest; /* intermediate digest          */
};

struct _cpMD5 {
   IppCtxId    idCtx;   /* MD5 identifier                 */
   int         index;   /* internal buffer entry (free)   */
   Ipp64u      mlenLo;  /* message length (bits)          */
   Ipp8u       mBuffer[MBS_MD5]; /* buffer                */
   DigestMD5   mDigest; /* intermediate digest            */
};

/*
// alignments
*/
#define   SHA1_ALIGNMENT   ((int)(sizeof(Ipp64u)  ))
#define SHA224_ALIGNMENT   ((int)(sizeof(Ipp64u)  ))
#define SHA256_ALIGNMENT   ((int)(sizeof(Ipp64u)  ))
#define SHA384_ALIGNMENT   ((int)(sizeof(Ipp64u)*2))
#define SHA512_ALIGNMENT   ((int)(sizeof(Ipp64u)*2))
#define    MD5_ALIGNMENT   ((int)(sizeof(Ipp64u)  ))


/*
// Useful macros
*/
#define SHS_ID(stt)     ((stt)->idCtx)
#define SHS_INDX(stt)   ((stt)->index)
#define SHS_LENL(stt)   ((stt)->mlenLo)
#define SHS_LENH(stt)   ((stt)->mlenHi)
#define SHS_BUFF(stt)   ((stt)->mBuffer)
#define SHS_DGST(stt)   ((stt)->mDigest)

/*
// SHAx specific constants
*/
#define FIPS_180_K_SHA1    { \
   0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6}

#define FIPS_180_K_SHA256  { \
   0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, \
   0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5, \
   0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, \
   0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174, \
   0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, \
   0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA, \
   0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, \
   0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967, \
   0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, \
   0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85, \
   0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, \
   0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070, \
   0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, \
   0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3, \
   0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, \
   0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2}

#define FIPS_180_K_SHA512  { \
   CONST_64(0x428A2F98D728AE22), CONST_64(0x7137449123EF65CD), CONST_64(0xB5C0FBCFEC4D3B2F), CONST_64(0xE9B5DBA58189DBBC), \
   CONST_64(0x3956C25BF348B538), CONST_64(0x59F111F1B605D019), CONST_64(0x923F82A4AF194F9B), CONST_64(0xAB1C5ED5DA6D8118), \
   CONST_64(0xD807AA98A3030242), CONST_64(0x12835B0145706FBE), CONST_64(0x243185BE4EE4B28C), CONST_64(0x550C7DC3D5FFB4E2), \
   CONST_64(0x72BE5D74F27B896F), CONST_64(0x80DEB1FE3B1696B1), CONST_64(0x9BDC06A725C71235), CONST_64(0xC19BF174CF692694), \
   CONST_64(0xE49B69C19EF14AD2), CONST_64(0xEFBE4786384F25E3), CONST_64(0x0FC19DC68B8CD5B5), CONST_64(0x240CA1CC77AC9C65), \
   CONST_64(0x2DE92C6F592B0275), CONST_64(0x4A7484AA6EA6E483), CONST_64(0x5CB0A9DCBD41FBD4), CONST_64(0x76F988DA831153B5), \
   CONST_64(0x983E5152EE66DFAB), CONST_64(0xA831C66D2DB43210), CONST_64(0xB00327C898FB213F), CONST_64(0xBF597FC7BEEF0EE4), \
   CONST_64(0xC6E00BF33DA88FC2), CONST_64(0xD5A79147930AA725), CONST_64(0x06CA6351E003826F), CONST_64(0x142929670A0E6E70), \
   CONST_64(0x27B70A8546D22FFC), CONST_64(0x2E1B21385C26C926), CONST_64(0x4D2C6DFC5AC42AED), CONST_64(0x53380D139D95B3DF), \
   CONST_64(0x650A73548BAF63DE), CONST_64(0x766A0ABB3C77B2A8), CONST_64(0x81C2C92E47EDAEE6), CONST_64(0x92722C851482353B), \
   CONST_64(0xA2BFE8A14CF10364), CONST_64(0xA81A664BBC423001), CONST_64(0xC24B8B70D0F89791), CONST_64(0xC76C51A30654BE30), \
   CONST_64(0xD192E819D6EF5218), CONST_64(0xD69906245565A910), CONST_64(0xF40E35855771202A), CONST_64(0x106AA07032BBD1B8), \
   CONST_64(0x19A4C116B8D2D0C8), CONST_64(0x1E376C085141AB53), CONST_64(0x2748774CDF8EEB99), CONST_64(0x34B0BCB5E19B48A8), \
   CONST_64(0x391C0CB3C5C95A63), CONST_64(0x4ED8AA4AE3418ACB), CONST_64(0x5B9CCA4F7763E373), CONST_64(0x682E6FF3D6B2B8A3), \
   CONST_64(0x748F82EE5DEFB2FC), CONST_64(0x78A5636F43172F60), CONST_64(0x84C87814A1F0AB72), CONST_64(0x8CC702081A6439EC), \
   CONST_64(0x90BEFFFA23631E28), CONST_64(0xA4506CEBDE82BDE9), CONST_64(0xBEF9A3F7B2C67915), CONST_64(0xC67178F2E372532B), \
   CONST_64(0xCA273ECEEA26619C), CONST_64(0xD186B8C721C0C207), CONST_64(0xEADA7DD6CDE0EB1E), CONST_64(0xF57D4F7FEE6ED178), \
   CONST_64(0x06F067AA72176FBA), CONST_64(0x0A637DC5A2C898A6), CONST_64(0x113F9804BEF90DAE), CONST_64(0x1B710B35131C471B), \
   CONST_64(0x28DB77F523047D84), CONST_64(0x32CAAB7B40C72493), CONST_64(0x3C9EBE0A15C9BEBC), CONST_64(0x431D67C49C100D4C), \
   CONST_64(0x4CC5D4BECB3E42B6), CONST_64(0x597F299CFC657E2A), CONST_64(0x5FCB6FAB3AD6FAEC), CONST_64(0x6C44198C4A475817)}

#if defined(_IPP_TRS)
#else
   extern Ipp32u K_SHA1[];
   extern Ipp32u K_SHA256[];
   extern Ipp64u K_SHA512[];
#endif
extern Ipp32u K_MD5[];

#define FIPS_180_SHA1_HASH0   {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}

/*
// internal functions
*/
void InitSHA1   (IppsSHA1State* pState);
void UpdateSHA1 (const Ipp8u* mblk, DigestSHA1 digest);
void ProcessSHA1(Ipp8u* buffer, DigestSHA1 digest, const Ipp8u* pSrc, int len);
void ComputeDigestSHA1(Ipp8u* pMD, const IppsSHA1State* pState);
__INLINE void CopyDigestSHA1(const DigestSHA1 src, DigestSHA1 dst)
{
   dst[0] = src[0];
   dst[1] = src[1];
   dst[2] = src[2];
   dst[3] = src[3];
   dst[4] = src[4];
}

void InitSHA224   (IppsSHA224State* pState);

void InitSHA256   (IppsSHA256State* pState);
void UpdateSHA256 (const Ipp8u* mblk, DigestSHA256 digest);
void ProcessSHA256(Ipp8u* buffer, DigestSHA256 digest, const Ipp8u* pSrc, int len);
void ComputeDigestSHA256(Ipp8u* pMD, const IppsSHA256State* pState);

void InitSHA384   (IppsSHA384State* pState);

void InitSHA512   (IppsSHA512State* pState);
void UpdateSHA512 (const Ipp8u* mblk, DigestSHA512 digest);
void ProcessSHA512(Ipp8u* buffer, DigestSHA512 digest, const Ipp8u* pSrc, int len);
void ComputeDigestSHA512(Ipp8u* pMD, const IppsSHA512State* pState);

void InitMD5    (IppsMD5State* pState);
void UpdateMD5  (const Ipp8u* mblk, DigestMD5 digest);
void ProcessMD5(Ipp8u* buffer, DigestMD5 digest, const Ipp8u* pSrc, int len);
void ComputeDigestMD5(Ipp8u* pMD, const IppsMD5State* pState);

#endif /* _PCP_SHS_H */
