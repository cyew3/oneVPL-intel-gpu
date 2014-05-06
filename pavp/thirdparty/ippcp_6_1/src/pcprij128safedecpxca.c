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
//    Decrypt 128-bit data block according to Rijndael
//    (It's the special free from Sbox/tables implementation)
//
// Contents:
//    SafeDecrypt_RIJ128()
//
//
//    Created: Mon 08-Oct-2007 17:23
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
void InvSubByte(Ipp8u inp_out[])
{
   Ipp8u AffineMatrix[] = {0x50,0x36,0x15,0x82,0x01,0x34,0x40,0x3E};
   Ipp8u AffineCnt = 0x48;
   int n;
   for(n=0; n<16; n++) {
      Ipp8u x = inp_out[n];
      x = TransformByte(x, AffineMatrix);
      x^= AffineCnt;
      x = InverseComposite(x);
      inp_out[n] = x;
   }
}

static
void InvShiftRows(Ipp8u inp_out[])
{
   int ShiftRowsInx[] = {0,13,10,7,4,1,14,11,8,5,2,15,12,9,6,3};
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
void InvMixColumn(Ipp8u inp_out[])
{
   Ipp8u GF16mul_4_2x[] = {0x00,0x24,0x48,0x6C,0x83,0xA7,0xCB,0xEF,
                           0x36,0x12,0x7E,0x5A,0xB5,0x91,0xFD,0xD9};
   Ipp8u GF16mul_1_6x[] = {0x00,0x61,0xC2,0xA3,0xB4,0xD5,0x76,0x17,
                           0x58,0x39,0x9A,0xFB,0xEC,0x8D,0x2E,0x4F};

   Ipp8u GF16mul_C_6x[] = {0x00,0x6C,0xCB,0xA7,0xB5,0xD9,0x7E,0x12,
                           0x5A,0x36,0x91,0xFD,0xEF,0x83,0x24,0x48};
   Ipp8u GF16mul_3_Ax[] = {0x00,0xA3,0x76,0xD5,0xEC,0x4F,0x9A,0x39,
                           0xFB,0x58,0x8D,0x2E,0x17,0xB4,0x61,0xC2};

   Ipp8u GF16mul_B_0x[] = {0x00,0x0B,0x05,0x0E,0x0A,0x01,0x0F,0x04,
                           0x07,0x0C,0x02,0x09,0x0D,0x06,0x08,0x03};
   Ipp8u GF16mul_0_Bx[] = {0x00,0xB0,0x50,0xE0,0xA0,0x10,0xF0,0x40,
                           0x70,0xC0,0x20,0x90,0xD0,0x60,0x80,0x30};

   Ipp8u GF16mul_2_4x[] = {0x00,0x42,0x84,0xC6,0x38,0x7A,0xBC,0xFE,
                           0x63,0x21,0xE7,0xA5,0x5B,0x19,0xDF,0x9D};
   Ipp8u GF16mul_2_6x[] = {0x00,0x62,0xC4,0xA6,0xB8,0xDA,0x7C,0x1E,
                           0x53,0x31,0x97,0xF5,0xEB,0x89,0x2F,0x4D};

   Ipp8u out[16];
   Ipp32u* pInp32 = (Ipp32u*)inp_out;

   int n;

   for(n=0; n<16; n++) {
      int xL = inp_out[n] & 0xF;
      int xH = (inp_out[n]>>4) & 0xF;
      out[n] = (Ipp8u)( GF16mul_4_2x[xL] ^ GF16mul_1_6x[xH] );
   }

   pInp32[0] = ROR32(pInp32[0], 8);
   pInp32[1] = ROR32(pInp32[1], 8);
   pInp32[2] = ROR32(pInp32[2], 8);
   pInp32[3] = ROR32(pInp32[3], 8);
   for(n=0; n<16; n++) {
      int xL = inp_out[n] & 0xF;
      int xH = (inp_out[n]>>4) & 0xF;
      out[n]^= (Ipp8u)( GF16mul_C_6x[xL] ^ GF16mul_3_Ax[xH] );
   }

   pInp32[0] = ROR32(pInp32[0], 8);
   pInp32[1] = ROR32(pInp32[1], 8);
   pInp32[2] = ROR32(pInp32[2], 8);
   pInp32[3] = ROR32(pInp32[3], 8);
   for(n=0; n<16; n++) {
      int xL = inp_out[n] & 0xF;
      int xH = (inp_out[n]>>4) & 0xF;
      out[n]^= (Ipp8u)( GF16mul_B_0x[xL] ^ GF16mul_0_Bx[xH] );
   }

   pInp32[0] = ROR32(pInp32[0], 8);
   pInp32[1] = ROR32(pInp32[1], 8);
   pInp32[2] = ROR32(pInp32[2], 8);
   pInp32[3] = ROR32(pInp32[3], 8);
   for(n=0; n<16; n++) {
      int xL = inp_out[n] & 0xF;
      int xH = (inp_out[n]>>4) & 0xF;
      out[n]^= (Ipp8u)( GF16mul_2_4x[xL] ^ GF16mul_2_6x[xH] );
   }

   for(n=0; n<16; n++)
      inp_out[n] = out[n];
}

/* define number of column in the state */
#define SC           NB(128)
#define STATE_SIZE   (sizeof(Ipp32u)*SC)

void SafeDecrypt_RIJ128(const Ipp32u* pInpBlk,
                              Ipp32u* pOutBlk,
                              int nr,
                        const Ipp32u* pKeys)
{
   int r;

   Ipp8u* roundKeys = ((Ipp8u*)pKeys)+nr*STATE_SIZE;

   Ipp8u state[STATE_SIZE]; /* state */

   /* native => composite */
   TransformNative2Composite(state, (Ipp8u*)pInpBlk, STATE_SIZE);

   /* input whitening */
   AddRoundKey(state, state, roundKeys);
   roundKeys -= STATE_SIZE;

   /* regular (nr-1) rounds */
   for(r=1; r<nr; r++) {
      InvSubByte(state);
      InvShiftRows(state);
      InvMixColumn(state);
      AddRoundKey(state, state, roundKeys);
      roundKeys -= STATE_SIZE;
   }

   /* irregular round */
   InvSubByte(state);
   InvShiftRows(state);
   AddRoundKey(state, state, roundKeys);

   /* composite => native */
   TransformComposite2Native((Ipp8u*)pOutBlk, state, STATE_SIZE);
}

#endif
#endif

