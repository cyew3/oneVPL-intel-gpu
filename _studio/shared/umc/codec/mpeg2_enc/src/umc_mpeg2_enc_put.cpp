//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include <math.h>
#include "ippi.h"
#include "umc_mpeg2_enc_defs.h"

using namespace UMC;
/////////////////////////////////////
#ifndef UMC_RESTRICTED_CODE
//#include "ippdefs.h"
//
//#define Saturate2047(x) (((x) > 2047) ? 2047 : ((x) < -2047) ? -2047 : (x))
//
//static
//Ipp32f DefaultIntraQuantMatrix[64]=
//{
//  0.125000f, 0.062500f, 0.052632f, 0.045455f, 0.038462f, 0.037037f, 0.034483f, 0.029412f,
//    0.062500f, 0.062500f, 0.045455f, 0.041667f, 0.037037f, 0.034483f, 0.029412f, 0.027027f,
//    0.052632f, 0.045455f, 0.038462f, 0.037037f, 0.034483f, 0.029412f, 0.029412f, 0.026316f,
//    0.045455f, 0.045455f, 0.038462f, 0.037037f, 0.034483f, 0.029412f, 0.027027f, 0.025000f,
//    0.045455f, 0.038462f, 0.037037f, 0.034483f, 0.031250f, 0.028571f, 0.025000f, 0.020833f,
//    0.038462f, 0.037037f, 0.034483f, 0.031250f, 0.028571f, 0.025000f, 0.020833f, 0.017241f,
//    0.038462f, 0.037037f, 0.034483f, 0.029412f, 0.026316f, 0.021739f, 0.017857f, 0.014493f,
//    0.037037f, 0.034483f, 0.028571f, 0.026316f, 0.021739f, 0.017857f, 0.014493f, 0.012048f
//};
//
//IppStatus new_ippiQuantIntra_MPEG2_16s_C1I(Ipp16s *pSrcDst,
//    Ipp32s QP,
//    const Ipp32f *pQPMatrix,
//    Ipp32s *pCount)
//  {
//    /* check error(s) */
//    if (0 == QP)
//      return ippStsDivByZeroErr;
//
//    // reset counter
//    *pCount = 0;
//
//    // there is no QP matrix
//    if (0 == pQPMatrix)
//      pQPMatrix = DefaultIntraQuantMatrix;
//
//    {
//      Ipp32u i;
//      Ipp32f invQuantValue;
//
//      // generate inverse QP value
//      invQuantValue = 16.0f / ((Ipp32f) QP);
//
//      // cycle over the block
//      for (i = 1; i < 64; i+= 1)
//      {
//        Ipp32f tmp;
//
//        // calculate the quantized value
//        tmp = (Ipp32f) pSrcDst[i];
//        tmp *= invQuantValue;
//        tmp *= pQPMatrix[i];
//
//        // saturate it
//        pSrcDst[i] = (Ipp16s)Saturate2047(((Ipp32s) tmp));
//        if(pSrcDst[i])
//          (*pCount)++;
//      }
//    }
//
//    return ippStsOk;
//
//  }
#endif // UMC_RESTRICTED_CODE
//////////////////////////////////////
/* zigzag scan order ISO/IEC 13818-2, 7.3, fig. 7-2 */
static Ipp32s ZigZagScan[64] =
{
   0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,12,19,26,33,40,48,41,34,27,20,13, 6, 7,14,21,28,
  35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};

/* alternate scan order ISO/IEC 13818-2, 7.3, fig. 7-3 */
static Ipp32s AlternateScan[64] =
{
    0,
    8,

    16,         24, 1,  9,              2,  10, 17,
    25,         32,         40,         48,         56,
    57,         49,             41,     33,             26,
    18,         3,              11,     4,              12,
    19,         27,             34,     42,             50,
    58,         35,             43,     51,             59,
    20,         28,         5,          13,         6,
    14,         21, 29, 36,             44, 52, 60,
    37,         45,                     53,
    61,         22,                     30,
    7,          15,                     23,
    31,         38,                     46,
    54,         62,                     39,
    47,         55,                     63
};

/* color index by block number */
const Ipp32s MPEG2VideoEncoderBase::color_index[12] = {
  0, 0, 0, 0, 1, 2, 1, 2, 1, 2, 1, 2
};

/* reset DC value ISO/IEC 13818-2, 7.2.1, Table 7-2. */
const Ipp32s MPEG2VideoEncoderBase::ResetTbl[4] =
{
  128, 256, 512, 1024
};

/* VL codes for coded_block_pattern ISO/IEC 13818-2, B.3, Table B-9. */
const MPEG2VideoEncoderBase::VLCode_8u MPEG2VideoEncoderBase::CBP_VLC_Tbl[64] =
{
  {0x01,9}, {0x0b,5}, {0x09,5}, {0x0d,6}, {0x0d,4}, {0x17,7}, {0x13,7}, {0x1f,8},
  {0x0c,4}, {0x16,7}, {0x12,7}, {0x1e,8}, {0x13,5}, {0x1b,8}, {0x17,8}, {0x13,8},
  {0x0b,4}, {0x15,7}, {0x11,7}, {0x1d,8}, {0x11,5}, {0x19,8}, {0x15,8}, {0x11,8},
  {0x0f,6}, {0x0f,8}, {0x0d,8}, {0x03,9}, {0x0f,5}, {0x0b,8}, {0x07,8}, {0x07,9},
  {0x0a,4}, {0x14,7}, {0x10,7}, {0x1c,8}, {0x0e,6}, {0x0e,8}, {0x0c,8}, {0x02,9},
  {0x10,5}, {0x18,8}, {0x14,8}, {0x10,8}, {0x0e,5}, {0x0a,8}, {0x06,8}, {0x06,9},
  {0x12,5}, {0x1a,8}, {0x16,8}, {0x12,8}, {0x0d,5}, {0x09,8}, {0x05,8}, {0x05,9},
  {0x0c,5}, {0x08,8}, {0x04,8}, {0x04,9}, {0x07,3}, {0x0a,5}, {0x08,5}, {0x0c,6}
};

/* VL codes for macroblock_address_increment ISO/IEC 13818-2, B.1, Table B-1. */
const MPEG2VideoEncoderBase::VLCode_8u MPEG2VideoEncoderBase::AddrIncrementTbl[35]=
{
  {0x00,0}, // forbidden value
  {0x01,1},
  {0x03,3},  {0x02,3},
  {0x03,4},  {0x02,4},
  {0x03,5},  {0x02,5},
  {0x07,7},  {0x06,7},
  {0x0b,8},  {0x0a,8},  {0x09,8},  {0x08,8},  {0x07,8},  {0x06,8},
  {0x17,10}, {0x16,10}, {0x15,10}, {0x14,10}, {0x13,10}, {0x12,10},
  {0x23,11}, {0x22,11}, {0x21,11}, {0x20,11}, {0x1f,11}, {0x1e,11},{0x1d,11}, {0x1c,11}, {0x1b,11}, {0x1a,11}, {0x19,11}, {0x18,11},
  {0x08,11} // macroblock_escape
};

/* VL codes for macroblock_type ISO/IEC 13818-2, B.2, Tables B-2, B-3, B-4. */
const MPEG2VideoEncoderBase::VLCode_8u MPEG2VideoEncoderBase::mbtypetab[3][32]=
{
  /* I */
  {
    {0x00,0}, {0x01,1}, {0x00,0}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},
    {0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},
    {0x00,0}, {0x01,2}, {0x00,0}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},
    {0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0}
  },
  /* P */
  {
    {0x00,0}, {0x03,5}, {0x01,2}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},
    {0x01,3}, {0x00,0}, {0x01,1}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},
    {0x00,0}, {0x01,6}, {0x01,5}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0},
    {0x00,0}, {0x00,0}, {0x02,5}, {0x00,0},{0x00,0}, {0x00,0}, {0x00,0}, {0x00,0}
  },
  /* B */
  {
    {0x00,0}, {0x03,5}, {0x00,0}, {0x00,0}, {0x02,3}, {0x00,0}, {0x03,3}, {0x00,0},
    {0x02,4}, {0x00,0}, {0x03,4}, {0x00,0}, {0x02,2}, {0x00,0}, {0x03,2}, {0x00,0},
    {0x00,0}, {0x01,6}, {0x00,0}, {0x00,0}, {0x00,0}, {0x00,0}, {0x02,6}, {0x00,0},
    {0x00,0}, {0x00,0}, {0x03,6}, {0x00,0}, {0x00,0}, {0x00,0}, {0x02,5}, {0x00,0}
  }
};

/* VL codes for motion_code+16 ISO/IEC 13818-2, B.4, Table B-10. */
const MPEG2VideoEncoderBase::VLCode_8u MPEG2VideoEncoderBase::MV_VLC_Tbl[33]=
{
  // negative motion_code
  {0x19,11}, {0x1b,11}, {0x1d,11}, {0x1f,11}, {0x21,11}, {0x23,11},
  {0x13,10}, {0x15,10}, {0x17,10},
  {0x07,8},  {0x09,8},  {0x0b,8},
  {0x07,7},
  {0x03,5},
  {0x03,4},
  {0x03,3},
  // zero motion_code
  {0x01,1},
  // positive motion_code
  {0x02,3},
  {0x02,4},
  {0x02,5},
  {0x06,7},
  {0x0a,8},  {0x08,8},  {0x06,8},
  {0x16,10}, {0x14,10}, {0x12,10},
  {0x22,11}, {0x20,11}, {0x1e,11}, {0x1c,11}, {0x1a,11}, {0x18,11}
};

/* VL codes for dct_dc_size_luminance ISO/IEC 13818-2, B.5, Table B-12. */
const IppVCHuffmanSpec_32u MPEG2VideoEncoderBase::Y_DC_Tbl[12]=
{
  {0x0004,3}, {0x0000,2}, {0x0001,2}, {0x0005,3}, {0x0006,3}, {0x000e,4},
  {0x001e,5}, {0x003e,6}, {0x007e,7}, {0x00fe,8}, {0x01fe,9}, {0x01ff,9}
};

/* VL codes for dct_dc_size_chrominance ISO/IEC 13818-2, B.5, Table B-13. */
const IppVCHuffmanSpec_32u MPEG2VideoEncoderBase::Cr_DC_Tbl[12]=
{
  {0x0000,2}, {0x0001,2}, {0x0002,2}, {0x0006,3}, {0x000e,4}, {0x001e,5},
  {0x003e,6}, {0x007e,7}, {0x00fe,8}, {0x01fe,9}, {0x03fe,10},{0x03ff,10}
};

