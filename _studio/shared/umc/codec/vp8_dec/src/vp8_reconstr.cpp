//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "umc_vp8_decoder.h"

using namespace UMC;

namespace UMC
{

static const Ipp16s vp8_6tap_filters[8][6] =
{
  { 0,  0,  128,    0,   0,  0 },
  { 0, -6,  123,   12,  -1,  0 },
  { 2, -11, 108,   36,  -8,  1 },
  { 0, -9,   93,   50,  -6,  0 },
  { 3, -16,  77,   77, -16,  3 },
  { 0, -6,   50,   93,  -9,  0 },
  { 1, -8,   36,  108, -11,  2 },
  { 0, -1,   12,  123,  -6,  0 },
};


static const Ipp8u vp8_bilinear_filters[8][2] =
{
  { 128,   0 },
  { 112,  16 },
  {  96,  32 },
  {  80,  48 },
  {  64,  64 },
  {  48,  80 },
  {  32,  96 },
  {  16, 112 }
};


#define IDCT_20091 20091
#define IDCT_35468 35468

static void vp8_idct_4x4(Ipp16s* coefs, Ipp16s* data, Ipp32u step)
{
  Ipp32s  i   = 0;
  Ipp16s* src = coefs;
  Ipp16s* dst = data;
  //Ipp8u*    dst = data;

  Ipp16s temp[16];

  Ipp32s  a1;
  Ipp32s  a2;
  Ipp32s  a3;
  Ipp32s  a4;

  for(i = 0 ; i < 4; i++)
  {
    a1 = src[i] + src[8+i];
    a2 = src[i] - src[8+i];

    a3 = ((IDCT_35468*src[4+i])>>16)            - (((IDCT_20091*src[12+i])>>16) + src[12+i]);
    a4 = (src[4+i] + ((IDCT_20091*src[4+i])>>16)) + ((src[12+i]*IDCT_35468)>>16);

    //dst[i + 0*step]        = a1 + a4;
    //dst[i + /*4*/1*step]   = a2 + a3;
    //dst[i + /*8*/ 2*step]  = a2 - a3;
    //dst[i + /*12*/3*step]  = a1 - a4;
    temp[i + 0]  = (Ipp16s)(a1 + a4);
    temp[i + 4]  = (Ipp16s)(a2 + a3);
    temp[i + 8]  = (Ipp16s)(a2 - a3);
    temp[i + 12] = (Ipp16s)(a1 - a4);
  }

  for(i = 0; i < 4; i++)
  {
    a1 = temp[i*4 + 0] + temp[i*4 + 2];
    a2 = temp[i*4 + 0] - temp[i*4 + 2];

    a3 = ((temp[i*4+1]*IDCT_35468)>>16)                 - (temp[i*4+3] + ((temp[i*4+3]*IDCT_20091)>>16));
    a4 = (temp[i*4+1] + ((temp[i*4+1]*IDCT_20091)>>16)) + ((temp[i*4+3]*IDCT_35468)>>16);

    dst[0 + i*step] = (Ipp16s)((a1 + a4 + 4) >>3);
    dst[1 + i*step] = (Ipp16s)((a2 + a3 + 4) >>3);
    dst[2 + i*step] = (Ipp16s)((a2 - a3 + 4) >>3);
    dst[3 + i*step] = (Ipp16s)((a1 - a4 + 4) >>3);
  }

  return;
} // vp8_idct_4x4()


// then only dc coefs is not equal to zero
static void vp8_idct_1x1(Ipp16s* coefs, Ipp16s* data/*Ipp8u* data*/, Ipp32u step)
{
  Ipp32s i;
  Ipp16s a1;

  a1 = (coefs[0] + 4) >> 3;

  for(i = 0; i < 4; i++)
  {
    data[0 + i*step] = a1;
    data[1 + i*step] = a1;
    data[2 + i*step] = a1;
    data[3 + i*step] = a1;
  }

  return;
} // vp8_idct_1x1()


static void vp8_iwht_4x4(Ipp16s* dcCoefs, Ipp16s* coefs)
{
  Ipp32s i = 0;
  Ipp32s a1, b1, c1, d1;

  Ipp16s* ptr = dcCoefs;

  for(i = 0; i < 4; i++)
  {
    a1 = ptr[i+0] + ptr[i+12];
    b1 = ptr[i+4] + ptr[i+8];

    c1 = ptr[i+4] - ptr[i+8];
    d1 = ptr[i+0] - ptr[i+12];

    ptr[i+0]  = (Ipp16s)(a1 + b1);
    ptr[i+4]  = (Ipp16s)(c1 + d1);
    ptr[i+8]  = (Ipp16s)(a1 - b1);
    ptr[i+12] = (Ipp16s)(d1 - c1);
  }

  for(i = 0; i < 4; i++)
  {
    a1 = ptr[i*4+0] + ptr[i*4+3];
    b1 = ptr[i*4+1] + ptr[i*4+2];

    c1 = ptr[i*4+1] - ptr[i*4+2];
    d1 = ptr[i*4+0] - ptr[i*4+3];


    coefs[i*4*16 + 0]    = (Ipp16s)((a1 + b1 + 3) >> 3);
    coefs[i*4*16 + 1*16] = (Ipp16s)((c1 + d1 + 3) >> 3);
    coefs[i*4*16 + 2*16] = (Ipp16s)((a1 - b1 + 3) >> 3);
    coefs[i*4*16 + 3*16] = (Ipp16s)((d1 - c1 + 3) >> 3);
  }

  return;
} // vp8_iwht_4x4()


// than only dc coefs is not equal to zero
static void vp8_iwht_1x1(Ipp16s* dcCoefs, Ipp16s* coefs/*, Ipp32u dataStep*/)
{
  Ipp32s i;
  Ipp16s a1;

  a1 = (dcCoefs[0] + 3) >> 3;

  for(i = 0; i < 4; i++)
  {
    coefs[i*4*16 + 0]    = a1;
    coefs[i*4*16 + 1*16] = a1;
    coefs[i*4*16 + 2*16] = a1;
    coefs[i*4*16 + 3*16] = a1;
  }

  return;
} // vp8_iwht_1x1()


static void vp8_intra_predict4x4_add(Ipp16s* src,  Ipp32s srcStep, Ipp8u*  dst,
                                      Ipp32u dstStep, Ipp8u bMode, Ipp8u* above,
                                      Ipp8u*  left, Ipp8u ltp)
{
  Ipp32s i = 0;
  Ipp32s j = 0;

  Ipp8u* a = above;
  Ipp8u* l = left;

  Ipp16s v1, v2, v3, v4, v5, v6 ,v7, v8, v9, v10;

  switch(bMode)
  {
  case VP8_B_DC_PRED:
    {
      Ipp32s v = 4;

      for(i = 0; i < 4; i++)
        v += l[i] + a[i];

      v = v >> 3;

      for(i = 0; i < 4; i++)
      {
        dst[0] = vp8_ClampTblLookup(v, src[0]);
        dst[1] = vp8_ClampTblLookup(v, src[1]);
        dst[2] = vp8_ClampTblLookup(v, src[2]);
        dst[3] = vp8_ClampTblLookup(v, src[3]);

        dst += dstStep;
        src += srcStep;
      }
    }
    break;

  case VP8_B_TM_PRED:
    {
      for(i = 0; i < 4; i++)
      {
        for(j = 0; j < 4; j++)
        {
          dst[i*dstStep + j] = vp8_ClampTblLookup(vp8_CLIP255(l[i] + a[j] - ltp), src[i*srcStep + j]); //??????
        }
      }
    }
    break;

  case VP8_B_VE_PRED:
    {
      v1 = (ltp  + 2*a[0] + a[1] + 2) >> 2;
      v2 = (a[0] + 2*a[1] + a[2] + 2) >> 2;
      v3 = (a[1] + 2*a[2] + a[3] + 2) >> 2;
      v4 = (a[2] + 2*a[3] + a[4] + 2) >> 2;

      for(i = 0; i < 4; i++)
        dst[0 + i*dstStep] = vp8_ClampTblLookup(v1 , src[0 + i*srcStep]);

      for(i = 0; i < 4; i++)
        dst[1 + i*dstStep] = vp8_ClampTblLookup(v2 , src[1 + i*srcStep]);

      for(i = 0; i < 4; i++)
        dst[2 + i*dstStep] = vp8_ClampTblLookup(v3 , src[2 + i*srcStep]);

      for(i = 0; i < 4; i++)
        dst[3 + i*dstStep] = vp8_ClampTblLookup(v4 , src[3 + i*srcStep]);

    }
    break;

  case VP8_B_HE_PRED:
    {
      v1 = (ltp  + 2*l[0] + l[1] + 2) >> 2;
      v2 = (l[0] + 2*l[1] + l[2] + 2) >> 2;
      v3 = (l[1] + 2*l[2] + l[3] + 2) >> 2;
      v4 = (l[2] + 2*l[3] + l[3] + 2) >> 2;

      for(i = 0; i < 4; i++)
        dst[i + 0*dstStep] = vp8_ClampTblLookup(v1 , src[i + 0*srcStep]);

      for(i = 0; i < 4; i++)
        dst[i + 1*dstStep] = vp8_ClampTblLookup(v2 , src[i + 1*srcStep]);

      for(i = 0; i < 4; i++)
        dst[i + 2*dstStep] = vp8_ClampTblLookup(v3 , src[i + 2*srcStep]);

      for(i = 0; i < 4; i++)
        dst[i + 3*dstStep] = vp8_ClampTblLookup(v4 , src[i + 3*srcStep]);

    }
    break;

  case VP8_B_LD_PRED:
    {
      v1 = (a[0] + 2*a[1] + a[2] + 2) >> 2;
      v2 = (a[1] + 2*a[2] + a[3] + 2) >> 2;
      v3 = (a[2] + 2*a[3] + a[4] + 2) >> 2;
      v4 = (a[3] + 2*a[4] + a[5] + 2) >> 2;
      v5 = (a[4] + 2*a[5] + a[6] + 2) >> 2;
      v6 = (a[5] + 2*a[6] + a[7] + 2) >> 2;
      v7 = (a[6] + 2*a[7] + a[7] + 2) >> 2;

      dst[0 + 0*dstStep] = vp8_ClampTblLookup(v1 , src[0 + 0*srcStep]);

      dst[1 + 0*dstStep] = vp8_ClampTblLookup(v2, src[1 + 0*srcStep]);
      dst[0 + 1*dstStep] = vp8_ClampTblLookup(v2, src[0 + 1*srcStep]);

      dst[2 + 0*dstStep] = vp8_ClampTblLookup(v3, src[2 + 0*srcStep]);
      dst[1 + 1*dstStep] = vp8_ClampTblLookup(v3, src[1 + 1*srcStep]);
      dst[0 + 2*dstStep] = vp8_ClampTblLookup(v3, src[0 + 2*srcStep]);

      dst[3 + 0*dstStep] = vp8_ClampTblLookup(v4, src[3 + 0*srcStep]);
      dst[2 + 1*dstStep] = vp8_ClampTblLookup(v4, src[2 + 1*srcStep]);
      dst[1 + 2*dstStep] = vp8_ClampTblLookup(v4, src[1 + 2*srcStep]);
      dst[0 + 3*dstStep] = vp8_ClampTblLookup(v4, src[0 + 3*srcStep]);

      dst[3 + 1*dstStep] = vp8_ClampTblLookup(v5, src[3 + 1*srcStep]);
      dst[2 + 2*dstStep] = vp8_ClampTblLookup(v5, src[2 + 2*srcStep]);
      dst[1 + 3*dstStep] = vp8_ClampTblLookup(v5, src[1 + 3*srcStep]);

      dst[3 + 2*dstStep] = vp8_ClampTblLookup(v6, src[3 + 2*srcStep]);
      dst[2 + 3*dstStep] = vp8_ClampTblLookup(v6, src[2 + 3*srcStep]);

      dst[3 + 3*dstStep] = vp8_ClampTblLookup(v7, src[3 + 3*srcStep]);
    }
    break;

  case VP8_B_RD_PRED:
    {
      v1 = (l[3] + 2*l[2] + l[1] + 2) >> 2;
      v2 = (l[2] + 2*l[1] + l[0] + 2) >> 2;
      v3 = (l[1] + 2*l[0] + ltp  + 2) >> 2;
      v4 = (l[0] + 2*ltp  + a[0] + 2) >> 2;
      v5 = (ltp  + 2*a[0] + a[1] + 2) >> 2;
      v6 = (a[0] + 2*a[1] + a[2] + 2) >> 2;
      v7 = (a[1] + 2*a[2] + a[3] + 2) >> 2;

      dst[0 + 0*dstStep] = vp8_ClampTblLookup(v4, src[0 + 0*srcStep]);
      dst[1 + 1*dstStep] = vp8_ClampTblLookup(v4, src[1 + 1*srcStep]);
      dst[2 + 2*dstStep] = vp8_ClampTblLookup(v4, src[2 + 2*srcStep]);
      dst[3 + 3*dstStep] = vp8_ClampTblLookup(v4, src[3 + 3*srcStep]);

      dst[1 + 0*dstStep] = vp8_ClampTblLookup(v5, src[1 + 0*srcStep]);
      dst[2 + 1*dstStep] = vp8_ClampTblLookup(v5, src[2 + 1*srcStep]);
      dst[3 + 2*dstStep] = vp8_ClampTblLookup(v5, src[3 + 2*srcStep]);

      dst[2 + 0*dstStep] = vp8_ClampTblLookup(v6, src[2 + 0*srcStep]);
      dst[3 + 1*dstStep] = vp8_ClampTblLookup(v6, src[3 + 1*srcStep]);

      dst[3 + 0*dstStep] = vp8_ClampTblLookup(v7, src[3 + 0*srcStep]);

      dst[0 + 1*dstStep] = vp8_ClampTblLookup(v3, src[0 + 1*srcStep]);
      dst[1 + 2*dstStep] = vp8_ClampTblLookup(v3, src[1 + 2*srcStep]);
      dst[2 + 3*dstStep] = vp8_ClampTblLookup(v3, src[2 + 3*srcStep]);

      dst[0 + 2*dstStep] = vp8_ClampTblLookup(v2, src[0 + 2*srcStep]);
      dst[1 + 3*dstStep] = vp8_ClampTblLookup(v2, src[1 + 3*srcStep]);

      dst[0 + 3*dstStep] = vp8_ClampTblLookup(v1, src[0 + 3*srcStep]);
    }
    break;

  case VP8_B_VR_PRED:
    {
      v1 = (ltp  + a[0] + 1) >> 1;
      v2 = (a[0] + a[1] + 1) >> 1;
      v3 = (a[1] + a[2] + 1) >> 1;
      v4 = (a[2] + a[3] + 1) >> 1;

      v5  =(l[0] + 2*ltp  + a[0] + 2) >> 2;
      v6  =(ltp  + 2*a[0] + a[1] + 2) >> 2;
      v7  =(a[0] + 2*a[1] + a[2] + 2) >> 2;
      v8  =(a[1] + 2*a[2] + a[3] + 2) >> 2;
      v9  =(ltp  + 2*l[0] + l[1] + 2) >> 2;
      v10 =(l[0] + 2*l[1] + l[2] + 2) >> 2;

      dst[0 + 0*dstStep] = vp8_ClampTblLookup(v1, src[0 + 0*srcStep]);
      dst[1 + 2*dstStep] = vp8_ClampTblLookup(v1, src[1 + 2*srcStep]);

      dst[1 + 0*dstStep] = vp8_ClampTblLookup(v2, src[1 + 0*srcStep]);
      dst[2 + 2*dstStep] = vp8_ClampTblLookup(v2, src[2 + 2*srcStep]);

      dst[2 + 0*dstStep] = vp8_ClampTblLookup(v3 ,src[2 + 0*srcStep]);
      dst[3 + 2*dstStep] = vp8_ClampTblLookup(v3, src[3 + 2*srcStep]);

      dst[3 + 0*dstStep] = vp8_ClampTblLookup(v4, src[3 + 0*srcStep]);

      dst[0 + 1*dstStep] = vp8_ClampTblLookup(v5, src[0 + 1*srcStep]);
      dst[1 + 3*dstStep] = vp8_ClampTblLookup(v5, src[1 + 3*srcStep]);

      dst[1 + 1*dstStep] = vp8_ClampTblLookup(v6, src[1 + 1*srcStep]);
      dst[2 + 3*dstStep] = vp8_ClampTblLookup(v6, src[2 + 3*srcStep]);

      dst[2 + 1*dstStep] = vp8_ClampTblLookup(v7, src[2 + 1*srcStep]);
      dst[3 + 3*dstStep] = vp8_ClampTblLookup(v7, src[3 + 3*srcStep]);

      dst[3 + 1*dstStep] = vp8_ClampTblLookup(v8, src[3 + 1*srcStep]);

      dst[0 + 2*dstStep] = vp8_ClampTblLookup(v9, src[0 + 2*srcStep]);

      dst[0 + 3*dstStep] = vp8_ClampTblLookup(v10, src[0 + 3*srcStep]);
    }
    break;

  case VP8_B_VL_PRED:
    {
      v1 = (a[0]  + a[1] + 1) >> 1;
      v2 = (a[1]  + a[2] + 1) >> 1;

      v3 = (a[2]  + a[3] + 1) >> 1;

      v4 = (a[3]  + a[4] + 1) >> 1;
      v5 = (a[4]  + 2*a[5] + a[6] + 2) >> 2;

      v6   = (a[0] + 2*a[1]  + a[2] + 2) >> 2;
      v7   = (a[1] + 2*a[2]  + a[3] + 2) >> 2;
      v8   = (a[2] + 2*a[3]  + a[4] + 2) >> 2;
      v9   = (a[3] + 2*a[4]  + a[5] + 2) >> 2;
      v10  = (a[5] + 2*a[6]  + a[7] + 2) >> 2;

      dst[0 + 0*dstStep] = vp8_ClampTblLookup(v1, src[0 + 0*srcStep]);

      dst[1 + 0*dstStep] = vp8_ClampTblLookup(v2, src[1 + 0*srcStep]);
      dst[0 + 2*dstStep] = vp8_ClampTblLookup(v2, src[0 + 2*srcStep]);

      dst[2 + 0*dstStep] = vp8_ClampTblLookup(v3, src[2 + 0*srcStep]);
      dst[1 + 2*dstStep] = vp8_ClampTblLookup(v3, src[1 + 2*srcStep]);

      dst[3 + 0*dstStep] = vp8_ClampTblLookup(v4, src[3 + 0*srcStep]);
      dst[2 + 2*dstStep] = vp8_ClampTblLookup(v4, src[2 + 2*srcStep]);

      dst[0 + 1*dstStep] = vp8_ClampTblLookup(v6, src[0 + 1*srcStep]);

      dst[1 + 1*dstStep] = vp8_ClampTblLookup(v7, src[1 + 1*srcStep]);
      dst[0 + 3*dstStep] = vp8_ClampTblLookup(v7, src[0 + 3*srcStep]);

      dst[2 + 1*dstStep] = vp8_ClampTblLookup(v8, src[2 + 1*srcStep]);
      dst[1 + 3*dstStep] = vp8_ClampTblLookup(v8, src[1 + 3*srcStep]);


      dst[3 + 1*dstStep] = vp8_ClampTblLookup(v9, src[3 + 1*srcStep]);
      dst[2 + 3*dstStep] = vp8_ClampTblLookup(v9, src[2 + 3*srcStep]);

      dst[3 + 2*dstStep] = vp8_ClampTblLookup(v5, src[3 + 2*srcStep]);

      dst[3 + 3*dstStep] = vp8_ClampTblLookup(v10, src[3 + 3*srcStep]);
    }
    break;

  case VP8_B_HD_PRED:
    {
      v1 = (ltp  + l[0] + 1) >> 1;
      v5 = (l[0] + l[1] + 1) >> 1;
      v7 = (l[1] + l[2] + 1) >> 1;
      v9 = (l[2] + l[3] + 1) >> 1;

      v2  = (l[0] + 2*ltp  + a[0] + 2) >> 2;
      v3  = (ltp  + 2*a[0] + a[1] + 2) >> 2;
      v4  = (a[0] + 2*a[1] + a[2] + 2) >> 2;
      v6  = (ltp  + 2*l[0] + l[1] + 2) >> 2;
      v8  = (l[0] + 2*l[1] + l[2] + 2) >> 2;
      v10 = (l[1] + 2*l[2] + l[3] + 2) >> 2;

      dst[0 + 0*dstStep] = vp8_ClampTblLookup(v1, src[0 + 0*srcStep]);
      dst[2 + 1*dstStep] = vp8_ClampTblLookup(v1, src[2 + 1*srcStep]);

      dst[1 + 0*dstStep] = vp8_ClampTblLookup(v2, src[1 + 0*srcStep]);
      dst[3 + 1*dstStep] = vp8_ClampTblLookup(v2, src[3 + 1*srcStep]);

      dst[2 + 0*dstStep] = vp8_ClampTblLookup(v3, src[2 + 0*srcStep]);

      dst[3 + 0*dstStep] = vp8_ClampTblLookup(v4, src[3 + 0*srcStep]);

      dst[0 + 1*dstStep] = vp8_ClampTblLookup(v5, src[0 + 1*srcStep]);
      dst[2 + 2*dstStep] = vp8_ClampTblLookup(v5, src[2 + 2*srcStep]);

      dst[1 + 1*dstStep] = vp8_ClampTblLookup(v6, src[1 + 1*srcStep]);
      dst[3 + 2*dstStep] = vp8_ClampTblLookup(v6, src[3 + 2*srcStep]);

      dst[0 + 2*dstStep] = vp8_ClampTblLookup(v7, src[0 + 2*srcStep]);
      dst[2 + 3*dstStep] = vp8_ClampTblLookup(v7, src[2 + 3*srcStep]);

      dst[1 + 2*dstStep] = vp8_ClampTblLookup(v8, src[1 + 2*srcStep]);
      dst[3 + 3*dstStep] = vp8_ClampTblLookup(v8, src[3 + 3*srcStep]);

      dst[0 + 3*dstStep] = vp8_ClampTblLookup(v9, src[0 + 3*srcStep]);

      dst[1 + 3*dstStep] = vp8_ClampTblLookup(v10, src[1 + 3*srcStep]);
    }
    break;

  case VP8_B_HU_PRED:
    {
      v1 = (l[0]  + l[1] + 1) >> 1;
      v3 = (l[1]  + l[2] + 1) >> 1;
      v5 = (l[2]  + l[3] + 1) >> 1;

      v2  = (l[0] + 2*l[1]  + l[2] + 2) >> 2;
      v4  = (l[1] + 2*l[2]  + l[3] + 2) >> 2;
      v6  = (l[2] + 3*l[3]         + 2) >> 2;

      v7 = l[3];

      dst[0 + 0*dstStep] = vp8_ClampTblLookup(v1, src[0 + 0*srcStep]);

      dst[1 + 0*dstStep] = vp8_ClampTblLookup(v2, src[1 + 0*srcStep]);

      dst[2 + 0*dstStep] = vp8_ClampTblLookup(v3, src[2 + 0*srcStep]);
      dst[0 + 1*dstStep] = vp8_ClampTblLookup(v3, src[0 + 1*srcStep]);

      dst[3 + 0*dstStep] = vp8_ClampTblLookup(v4, src[3 + 0*srcStep]);
      dst[1 + 1*dstStep] = vp8_ClampTblLookup(v4, src[1 + 1*srcStep]);

      dst[2 + 1*dstStep] = vp8_ClampTblLookup(v5, src[2 + 1*srcStep]);
      dst[0 + 2*dstStep] = vp8_ClampTblLookup(v5, src[0 + 2*srcStep]);

      dst[3 + 1*dstStep] = vp8_ClampTblLookup(v6, src[3 + 1*srcStep]);
      dst[1 + 2*dstStep] = vp8_ClampTblLookup(v6, src[1 + 2*srcStep]);

      dst[2 + 2*dstStep] = vp8_ClampTblLookup(v7, src[2 + 2*srcStep]);
      dst[3 + 2*dstStep] = vp8_ClampTblLookup(v7, src[3 + 2*srcStep]);
      dst[0 + 3*dstStep] = vp8_ClampTblLookup(v7, src[0 + 3*srcStep]);
      dst[1 + 3*dstStep] = vp8_ClampTblLookup(v7, src[1 + 3*srcStep]);
      dst[2 + 3*dstStep] = vp8_ClampTblLookup(v7, src[2 + 3*srcStep]);
      dst[3 + 3*dstStep] = vp8_ClampTblLookup(v7, src[3 + 3*srcStep]);
    }
    break;

  default:
    break;
  }

  return;
} // vp8_intra_predict4x4_add


static void vp8_intra_predict8x8_add_uv(Ipp16s* src, Ipp32u srcStep, Ipp8u bMode,
                                        Ipp8u* dst, Ipp32u dstStep,
                                        Ipp8u* above, Ipp8u* left, Ipp8u ltp,
                                        Ipp8u haveUp, Ipp8u haveLeft)
{
  Ipp32s i  = 0;
  Ipp32s j  = 0;
  Ipp32s sh = 2;

  switch(bMode)
  {
  case VP8_MB_DC_PRED:
    {
      Ipp32s average = 0;
      Ipp32s dcVal;

      if(haveUp)
      {
        for(i = 0; i < 8; i++)
          average += above[i];
      }

      if(haveLeft)
      {
        for(i = 0; i < 8; i++)
          average += left[i];
      }

      if(!haveLeft && !haveUp)
      {
        dcVal = 128;
      }
      else
      {
        sh   += (haveLeft + haveUp);
        dcVal = (average + (1 << (sh - 1))) >> sh;
      }

      for(i = 0; i < 8; i++)
      {
        for(j = 0; j < 8; j++)
         dst[j] = vp8_ClampTblLookup(dcVal, src[j]);

        dst += dstStep;
        src += srcStep;
      }
    }
    break;

  case VP8_MB_V_PRED:
    {
      for(i = 0; i < 8; i++)
      {
        for(j = 0; j < 8; j++)
         dst[j] = vp8_ClampTblLookup(above[j], src[j]);

        dst += dstStep;
        src += srcStep;
      }
    }
    break;

  case VP8_MB_H_PRED:
    {
      for(i = 0; i < 8; i++)
      {
        for(j = 0; j < 8; j++)
         dst[j] = vp8_ClampTblLookup(left[i], src[j]);

        dst += dstStep;
        src += srcStep;
      }
    }
    break;

  case VP8_MB_TM_PRED:
    {
      for(i = 0; i < 8; i++)
      {
        for(j = 0; j < 8; j++)
         dst[i*dstStep + j] = vp8_ClampTblLookup(vp8_CLIP255(above[j] + left[i] - ltp),
                                           src[i*srcStep + j]);
      }
    }
    break;

  default:
    break;
  }

  return;
} // vp8_intra_predict8x8_add_uv()


////////////////////////////////////////////////////////////////

Ipp8u vp8_filter_hv[8] = {0, 1, 1, 1, 1, 1, 1, 1};


static void vp8_inter_predict_6tap_h(Ipp16s* data,    Ipp32u dataStep,
                                     Ipp8u*  refSrc,  Ipp32u refStep,
                                     Ipp8u*  dst,     Ipp32u dstStep,
                                     Ipp8u   size_h,  Ipp8u  size_v,
                                     Ipp8u   hf_indx, Ipp8u  vf_indx)
{
  Ipp32u i;
  Ipp32u j;
  Ipp16s val;

  vf_indx;hf_indx;

  const Ipp16s* filter = vp8_6tap_filters[hf_indx];

  for(i = 0; i < size_v; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((refSrc[j-2] * filter[0] +
                     refSrc[j-1] * filter[1] +
                     refSrc[j+0] * filter[2] +
                     refSrc[j+1] * filter[3] +
                     refSrc[j+2] * filter[4] +
                     refSrc[j+3] * filter[5] + 64) >> 7);

      dst[j] = vp8_ClampTblLookup(data[j], vp8_clamp8u(val));
    }

    data   += dataStep;
    refSrc += refStep;
    dst    += dstStep;
  }

