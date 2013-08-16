/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//   Copyright (c) 2007-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Encrypt 128-bit data block according to Rijndael
//    (It's the special free from Sbox/tables implementation)
//
// Contents:
//    SafeEncrypt_RIJ128()
//
//
//    Created: Mon 08-Oct-2007 21:35
//  Author(s): Sergey Kirillov
*/
#if !defined(_IPP_TRS)

#include "precomp.h"
#include "owncp.h"
#include "pcprij.h"
#include "pcprij128safe.h"


#if !((_IPP ==_IPP_V8) || (_IPP==_IPP_P8) || (_IPP==_IPP_G9) || \
      (_IPPLP32 ==_IPPLP32_S8) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) )

static
void FwdSubByte(Ipp8u inp_out[])
{
   Ipp8u AffineMatrix[] = {0x10,0x22,0x55,0x82,0x41,0x34,0x40,0x2A};
   Ipp8u AffineCnt = 0xC2;
   int n;
   for(n=0; n<16; n++) {
      Ipp8u x = inp_out[n];
      x = InverseComposite(x);
      x = TransformByte(x, AffineMatrix);
      x^= AffineCnt;
      inp_out[n] = x;
   }
}

static
void FwdShiftRows(Ipp8u inp_out[])
{
   int ShiftRowsInx[] = {0,5,10,15,4,9,14,3,8,13,2,7,12,1,6,11};
   Ipp8u tmp[16];

   int n;
   for(n=0; n<16; n++)
      tmp[n] = inp_out[n];

   for(n=0; n<16; n++) {
      int idx = ShiftRowsInx[n];
      inp_out[n] = tmp[idx];
   }
}

static
void FwdMixColumn(Ipp8u inp_out[])
{
   Ipp8u GF16mul_E_2x[] = {0x00,0x2E,0x4F,0x61,0x8D,0xA3,0xC2,0xEC,
                           0x39,0x17,0x76,0x58,0xB4,0x9A,0xFB,0xD5};
   Ipp8u GF16mul_1_Cx[] = {0x00,0xC1,0xB2,0x73,0x54,0x95,0xE6,0x27,
                           0xA8,0x69,0x1A,0xDB,0xFC,0x3D,0x4E,0x8F};

   Ipp8u tmp[16];
   Ipp32u* pTmp32 = (Ipp32u*)tmp;
   Ipp32u* pInp32 = (Ipp32u*)inp_out;

   int n;
   for(n=0; n<16; n++) {
      Ipp8u xL = (Ipp8u)( inp_out[n] & 0xF );
      Ipp8u xH = (Ipp8u)( (inp_out[n]>>4) & 0xF );
      tmp[n] = (Ipp8u)( GF16mul_E_2x[xL] ^ GF16mul_1_Cx[xH] );
   }

   pTmp32[0] ^= ROR32(pTmp32[0], 8);
   pTmp32[1] ^= ROR32(pTmp32[1], 8);
   pTmp32[2] ^= ROR32(pTmp32[2], 8);
   pTmp32[3] ^= ROR32(pTmp32[3], 8);

   pInp32[0] = ROR32(pInp32[0], 8);
   pInp32[1] = ROR32(pInp32[1], 8);
   pInp32[2] = ROR32(pInp32[2], 8);
   pInp32[3] = ROR32(pInp32[3], 8);
   pTmp32[0] ^= pInp32[0];
   pTmp32[1] ^= pInp32[1];
   pTmp32[2] ^= pInp32[2];
   pTmp32[3] ^= pInp32[3];

   pInp32[0] = ROR32(pInp32[0], 8);
   pInp32[1] = ROR32(pInp32[1], 8);
   pInp32[2] = ROR32(pInp32[2], 8);
   pInp32[3] = ROR32(pInp32[3], 8);
   pTmp32[0] ^= pInp32[0];
   pTmp32[1] ^= pInp32[1];
   pTmp32[2] ^= pInp32[2];
   pTmp32[3] ^= pInp32[3];

   pInp32[0] = ROR32(pInp32[0], 8);
   pInp32[1] = ROR32(pInp32[1], 8);
   pInp32[2] = ROR32(pInp32[2], 8);
   pInp32[3] = ROR32(pInp32[3], 8);
   pInp32[0]^= pTmp32[0];
   pInp32[1]^= pTmp32[1];
   pInp32[2]^= pTmp32[2];
   pInp32[3]^= pTmp32[3];
}

/* define number of column in the state */
#define SC           NB(128)
#define STATE_SIZE   (sizeof(Ipp32u)*SC)

void SafeEncrypt_RIJ128(const Ipp32u* pInpBlk,
                              Ipp32u* pOutBlk,
                              int nr,
                        const Ipp32u* pKeys)
{
   int r;

   Ipp8u* roundKeys = (Ipp8u*)pKeys;

   Ipp8u state[STATE_SIZE]; /* state */

   /* native => composite */
   TransformNative2Composite(state, (Ipp8u*)pInpBlk, STATE_SIZE);

   /* input whitening */
   AddRoundKey(state, state, roundKeys);
   roundKeys += STATE_SIZE;

   /* regular (nr-1) rounds */
   for(r=1; r<nr; r++) {
      FwdSubByte(state);
      FwdShiftRows(state);
      FwdMixColumn(state);
      AddRoundKey(state, state, roundKeys);
      roundKeys += STATE_SIZE;
   }

   /* irregular round */
   FwdSubByte(state);
   FwdShiftRows(state);
   AddRoundKey(state, state, roundKeys);

   /* composite => native */
   TransformComposite2Native((Ipp8u*)pOutBlk, state, STATE_SIZE);
}

#endif
#endif