/* VL codes for DCT coefficients ISO/IEC 13818-2, B.5, Table B-14. */
const Ipp32s MPEG2VideoEncoderBase::dct_coeff_next_RL[] =
{
17,  /* max bits */
3 ,  /* total subtables */
9 ,5, 3,/* subtable sizes */

 0, /* 1-bit codes */
 1, /* 2-bit codes */
0x00000002, IPPVC_ENDOFBLOCK, IPPVC_ENDOFBLOCK,
 2 , /* 3-bit codes */
0x00000006, 0x00000000, 0x00000001, 0x00000007, 0x00000000, static_cast<Ipp32s>(0xffffffff),
 2 , /* 4-bit codes */
0x00000006, 0x00000001, 0x00000001, 0x00000007, 0x00000001, static_cast<Ipp32s>(0xffffffff),
 4 , /* 5-bit codes */
0x00000008, 0x00000000, 0x00000002, 0x00000009, 0x00000000, static_cast<Ipp32s>(0xfffffffe), 0x0000000a, 0x00000002, 0x00000001,
0x0000000b, 0x00000002, static_cast<Ipp32s>(0xffffffff),
 7 , /* 6-bit codes */
0x00000001, IPPVC_ESCAPE, IPPVC_ESCAPE,
0x0000000a, 0x00000000, 0x00000003, 0x0000000b, 0x00000000, static_cast<Ipp32s>(0xfffffffd), 0x0000000e, 0x00000003, 0x00000001,
0x0000000f, 0x00000003, static_cast<Ipp32s>(0xffffffff), 0x0000000c, 0x00000004, 0x00000001, 0x0000000d, 0x00000004, static_cast<Ipp32s>(0xffffffff),

 8 , /* 7-bit codes */
0x0000000c, 0x00000001, 0x00000002, 0x0000000d, 0x00000001, static_cast<Ipp32s>(0xfffffffe), 0x0000000e, 0x00000005, 0x00000001,
0x0000000f, 0x00000005, static_cast<Ipp32s>(0xffffffff), 0x0000000a, 0x00000006, 0x00000001, 0x0000000b, 0x00000006, static_cast<Ipp32s>(0xffffffff),
0x00000008, 0x00000007, 0x00000001, 0x00000009, 0x00000007, static_cast<Ipp32s>(0xffffffff),
 8 , /* 8-bit codes */
0x0000000c, 0x00000000, 0x00000004, 0x0000000d, 0x00000000, static_cast<Ipp32s>(0xfffffffc), 0x00000008, 0x00000002, 0x00000002,
0x00000009, 0x00000002, static_cast<Ipp32s>(0xfffffffe), 0x0000000e, 0x00000008, 0x00000001, 0x0000000f, 0x00000008, static_cast<Ipp32s>(0xffffffff),
0x0000000a, 0x00000009, 0x00000001, 0x0000000b, 0x00000009, static_cast<Ipp32s>(0xffffffff),
 16, /* 9-bit codes */
0x0000004c, 0x00000000, 0x00000005, 0x0000004d, 0x00000000, static_cast<Ipp32s>(0xfffffffb), 0x00000042, 0x00000000, 0x00000006,
0x00000043, 0x00000000, static_cast<Ipp32s>(0xfffffffa), 0x0000004a, 0x00000001, 0x00000003, 0x0000004b, 0x00000001, static_cast<Ipp32s>(0xfffffffd),
0x00000048, 0x00000003, 0x00000002, 0x00000049, 0x00000003, static_cast<Ipp32s>(0xfffffffe), 0x0000004e, 0x0000000a, 0x00000001,
0x0000004f, 0x0000000a, static_cast<Ipp32s>(0xffffffff), 0x00000046, 0x0000000b, 0x00000001, 0x00000047, 0x0000000b, static_cast<Ipp32s>(0xffffffff),
0x00000044, 0x0000000c, 0x00000001, 0x00000045, 0x0000000c, static_cast<Ipp32s>(0xffffffff), 0x00000040, 0x0000000d, 0x00000001,
0x00000041, 0x0000000d, static_cast<Ipp32s>(0xffffffff),
 0, /* 10-bit codes */
 16, /* 11-bit codes */
0x00000014, 0x00000000, 0x00000007, 0x00000015, 0x00000000, static_cast<Ipp32s>(0xfffffff9), 0x00000018, 0x00000001, 0x00000004,
0x00000019, 0x00000001, static_cast<Ipp32s>(0xfffffffc), 0x00000016, 0x00000002, 0x00000003, 0x00000017, 0x00000002, static_cast<Ipp32s>(0xfffffffd),
0x0000001e, 0x00000004, 0x00000002, 0x0000001f, 0x00000004, static_cast<Ipp32s>(0xfffffffe), 0x00000012, 0x00000005, 0x00000002,
0x00000013, 0x00000005, static_cast<Ipp32s>(0xfffffffe), 0x0000001c, 0x0000000e, 0x00000001, 0x0000001d, 0x0000000e, static_cast<Ipp32s>(0xffffffff),
0x0000001a, 0x0000000f, 0x00000001, 0x0000001b, 0x0000000f, static_cast<Ipp32s>(0xffffffff), 0x00000010, 0x00000010, 0x00000001,
0x00000011, 0x00000010, static_cast<Ipp32s>(0xffffffff),
 0, /* 12-bit codes */
 32, /* 13-bit codes */
0x0000003a, 0x00000000, 0x00000008, 0x0000003b, 0x00000000, static_cast<Ipp32s>(0xfffffff8), 0x00000030, 0x00000000, 0x00000009,
0x00000031, 0x00000000, static_cast<Ipp32s>(0xfffffff7), 0x00000026, 0x00000000, 0x0000000a, 0x00000027, 0x00000000, static_cast<Ipp32s>(0xfffffff6),
0x00000020, 0x00000000, 0x0000000b, 0x00000021, 0x00000000, static_cast<Ipp32s>(0xfffffff5), 0x00000036, 0x00000001, 0x00000005,
0x00000037, 0x00000001, static_cast<Ipp32s>(0xfffffffb), 0x00000028, 0x00000002, 0x00000004, 0x00000029, 0x00000002, static_cast<Ipp32s>(0xfffffffc),
0x00000038, 0x00000003, 0x00000003, 0x00000039, 0x00000003, static_cast<Ipp32s>(0xfffffffd), 0x00000024, 0x00000004, 0x00000003,
0x00000025, 0x00000004, static_cast<Ipp32s>(0xfffffffd), 0x0000003c, 0x00000006, 0x00000002, 0x0000003d, 0x00000006, static_cast<Ipp32s>(0xfffffffe),
0x0000002a, 0x00000007, 0x00000002, 0x0000002b, 0x00000007, static_cast<Ipp32s>(0xfffffffe), 0x00000022, 0x00000008, 0x00000002,
0x00000023, 0x00000008, static_cast<Ipp32s>(0xfffffffe), 0x0000003e, 0x00000011, 0x00000001, 0x0000003f, 0x00000011, static_cast<Ipp32s>(0xffffffff),
0x00000034, 0x00000012, 0x00000001, 0x00000035, 0x00000012, static_cast<Ipp32s>(0xffffffff), 0x00000032, 0x00000013, 0x00000001,
0x00000033, 0x00000013, static_cast<Ipp32s>(0xffffffff), 0x0000002e, 0x00000014, 0x00000001, 0x0000002f, 0x00000014, static_cast<Ipp32s>(0xffffffff),
0x0000002c, 0x00000015, 0x00000001, 0x0000002d, 0x00000015, static_cast<Ipp32s>(0xffffffff),
 32, /* 14-bit codes */
0x00000034, 0x00000000, 0x0000000c, 0x00000035, 0x00000000, static_cast<Ipp32s>(0xfffffff4), 0x00000032, 0x00000000, 0x0000000d,
0x00000033, 0x00000000, static_cast<Ipp32s>(0xfffffff3), 0x00000030, 0x00000000, 0x0000000e, 0x00000031, 0x00000000, static_cast<Ipp32s>(0xfffffff2),
0x0000002e, 0x00000000, 0x0000000f, 0x0000002f, 0x00000000, static_cast<Ipp32s>(0xfffffff1), 0x0000002c, 0x00000001, 0x00000006,
0x0000002d, 0x00000001, static_cast<Ipp32s>(0xfffffffa), 0x0000002a, 0x00000001, 0x00000007, 0x0000002b, 0x00000001, static_cast<Ipp32s>(0xfffffff9),
0x00000028, 0x00000002, 0x00000005, 0x00000029, 0x00000002, static_cast<Ipp32s>(0xfffffffb), 0x00000026, 0x00000003, 0x00000004,
0x00000027, 0x00000003, static_cast<Ipp32s>(0xfffffffc), 0x00000024, 0x00000005, 0x00000003, 0x00000025, 0x00000005, static_cast<Ipp32s>(0xfffffffd),
0x00000022, 0x00000009, 0x00000002, 0x00000023, 0x00000009, static_cast<Ipp32s>(0xfffffffe), 0x00000020, 0x0000000a, 0x00000002,
0x00000021, 0x0000000a, static_cast<Ipp32s>(0xfffffffe), 0x0000003e, 0x00000016, 0x00000001, 0x0000003f, 0x00000016, static_cast<Ipp32s>(0xffffffff),
0x0000003c, 0x00000017, 0x00000001, 0x0000003d, 0x00000017, static_cast<Ipp32s>(0xffffffff), 0x0000003a, 0x00000018, 0x00000001,
0x0000003b, 0x00000018, static_cast<Ipp32s>(0xffffffff), 0x00000038, 0x00000019, 0x00000001, 0x00000039, 0x00000019, static_cast<Ipp32s>(0xffffffff),
0x00000036, 0x0000001a, 0x00000001, 0x00000037, 0x0000001a, static_cast<Ipp32s>(0xffffffff),
 32, /* 15-bit codes */
0x0000003e, 0x00000000, 0x00000010, 0x0000003f, 0x00000000, static_cast<Ipp32s>(0xfffffff0), 0x0000003c, 0x00000000, 0x00000011,
0x0000003d, 0x00000000, static_cast<Ipp32s>(0xffffffef), 0x0000003a, 0x00000000, 0x00000012, 0x0000003b, 0x00000000, static_cast<Ipp32s>(0xffffffee),
0x00000038, 0x00000000, 0x00000013, 0x00000039, 0x00000000, static_cast<Ipp32s>(0xffffffed), 0x00000036, 0x00000000, 0x00000014,
0x00000037, 0x00000000, static_cast<Ipp32s>(0xffffffec), 0x00000034, 0x00000000, 0x00000015, 0x00000035, 0x00000000, static_cast<Ipp32s>(0xffffffeb),
0x00000032, 0x00000000, 0x00000016, 0x00000033, 0x00000000, static_cast<Ipp32s>(0xffffffea), 0x00000030, 0x00000000, 0x00000017,
0x00000031, 0x00000000, static_cast<Ipp32s>(0xffffffe9), 0x0000002e, 0x00000000, 0x00000018, 0x0000002f, 0x00000000, static_cast<Ipp32s>(0xffffffe8),
0x0000002c, 0x00000000, 0x00000019, 0x0000002d, 0x00000000, static_cast<Ipp32s>(0xffffffe7), 0x0000002a, 0x00000000, 0x0000001a,
0x0000002b, 0x00000000, static_cast<Ipp32s>(0xffffffe6), 0x00000028, 0x00000000, 0x0000001b, 0x00000029, 0x00000000, static_cast<Ipp32s>(0xffffffe5),
0x00000026, 0x00000000, 0x0000001c, 0x00000027, 0x00000000, static_cast<Ipp32s>(0xffffffe4), 0x00000024, 0x00000000, 0x0000001d,
0x00000025, 0x00000000, static_cast<Ipp32s>(0xffffffe3), 0x00000022, 0x00000000, 0x0000001e, 0x00000023, 0x00000000, static_cast<Ipp32s>(0xffffffe2),
0x00000020, 0x00000000, 0x0000001f, 0x00000021, 0x00000000, static_cast<Ipp32s>(0xffffffe1),
 32, /* 16-bit codes */
0x00000030, 0x00000000, 0x00000020, 0x00000031, 0x00000000, static_cast<Ipp32s>(0xffffffe0), 0x0000002e, 0x00000000, 0x00000021,
0x0000002f, 0x00000000, static_cast<Ipp32s>(0xffffffdf), 0x0000002c, 0x00000000, 0x00000022, 0x0000002d, 0x00000000, static_cast<Ipp32s>(0xffffffde),
0x0000002a, 0x00000000, 0x00000023, 0x0000002b, 0x00000000, static_cast<Ipp32s>(0xffffffdd), 0x00000028, 0x00000000, 0x00000024,
0x00000029, 0x00000000, static_cast<Ipp32s>(0xffffffdc), 0x00000026, 0x00000000, 0x00000025, 0x00000027, 0x00000000, static_cast<Ipp32s>(0xffffffdb),
0x00000024, 0x00000000, 0x00000026, 0x00000025, 0x00000000, static_cast<Ipp32s>(0xffffffda), 0x00000022, 0x00000000, 0x00000027,
0x00000023, 0x00000000, static_cast<Ipp32s>(0xffffffd9), 0x00000020, 0x00000000, 0x00000028, 0x00000021, 0x00000000, static_cast<Ipp32s>(0xffffffd8),
0x0000003e, 0x00000001, 0x00000008, 0x0000003f, 0x00000001, static_cast<Ipp32s>(0xfffffff8), 0x0000003c, 0x00000001, 0x00000009,
0x0000003d, 0x00000001, static_cast<Ipp32s>(0xfffffff7), 0x0000003a, 0x00000001, 0x0000000a, 0x0000003b, 0x00000001, static_cast<Ipp32s>(0xfffffff6),
0x00000038, 0x00000001, 0x0000000b, 0x00000039, 0x00000001, static_cast<Ipp32s>(0xfffffff5), 0x00000036, 0x00000001, 0x0000000c,
0x00000037, 0x00000001, static_cast<Ipp32s>(0xfffffff4), 0x00000034, 0x00000001, 0x0000000d, 0x00000035, 0x00000001, static_cast<Ipp32s>(0xfffffff3),
0x00000032, 0x00000001, 0x0000000e, 0x00000033, 0x00000001, static_cast<Ipp32s>(0xfffffff2),
 32, /* 17-bit codes */
0x00000026, 0x00000001, 0x0000000f, 0x00000027, 0x00000001, static_cast<Ipp32s>(0xfffffff1), 0x00000024, 0x00000001, 0x00000010,
0x00000025, 0x00000001, static_cast<Ipp32s>(0xfffffff0), 0x00000022, 0x00000001, 0x00000011, 0x00000023, 0x00000001, static_cast<Ipp32s>(0xffffffef),
0x00000020, 0x00000001, 0x00000012, 0x00000021, 0x00000001, static_cast<Ipp32s>(0xffffffee), 0x00000028, 0x00000006, 0x00000003,
0x00000029, 0x00000006, static_cast<Ipp32s>(0xfffffffd), 0x00000034, 0x0000000b, 0x00000002, 0x00000035, 0x0000000b, static_cast<Ipp32s>(0xfffffffe),
0x00000032, 0x0000000c, 0x00000002, 0x00000033, 0x0000000c, static_cast<Ipp32s>(0xfffffffe), 0x00000030, 0x0000000d, 0x00000002,
0x00000031, 0x0000000d, static_cast<Ipp32s>(0xfffffffe), 0x0000002e, 0x0000000e, 0x00000002, 0x0000002f, 0x0000000e, static_cast<Ipp32s>(0xfffffffe),
0x0000002c, 0x0000000f, 0x00000002, 0x0000002d, 0x0000000f, static_cast<Ipp32s>(0xfffffffe), 0x0000002a, 0x00000010, 0x00000002,
0x0000002b, 0x00000010, static_cast<Ipp32s>(0xfffffffe), 0x0000003e, 0x0000001b, 0x00000001, 0x0000003f, 0x0000001b, static_cast<Ipp32s>(0xffffffff),
0x0000003c, 0x0000001c, 0x00000001, 0x0000003d, 0x0000001c, static_cast<Ipp32s>(0xffffffff), 0x0000003a, 0x0000001d, 0x00000001,
0x0000003b, 0x0000001d, static_cast<Ipp32s>(0xffffffff), 0x00000038, 0x0000001e, 0x00000001, 0x00000039, 0x0000001e, static_cast<Ipp32s>(0xffffffff),
0x00000036, 0x0000001f, 0x00000001, 0x00000037, 0x0000001f, static_cast<Ipp32s>(0xffffffff),
-1 /* end of table */
};