  return;
} // vp8_inter_predict_6tap_h()


static void vp8_inter_predict_6tap_v(Ipp16s* data,    Ipp32u dataStep,
                                     Ipp8u*  refSrc,  Ipp32u refStep,
                                     Ipp8u*  dst,     Ipp32u dstStep,
                                     Ipp8u   size_h,  Ipp8u  size_v,
                                     Ipp8u   hf_indx, Ipp8u  vf_indx)
{
  
  hf_indx; vf_indx;

  Ipp32u i;
  Ipp32u j;
  Ipp16s val;

  const Ipp16s* filter = vp8_6tap_filters[vf_indx];

  for(i = 0; i < size_v; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((refSrc[j-2*refStep] * filter[0] +
                     refSrc[j-1*refStep] * filter[1] +
                     refSrc[j+0*refStep] * filter[2] +
                     refSrc[j+1*refStep] * filter[3] +
                     refSrc[j+2*refStep] * filter[4] +
                     refSrc[j+3*refStep] * filter[5] + 64) >> 7);

      dst[j] = vp8_ClampTblLookup(data[j], vp8_clamp8u(val));
    }

    data   += dataStep;
    refSrc += refStep;
    dst    += dstStep;
  }

  return;
} // vp8_inter_predict_6tap_v()


static void vp8_inter_predict_6tap_hv(Ipp16s* data,    Ipp32u dataStep,
                                      Ipp8u*  refSrc,  Ipp32u refStep,
                                      Ipp8u*  dst,     Ipp32u dstStep,
                                      Ipp8u   size_h,  Ipp8u  size_v,
                                      Ipp8u   hf_indx, Ipp8u  vf_indx)
{
  Ipp32u  i;
  Ipp32u  j;
  Ipp16s  val; // must be more than 8-bit

  Ipp8u tmp[21*24];

  Ipp8u* tmp_ptr = tmp;

  refSrc -= 2*refStep;

  const Ipp16s* filter_h = vp8_6tap_filters[hf_indx];
  const Ipp16s* filter_v = vp8_6tap_filters[vf_indx];

  for(i = 0; i < size_v+5u; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((refSrc[j-2] * filter_h[0] +
                     refSrc[j-1] * filter_h[1] +
                     refSrc[j+0] * filter_h[2] +
                     refSrc[j+1] * filter_h[3] +
                     refSrc[j+2] * filter_h[4] +
                     refSrc[j+3] * filter_h[5] + 64) >> 7);

      tmp_ptr[j] = vp8_clamp8u(val);
    }

    refSrc  += refStep;
    tmp_ptr += size_h; //????
  }

  tmp_ptr = tmp + 2*size_h;

  for(i = 0; i < size_v; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((tmp_ptr[j-2*size_h] * filter_v[0] +
                     tmp_ptr[j-1*size_h] * filter_v[1] +
                     tmp_ptr[j+0*size_h] * filter_v[2] +
                     tmp_ptr[j+1*size_h] * filter_v[3] +
                     tmp_ptr[j+2*size_h] * filter_v[4] +
                     tmp_ptr[j+3*size_h] * filter_v[5] + 64) >> 7);

      dst[j] = vp8_ClampTblLookup(data[j], vp8_clamp8u(val));
    }

    data    += dataStep;
    tmp_ptr += size_h; //????
    dst     += dstStep;
  }

  return;
} // vp8_inter_predict_6tap_hv()


