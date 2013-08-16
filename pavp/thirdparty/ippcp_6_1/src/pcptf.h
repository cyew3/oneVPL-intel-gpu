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
//    Internal Definitions and
//    Internal Twofish based Encrypt/Decrypt Function Prototypes
//
//    Created: Mon 19-Aug-2002 16:43
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_TF_H)
#define _PCP_TF_H


/*
// Main Twofish paramentrs
*/
#define MBS_TF          (16)  /* data block size (bytes) */
#define MIN_TF_KEY_LEN  (16)  /* Min length (bytes) of Twofish key */
#define MAX_TF_KEY_LEN  (32)  /* Max length (bytes) of Twofish key */
#define TF_NR           (16)  /* Number of rounds */

/* subkey array indices */
#define INP_WHITEN      (0)
#define OUT_WHITEN      (INP_WHITEN + MBS_TF/4)    /* 4 for input  whitening */
#define ROUND_SUBKEYS   (OUT_WHITEN + MBS_TF/4)    /* 4 for output whitening */
#define TOTAL_SUBKEYS   (ROUND_SUBKEYS + 2*TF_NR)  /* total 40 */

#define TF_KEYS_TOTAL   (40)


/*
// Make WORD using 4 arbitrary bytes
*/
#define BYTES_TO_WORD(b0,b1,b2,b3) ( ( ((Ipp32u)((Ipp8u)(b3))) <<24 ) \
                                    |( ((Ipp32u)((Ipp8u)(b2))) <<16 ) \
                                    |( ((Ipp32u)((Ipp8u)(b1))) << 8 ) \
                                    |( ((Ipp32u)((Ipp8u)(b0))) ) )

/*
// Extract byte from specified position n.
// Sure, n=0,1,2 or 3 only
*/
#define EBYTE(w,n) ((Ipp8u)((w) >> (8 * (n))))


/*
// Twofish spec structure
*/
struct _cpTwofish {
   IppCtxId idCtx;               /* Twofish spec identifier         */
   Ipp32u   Rkey[TOTAL_SUBKEYS]; /* 40 round keys                   */
   Ipp32u   Sbox[4][256];        /* 4 S-boxes                       */
};

/* alignment */
#define TF_ALIGNMENT ((int)(sizeof(Ipp32u)))

/*
// Useful macros
*/
#define TF_ID(ctx)      ((ctx)->idCtx)
#define TF_ID_TEST(ctx) (TF_ID((ctx))==idCtxTwofish)
#define TF_KEYS(ctx)    ((ctx)->Rkey)
#define TF_SBOX(ctx)    ((ctx)->Sbox)

/*
// Twofish's tables
*/
extern Ipp8u   QT[2][256];    /* q0 and q1 permutations */
extern Ipp32u MDS[4][256];    /* MDS table */

/*
// internal functions
*/
void Encrypt_TF(const Ipp32u* pInpBlk, Ipp32u* pOutBlk, const Ipp32u* pKeys, const Ipp32u pSbox[][256]);
void Decrypt_TF(const Ipp32u* pInpBlk, Ipp32u* pOutBlk, const Ipp32u* pKeys, const Ipp32u pSbox[][256]);

#endif /* _PCP_TF_H */