/* VL codes for DCT coefficients ISO/IEC 13818-2, B.5, Table B-15. */
const Ipp32s MPEG2VideoEncoderBase::Table15[] =
{

17, /* max bits */
3 ,  /* total subtables */
9 ,5, 3,/* subtable sizes */

 0, /* 1-bit codes */
 0, /* 2-bit codes */
 2 , /* 3-bit codes */
0x00000004, 0x00000000, 0x00000001, 0x00000005, 0x00000000, static_cast<Ipp32s>(0xffffffff),
 5 , /* 4-bit codes */
0x00000004, 0x00000001, 0x00000001, 0x00000005, 0x00000001, static_cast<Ipp32s>(0xffffffff), 0x00000006, IPPVC_ENDOFBLOCK, IPPVC_ENDOFBLOCK,
0x0000000c, 0x00000000, 0x00000002, 0x0000000d, 0x00000000, static_cast<Ipp32s>(0xfffffffe),
 2 , /* 5-bit codes */
0x0000000e, 0x00000000, 0x00000003, 0x0000000f, 0x00000000, static_cast<Ipp32s>(0xfffffffd),
 11, /* 6-bit codes */
0x00000001, IPPVC_ESCAPE, IPPVC_ESCAPE,
0x0000000a, 0x00000002, 0x00000001, 0x0000000b, 0x00000002, static_cast<Ipp32s>(0xffffffff), 0x0000000e, 0x00000003, 0x00000001,
0x0000000f, 0x00000003, static_cast<Ipp32s>(0xffffffff), 0x0000000c, 0x00000001, 0x00000002, 0x0000000d, 0x00000001, static_cast<Ipp32s>(0xfffffffe),
0x00000038, 0x00000000, 0x00000004, 0x00000039, 0x00000000, static_cast<Ipp32s>(0xfffffffc), 0x0000003a, 0x00000000, 0x00000005,
0x0000003b, 0x00000000, static_cast<Ipp32s>(0xfffffffb),
 8 , /* 7-bit codes */
0x0000000c, 0x00000004, 0x00000001, 0x0000000d, 0x00000004, static_cast<Ipp32s>(0xffffffff), 0x0000000e, 0x00000005, 0x00000001,
0x0000000f, 0x00000005, static_cast<Ipp32s>(0xffffffff), 0x0000000a, 0x00000000, 0x00000006, 0x0000000b, 0x00000000, static_cast<Ipp32s>(0xfffffffa),
0x00000008, 0x00000000, 0x00000007, 0x00000009, 0x00000000, static_cast<Ipp32s>(0xfffffff9),
 18, /* 8-bit codes */
0x0000000c, 0x00000006, 0x00000001, 0x0000000d, 0x00000006, static_cast<Ipp32s>(0xffffffff), 0x00000008, 0x00000007, 0x00000001,
0x00000009, 0x00000007, static_cast<Ipp32s>(0xffffffff), 0x0000000e, 0x00000002, 0x00000002, 0x0000000f, 0x00000002, static_cast<Ipp32s>(0xfffffffe),
0x0000000a, 0x00000008, 0x00000001, 0x0000000b, 0x00000008, static_cast<Ipp32s>(0xffffffff), 0x000000f0, 0x00000009, 0x00000001,
0x000000f1, 0x00000009, static_cast<Ipp32s>(0xffffffff), 0x000000f2, 0x00000001, 0x00000003, 0x000000f3, 0x00000001, static_cast<Ipp32s>(0xfffffffd),
0x000000f4, 0x0000000a, 0x00000001, 0x000000f5, 0x0000000a, static_cast<Ipp32s>(0xffffffff), 0x000000f6, 0x00000000, 0x00000008,
0x000000f7, 0x00000000, static_cast<Ipp32s>(0xfffffff8), 0x000000f8, 0x00000000, 0x00000009, 0x000000f9, 0x00000000, static_cast<Ipp32s>(0xfffffff7),

 28, /* 9-bit codes */
0x0000004c, 0x00000003, 0x00000002, 0x0000004d, 0x00000003, static_cast<Ipp32s>(0xfffffffe), 0x00000042, 0x0000000b, 0x00000001,
0x00000043, 0x0000000b, static_cast<Ipp32s>(0xffffffff), 0x0000004a, 0x0000000c, 0x00000001, 0x0000004b, 0x0000000c, static_cast<Ipp32s>(0xffffffff),
0x00000048, 0x0000000d, 0x00000001, 0x00000049, 0x0000000d, static_cast<Ipp32s>(0xffffffff), 0x0000004e, 0x00000001, 0x00000004,
0x0000004f, 0x00000001, static_cast<Ipp32s>(0xfffffffc), 0x000001f8, 0x00000002, 0x00000003, 0x000001f9, 0x00000002, static_cast<Ipp32s>(0xfffffffd),
0x000001fa, 0x00000004, 0x00000002, 0x000001fb, 0x00000004, static_cast<Ipp32s>(0xfffffffe), 0x00000046, 0x00000000, 0x0000000a,
0x00000047, 0x00000000, static_cast<Ipp32s>(0xfffffff6), 0x00000044, 0x00000000, 0x0000000b, 0x00000045, 0x00000000, static_cast<Ipp32s>(0xfffffff5),
0x00000040, 0x00000001, 0x00000005, 0x00000041, 0x00000001, static_cast<Ipp32s>(0xfffffffb), 0x000001f4, 0x00000000, 0x0000000c,
0x000001f5, 0x00000000, static_cast<Ipp32s>(0xfffffff4), 0x000001f6, 0x00000000, 0x0000000d, 0x000001f7, 0x00000000, static_cast<Ipp32s>(0xfffffff3),
0x000001fc, 0x00000000, 0x0000000e, 0x000001fd, 0x00000000, static_cast<Ipp32s>(0xfffffff2), 0x000001fe, 0x00000000, 0x0000000f,
0x000001ff, 0x00000000, static_cast<Ipp32s>(0xfffffff1),
 6 , /* 10-bit codes */
0x00000008, 0x00000005, 0x00000002, 0x00000009, 0x00000005, static_cast<Ipp32s>(0xfffffffe), 0x0000000a, 0x0000000e, 0x00000001,
0x0000000b, 0x0000000e, static_cast<Ipp32s>(0xffffffff), 0x0000000e, 0x0000000f, 0x00000001, 0x0000000f, 0x0000000f, static_cast<Ipp32s>(0xffffffff),

 4 , /* 11-bit codes */
0x0000001a, 0x00000010, 0x00000001, 0x0000001b, 0x00000010, static_cast<Ipp32s>(0xffffffff), 0x00000018, 0x00000002, 0x00000004,
0x00000019, 0x00000002, static_cast<Ipp32s>(0xfffffffc),
 0, /* 12-bit codes */
 20, /* 13-bit codes */
0x00000038, 0x00000003, 0x00000003, 0x00000039, 0x00000003, static_cast<Ipp32s>(0xfffffffd), 0x00000024, 0x00000004, 0x00000003,
0x00000025, 0x00000004, static_cast<Ipp32s>(0xfffffffd), 0x0000003c, 0x00000006, 0x00000002, 0x0000003d, 0x00000006, static_cast<Ipp32s>(0xfffffffe),
0x0000002a, 0x00000007, 0x00000002, 0x0000002b, 0x00000007, static_cast<Ipp32s>(0xfffffffe), 0x00000022, 0x00000008, 0x00000002,
0x00000023, 0x00000008, static_cast<Ipp32s>(0xfffffffe), 0x0000003e, 0x00000011, 0x00000001, 0x0000003f, 0x00000011, static_cast<Ipp32s>(0xffffffff),
0x00000034, 0x00000012, 0x00000001, 0x00000035, 0x00000012, static_cast<Ipp32s>(0xffffffff), 0x00000032, 0x00000013, 0x00000001,
0x00000033, 0x00000013, static_cast<Ipp32s>(0xffffffff), 0x0000002e, 0x00000014, 0x00000001, 0x0000002f, 0x00000014, static_cast<Ipp32s>(0xffffffff),
0x0000002c, 0x00000015, 0x00000001, 0x0000002d, 0x00000015, static_cast<Ipp32s>(0xffffffff),
 24, /* 14-bit codes */
0x0000002c, 0x00000001, 0x00000006, 0x0000002d, 0x00000001, static_cast<Ipp32s>(0xfffffffa), 0x0000002a, 0x00000001, 0x00000007,
0x0000002b, 0x00000001, static_cast<Ipp32s>(0xfffffff9), 0x00000028, 0x00000002, 0x00000005, 0x00000029, 0x00000002, static_cast<Ipp32s>(0xfffffffb),
0x00000026, 0x00000003, 0x00000004, 0x00000027, 0x00000003, static_cast<Ipp32s>(0xfffffffc), 0x00000024, 0x00000005, 0x00000003,
0x00000025, 0x00000005, static_cast<Ipp32s>(0xfffffffd), 0x00000022, 0x00000009, 0x00000002, 0x00000023, 0x00000009, static_cast<Ipp32s>(0xfffffffe),
0x00000020, 0x0000000a, 0x00000002, 0x00000021, 0x0000000a, static_cast<Ipp32s>(0xfffffffe), 0x0000003e, 0x00000016, 0x00000001,
0x0000003f, 0x00000016, static_cast<Ipp32s>(0xffffffff), 0x0000003c, 0x00000017, 0x00000001, 0x0000003d, 0x00000017, static_cast<Ipp32s>(0xffffffff),
0x0000003a, 0x00000018, 0x00000001, 0x0000003b, 0x00000018, static_cast<Ipp32s>(0xffffffff), 0x00000038, 0x00000019, 0x00000001,
0x00000039, 0x00000019, static_cast<Ipp32s>(0xffffffff), 0x00000036, 0x0000001a, 0x00000001, 0x00000037, 0x0000001a, static_cast<Ipp32s>(0xffffffff),

 32, /* 15-bit codes */
0x0000003e, 0x00000000, 0x00000010, 0x0000003f, 0x00000000, static_cast<Ipp32s>(0xfffffff0), 0x0000003c, 0x00000000, 0x00000011,
0x0000003d, 0x00000000, static_cast<Ipp32s>(0xffffffef), 0x0000003a, 0x00000000, 0x00000012, 0x0000003b, 0x00000000, static_cast<Ipp32s>(0xffffffee),
0x00000038, 0x00000000, 0x00000013, 0x00000039, 0x00000000, static_cast<Ipp32s>(0xffffffed), 0x00000036, 0x00000000, 0x00000014,
0x00000037, 0x00000000, static_cast<Ipp32s>(0xffffffec), 0x00000034, 0x00000000, 0x00000015, 0x00000035, 0x00000000, static_cast<Ipp32s>(0xffffffeb),
0x00000032, 0x00000000, 0x00000016, 0x00000033, 0x00000000, static_cast<Ipp32s>(0xffffffea), 0x00000030, 0x00000000, 0x00000017,
0x00000031, 0x00000000, static_cast<Ipp32s>(0xffffffe9), 0x0000002e, 0x00000000, 0x00000018, 0x0000002f, 0x00000000, static_cast<Ipp32s>(0xffffffe8),
0x0000002c, 0x00000000, 0x00000019, 0x0000002d, 0x00000000, static_cast<Ipp32s>(0xffffffe7), 0x0000002a, 0x00000000, 0x0000001a,
0x0000002b, 0x00000000, static_cast<Ipp32s>(0xffffffe6), 0x00000028, 0x00000000, 0x0000001b, 0x00000029, 0x00000000, static_cast<Ipp32s>(0xffffffe5),
0x00000026, 0x00000000, 0x0000001c, 0x00000027, 0x00000000, static_cast<Ipp32s>(0xffffffe4), 0x00000024, 0x00000000, 0x0000001d,
0x00000025, 0x00000000, static_cast<Ipp32s>(0xffffffe3), 0x00000022, 0x00000000, 0x0000001e, 0x00000023, 0x00000000, static_cast<Ipp32s>(0xffffffe2),
0x00000020, 0x00000000, 0x0000001f, 0x00000021, 0x00000000, static_cast<Ipp32s>(0xffffffe1),
 32, /* 16-bit codes */
0x00000030, 0x00000000, 0x00000020, 0x00000031, 0x00000000, static_cast<Ipp32s>(0xffffffe0), 0x0000002e, 0x00000000, 0x00000021,
0x0000002f, 0x00000000, static_cast<Ipp32s>(0xffffffdf), 0x0000002c, 0x00000000, 0x00000022, 0x0000002d, 0x00000000, static_cast<Ipp32s>(0xffffffde),
0x0000002a, 0x00000000, 0x00000023, 0x0000002b, 0x00000000, static_cast<Ipp32s>(0xffffffdd), 0x00000028, 0x00000000, 0x00000024,
0x00000029, 0x00000000, static_cast<Ipp32s>(0xffffffdc), 0x00000026, 0x00000000, 0x00000025, 0x00000027, 0x00000000, static_cast<Ipp32s>(0xffffffdb),
0x00000024, 0x00000000, 0x00000026, 0x00000025, 0x00000000, static_cast<Ipp32s>(0xffffffda), 0x00000022, 0x00000000, 0x00000027,
0x00000023, 0x00000000, static_cast<Ipp32s>(0xffffffd9), 0x00000020, 0x00000000, 0x00000028, 0x00000021, 0x00000000, static_cast<Ipp32s>(0xffffffd8),
0x0000003e, 0x00000001, 0x00000008, 0x0000003f, 0x00000001, static_cast<Ipp32s>(0xfffffff8), 0x0000003c, 0x00000001, 0x00000009,
0x0000003d, 0x00000001, static_cast<Ipp32s>(0xfffffff7), 0x0000003a, 0x00000001, 0x0000000a, 0x0000003b, 0x00000001, static_cast<Ipp32s>(0xfffffff6),
0x00000038, 0x00000001, 0x0000000b, 0x00000039, 0x00000001, static_cast<Ipp32s>(0xfffffff5), 0x00000036, 0x00000001, 0x0000000c,
0x00000037, 0x00000001, static_cast<Ipp32s>(0xfffffff4), 0x00000034, 0x00000001, 0x0000000d, 0x00000035, 0x00000001, static_cast<Ipp32s>(0xfffffff3),
0x00000032, 0x00000001, 0x0000000e, 0x00000033, 0x00000001, static_cast<Ipp32s>(0xfffffff2),
 32, /* 17-bit codes */
0x00000026, 0x00000001, 0x0000000f, 0x00000027, 0x00000001, static_cast<Ipp32s>(0xfffffff1), 0x00000024, 0x00000001, 0x00000010,
0x00000025, 0x00000001, static_cast<Ipp32s>(0xfffffff0), 0x00000022, 0x00000001, 0x00000011, 0x00000023, 0x00000001, static_cast<Ipp32s>(0xffffffef),
0x00000020, 0x00000001, 0x00000012, 0x00000021, 0x00000001, static_cast<Ipp32s>(0xffffffee), 0x00000028, 0x00000006, 0x00000003,
0x00000029, 0x00000006, static_cast<Ipp32s>(0xfffffffd), 0x00000034, 0x0000000b, 0x00000002, 0x00000035, 0x0000000b, static_cast<Ipp32s>(0xfffffffe),
0x00000032, 0x0000000c, 0x00000002, 0x00000033, 0x0000000c, static_cast<Ipp32s>(0xfffffffe), 0x00000030, 0x0000000d, 0x00000002,
0x00000031, 0x0000000d, static_cast<Ipp32s>(0xfffffffe), 0x0000002e, 0x0000000e, 0x00000002, 0x0000002f, 0x0000000e, static_cast<Ipp32s>(0xfffffffe),
0x0000002c, 0x0000000f, 0x00000002, 0x0000002d, 0x0000000f, static_cast<Ipp32s>(0xfffffffe), 0x0000002a, 0x00000010, 0x00000002,
0x0000002b, 0x00000010, static_cast<Ipp32s>(0xfffffffe), 0x0000003e, 0x0000001b, 0x00000001, 0x0000003f, 0x0000001b, static_cast<Ipp32s>(0xffffffff),
0x0000003c, 0x0000001c, 0x00000001, 0x0000003d, 0x0000001c, static_cast<Ipp32s>(0xffffffff), 0x0000003a, 0x0000001d, 0x00000001,
0x0000003b, 0x0000001d, static_cast<Ipp32s>(0xffffffff), 0x00000038, 0x0000001e, 0x00000001, 0x00000039, 0x0000001e, static_cast<Ipp32s>(0xffffffff),
0x00000036, 0x0000001f, 0x00000001, 0x00000037, 0x0000001f, static_cast<Ipp32s>(0xffffffff),
-1 /* end of table */
};