//
// bilinear filters
//
static void vp8_inter_predict_bilinear_h(Ipp16s* data,    Ipp32u dataStep,
                                         Ipp8u*  refSrc,  Ipp32u refStep,
                                         Ipp8u*  dst,     Ipp32u dstStep,
                                         Ipp8u   size_h,  Ipp8u  size_v,
                                         Ipp8u   hf_indx, Ipp8u  vf_indx)
{
  
  vf_indx; hf_indx;

  Ipp32u i;
  Ipp32u j;
  Ipp16s val;

  const Ipp8u* filter = vp8_bilinear_filters[hf_indx];

  for(i = 0; i < size_v; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((refSrc[j+0] * filter[0] +
                      refSrc[j+1] * filter[1] + 64) >> 7);

      dst[j] = vp8_ClampTblLookup(data[j], vp8_clamp8u(val));
    }

    data   += dataStep;
    refSrc += refStep;
    dst    += dstStep;
  }

  return;
} // vp8_inter_predict_bilinear_h()


static void vp8_inter_predict_bilinear_v(Ipp16s* data,    Ipp32u dataStep,
                                         Ipp8u*  refSrc,  Ipp32u refStep,
                                         Ipp8u*  dst,     Ipp32u dstStep,
                                         Ipp8u   size_h,  Ipp8u  size_v,
                                         Ipp8u   hf_indx, Ipp8u  vf_indx)
{

  hf_indx; vf_indx;

  Ipp32u i;
  Ipp32u j;
  Ipp16s val;

  const Ipp8u* filter = vp8_bilinear_filters[vf_indx];

  for(i = 0; i < size_v; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((refSrc[j+0*refStep] * filter[0] +
                      refSrc[j+1*refStep] * filter[1] + 64) >> 7);

      dst[j] = vp8_ClampTblLookup(data[j], vp8_clamp8u(val));
    }

    data   += dataStep;
    refSrc += refStep;
    dst    += dstStep;
  }

  return;
} // vp8_inter_predict_bilinear_v()