const IppiPoint MPEG2VideoEncoderBase::MV_ZERO = {0, 0};

Ipp32u MPEG2VideoEncoderBase::PutSequenceHeader2Mem(Ipp32u * buf, Ipp32u len)
{
    using namespace mpeg2_helpers;
    bitBuffer bBuf = { 32, static_cast<mfxI32>(len), (Ipp8u *)buf, buf };
    Ipp32s i=0;

    putStartCode(bBuf,SEQ_START_CODE);                // sequence_header_code
    putBits(bBuf, (encodeInfo.info.clip_info.width & 0xfff), 12); // horizontal_size_value
    putBits(bBuf, (encodeInfo.info.clip_info.height & 0xfff), 12); // vertical_size_value
    putBits(bBuf, aspectRatio_code, 4);      // aspect_ratio_information
    putBits(bBuf, frame_rate_code, 4);                  // frame_rate_code
    putBits(bBuf, ((Ipp32s)ceil(encodeInfo.info.bitrate / 400.0) & 0x3ffff), 18); // bit_rate_value
    putBits(bBuf, 1, 1);                                // marker_bit
    putBits(bBuf, (encodeInfo.VBV_BufferSize & 0x3ff), 10); // vbv_buffer_size_value
    putBits(bBuf, 0, 1);                                // constrained_parameters_flag

    putBits(bBuf, encodeInfo.CustomIntraQMatrix, 1);    // load_intra_quantizer_matrix
    if( encodeInfo.CustomIntraQMatrix )
        for(i=0; i < 64; i++) // matrices are always downloaded in zig-zag order
            putBits(bBuf, IntraQMatrix[ZigZagScan[i]], 8);  // intra_quantizer_matrix

    putBits(bBuf, encodeInfo.CustomNonIntraQMatrix, 1); // load_non_intra_quantizer_matrix
    if( encodeInfo.CustomNonIntraQMatrix )
        for(i=0; i < 64; i++)
            putBits(bBuf, NonIntraQMatrix[ZigZagScan[i]], 8); // non_intra_quantizer_matrix

    FLUSH_BITSTREAM(bBuf.current_pointer, bBuf.bit_offset);

    return Ipp32u((Ipp8u *)bBuf.current_pointer - (Ipp8u *)bBuf.start_pointer) + (32 - bBuf.bit_offset + 7) / 8;
}