static void vp8_inter_predict_bilinear_hv(Ipp16s* data,    Ipp32u dataStep,
                                          Ipp8u*  refSrc,  Ipp32u refStep,
                                          Ipp8u*  dst,     Ipp32u dstStep,
                                          Ipp8u   size_h,  Ipp8u  size_v,
                                          Ipp8u   hf_indx, Ipp8u  vf_indx)
{
  Ipp32u i;
  Ipp32u j;
  Ipp16s val;

  Ipp8u tmp[17*16];

  Ipp8u* tmp_ptr = tmp;

  const Ipp8u* filter_h = vp8_bilinear_filters[hf_indx];
  const Ipp8u* filter_v = vp8_bilinear_filters[vf_indx];

  for(i = 0; i < size_v+1u; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((refSrc[j+0] * filter_h[0] +
                      refSrc[j+1] * filter_h[1] + 64) >> 7);

      tmp_ptr[j] = vp8_clamp8u(val);
    }

    refSrc  += refStep;
    tmp_ptr += size_h; //????
  }

  tmp_ptr = tmp;

  for(i = 0; i < size_v; i++)
  {
    for(j = 0; j < size_h; j++)
    {
      val = (Ipp16s)((tmp_ptr[j+0*size_h] * filter_v[0] +
                      tmp_ptr[j+1*size_h] * filter_v[1] + 64) >> 7);

      dst[j] = vp8_ClampTblLookup(data[j], vp8_clamp8u(val));
    }

    data    += dataStep;
    tmp_ptr += size_h; //????
    dst     += dstStep;
  }

  return;
} // vp8_inter_predict_bilinear_hv()