Ipp32u MPEG2VideoEncoderBase::PutSequenceExt2Mem(Ipp32u * buf, Ipp32u len)
{
    using namespace mpeg2_helpers;
    bitBuffer bBuf = { 32, static_cast<mfxI32>(len), (Ipp8u *)buf, buf };

    Ipp32s chroma_format_code;
    Ipp32s prog_seq = (encodeInfo.info.interlace_type == PROGRESSIVE) ? 1 : 0;
    switch(encodeInfo.info.color_format) {
        case NV12:   chroma_format_code = 1; break;
        case YUV420: chroma_format_code = 1; break;
        case YUV422: chroma_format_code = 2; break;
        case YUV444: chroma_format_code = 3; break;
        default:     chroma_format_code = 1;
    }
    putStartCode(bBuf, EXT_START_CODE);               // extension_start_code
    putBits(bBuf, SEQ_ID, 4);                          // extension_start_code_identifier
    putBits(bBuf, ( encodeInfo.profile << 4 ) | encodeInfo.level, 8);    // profile_and_level_indication
    putBits(bBuf, prog_seq, 1);                        // progressive sequence
    putBits(bBuf, chroma_format_code, 2);              // chroma_format
    putBits(bBuf, encodeInfo.info.clip_info.width >> 12, 2);      // horizontal_size_extension
    putBits(bBuf, encodeInfo.info.clip_info.height >> 12, 2);     // vertical_size_extension
    putBits(bBuf, ((Ipp32s)ceil(encodeInfo.info.bitrate / 400.0)) >> 18, 12);    // bit_rate_extension
    putBits(bBuf, 1, 1);                               // marker_bit
    putBits(bBuf, encodeInfo.VBV_BufferSize >> 10, 8); // vbv_buffer_size_extension
    putBits(bBuf, 0, 1);                               // low_delay  (not implemented)
    putBits(bBuf, frame_rate_extension_n, 2);
    putBits(bBuf, frame_rate_extension_d, 5);

    FLUSH_BITSTREAM(bBuf.current_pointer, bBuf.bit_offset);

    return Ipp32u((Ipp8u *)bBuf.current_pointer - (Ipp8u *)bBuf.start_pointer) + (32 - bBuf.bit_offset + 7) / 8;
}

Ipp32u MPEG2VideoEncoderBase::PutSequenceDisplayExt2Mem(Ipp32u * buf, Ipp32u len)
{
    using namespace mpeg2_helpers;
    bitBuffer bBuf = { 32, static_cast<mfxI32>(len), (Ipp8u *)buf, buf };

    putStartCode(bBuf, EXT_START_CODE); // extension_start_code
    putBits(bBuf, DISP_ID, 4);           // extension_start_code_identifier
    putBits(bBuf, encodeInfo.video_format, 3);
    putBits(bBuf, encodeInfo.color_description, 1);

    if (encodeInfo.color_description)
    {
        putBits(bBuf, encodeInfo.color_primaries, 8);
        putBits(bBuf, encodeInfo.transfer_characteristics, 8);
        putBits(bBuf, encodeInfo.matrix_coefficients, 8);
    }

    putBits(bBuf, encodeInfo.info.clip_info.width, 14);  // display_horizontal_size
    putBits(bBuf, 1, 1);                                 // marker_bit
    putBits(bBuf, encodeInfo.info.clip_info.height, 14); // display_vertical_size

    FLUSH_BITSTREAM(bBuf.current_pointer, bBuf.bit_offset);

    return Ipp32u((Ipp8u *)bBuf.current_pointer - (Ipp8u *)bBuf.start_pointer) + (32 - bBuf.bit_offset + 7) / 8;
}

// generate sequence header (6.2.2.1, 6.3.3)
void MPEG2VideoEncoderBase::PutSequenceHeader()
{
  Ipp32s i=0;

  PUT_START_CODE(SEQ_START_CODE);                // sequence_header_code
  PUT_BITS((encodeInfo.info.clip_info.width & 0xfff), 12);  // horizontal_size_value
  PUT_BITS((encodeInfo.info.clip_info.height & 0xfff), 12); // vertical_size_value
  PUT_BITS(aspectRatio_code, 4);      // aspect_ratio_information
  PUT_BITS(frame_rate_code, 4);                  // frame_rate_code
  PUT_BITS(((Ipp32s)ceil(encodeInfo.info.bitrate / 400.0) & 0x3ffff), 18); // bit_rate_value
  PUT_BITS(1, 1);                                // marker_bit
  PUT_BITS((encodeInfo.VBV_BufferSize & 0x3ff), 10); // vbv_buffer_size_value
  PUT_BITS(0, 1);                                // constrained_parameters_flag

  PUT_BITS(encodeInfo.CustomIntraQMatrix, 1);    // load_intra_quantizer_matrix
  if( encodeInfo.CustomIntraQMatrix )
    for(i=0; i < 64; i++) // matrices are always downloaded in zig-zag order
      PUT_BITS(IntraQMatrix[ZigZagScan[i]], 8);  // intra_quantizer_matrix

  PUT_BITS(encodeInfo.CustomNonIntraQMatrix, 1); // load_non_intra_quantizer_matrix
  if( encodeInfo.CustomNonIntraQMatrix )
    for(i=0; i < 64; i++)
      PUT_BITS(NonIntraQMatrix[ZigZagScan[i]], 8); // non_intra_quantizer_matrix
}

// generate sequence extension (6.2.2.3, 6.3.5) header (MPEG-2 only)
void MPEG2VideoEncoderBase::PutSequenceExt()
{
  Ipp32s chroma_format_code;
  Ipp32s prog_seq = (encodeInfo.info.interlace_type == PROGRESSIVE) ? 1 : 0;
  switch(encodeInfo.info.color_format) {
      case NV12:   chroma_format_code = 1; break;
      case YUV420: chroma_format_code = 1; break;
      case YUV422: chroma_format_code = 2; break;
      case YUV444: chroma_format_code = 3; break;
      default:     chroma_format_code = 1;
  }
  PUT_START_CODE(EXT_START_CODE);               // extension_start_code
  PUT_BITS(SEQ_ID, 4);                          // extension_start_code_identifier
  PUT_BITS(( encodeInfo.profile << 4 ) | encodeInfo.level, 8);    // profile_and_level_indication
  PUT_BITS(prog_seq, 1);                        // progressive sequence
  PUT_BITS(chroma_format_code, 2);              // chroma_format
  PUT_BITS(encodeInfo.info.clip_info.width >> 12, 2);      // horizontal_size_extension
  PUT_BITS(encodeInfo.info.clip_info.height >> 12, 2);     // vertical_size_extension
  PUT_BITS(((Ipp32s)ceil(encodeInfo.info.bitrate / 400.0)) >> 18, 12);    // bit_rate_extension
  PUT_BITS(1, 1);                               // marker_bit
  PUT_BITS(encodeInfo.VBV_BufferSize >> 10, 8); // vbv_buffer_size_extension
  PUT_BITS(0, 1);                               // low_delay  (not implemented)
  PUT_BITS(frame_rate_extension_n, 2);
  PUT_BITS(frame_rate_extension_d, 5);
}

// generate sequence display extension (6.2.2.4, 6.3.6)
void MPEG2VideoEncoderBase::PutSequenceDisplayExt()
{
  PUT_START_CODE(EXT_START_CODE); // extension_start_code
  PUT_BITS(DISP_ID, 4);           // extension_start_code_identifier
  PUT_BITS(encodeInfo.video_format, 3);
  PUT_BITS(encodeInfo.color_description, 1);

  if (encodeInfo.color_description)
  {
      PUT_BITS(encodeInfo.color_primaries, 8);
      PUT_BITS(encodeInfo.transfer_characteristics, 8);
      PUT_BITS(encodeInfo.matrix_coefficients, 8);
  }

  PUT_BITS(encodeInfo.info.clip_info.width, 14);  // display_horizontal_size
  PUT_BITS(1, 1);                                 // marker_bit
  PUT_BITS(encodeInfo.info.clip_info.height, 14); // display_vertical_size
}