static void vp8_inter_copy_add(Ipp16s* data,    Ipp32u dataStep,
                               Ipp8u*  refSrc,  Ipp32u refStep,
                               Ipp8u*  dst,     Ipp32u dstStep,
                               Ipp8u   size_h,  Ipp8u  size_v,
                               Ipp8u   hf_indx, Ipp8u  vf_indx)
{

  hf_indx; vf_indx;

  Ipp32u i;
  Ipp32u j;

  Ipp8u* dstPtr = dst;
  Ipp8u* refPtr = refSrc;

  for(i = 0; i < size_v; i++)
  {
    for(j = 0; j < size_h; j += 4)
    {
      dstPtr[j + 0] = vp8_ClampTblLookup(refPtr[j + 0], data[j + 0]);
      dstPtr[j + 1] = vp8_ClampTblLookup(refPtr[j + 1], data[j + 1]);
      dstPtr[j + 2] = vp8_ClampTblLookup(refPtr[j + 2], data[j + 2]);
      dstPtr[j + 3] = vp8_ClampTblLookup(refPtr[j + 3], data[j + 3]);
    }

    dstPtr += dstStep;
    refPtr += refStep;
    data   += dataStep;
  }

  return;
} // vp8_inter_copy_add()


typedef void (*vp8_inter_predict_fptr)(Ipp16s* data,    Ipp32u dataStep,
                                       Ipp8u*  refSrc,  Ipp32u refStep,
                                       Ipp8u*  dst,     Ipp32u dstStep,
                                       Ipp8u   size_v,  Ipp8u  size_h,
                                       Ipp8u   vf_indx, Ipp8u  hf_indx);


//                                         6T/B|h |v
vp8_inter_predict_fptr vp8_inter_predict_fun[2][2][2] = 
{
  {// 6TAP functions
    {
      vp8_inter_copy_add,       vp8_inter_predict_6tap_v
    },
    {
      vp8_inter_predict_6tap_h, vp8_inter_predict_6tap_hv
    }
  },
  {// bilinear func`s
    {
      vp8_inter_copy_add,           vp8_inter_predict_bilinear_v
    },
    {
      vp8_inter_predict_bilinear_h, vp8_inter_predict_bilinear_hv
    }
  }
};


#define VP8_MB_INTER_PREDICT(MbData,  dataStep,\
                             refSrc,  refStep,\
                             dst,     dstStep,\
                             size_h,  size_v,\
                             hf_indx, vf_indx,\
                             filter_type)\
{\
  Ipp8u v_filter = vp8_filter_hv[vf_indx];\
  Ipp8u h_filter = vp8_filter_hv[hf_indx];\
  \
  vp8_inter_predict_fun[filter_type][h_filter][v_filter](MbData,  dataStep,\
                                                         refSrc,  refStep,\
                                                         dst,     dstStep,\
                                                         size_h,  size_v,\
                                                         hf_indx, vf_indx);\
}


#define GET_UV_MV(mv) \
{\
  mv_x = (mv.mvx < 0) ? mv.mvx-1 : mv.mvx+1;\
  mv_y = (mv.mvy < 0) ? mv.mvy-1 : mv.mvy+1;\
  mv_x /= 2;\
  mv_y /= 2;\
}


#define GET_UV_MV_SPLIT(block_mv, mv_offset) \
{\
  mv_x = block_mv[mv_offset + 0].mvx +  \
         block_mv[mv_offset + 1].mvx +  \
         block_mv[mv_offset + 4].mvx +  \
         block_mv[mv_offset + 5].mvx;   \
                                           \
  mv_y = block_mv[mv_offset + 0].mvy +  \
         block_mv[mv_offset + 1].mvy +  \
         block_mv[mv_offset + 4].mvy +  \
         block_mv[mv_offset + 5].mvy;   \
                                           \
      mv_x = (mv_x < 0) ? mv_x-4 : mv_x+4; \
      mv_y = (mv_y < 0) ? mv_y-4 : mv_y+4; \
      mv_x /= 8;                           \
      mv_y /= 8;                           \
}


void vp8_intra_predict16x16_add(Ipp16s* src, Ipp32u srcStep, Ipp8u bMode,
                                Ipp8u* dst, Ipp32u dstStep,
                                Ipp8u* above, Ipp8u* left, Ipp8u ltp,
                                Ipp8u haveUp, Ipp8u haveLeft)
{
  Ipp32s i  = 0;
  Ipp32s j  = 0;
  Ipp32s sh = 3;

  switch(bMode)
  {
  case VP8_MB_DC_PRED:
    {
      Ipp32s average = 0;
      Ipp32s dcVal;

      if(haveUp)
      {
        for(i = 0; i < 16; i++)
          average += above[i];
      }

      if(haveLeft)
      {
        for(i = 0; i < 16; i++)
          average += left[i];
      }

      if(!haveLeft && !haveUp)
      {
        dcVal = 128;
      }
      else
      {
        sh   += (haveLeft + haveUp);
        dcVal = (average + (1 << (sh - 1))) >> sh;
      }

      for(i = 0; i < 16; i++)
      {
        for(j = 0; j < 16; j++)
         dst[j] = vp8_ClampTblLookup(dcVal, src[j]);

        dst += dstStep;
        src += srcStep;
      }
    }
    break;

  case VP8_MB_V_PRED:
    {
      for(i = 0; i < 16; i++)
      {
        for(j = 0; j < 16; j++)
         dst[j] = vp8_ClampTblLookup(above[j], src[j]);

        dst += dstStep;
        src += srcStep;
      }
    }
    break;

  case VP8_MB_H_PRED:
    {
      for(i = 0; i < 16; i++)
      {
        for(j = 0; j < 16; j++)
         dst[j] = vp8_ClampTblLookup(left[i], src[j]);

        dst += dstStep;
        src += srcStep;
      }
    }
    break;

  case VP8_MB_TM_PRED:
    {
      for(i = 0; i < 16; i++)
      {
        for(j = 0; j < 16; j++)
         dst[i*dstStep + j] = vp8_ClampTblLookup(vp8_CLIP255(above[j] + left[i] - ltp),
                                           src[i*srcStep + j]);
      }
    }
    break;

  default:
    break;
  }

  return;
} // vp8_intra_predict16x16_add()



void VP8VideoDecoder::ReconstructMbRow(vp8BooleanDecoder *pBoolDec, Ipp32s mb_row)
{

  pBoolDec;
    
  Ipp16s      idctMb[24*4*4];
  Ipp32u      idctMbStep = /*24*4*/ 16 + 2*4;

  Ipp32u      mb_col   = 0;
  vp8_MbInfo* currMb   = 0;

  for(mb_col = 0; mb_col < m_FrameInfo.mbPerRow; mb_col++)
  {
    currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

    UMC_SET_ZERO(idctMb);

    if(0 == currMb->skipCoeff)
    {
      DequantMbCoeffs(currMb);

      if(currMb->mode != VP8_MV_SPLIT && currMb->mode != VP8_MB_B_PRED)
      {
        if(currMb->numNNZ[0] == 1)
          vp8_iwht_1x1(currMb->coeffs, currMb->coeffs + 16);
        else
          vp8_iwht_4x4(currMb->coeffs, currMb->coeffs + 16);

        memset(currMb->coeffs, 0, 16*sizeof(Ipp16s));
      }

      InverseDCT_Mb(currMb, idctMb, idctMbStep);
    }

    if(I_PICTURE == m_FrameInfo.frameType || VP8_INTRA_FRAME == currMb->refFrame)
    {
       PredictMbIntra(idctMb, idctMbStep, mb_row, mb_col);
    }
    else
    {
      // inter prediction
      PredictMbInter(idctMb, idctMbStep, mb_row, mb_col);
    }
  }

  return;
} // ReconstructMbRow()


void VP8VideoDecoder::InverseDCT_Mb(vp8_MbInfo* pMb, Ipp16s* pOut, Ipp32u outStep)
//void VP8VideoDecoder::InverseDCT_Mb(vp8_MbInfo* pMb, Ipp8u* pOut, Ipp32s outStep)
{
  Ipp32u      i     = 0;
  Ipp32u      j     = 0;
  vp8_MbInfo* currMb = pMb;
  Ipp16s*     coeffs = currMb->coeffs;
  Ipp16s*     dst;
//  Ipp8u*     dst;
  Ipp32u      dstStep = outStep;
  Ipp8u*      nnz;
  Ipp8u       nnz_y2;

  nnz_y2 = (currMb->numNNZ[0] > 0) ? 1 : 0;

  nnz = &currMb->numNNZ[1];
  dst = pOut;

  coeffs += 4*4; //skip Y2 block

  // 4x4 Y blocks
  for(i = 0; i < 4; i++)
  {
    for(j = 0; j < 4; j++)
    {
      if((nnz[4*i + j] + nnz_y2) == 1)
        vp8_idct_1x1(coeffs, dst + j*4, dstStep); // when only dc coef != 0
      else
        vp8_idct_4x4(coeffs, dst + j*4, dstStep);

      coeffs += 16;
    }

    dst += 4*dstStep;
  }

  dst = pOut + 16;

  nnz = &currMb->numNNZ[17];

  // 2x2 U blocks + 2x2 V blocks
  for(i = 0; i < 4; i++)
  {
    for(j = 0; j < 2; j++)
    {
      if(nnz[2*i + j] == 1)
        vp8_idct_1x1(coeffs, dst + j*4, dstStep); // when only dc coef != 0
      else
        vp8_idct_4x4(coeffs, dst + j*4, dstStep);

      coeffs += 16;
    }

    dst += 4*dstStep;
  }

  return;
} // InverseDCT_Mb()


void VP8VideoDecoder::PredictMbIntra(Ipp16s* pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col)
{
  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  if(VP8_MB_B_PRED == currMb->mode)
  {
    PredictMbIntra4x4(pMbData, dataStep, mb_row, mb_col);
  }
  else
  {
    //variant if(VP8_MB_B_PRED != currMb->mod) -> 16x16
    PredictMbIntra16x16(pMbData, dataStep, mb_row, mb_col);
  }

  PredictMbIntraUV(pMbData + 16, dataStep, mb_row, mb_col);

  return;
} // VP8VideoDecoder::PredictMbIntra()


///
//TODO: may be need to redesign all Intra prediction to support full frame height/lenght Left/Above borders????
//
void VP8VideoDecoder::PredictMbIntra4x4(Ipp16s* pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col)
{
  Ipp32u i = 0;
  Ipp32u j = 0;

  Ipp8u   left[4];
  Ipp8u   above[8];
  Ipp8u   ltp;
  Ipp8u*  above_ptr;

  Ipp8u*  dst_y  = m_CurrFrame->data_y + m_CurrFrame->step_y*16*mb_row + 16*mb_col;
  Ipp32u  step_y = m_CurrFrame->step_y;

  Ipp16s* src     = pMbData;
  Ipp32u  srcStep = dataStep;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  for(i = 0; i < 4; i++)
  {
    for(j = 0; j < 4; j++)
    {
      if(!(mb_row + i)) // left-top block in Mb
      {
        above[0] = above[1] = above[2] = above[3] = 
        above[4] = above[5] = above[6] = above[7] = 127;

        above_ptr = &above[0];
      }
      else
      {
        if(mb_col != static_cast<signed>(m_FrameInfo.mbPerRow) - 1 && mb_row)
        {
          if(j != 3 || !i)
            above_ptr = dst_y - step_y;
          else
          {
            above[0] = dst_y[0 - step_y];
            above[1] = dst_y[1 - step_y];
            above[2] = dst_y[2 - step_y];
            above[3] = dst_y[3 - step_y];

            above[4] = dst_y[4 + 0 - i*4*step_y - step_y];
            above[5] = dst_y[4 + 1 - i*4*step_y - step_y];
            above[6] = dst_y[4 + 2 - i*4*step_y - step_y];
            above[7] = dst_y[4 + 3 - i*4*step_y - step_y];

            above_ptr = &above[0];
          }
        }
        else
        {
          if(j != 3)
            above_ptr = dst_y - step_y;
          else
          {
            above[0] = dst_y[0 - step_y];
            above[1] = dst_y[1 - step_y];
            above[2] = dst_y[2 - step_y];
            above[3] = dst_y[3 - step_y];

            if(!mb_row)
            {
              above[4] = above[5] = above[6] = above[7] = 127;
            }
            else
            {
              above[4] = above[5] = above[6] = above[7] = 
                dst_y[3 + 0 - i*4*step_y - step_y];
            }
            above_ptr = &above[0];
          }
        }
      }

      if(!(mb_col + j)) // left column of blocks in leftmost column of Mb`s
      {
        left[0] = left[1] = left[2] = left[3] = 129;

        if(!(mb_row + i))
          ltp = 127;
        else
          ltp = 129;
      }
      else
      {
        left[0] = dst_y[         - 1];
        left[1] = dst_y[1*step_y - 1];
        left[2] = dst_y[2*step_y - 1];
        left[3] = dst_y[3*step_y - 1];

        if(!(mb_row + i))
          ltp = 127;
        else
          ltp = dst_y[-1 - step_y];
      }

      vp8_intra_predict4x4_add(src + 4*j, srcStep, dst_y, step_y, currMb->blockMode[4*i+j],
                               above_ptr, left, ltp);

      dst_y += 4;
    }

     src   += 4*srcStep;
     dst_y += 4* step_y - 4*4;
  }

  return;
} //  VP8VideoDecoder::PredictMbIntra4x4()


void VP8VideoDecoder::PredictMbIntraUV(Ipp16s* pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col)
{
  Ipp8u i;

  Ipp8u   left_u[8];
  Ipp8u   left_v[8];

  Ipp8u   above_u[8] = {127, 127, 127, 127, 127, 127, 127, 127};
  Ipp8u   above_v[8] = {127, 127, 127, 127, 127, 127, 127, 127};

  Ipp8u*   aptr_u;
  Ipp8u*   aptr_v;

  Ipp8u   ltp_u;
  Ipp8u   ltp_v;

  Ipp8u haveLeft = (mb_col == 0) ? 0 : 1;
  Ipp8u haveUp   = (mb_row == 0) ? 0 : 1;

  Ipp8u*  dst_u  = m_CurrFrame->data_u + m_CurrFrame->step_uv*8*mb_row + 8*mb_col;
  Ipp8u*  dst_v  = m_CurrFrame->data_v + m_CurrFrame->step_uv*8*mb_row + 8*mb_col;

  Ipp32u  step_uv = m_CurrFrame->step_uv;

  Ipp16s* src     = pMbData;
  Ipp32u  srcStep = dataStep;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  if(!mb_col)
  {
    memset(left_u, 129, 8*sizeof(Ipp8u));
    memset(left_v, 129, 8*sizeof(Ipp8u));
  }
  else
  {
    for(i = 0; i < 8; i++)
    {
      left_u[i] = dst_u[i*step_uv - 1];
      left_v[i] = dst_v[i*step_uv - 1];
    }
  }

  if(!mb_row)
  {
    aptr_u = above_u;
    aptr_v = above_v;

    ltp_u = ltp_v = 127;
  }
  else
  {
    aptr_u = dst_u - step_uv;
    aptr_v = dst_v - step_uv;

    if(mb_col)
    {
      ltp_u = dst_u[-1 - step_uv];
      ltp_v = dst_v[-1 - step_uv];
    }
    else
      ltp_u = ltp_v = 129;
  }

   vp8_intra_predict8x8_add_uv(src, srcStep, currMb->modeUV, dst_u, step_uv,
                               aptr_u, left_u, ltp_u, haveUp, haveLeft);

   vp8_intra_predict8x8_add_uv(src + 4*2*srcStep, srcStep, currMb->modeUV, dst_v, step_uv,
                               aptr_v, left_v, ltp_v, haveUp, haveLeft);
  return;
} // VP8VideoDecoder::PredictMbIntraUV()


void VP8VideoDecoder::PredictMbIntra16x16(Ipp16s* pMbData, Ipp32u dataStep, 
                                          Ipp32s mb_row,   Ipp32s mb_col)
{
  Ipp8u i;

  Ipp8u   left[16];

  Ipp8u   above[16] = {127, 127, 127, 127, 127, 127, 127, 127,
                       127, 127, 127, 127, 127, 127, 127, 127};

  Ipp8u*  aptr;
  Ipp8u   ltp;

  Ipp8u haveLeft = (mb_col == 0) ? 0 : 1;
  Ipp8u haveUp   = (mb_row == 0) ? 0 : 1;

  Ipp8u*  dst_y  = m_CurrFrame->data_y + m_CurrFrame->step_y*16*mb_row + 16*mb_col;

  Ipp32u  step_y = m_CurrFrame->step_y;

  Ipp16s* src     = pMbData;
  Ipp32u  srcStep = dataStep;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  if(!mb_col)
  {
    memset(left, 129, 16*sizeof(Ipp8u));
  }
  else
  {
    for(i = 0; i < 16; i++)
      left[i] = dst_y[i*step_y - 1];
  }

  if(!mb_row)
  {
    aptr = above;
    ltp  = 127;
  }
  else
  {
    aptr = dst_y - step_y;

    if(mb_col)
      ltp = dst_y[-1 - step_y];
    else
      ltp = 129;
  }

   vp8_intra_predict16x16_add(src, srcStep, currMb->mode, dst_y, step_y,
                              aptr, left, ltp, haveUp, haveLeft);

  return;
} // VP8VideoDecoder::PredictMbIntra16x16()