// put a zero terminated string as user data (6.2.2.2.2, 6.3.4.1)
void MPEG2VideoEncoderBase::PutUserData(Ipp32s part)
{
  Ipp8u* ptr=0;
  Ipp32s len=0, i=0;
  bool   bFirstStartCode = false;

  // select data by part, no standard rules
  if(part==0 && encodeInfo.idStr[0]) {
    ptr = (Ipp8u*)encodeInfo.idStr;
    len = (Ipp32s)strlen(encodeInfo.idStr);
  }
  else if(part==2 && encodeInfo.UserData != 0 && encodeInfo.UserDataLen > 0) {
    ptr = encodeInfo.UserData;
    len = encodeInfo.UserDataLen;
  }

  if(len==0) return;
  for(i=0; i<len-2; i++) { // stop len if start code happens
    if(!ptr[i] && !ptr[i+1] && ptr[i+2]==1) {
      // start code! - stop right before
      if (i == 0){
          bFirstStartCode = true;      
      }else{
        break;
      }
    }
  }
  if(i>=len-2)
    i = len;
  if(i==0) return;
  
  PUT_START_CODE(USER_START_CODE); // user_data_start_code
  len = i;
 
  if (bFirstStartCode)
  {
      if (!(i > 3 && ptr[3] == 0xb2))
          return;
      ptr +=4;
      len = len - 4;
  }
  for( i=0; i<len; i++ ) {
    PUT_BITS((Ipp8u)(ptr[i]), 8);
  }
}
void MPEG2VideoEncoderBase::PutSequenceHeader(Ipp8u *pHeader, Ipp32u len)
{
    PUT_ALIGNMENT(0);
    for (Ipp32u i=0; i<len; i++)
    {
        PUT_BITS((Ipp8u)(pHeader[i]), 8);    
    }
}

// generate group of pictures header (6.2.2.6, 6.3.9)
void MPEG2VideoEncoderBase::PutGOPHeader(Ipp32s Count )
{
  Ipp32s tc;

  PUT_START_CODE(GOP_START_CODE);
  tc = FrameToTimecode(Count);
  PUT_BITS(tc, 25);            // time_code
  //closed_gop = (Count && encodeInfo.IPDistance>1)?0:1;
  PUT_BITS(closed_gop, 1);     // closed_gop (all except first GOP are open)
  PUT_BITS(0, 1);              // broken_link
}

Ipp32s MPEG2VideoEncoderBase::FrameToTimecode(Ipp32s frame)
{
  Ipp32s fps, pict, sec, minute, hour, tc;

  fps = (Ipp32s)(encodeInfo.info.framerate + 0.5);
  pict = frame % fps;
  frame = (frame - pict) / fps;
  sec = frame % 60;
  frame = (frame - sec) / 60;
  minute = frame % 60;
  frame = (frame - minute) / 60;
  hour = frame % 24;
  tc = (hour<<19) | (minute<<13) | (1<<12) | (sec<<6) | pict;

  return tc;
}

// generate picture header (6.2.3, 6.3.9)
void MPEG2VideoEncoderBase::PutPictureHeader()
{
  PUT_START_CODE(PICTURE_START_CODE); // picture_start_code
  PUT_BITS((temporal_reference & 0x3ff), 10);       // temporal_reference
  PUT_BITS(picture_coding_type, 3);   // picture_coding_type
  PUT_BITS(vbv_delay, 16);            // vbv_delay

  if( picture_coding_type == MPEG2_P_PICTURE || picture_coding_type == MPEG2_B_PICTURE )
  {
    PUT_BITS(0, 1);                 // full_pel_forward_vector
    PUT_BITS(7, 3);                 // forward_f_code
  }

  if( picture_coding_type == MPEG2_B_PICTURE )
  {
    PUT_BITS(0, 1);                 // full_pel_backward_vector
    PUT_BITS(7, 3);                 // backward_f_code
  }

  PUT_BITS(0, 1);                     // extra_bit_picture
}

// generate picture coding extension (6.2.3.1, 6.3.11)
void MPEG2VideoEncoderBase::PutPictureCodingExt()
{
  Ipp32s chroma_420_type, actual_tff;
  PUT_START_CODE(EXT_START_CODE);     // extension_start_code
  PUT_BITS(CODING_ID, 4);             // extension_start_code_identifier
  if(mp_f_code == 0) {
    PUT_BITS(0xffff, 16);             // forward_horizontal_f_code
  } else {
    PUT_BITS(mp_f_code[0][0], 4);     // forward_horizontal_f_code
    PUT_BITS(mp_f_code[0][1], 4);     // forward_vertical_f_code
    PUT_BITS(mp_f_code[1][0], 4);     // backward_horizontal_f_code
    PUT_BITS(mp_f_code[1][1], 4);     // backward_vertical_f_code
  }
  PUT_BITS(intra_dc_precision, 2);    // encodeInfo.intra_dc_precision
  PUT_BITS(picture_structure, 2);     // picture_structure
  actual_tff = ((picture_structure == MPEG2_FRAME_PICTURE && (encodeInfo.info.interlace_type != PROGRESSIVE)) ||
    repeat_first_field);
  PUT_BITS((actual_tff) ? top_field_first : 0, 1);// top_field_first
  PUT_BITS((picture_structure == MPEG2_FRAME_PICTURE ) ? curr_frame_pred:0 , 1); // frame_pred_frame_dct
  PUT_BITS(0, 1);                     // concealment_motion_vectors (not implemented yet)
  PUT_BITS(q_scale_type, 1);          // q_scale_type
  PUT_BITS(curr_intra_vlc_format, 1); // intra_vlc_format
  PUT_BITS(curr_scan, 1);             // alternate_scan
  PUT_BITS(repeat_first_field, 1);    // repeat_first_field
  chroma_420_type = ((encodeInfo.info.color_format == YUV420 || encodeInfo.info.color_format == NV12) &&
                      progressive_frame) ? 1 : 0;
  PUT_BITS(chroma_420_type, 1);       // chroma_420_type
  PUT_BITS(progressive_frame, 1);    // progressive_frame
  PUT_BITS(0, 1);                     // composite_display_flag
  // composite display information not implemented yet
}

// generate sequence_end_code (6.2.2)
void MPEG2VideoEncoderBase::PutSequenceEnd()
{
  PUT_START_CODE(SEQ_END_CODE);
}

// slice header (6.2.4)
void MPEG2VideoEncoderBase::PutSliceHeader(Ipp32s RowNumber, Ipp32s numTh)
{
  if( encodeInfo.info.clip_info.height <= 2800 )
  {
    PUT_START_CODE_TH(numTh, SLICE_MIN_START + RowNumber); // slice_start_code
  }
  else
  {
    PUT_START_CODE_TH(numTh, SLICE_MIN_START + (RowNumber & 127)); // slice_start_code
    PUT_BITS_TH(RowNumber >> 7, 3); // slice_vertical_position_extension
  }

  PUT_BITS_TH((quantiser_scale_code<<1), (5+1)); // + extra_bit_slice
}

// put variable length code for macroblock_address_increment (6.3.17)
void MPEG2VideoEncoderBase::PutAddrIncrement(Ipp32s increment, Ipp32s numTh )
{
  while( increment > 33 )
  {
    PUT_BITS_TH(0x08, 11); // put macroblock_escape
    increment -= 33;
  }

  PUT_BITS_TH(AddrIncrementTbl[increment].code, AddrIncrementTbl[increment].len);
}

void MPEG2VideoEncoderBase::PutMV(Ipp32s delta, Ipp32s f_code, Ipp32s numTh)
{
  Ipp32s f, r_size, low, high, range , temp, motion_code, motion_residual;
  Ipp32s tpos; // LUT position

  r_size = f_code - 1;
  f = 1<<r_size;

  low   = -16*f;
  high  = 16*f - 1;
  range = 32*f;

  /* fold vector difference into [vmin...vmax] */
  if(delta > high)     delta -= range;
  else if(delta < low) delta += range;

  /* check value */
  mpeg2_assert(delta >= low);
  mpeg2_assert(delta <= high);

  if(f == 1 || delta == 0)
  {
    PUT_BITS_TH(MV_VLC_Tbl[16 + delta].code, MV_VLC_Tbl[16 + delta].len);
    return;
  }

  // present delta as motion_code and motion_residual
  if( delta < 0 )
  {
    temp = -delta + f - 1;
    motion_code = temp >> r_size;
    tpos = 16 - motion_code;
  }
  else // delta > 0
  {
    temp = delta + f - 1;
    motion_code = temp >> r_size;
    tpos = 16 + motion_code;
  }
  mpeg2_assert(motion_code > 0 && motion_code <= 16);
  // put variable length code for motion_code (6.3.16.3)
  // put fixed length code for motion_residual
  motion_residual = (MV_VLC_Tbl[tpos].code << r_size) + (temp & (f - 1));
  r_size += MV_VLC_Tbl[tpos].len;

  PUT_BITS_TH(motion_residual, r_size);
}

/* generate variable length code for other DCT coefficients (7.2.2)*/
static void mp2PutAC(Ipp32s numTh,
                     MPEG2VideoEncoderBase::threadSpecificData* threadSpec,
                     Ipp32s run,
                     Ipp32s signed_level,
                     IppVCHuffmanSpec_32s *ptab)
{
    Ipp32s level, code, len;
    Ipp32s maxRun = ptab[0] >> 20;
    Ipp32s addr;
    Ipp32s * table;
    Ipp32u val;

  if(run > maxRun)
  {
    /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
    /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
    signed_level &= (1 << 12) - 1;
    val = signed_level + (run<<12) + (1<<18);
    PUT_BITS_TH(val, 24 )
    return;
  }

  level = (signed_level < 0) ? -signed_level : signed_level;

  addr = ptab[run + 1];
  table = (Ipp32s*)((Ipp8s*)ptab + addr);
  if(level <= table[0])
  {
    len  = *(table + signed_level) & 0x1f;
    code = (*(table + signed_level) >> 16);
    PUT_BITS_TH(code, len )
  }
  else
  {
    /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
    /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
    signed_level &= (1 << 12) - 1;
    val = signed_level + (run<<12) + (1<<18);
    PUT_BITS_TH(val, 24 )
  }
}

static Status mp2PutIntraBlock(   Ipp32s numTh,
                                  MPEG2VideoEncoderBase::threadSpecificData* threadSpec,
                                  Ipp16s *block,
                                  Ipp32s *dc_dct_pred,
                                  IppVCHuffmanSpec_32u* DC_Tbl,
                                  IppVCHuffmanSpec_32s* AC_Tbl,
                                  Ipp32s *scan,
                                  Ipp32s EOBLen,
                                  Ipp32s EOBCode,
                                  Ipp32s count)
{
  Ipp32s n, m, dct_diff, run, signed_level;
  Ipp32s absval, size1;

  dct_diff = block[0] - *dc_dct_pred;
  *dc_dct_pred = block[0];

  /* generate variable length code for DC coefficient (7.2.1)*/
  if(dct_diff == 0) {
    PUT_BITS_TH(DC_Tbl[0].code, DC_Tbl[0].len )
  } else {
    m = dct_diff >> 31;
    absval = (dct_diff + m) ^ m;
    for (size1=1; absval >>= 1; size1++);

    /* generate VLC for dct_dc_size (Table B-12 or B-13)*/
    /* append fixed length code (dc_dct_differential)*/
    //absval = (DC_Tbl[size1].code << size1) - (m<<size1) + (dct_diff + m);
    absval = ((DC_Tbl[size1].code - m) << size1) + (dct_diff + m);
    n  = DC_Tbl[size1].len + size1;
    PUT_BITS_TH(absval, n )
  }

  run = 0;
  m = 0;

  for(n=1; (m < count) && (n < 64) ; n++)
  {
//if(n>63)
//break;
    signed_level = block[scan[n]];
    if( signed_level )
    {
      mp2PutAC(numTh, threadSpec, run, signed_level, AC_Tbl);
      m++;
      run = 0;
    }
    else
      run++;
  }
  PUT_BITS_TH( EOBCode, EOBLen )

  return UMC_OK;
}