void VP8VideoDecoder::PredictMbInter(Ipp16s* pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col)
{

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  if(VP8_MV_SPLIT != currMb->mode)
  {
    PredictMbInter16x16(pMbData, dataStep, mb_row, mb_col);
  }
  else
  {
    switch(currMb->partitions)
    {
    case VP8_MV_TOP_BOTTOM:
      {
        PredictMbInter16x8(pMbData, dataStep, mb_row, mb_col);
      }
    break;

    case VP8_MV_LEFT_RIGHT:
      {
        PredictMbInter8x16(pMbData, dataStep, mb_row, mb_col);
      }
      break;

    case VP8_MV_QUARTERS:
      {
        PredictMbInter8x8(pMbData, dataStep, mb_row, mb_col);
      }
      break;

    case VP8_MV_16:
      {
        PredictMbInter4x4(pMbData, dataStep, mb_row, mb_col);
      }
      break;

    default:
      break;
    }
  }


  return;
} // VP8VideoDecoder::PredictMbInter()


void VP8VideoDecoder::PredictMbInter16x16(Ipp16s* pMbData, Ipp32u dataStep,
                                          Ipp32s  mb_row,  Ipp32s mb_col)
{
  Ipp8u ref_indx;

  Ipp8u filter_type = m_FrameInfo.interpolationFlags & 1; //0 - 6TAP, 1 - bilinear

  Ipp32s mv_x;
  Ipp32s mv_y;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  ref_indx = m_RefFrameIndx[currMb->refFrame];

  // predict Y
  Ipp32u refStep = m_FrameData[ref_indx].step_y;
  Ipp8u* ref_y   = m_FrameData[ref_indx].data_y + refStep*16*mb_row + 16*mb_col;

  Ipp32u dstStep = m_CurrFrame->step_y;
  Ipp8u* dst_y   = m_CurrFrame->data_y + dstStep*16*mb_row + 16*mb_col;


  mv_x = currMb->mv.mvx;
  mv_y = currMb->mv.mvy;

  ref_y += (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData, dataStep, ref_y, refStep, dst_y, dstStep, 16, 16,
                       mv_x&7, mv_y&7, filter_type)

  // predict U/V
  refStep = m_FrameData[ref_indx].step_uv;
  dstStep = m_CurrFrame->step_uv;

  Ipp8u* ref_u = m_FrameData[ref_indx].data_u + refStep*8*mb_row + 8*mb_col;
  Ipp8u* ref_v = m_FrameData[ref_indx].data_v + refStep*8*mb_row + 8*mb_col;

  Ipp8u* dst_u   = m_CurrFrame->data_u + dstStep*8*mb_row + 8*mb_col;
  Ipp8u* dst_v   = m_CurrFrame->data_v + dstStep*8*mb_row + 8*mb_col;


  GET_UV_MV(currMb->mv)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  ref_u += (mv_y >> 3) * refStep + (mv_x >> 3); 
  ref_v += (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData + 16, dataStep, ref_u, refStep, dst_u, dstStep,
                       8, 8, mv_x&7, mv_y&7, filter_type)

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 16, dataStep, ref_v, refStep,
                       dst_v, dstStep, 8, 8,  mv_x&7, mv_y&7, filter_type)

  return;
} // VP8VideoDecoder::PredictMbInter16x16()


//VP8_MV_LEFT_RIGHT
void VP8VideoDecoder::PredictMbInter8x16(Ipp16s* pMbData, Ipp32u dataStep,
                                         Ipp32s  mb_row,  Ipp32s mb_col)
{
  Ipp8u ref_indx;

  Ipp8u filter_type = m_FrameInfo.interpolationFlags & 1; //0 - 6TAP, 1 - bilinear

  Ipp32s mv_x;
  Ipp32s mv_y;
  Ipp32s offset;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  ref_indx = m_RefFrameIndx[currMb->refFrame];

  // predict Y
  Ipp32u refStep = m_FrameData[ref_indx].step_y;
  Ipp8u* ref_y   = m_FrameData[ref_indx].data_y + refStep*16*mb_row + 16*mb_col;

  Ipp32u dstStep = m_CurrFrame->step_y;
  Ipp8u* dst_y   = m_CurrFrame->data_y + dstStep*16*mb_row + 16*mb_col;


  mv_x = currMb->blockMV[0].mvx;
  mv_y = currMb->blockMV[0].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData, dataStep,
                       ref_y + offset, refStep,
                       dst_y, dstStep,
                       8, 16,
                       mv_x&7, mv_y&7,
                       filter_type)

  mv_x = currMb->blockMV[2].mvx;
  mv_y = currMb->blockMV[2].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData + 8, dataStep,
                       ref_y + 8 + offset, refStep,
                       dst_y + 8, dstStep,
                       8, 16,
                       mv_x&7, mv_y&7,
                       filter_type)


  // predict U/V
  dstStep = m_CurrFrame->step_uv;
  refStep = m_FrameData[ref_indx].step_uv;

  Ipp8u* ref_u = m_FrameData[ref_indx].data_u + refStep*8*mb_row + 8*mb_col;;
  Ipp8u* ref_v = m_FrameData[ref_indx].data_v + refStep*8*mb_row + 8*mb_col;;

  Ipp8u* dst_u   = m_CurrFrame->data_u + dstStep*8*mb_row + 8*mb_col;
  Ipp8u* dst_v   = m_CurrFrame->data_v + dstStep*8*mb_row + 8*mb_col;


  GET_UV_MV_SPLIT(currMb->blockMV, 0)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16,   dataStep,
                       ref_u + offset, refStep,
                       dst_u, dstStep,
                       4, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 16, dataStep,
                       ref_v + offset, refStep,
                       dst_v, dstStep,
                       4, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  GET_UV_MV_SPLIT(currMb->blockMV, 2)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16 + 4,   dataStep,
                       ref_u + 4 + offset, refStep,
                       dst_u + 4, dstStep,
                       4, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 16 + 4, dataStep,
                       ref_v + 4 + offset, refStep,
                       dst_v + 4, dstStep,
                       4, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  return;
} // VP8VideoDecoder::PredictMbInter8x16()


//VP8_MV_TOP_BOTTOM
void VP8VideoDecoder::PredictMbInter16x8(Ipp16s* pMbData, Ipp32u dataStep,
                                         Ipp32s  mb_row,  Ipp32s mb_col)
{
  Ipp8u ref_indx;

  Ipp8u filter_type = m_FrameInfo.interpolationFlags & 1; //0 - 6TAP, 1 - bilinear

  Ipp32s mv_x;
  Ipp32s mv_y;
  Ipp32s offset;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  ref_indx = m_RefFrameIndx[currMb->refFrame];

  // predict Y
  Ipp32u refStep = m_FrameData[ref_indx].step_y;
  Ipp8u* ref_y   = m_FrameData[ref_indx].data_y +refStep*16*mb_row + 16*mb_col;


  Ipp32u dstStep = m_CurrFrame->step_y;
  Ipp8u* dst_y   = m_CurrFrame->data_y + dstStep*16*mb_row + 16*mb_col;

  mv_x = currMb->blockMV[0].mvx;
  mv_y = currMb->blockMV[0].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData, dataStep,
                       ref_y + offset, refStep,
                       dst_y, dstStep,
                       16, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  mv_x = currMb->blockMV[8].mvx;
  mv_y = currMb->blockMV[8].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep, dataStep,
                       ref_y + 8*refStep + offset, refStep,
                       dst_y + 8*dstStep, dstStep,
                       16, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  // predict U/V
  dstStep = m_CurrFrame->step_uv;
  refStep = m_FrameData[ref_indx].step_uv;

  Ipp8u* ref_u = m_FrameData[ref_indx].data_u + refStep*8*mb_row + 8*mb_col;
  Ipp8u* ref_v = m_FrameData[ref_indx].data_v + refStep*8*mb_row + 8*mb_col;

  Ipp8u* dst_u   = m_CurrFrame->data_u + dstStep*8*mb_row + 8*mb_col;
  Ipp8u* dst_v   = m_CurrFrame->data_v + dstStep*8*mb_row + 8*mb_col;

  GET_UV_MV_SPLIT(currMb->blockMV, 0)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16,   dataStep,
                       ref_u + offset, refStep,
                       dst_u, dstStep,
                       8, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 16, dataStep,
                       ref_v + offset, refStep,
                       dst_v, dstStep,
                       8, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  GET_UV_MV_SPLIT(currMb->blockMV, 8)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16 + 4*dataStep,   dataStep,
                       ref_u + 4*refStep + offset, refStep,
                       dst_u + 4*dstStep, dstStep,
                       8, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 16 + 4*dataStep, dataStep,
                       ref_v + 4*refStep + offset, refStep,
                       dst_v + 4*dstStep, dstStep,
                       8, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  return;
} // VP8VideoDecoder::PredictMbInter16x8()