static Status mp2PutNonIntraBlock(   Ipp32s numTh,
                                     MPEG2VideoEncoderBase::threadSpecificData* threadSpec,
                                     Ipp16s *block,
                                     IppVCHuffmanSpec_32s *AC_Tbl,
                                     Ipp32s *scan,
                                     Ipp32s EOBLen,
                                     Ipp32s EOBCode,
                                     Ipp32s count)
{
  Ipp32s n, m, run, signed_level;

  run = 0;
  m = 0;

  signed_level = block[0];
  if( signed_level)
  {
    if (signed_level == 1 || signed_level == -1) {
      //Ipp32s tmp = (signed_level == 1) ? 2 : 3;
      Ipp32s tmp = 2 + (signed_level==-1);
      PUT_BITS_TH(tmp, 2)
    } else {
      mp2PutAC(numTh,threadSpec, 0, signed_level, AC_Tbl);
    }
    m++;
  }
  else run++;

  for(n=1; (m < count) && (n < 64) ; n++)
  {
    signed_level = block[scan[n]];
    

    if( signed_level )
    {
      mp2PutAC(numTh,threadSpec, run, signed_level, AC_Tbl);
      m++;
      run = 0;
    }
    else
      run++;
  }

  PUT_BITS_TH(EOBCode, EOBLen )// End of Block

  return UMC_OK;
}

void MPEG2VideoEncoderBase::PutIntraBlock(Ipp16s* block, Ipp32s* dc_dct_pred, const IppVCHuffmanSpec_32u* DC_Table,Ipp32s count, Ipp32s numTh)
{
  Ipp32s EOBLen, EOBCode;
  IppVCHuffmanSpec_32s* AC_Tbl;
  Ipp32s *scan = curr_scan ? AlternateScan : ZigZagScan;


  if (curr_intra_vlc_format) {
    EOBCode = 6;
    EOBLen  = 4; // (Table B-15)
    AC_Tbl = vlcTableB15;
  } else {
    EOBCode = 2;
    EOBLen  = 2; // (Table B-14)
    AC_Tbl = vlcTableB5c_e;
  }

  mp2PutIntraBlock(numTh,threadSpec,block, dc_dct_pred, (IppVCHuffmanSpec_32u*)DC_Table, AC_Tbl, scan, EOBLen, EOBCode, count);

}

void MPEG2VideoEncoderBase::PutNonIntraBlock(Ipp16s* block, Ipp32s count, Ipp32s numTh)
{
  Ipp32s *scan = curr_scan ? AlternateScan : ZigZagScan;
  Ipp32s maxCoeff = count;
  Ipp32s numNonZero = 0;
  Ipp32s numCoeff = 0;

  switch(nLimitedMBMode)
  {
  case 1:
    maxCoeff = 3;
    break;
  case 2:        
    maxCoeff = 1;
    break;
  case 3:
    maxCoeff = 1;
    break;
  }

  if (nLimitedMBMode)
  {
    for(int n = 0; (n < 64) && (numNonZero < count); n++)
    {
       if (block[scan[n]] != 0)
       {
         numNonZero ++;
         if (numNonZero > maxCoeff || (n > 16 && numCoeff != 0))
         {
            block[scan[n]] = 0;
         }
         else
         {
            numCoeff++;
         }
       } 
     }
     count =  numCoeff;
  }
  mp2PutNonIntraBlock(numTh, threadSpec, block, vlcTableB5c_e, scan, 2, 2, count);
}

void MPEG2VideoEncoderBase::PutIntraMacroBlock(Ipp32s numTh, Ipp32s k, const Ipp8u *BlockSrc[3], Ipp8u *BlockRec[3], Ipp32s *dc_dct_pred)
{
  IppiSize roi = {8, 8};
  Ipp16s *pMBlock = threadSpec[numTh].pMBlock;
  Ipp32s Count[12] = {0};
  Ipp32s stride_src[3] = {0};
  Ipp32s stride_rec[3] = {0};
  Ipp32s *block_offset_src = 0;
  Ipp32s *block_offset_rec = 0;
  Ipp32s intra_dc_shift = 3 - intra_dc_precision;
  Ipp32s half_intra_dc = (1 << intra_dc_shift) >> 1;

  if (picture_coding_type == MPEG2_I_PICTURE) {
    PUT_BITS_TH(1, 1);
  } else {
    PUT_BITS_TH(3, 5);
  }
  if (!curr_frame_dct) {
    PUT_BITS_TH(pMBInfo[k].dct_type, 1);
  }

  if (pMBInfo[k].dct_type == DCT_FRAME) {
    block_offset_src = block_offset_frm_src;
    block_offset_rec = block_offset_frm_rec;
    stride_src[0] = YFramePitchSrc;
    stride_src[1] = stride_src[2] = UVFramePitchSrc;
    stride_rec[0] = YFramePitchRec;
    stride_rec[1] = stride_rec[2] = UVFramePitchRec;

  } else {
    block_offset_src = block_offset_fld_src;
    block_offset_rec = block_offset_fld_rec;
    stride_src[0] = 2*YFramePitchSrc;
    stride_src[1] = stride_src[2] = UVFramePitchSrc << chroma_fld_flag;
    stride_rec[0] = 2*YFramePitchRec;
    stride_rec[1] = stride_rec[2] = UVFramePitchRec << chroma_fld_flag;
  }
  if (encodeInfo.info.color_format == NV12) {
    stride_src[1] = stride_src[2] = YFramePitchSrc;
    stride_rec[1] = stride_rec[2] = YFramePitchRec;
  }

  for (Ipp32s blk = 0; blk < block_count; blk++) {
    Ipp32s cc = color_index[blk];
    if (blk < 4 || encodeInfo.info.color_format != NV12 )
    {
      ippiDCT8x8Fwd_8u16s_C1R(BlockSrc[cc] + block_offset_src[blk], stride_src[cc], pMBlock);
    }
    else
    {
#ifdef IPP_NV12       
      if(blk == 4)
      {
        //to change output blocks after bug fixing !!!
        ippiDCT8x8Fwd_8u16s_C2P2(BlockSrc[cc], stride_src[cc],pMBlock,pMBlock+64);
      }

#else
      tmp_ippiDCT8x8FwdUV_8u16s_C1R(BlockSrc[cc], stride_src[cc], pMBlock); // NV12 func!!!
#endif
    }

    if (pMBlock[0] < 0) {
      pMBlock[0] = (Ipp16s)(-((-pMBlock[0] + half_intra_dc) >> intra_dc_shift));
    } else {
      pMBlock[0] = (Ipp16s)((pMBlock[0] + half_intra_dc) >> intra_dc_shift);
    }
  
    ippiQuantIntra_MPEG2_16s_C1I(pMBlock, quantiser_scale_value, InvIntraQMatrix, &Count[blk]);

    switch(nLimitedMBMode)
    {
    case 1:
        {
        Ipp32s numCoeff = (blk < 4)? 2 : 1;
        Ipp32s *scan = curr_scan ? AlternateScan : ZigZagScan;
        Ipp32s numNonZero = 0;
        for(int n = 1; (n < 16) && (numNonZero < Count[blk]); n++)
        {
            if (pMBlock[scan[n]] != 0)
            {
                numNonZero ++;
                if (numNonZero > numCoeff)
                {
                    pMBlock[scan[n]] = 0;
            
                }            
            } 
        }
        Count[blk] =  (numNonZero > numCoeff) ? numCoeff : numNonZero;
        }
        break;
    case 2:        
        Count[blk] = 0;
        break;
    case 3:
        Count[blk] = 0;
        if (blk >= 1 && blk <= 3)
        {
            pMBlock[0] = (Ipp16s)dc_dct_pred[cc];            
        }
        break;
    }

    PutIntraBlock(pMBlock, &dc_dct_pred[cc], DC_Tbl[cc], Count[blk], numTh);
    if (picture_coding_type != MPEG2_B_PICTURE && !onlyIFrames) {
      pMBlock[0] <<= intra_dc_shift;

      if(Count[blk]) {
        ippiQuantInvIntra_MPEG2_16s_C1I(pMBlock, quantiser_scale_value, IntraQMatrix);
        if (blk < 4 || encodeInfo.info.color_format != NV12 )
        {
          ippiDCT8x8Inv_16s8u_C1R (pMBlock, BlockRec[cc] + block_offset_rec[blk], stride_rec[cc]);
        }
        else
        {
#ifndef IPP_NV12
          tmp_ippiDCT8x8InvUV_16s8u_C1R (pMBlock, BlockRec[cc], stride_rec[cc]); // NV12 func!!!
#endif
        }
      } else {
        if (blk < 4 || encodeInfo.info.color_format != NV12 )
          ippiSet_8u_C1R((Ipp8u)(pMBlock[0]/8), BlockRec[cc] + block_offset_rec[blk], stride_rec[cc], roi);
        else
        {
#ifndef IPP_NV12
          tmp_ippiSet8x8UV_8u_C2R((Ipp8u)(pMBlock[0]/8), BlockRec[cc], stride_rec[cc]); // NV12 func!!!
#endif
        }
      }
#ifdef MPEG2_ENC_DEBUG
    mpeg2_debug->GatherIntraBlockData(k,pMBlock,blk,Count[blk],intra_dc_shift);
#endif
    }
    pMBlock += 64;
  }
  
#ifdef IPP_NV12
  //pMBlock -= 128;
  if(BlockRec != NULL)
  {
      pMBlock = threadSpec[numTh].pMBlock + 256;
      
      if(Count[4] && Count[5])
      {
          ippiDCT8x8InvOrSet_16s8u_P2C2(pMBlock,pMBlock+64,BlockRec[1], stride_rec[1],0);
      }
      else if(Count[4] && Count[5] == 0)
      {
          ippiDCT8x8InvOrSet_16s8u_P2C2(pMBlock,pMBlock+64,BlockRec[1], stride_rec[1],1);
      }
      else if(Count[4] == 0 && Count[5])
      {
          ippiDCT8x8InvOrSet_16s8u_P2C2(pMBlock,pMBlock+64,BlockRec[1], stride_rec[1],2);
      }
       else if(Count[4]== 0 && Count[5] == 0)
      {
          ippiDCT8x8InvOrSet_16s8u_P2C2(pMBlock,pMBlock+64,BlockRec[1], stride_rec[1],3);
      }
  }
#endif
  
  
}

void MPEG2VideoEncoderBase::PutMV_FRAME(Ipp32s numTh, Ipp32s k, IppMotionVector2 *vector, Ipp32s motion_type)
{
  Ipp32s hor_f_code, ver_f_code;
  Ipp32s BW = 0;

  if (motion_type == MB_BACKWARD) {
    hor_f_code = mp_f_code[1][0];
    ver_f_code = mp_f_code[1][1];
    BW = 1;
  } else {
    hor_f_code = mp_f_code[0][0];
    ver_f_code = mp_f_code[0][1];
  }

  if (motion_type) {
    if (picture_structure != MPEG2_FRAME_PICTURE) {
      PUT_BITS_TH(pMBInfo[k].mv_field_sel[2][BW], 1);
    }
    PutMV(
      vector->x - threadSpec[numTh].PMV[0][BW].x,
      hor_f_code, numTh);
    PutMV(
      vector->y - threadSpec[numTh].PMV[0][BW].y,
      ver_f_code, numTh);
  }

  threadSpec[numTh].PMV[0][BW].x = threadSpec[numTh].PMV[1][BW].x = vector->x;
  threadSpec[numTh].PMV[0][BW].y = threadSpec[numTh].PMV[1][BW].y = vector->y;
}
#ifdef MPEG2_USE_DUAL_PRIME
void MPEG2VideoEncoderBase::PutMV_DP(Ipp32s numTh, Ipp32s /*k*/, IppMotionVector2 *vector,IppiPoint *dmvector)
{
  Ipp32s hor_f_code, ver_f_code;
  Ipp32s BW = 0;
  Ipp32s mv_shift = (picture_structure == MPEG2_FRAME_PICTURE) ? 1 : 0;
  Ipp32s dmvVLC[3]={3,0,2},val,bitLen;//-1,0,1

  hor_f_code = mp_f_code[0][0];
  ver_f_code = mp_f_code[0][1];

  PutMV(
      vector->x - threadSpec[numTh].PMV[0][BW].x,
      hor_f_code, numTh);
  val = dmvVLC[dmvector[0].x+1];
  bitLen=(dmvector[0].x!=0)+1;
  PUT_BITS_TH(val,bitLen);

  PutMV(
      vector->y - (threadSpec[numTh].PMV[0][BW].y >> mv_shift),
      ver_f_code, numTh);
  val = dmvVLC[dmvector[0].y+1];
  bitLen=(dmvector[0].y!=0)+1;
  PUT_BITS_TH(val,bitLen);

  threadSpec[numTh].PMV[0][BW].x = threadSpec[numTh].PMV[1][BW].x = vector->x;
  threadSpec[numTh].PMV[0][BW].y = threadSpec[numTh].PMV[1][BW].y = vector->y<<mv_shift;
}
#endif //MPEG2_USE_DUAL_PRIME

void MPEG2VideoEncoderBase::PutMV_FIELD(Ipp32s numTh, Ipp32s k, IppMotionVector2 *vector, IppMotionVector2 *vector2, Ipp32s motion_type)
{
  Ipp32s hor_f_code, ver_f_code;
  Ipp32s BW = 0;
  Ipp32s mv_shift = (picture_structure == MPEG2_FRAME_PICTURE) ? 1 : 0;

  if (motion_type == MB_BACKWARD) {
    hor_f_code = mp_f_code[1][0];
    ver_f_code = mp_f_code[1][1];
    BW = 1;
  } else {
    hor_f_code = mp_f_code[0][0];
    ver_f_code = mp_f_code[0][1];
  }

  /*if (motion_type)*/ {
    PUT_BITS_TH(pMBInfo[k].mv_field_sel[0][BW],1);
    PutMV(
      vector->x - threadSpec[numTh].PMV[0][BW].x,
      hor_f_code, numTh);
    PutMV(
      vector->y - (threadSpec[numTh].PMV[0][BW].y >> mv_shift),
      ver_f_code, numTh);

    PUT_BITS_TH(pMBInfo[k].mv_field_sel[1][BW],1);
    PutMV(
      vector2->x - threadSpec[numTh].PMV[1][BW].x,
      hor_f_code, numTh);
    PutMV(
      vector2->y - (threadSpec[numTh].PMV[1][BW].y >> mv_shift),
      ver_f_code, numTh);
  }
  threadSpec[numTh].PMV[0][BW].x = vector->x;
  threadSpec[numTh].PMV[0][BW].y = vector->y << mv_shift;
  threadSpec[numTh].PMV[1][BW].x = vector2->x;
  threadSpec[numTh].PMV[1][BW].y = vector2->y << mv_shift;
}

void MPEG2VideoEncoderBase::PrepareBuffers()
{
  Ipp32s i;
  //DEBUG ippsSet_8u(0xFF, out_pointer, out_buffer_size);
  SET_BUFFER(threadSpec[0].bBuf, out_pointer, output_buffer_size)
  for (i = 1; i < encodeInfo.numThreads; i++) {
    SET_BUFFER(threadSpec[i].bBuf, threadSpec[i].pDst, threadSpec[i].dstSz)
  }
}

#ifndef UMC_RESTRICTED_CODE
Ipp32s MPEG2VideoEncoderBase::IntraMBSize(Ipp32s k, const Ipp8u *BlockSrc[3],
                                          Ipp32s *dc_dct_pred, Ipp32s increment,
                                          Ipp32s scantype, Ipp32s vlcFormat)
{
  Ipp16s coefs[64*8];
  Ipp16s *pMBlock = coefs;
  Ipp32s Count[12];
  Ipp32s stride[3];
  Ipp32s *block_offset;
  Ipp32s intra_dc_shift = 3 - intra_dc_precision;
  Ipp32s half_intra_dc = (1 << intra_dc_shift) >> 1;
  Ipp32s sz; //intra type len + some

  sz = (picture_coding_type == MPEG2_I_PICTURE) ? 1 : 5;

  while( increment > 33 ) {
    sz += 11; // macroblock_escape
    increment -= 33;
  }
  sz += AddrIncrementTbl[increment].len;

  if (!curr_frame_dct) {
    sz++;
  }

  if (pMBInfo[k].dct_type == DCT_FRAME) {
    block_offset = block_offset_frm_src;
    stride[0] = YFramePitchSrc;
    stride[1] = UVFramePitchSrc;
    stride[2] = UVFramePitchSrc;
  } else {
    block_offset = block_offset_fld_src;
    stride[0] = 2*YFramePitchSrc;
    stride[1] = UVFramePitchSrc << chroma_fld_flag;
    stride[2] = UVFramePitchSrc << chroma_fld_flag;
  }


  for (Ipp32s blk = 0; blk < block_count; blk++) {
    Ipp32s cc = color_index[blk];
    if (encodeInfo.info.color_format == NV12 && blk > 3)
    {
#ifdef IPP_NV12
      if(blk == 4)
        ippiDCT8x8Fwd_8u16s_C2P2(BlockSrc[cc], stride[0], pMBlock,pMBlock+64);
#else
      tmp_ippiDCT8x8FwdUV_8u16s_C1R(BlockSrc[cc] /*+ blk-4*/, stride[0], pMBlock);  // NV12 func!!!
#endif
    }
    else
    {
      ippiDCT8x8Fwd_8u16s_C1R(BlockSrc[cc] + block_offset[blk], stride[cc], pMBlock);
    }
    if (pMBlock[0] < 0) {
      pMBlock[0] = (Ipp16s)(-((-pMBlock[0] + half_intra_dc) >> intra_dc_shift));
    } else {
      pMBlock[0] = (Ipp16s)((pMBlock[0] + half_intra_dc) >> intra_dc_shift);
    }
    // scale quantizer
    ippiQuantIntra_MPEG2_16s_C1I(pMBlock, quantiser_scale_value, InvIntraQMatrix, &Count[blk]);
    //PutIntraBlock(pMBlock, &dc_dct_pred[cc], DC_Tbl[cc], Count[blk], numTh);
    {
      Ipp32s EOBLen;
      IppVCHuffmanSpec_32s* AC_Tbl;
      Ipp32s *scan = scantype ? AlternateScan : ZigZagScan;

      if (vlcFormat) {
        EOBLen  = 4; // (Table B-15)
        AC_Tbl = vlcTableB15;
      } else {
        EOBLen  = 2; // (Table B-14)
        AC_Tbl = vlcTableB5c_e;
      }

      //mp2PutIntraBlock(&threadSpec[numTh].bBuf.current_pointer, &threadSpec[numTh].bBuf.bit_offset,
      //  pMBlock, dc_dct_pred, (IppVCHuffmanSpec_32u*)DC_Tbl, AC_Tbl, scan, EOBLen, EOBCode, count);
      {
        Ipp32s n, m, dct_diff, run, signed_level;
        Ipp32s absval, size1;

        dct_diff = pMBlock[0] - dc_dct_pred[cc];
        dc_dct_pred[cc] = pMBlock[0];

        /* generate variable length code for DC coefficient (7.2.1)*/
        if(dct_diff == 0) {
          sz += (DC_Tbl[cc])[0].len;
        } else {
          m = dct_diff >> 31;
          absval = (dct_diff + m) ^ m;
          for (size1=1; absval >>= 1; size1++);

          /* generate VLC for dct_dc_size (Table B-12 or B-13)*/
          /* append fixed length code (dc_dct_differential)*/
          //absval = (DC_Tbl[size1].code << size1) - (m<<size1) + (dct_diff + m);
          n  = (DC_Tbl[cc])[size1].len + size1;
          sz += n;
        }

        run = 0;
        m = 0;

        for(n=1; m < Count[blk] ; n++)
        {
          signed_level = pMBlock[scan[n]];
          if( signed_level )
          {
            //mp2PutAC(pBitStream, pOffset, run, signed_level, AC_Tbl);
            {
              Ipp32s level, len;
              Ipp32s maxRun = AC_Tbl[0] >> 20;
              Ipp32s addr;
              Ipp32s * table;

              if(run > maxRun)
              {
                /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
                /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
                sz += 24;
              } else {
                level = (signed_level < 0) ? -signed_level : signed_level;

                addr = AC_Tbl[run + 1];
                table = (Ipp32s*)((Ipp8s*)AC_Tbl + addr);
                if(level <= table[0])
                {
                  len  = *(table + signed_level) & 0x1f;
                  sz += len;
                }
                else
                {
                  /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
                  /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
                  sz += 24;
                }
              }
            }
            m++;
            run = 0;
          }
          else
            run++;
        }
        sz += EOBLen;
      }

    }
    pMBlock += 64;
  }
  return sz;
}
#endif // UMC_RESTRICTED_CODE

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