//VP8_MV_QUARTERS
void VP8VideoDecoder::PredictMbInter8x8(Ipp16s* pMbData, Ipp32u dataStep,
                                        Ipp32s  mb_row,  Ipp32s mb_col)
{
  Ipp8u ref_indx;

  Ipp8u filter_type = m_FrameInfo.interpolationFlags & 1; //0 - 6TAP, 1 - bilinear

  Ipp32s mv_x;
  Ipp32s mv_y;
  Ipp32s offset;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  ref_indx = m_RefFrameIndx[currMb->refFrame];

  // predict Y
  Ipp32u refStep = m_FrameData[ref_indx].step_y;
  Ipp8u* ref_y   = m_FrameData[ref_indx].data_y + refStep*16*mb_row + 16*mb_col;

  Ipp32u dstStep = m_CurrFrame->step_y;
  Ipp8u* dst_y   = m_CurrFrame->data_y + dstStep*16*mb_row + 16*mb_col;


  mv_x = currMb->blockMV[0].mvx;
  mv_y = currMb->blockMV[0].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData, dataStep,
                       ref_y + offset, refStep,
                       dst_y, dstStep,
                       8, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  mv_x = currMb->blockMV[2].mvx;
  mv_y = currMb->blockMV[2].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData + 8, dataStep,
                       ref_y + 8 + offset, refStep,
                       dst_y + 8, dstStep,
                       8, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  mv_x = currMb->blockMV[8].mvx;
  mv_y = currMb->blockMV[8].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep, dataStep,
                       ref_y + 8*refStep + offset, refStep,
                       dst_y + 8*dstStep, dstStep,
                       8, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  mv_x = currMb->blockMV[10].mvx;
  mv_y = currMb->blockMV[10].mvy;

  offset = (mv_y >> 3) * refStep + (mv_x >> 3);

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 8, dataStep,
                       ref_y + 8*refStep + 8 + offset, refStep,
                       dst_y + 8*dstStep + 8, dstStep,
                       8, 8,
                       mv_x&7, mv_y&7,
                       filter_type)

  // predict U/V
  dstStep = m_CurrFrame->step_uv;
  refStep = m_FrameData[ref_indx].step_uv;

  Ipp8u* ref_u = m_FrameData[ref_indx].data_u + refStep*8*mb_row + 8*mb_col;
  Ipp8u* ref_v = m_FrameData[ref_indx].data_v + refStep*8*mb_row + 8*mb_col;

  Ipp8u* dst_u   = m_CurrFrame->data_u + dstStep*8*mb_row + 8*mb_col;
  Ipp8u* dst_v   = m_CurrFrame->data_v + dstStep*8*mb_row + 8*mb_col;

//  GET_UV_MV(currMb->blockMV[0])
  GET_UV_MV_SPLIT(currMb->blockMV, 0)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16,   dataStep,
                       ref_u + offset, refStep,
                       dst_u, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 16, dataStep,
                       ref_v + offset, refStep,
                       dst_v, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  GET_UV_MV_SPLIT(currMb->blockMV, 2)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16 + 4,   dataStep,
                       ref_u + 4 + offset, refStep,
                       dst_u + 4, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 16 + 4, dataStep,
                       ref_v + 4 + offset, refStep,
                       dst_v + 4, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  GET_UV_MV_SPLIT(currMb->blockMV, 8)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16 + 4*dataStep,   dataStep,
                       ref_u + 4*refStep + offset, refStep,
                       dst_u + 4*dstStep, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + (4+8)*dataStep + 16, dataStep,
                       ref_v + 4*refStep + offset, refStep,
                       dst_v + 4*dstStep, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  GET_UV_MV_SPLIT(currMb->blockMV, 10)

  if(m_FrameInfo.interpolationFlags & 2)
  {
    mv_x = mv_x & (~7);
    mv_y = mv_y & (~7);
  }

  offset = (mv_y >> 3) * refStep + (mv_x >> 3); 

  VP8_MB_INTER_PREDICT(pMbData + 16 + 4*dataStep + 4,   dataStep,
                       ref_u + 4*refStep  + 4 + offset, refStep,
                       dst_u + 4*dstStep + 4, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  VP8_MB_INTER_PREDICT(pMbData + (8+4)*dataStep + 16 + 4, dataStep,
                       ref_v + 4*refStep + 4 + offset, refStep,
                       dst_v + 4*dstStep + 4, dstStep,
                       4, 4,
                       mv_x&7, mv_y&7,
                       filter_type)

  return;
} // VP8VideoDecoder::PredictMbInter8x8()


void VP8VideoDecoder::PredictMbInter4x4(Ipp16s* pMbData, Ipp32u dataStep,
                                        Ipp32s  mb_row,  Ipp32s mb_col)
{
  Ipp32u i;
  Ipp32u j;
  Ipp32s offset;

  Ipp8u ref_indx;

  Ipp8u filter_type = m_FrameInfo.interpolationFlags & 1; //0 - 6TAP, 1 - bilinear

  Ipp32s mv_x;
  Ipp32s mv_y;

  vp8_MbInfo* currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

  ref_indx = m_RefFrameIndx[currMb->refFrame];

  // predict Y
  Ipp32u refStep = m_FrameData[ref_indx].step_y;
  Ipp8u* ref_y   = m_FrameData[ref_indx].data_y + refStep*16*mb_row + 16*mb_col;

  Ipp32u dstStep = m_CurrFrame->step_y;
  Ipp8u* dst_y   = m_CurrFrame->data_y + dstStep*16*mb_row + 16*mb_col;

  for(i = 0; i < 4; i++)
  {
    for(j = 0; j < 4; j++)
    {
      mv_x = currMb->blockMV[i*4 + j].mvx;
      mv_y = currMb->blockMV[i*4 + j].mvy;

      offset = (mv_y >> 3) * refStep + (mv_x >> 3);

      VP8_MB_INTER_PREDICT(pMbData + i*4*dataStep + j*4,       dataStep,
                           ref_y + i*4*refStep + j*4 + offset, refStep,
                           dst_y + i*4*dstStep + j*4,          dstStep,
                           4, 4,
                           mv_x&7, mv_y&7,
                           filter_type)
    }
  }

  // predict U/V

  dstStep = m_CurrFrame->step_uv;
  refStep = m_FrameData[ref_indx].step_uv;

  Ipp8u* ref_u = m_FrameData[ref_indx].data_u +refStep*8*mb_row + 8*mb_col;
  Ipp8u* ref_v = m_FrameData[ref_indx].data_v + refStep*8*mb_row + 8*mb_col;

  Ipp8u* dst_u   = m_CurrFrame->data_u + dstStep*8*mb_row + 8*mb_col;
  Ipp8u* dst_v   = m_CurrFrame->data_v + dstStep*8*mb_row + 8*mb_col;

  for(i = 0; i < 2; i++)
  {
    for(j = 0; j < 2; j++)
    {
      GET_UV_MV_SPLIT(currMb->blockMV, 2*j + 2*4*i)

      if(m_FrameInfo.interpolationFlags & 2)
      {
        mv_x = mv_x & (~7);
        mv_y = mv_y & (~7);
      }

      offset = (mv_y >> 3) * refStep + (mv_x >> 3);

      VP8_MB_INTER_PREDICT(pMbData + 16 + 4*i*dataStep + 4*j,   dataStep,
                           ref_u + i*4*refStep + j*4 + offset , refStep,
                           dst_u + i*4*refStep + j*4,           dstStep,
                           4, 4,
                           mv_x&7, mv_y&7,
                           filter_type)

      VP8_MB_INTER_PREDICT(pMbData + 8*dataStep + 4*i*dataStep + 4*j + 16, dataStep,
                           ref_v + i*4*refStep + j*4 + offset,             refStep,
                           dst_v + i*4*refStep + j*4,                      dstStep,
                           4, 4,
                           mv_x&7, mv_y&7,
                           filter_type)
    }
  }

  return;
} // VP8VideoDecoder::PredictMbInter4x4()



} // namespace UMC
#endif // UMC_ENABLE_VP8_VIDEO_DECODER

