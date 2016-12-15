//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#ifndef UMC_RESTRICTED_CODE
//#include <limits.h>
//#include <math.h>
#endif // UMC_RESTRICTED_CODE
#include "vm_time.h"
#include "umc_mpeg2_enc_defs.h"

using namespace UMC;

#ifndef UMC_RESTRICTED_CODE
//Ipp32f MVSearchPh(const Ipp8u* src, Ipp32s sstep, const Ipp8u* blk, Ipp32s height,
//                Ipp32s l, Ipp32s t, Ipp32s r, Ipp32s b, Ipp32s* px, Ipp32s* py);
//Ipp32f MVSearch(const Ipp8u* src, Ipp32s sstep, const Ipp8u* blk, Ipp32s height,
//                Ipp32s l, Ipp32s t, Ipp32s r, Ipp32s b, Ipp32s* px, Ipp32s* py);

//static Ipp32s mp2_WeightMV(Ipp32s dx, Ipp32s dy, Ipp32s fcode, Ipp32s quant);
#endif // UMC_RESTRICTED_CODE

#define SAD_FUNC_16x16 \
  ippiSAD16x16_8u32s(pBlock,     \
                     BlockStep,  \
                     ref_data,   \
                     ref_step,    \
                     &MAD,       \
                     flag)

#define SAD_FUNC_16x8 \
  ippiSAD16x8_8u32s_C1R(pBlock,      \
                        BlockStep,   \
                        ref_data,    \
                        ref_step,     \
                        &MAD,        \
                        flag)

/*#define SAD_FUNC_16x16_fullpix      \
  ippiSAD16x8_8u32s_C1R(pBlock,     \
                    2*BlockStep,    \
                    ref_data,       \
                    2*RefStep,      \
                    &MAD,           \
                    0)*/
#define SAD_FUNC_16x16_fullpix \
  SAD_FUNC_16x16

#define SAD_FUNC_16x8_fullpix \
  SAD_FUNC_16x8

#define VARMEAN_FUNC_16x16                  \
  ippiVarMeanDiff16x16_8u32s_C1R(           \
                          pBlock,           \
                          BlockStep,        \
                          ref_data,         \
                          ref_step,          \
                          pSrcMean,         \
                          Var[!min_index],  \
                          Mean[!min_index], \
                          flag);            \
  MAD = Var[!min_index][0] + Var[!min_index][1] + \
        Var[!min_index][2] + Var[!min_index][3]

#define VARMEAN_FUNC_16x8                   \
  ippiVarMeanDiff16x8_8u32s_C1R(            \
                          pBlock,           \
                          BlockStep,        \
                          ref_data,         \
                          ref_step,          \
                          pSrcMean,         \
                          Var[!min_index],  \
                          Mean[!min_index], \
                          flag);            \
  MAD = Var[!min_index][0] + Var[!min_index][1]

#define CMP_MAD                                            \
/*MAD += mp2_WeightMV(XN - InitialMV0.x,                     \
                    YN - (InitialMV0.y >> FIELD_FLAG),     \
                    mp_f_code[0][0],                       \
                    quantiser_scale_code); */                \
if(MAD < BestMAD)                                          \
{                                                          \
  min_point = 1;                                           \
  BestMAD = MAD;                                           \
  min_index = !min_index;                                  \
  XMIN = XN;                                               \
  YMIN = YN;                                               \
}
#ifndef UMC_RESTRICTED_CODE
/*{ \
Ipp32s dv=abs(XN - InitialMV0.x) \
         +abs(YN - (InitialMV0.y >> FIELD_FLAG)); \
  MAD += abs(dv-7); \
} \ */

#endif // UMC_RESTRICTED_CODE
/***********************************************************/

#define ME_FUNC     MotionEstimation_Frame
#define FIELD_FLAG  0
#define ME_STEP_X   2
#define ME_STEP_Y   2
#define ME_MIN_STEP 2
#define NUM_BLOCKS  4

#define TRY_POSITION_REC(FUNC)                               \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)          \
{ Ipp32s ref_step = RecPitch;                                \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;      \
  ref_data = (pRecFld + (XN >> 1) + (YN >> 1)*RecPitch);     \
  flag = ((XN & 1) << 3) | ((YN & 1) << 2);                  \
                                                             \
  FUNC##_16x16;                                              \
                                                             \
  CMP_MAD                                                    \
}

#define TRY_POSITION_REF(FUNC)                               \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)          \
{ Ipp32s ref_step = RefPitch;                                \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;      \
  ref_data = (pRefFld + (XN >> 1) + (YN >> 1)*RefPitch);     \
  flag = 0;                                                  \
                                                             \
  FUNC##_16x16_fullpix;                                      \
                                                             \
  CMP_MAD                                                    \
}

#include "umc_mpeg2_enc_me.h"

/***********************************************************/

#undef  ME_FUNC
#define ME_FUNC  MotionEstimation_FieldPict

#undef  NUM_BLOCKS
#define NUM_BLOCKS  2

#undef  TRY_POSITION_REC
#define TRY_POSITION_REC(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)     \
{ Ipp32s ref_step = RecPitch;                                                    \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id; \
  ref_data = (pRecFld + (XN >> 1) + (YN >> 1)*RecPitch); \
  flag = ((XN & 1) << 3) | ((YN & 1) << 2);             \
                                                        \
  FUNC##_16x8;                                          \
                                                        \
  CMP_MAD                                               \
}

#undef  TRY_POSITION_REF
#define TRY_POSITION_REF(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)     \
{ Ipp32s ref_step = RefPitch;                                                      \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id; \
  ref_data = (pRefFld + (XN >> 1) + (YN >> 1)*RefPitch); \
  flag = 0;                                             \
                                                        \
  FUNC##_16x8_fullpix;                                  \
                                                        \
  CMP_MAD                                               \
}

#include "umc_mpeg2_enc_me.h"

/***********************************************************/

#undef  ME_FUNC
#define ME_FUNC  MotionEstimation_Field

#undef  FIELD_FLAG
#define FIELD_FLAG  1

#undef  NUM_BLOCKS
#define NUM_BLOCKS  2

#undef  TRY_POSITION_REC
#define TRY_POSITION_REC(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)      \
{                                                        \
  Ipp32s ref_step = RecPitch;                                    \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;  \
  ref_data = (pRecFld + (XN >> 1) + (YN >> 1)*RecPitch);  \
  flag = ((XN & 1) << 3) | ((YN & 1) << 2);              \
                                                         \
  FUNC##_16x8;                                           \
                                                         \
  CMP_MAD                                                \
}

#undef  TRY_POSITION_REF
#define TRY_POSITION_REF(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)      \
{                                                        \
  Ipp32s ref_step = RefPitch;                                    \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;  \
  ref_data = (pRefFld + (XN >> 1) + (YN >> 1)*RefPitch);  \
  flag = 0;                                              \
                                                         \
  FUNC##_16x8_fullpix;                                   \
                                                         \
  CMP_MAD                                                \
}

#include "umc_mpeg2_enc_me.h"

/***********************************************************/

#ifndef UMC_RESTRICTED_CODE
//void MPEG2VideoEncoderBase::AdjustSearchRange(Ipp32s B_count, Ipp32s direction)
//{
//  Ipp32s coord;
//
//  for (coord = 0; coord<2; coord++) {
//    Ipp32s fcRangeMin, fcRangeMax, flRangeMin, flRangeMax, fc, r, co, ci, cnz, i;
//    fc = pMotionData[B_count].f_code[direction][coord];
//    fcRangeMax = 8 << fc;     // max range in half-pixels
//    fcRangeMin = -fcRangeMax;
//    r = 1 << fc;
//    r = IPP_MIN(r, 4);
//    flRangeMax = 4 << fc;     // max range in full pixels or half range in half-p
//    flRangeMin = -flRangeMax;
//    co = ci = cnz = 0;
//    for (i = 0; i < MBcount; i ++) {
//      if (!(pMBInfo[i].mb_type & MB_INTRA)) {
//        Ipp32s dx = (coord==0 ? pMBInfo[i].MV[0][direction].x
//                              : pMBInfo[i].MV[0][direction].y);
//        if (dx >= (fcRangeMax - r) || dx < (fcRangeMin + r))
//          co ++; // close to limit
//        if (dx >= (flRangeMax - r) || dx < (flRangeMin + r))
//          ci ++; // more than half limit
//        if (dx != 0)
//          cnz ++;
//
//        dx = (coord==0 ? pMBInfo[i].MV[1][direction].x
//                       : pMBInfo[i].MV[1][direction].y);
//        if (dx >= (fcRangeMax - r) || dx < (fcRangeMin + r))
//          co ++; // close to limit
//        if (dx >= (flRangeMax - r) || dx < (flRangeMin + r))
//          ci ++; // more than half limit
//        if (dx != 0)
//          cnz ++;
//      }
//    }
//
//    //printf("%.2f, %.2f\n", co*100.0/cnz, cnz/(ci*400.0));
//    if (co * 16 > cnz) {
//      if( coord == 0 && flRangeMax < encodeInfo.info.clip_info.width / 4 && fc < 5 ||
//          coord == 1 && flRangeMax < encodeInfo.info.clip_info.height / 4 && fc < 4) {
//        fc++;
//        //printf("%d: mul: %d\n", encodeInfo.numEncodedFrames, fcRangeMax);
//      }
//    } else if (ci * 16 < cnz && fc > 1) {
//      fc--;
//      //printf("%d: div: %d\n", encodeInfo.numEncodedFrames, fcRangeMax);
//    }
//    pMotionData[B_count].f_code[direction][coord] = fc;
//    pMotionData[B_count].searchRange[direction][coord] = 4 << fc;
//
//    //Ipp32s req_f_code = 1;
//    //while ((4<<req_f_code) < fcRangeMax) {
//    //  req_f_code++;
//    //}
//    //pMotionData[B_count].f_code[direction][coord] = req_f_code;
//  }
//}
#endif // UMC_RESTRICTED_CODE

void MPEG2VideoEncoderBase::AdjustSearchRange(Ipp32s B_index, Ipp32s direction)
{
  Ipp32s fcRangeMin, fcRangeMax, flRangeMin, flRangeMax, co, ci, cnz, r, i;

  fcRangeMax = IPP_MAX(
    pMotionData[B_index].searchRange[direction][0],
    pMotionData[B_index].searchRange[direction][1]);
  fcRangeMin = -fcRangeMax;
  flRangeMax = fcRangeMax >> 1;
  flRangeMin = -flRangeMax;
  co = ci = cnz = 0;
  r = 4;
  //r = flRangeMax / 2;
  if (r < 2) r = 2;
  for (i = 0; i < (encodeInfo.info.clip_info.height*encodeInfo.info.clip_info.width)/256; i ++) {
    if (!(pMBInfo[i].mb_type & MB_INTRA)) {
      Ipp32s dx = pMBInfo[i].MV[0][0].x;
      Ipp32s dy = pMBInfo[i].MV[0][0].y;
      if (dx >= (fcRangeMax - r) || dx < (fcRangeMin + r) || dy >= (fcRangeMax - r) || dy < (fcRangeMin + r))
        co ++;
      if (dx >= (flRangeMax - r) || dx < (flRangeMin + r) || dy >= (flRangeMax - r) || dy < (flRangeMin + r))
        ci ++;
      if (dx != 0 || dy != 0)
        cnz ++;
    }
  }
  if (cnz == 0) {
    return;
  }
  //printf("%.2f, %.2f\n", co*100.0/cnz, cnz/(ci*400.0));
  if (co * 100 >= cnz) {
    if (fcRangeMax <= 32) {
      fcRangeMax *= 2;
      //printf("%d: mul: %d\n", encodeInfo.numEncodedFrames, fcRangeMax);
    }
  } else if (ci * 100 < cnz) {
    if (fcRangeMax > 4) {
      fcRangeMax /= 2;
      //printf("%d: div: %d\n", encodeInfo.numEncodedFrames, fcRangeMax);
    }
  }
  pMotionData[B_index].searchRange[direction][0] = fcRangeMax;
  pMotionData[B_index].searchRange[direction][1] = fcRangeMax;

  Ipp32s req_f_code = 1;
  while ((4<<req_f_code) < fcRangeMax) {
    req_f_code++;
  }
  pMotionData[B_index].f_code[direction][0] = req_f_code;
  pMotionData[B_index].f_code[direction][1] = req_f_code;
}

#ifndef UMC_RESTRICTED_CODE


//#define SF  20. / 6.
//const Ipp8u mp2_MV_Weigth[32] = {
//  0, (Ipp8u)( 1.115*SF+0.5), (Ipp8u)( 1.243*SF+0.5), (Ipp8u)( 1.386*SF+0.5),
//    (Ipp8u)( 1.546*SF+0.5), (Ipp8u)( 1.723*SF+0.5), (Ipp8u)( 1.922*SF+0.5), (Ipp8u)( 2.143*SF+0.5),
//    (Ipp8u)( 2.389*SF+0.5), (Ipp8u)( 2.664*SF+0.5), (Ipp8u)( 2.970*SF+0.5), (Ipp8u)( 3.311*SF+0.5),
//    (Ipp8u)( 3.692*SF+0.5), (Ipp8u)( 4.117*SF+0.5), (Ipp8u)( 4.590*SF+0.5), (Ipp8u)( 5.118*SF+0.5),
//    (Ipp8u)( 5.707*SF+0.5), (Ipp8u)( 6.363*SF+0.5), (Ipp8u)( 7.095*SF+0.5), (Ipp8u)( 7.911*SF+0.5),
//    (Ipp8u)( 8.821*SF+0.5), (Ipp8u)( 9.835*SF+0.5), (Ipp8u)(10.966*SF+0.5), (Ipp8u)(12.227*SF+0.5),
//    (Ipp8u)(13.633*SF+0.5), (Ipp8u)(15.201*SF+0.5), (Ipp8u)(16.949*SF+0.5), (Ipp8u)(18.898*SF+0.5),
//    (Ipp8u)(21.072*SF+0.5), (Ipp8u)(23.495*SF+0.5), (Ipp8u)(26.197*SF+0.5), (Ipp8u)(29.209*SF+0.5)
//};
//
//const Ipp8u mp2_MVD_TB12_len[65] = {
//  13, 13, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 10, 10,
//    10,  8,  8,  8,  7,  5,  4,  3,  1,  3,  4,  5,  7,  8,  8,  8, 10, 10, 10, 11, 11, 11, 11, 11,
//    11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13
//};

//static Ipp32s mp2_WeightMV(Ipp32s dx, Ipp32s dy, Ipp32s fcode, Ipp32s quant)
//{
//  Ipp32s  b, a;
//
//  return abs(dx) + abs(dy);
//
//  fcode -= 2;
//
//  if (fcode <= 1) {
//    if (dx < -32) dx += 64;
//    if (dx >  31) dx -= 64;
//    if (dy < -32) dy += 64;
//    if (dy >  31) dy -= 64;
//    b = mp2_MVD_TB12_len[dx+32] + mp2_MVD_TB12_len[dy+32];
//  } else {
//    Ipp32s  fRangeMin = -(16 << fcode), fRangeMax = (16 << fcode) - 1, fRange = fRangeMax - fRangeMin + 1;
//    if (dx < fRangeMin) dx += fRange;
//    if (dx > fRangeMax) dx -= fRange;
//    if (dy < fRangeMin) dy += fRange;
//    if (dy > fRangeMax) dy -= fRange;
//    fcode --;
//    if (dx == 0) {
//      b = 1;
//    } else {
//      if (dx < 0)
//        dx = -dx;
//      a = ((dx - 1) >> fcode) + 1;
//      b = mp2_MVD_TB12_len[a+32] + fcode;
//    }
//    if (dy == 0) {
//      b ++;
//    } else {
//      if (dy < 0)
//        dy = -dy;
//      a = ((dy - 1) >> fcode) + 1;
//      b += mp2_MVD_TB12_len[a+32] + fcode;
//    }
//  }
////printf("%d %d -> %d\n", dx, dy, b*mp2_MV_Weigth[quant]);
//  return b * mp2_MV_Weigth[quant];
//}

//#define FFORDER 5
//#define FFSZ (1<<FFORDER)
//#define FFSZ2 (1<<(FFORDER-1))
//#define FFBRD (FFSZ2-8)
//
//#define M_PI       3.14159265358979323846
//class Tab
//{
//public:
//  Tab()
//  {
//    int i, j;
//    for(i=0; i<16; i++) {
//      ctabf[i] = ctabf[31-i] = sin((2*i+1)*M_PI/64);
//      //tabs[i] = tabs[31-i] = (int)(tabf[i]*0x8000);
//      if(i&1) {
//        ctabf16[i] = ctabf16[31-i] = sin((2*(i>>1)+1)*M_PI/32);
//        //tabs16[i] = tabs16[31-i] = (int)(tabf16[i]*0x8000);
//      }
//    }
//    for(i=0; i<16; i++) {
//      for(j=0; j<32; j++) {
//        tabf[2*i  ][j] = ctabf[2*i  ]*ctabf[j];
//        tabf[2*i+1][j] = ctabf[2*i+1]*ctabf[j];
//        tabf16[i  ][j] = ctabf16[i  ]*ctabf[j];
//        tabs[2*i  ][j] = (int)(tabf[2*i  ][j]*0x8000);
//        tabs[2*i+1][j] = (int)(tabf[2*i+1][j]*0x8000);
//        tabs16[i  ][j] = (int)(tabf16[i  ][j]*0x8000);
//      }
//    }
//    IppStatus sts = ippiFFTInitAlloc_C_32fc (&pFFTSpec,
//      FFORDER, FFORDER, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast);
//    sts = ippiFFTGetBufSize_C_32fc (pFFTSpec, &FFTsize);
//    if(sts == ippStsNoErr)
//      FFTbuff = ippsMalloc_8u(FFTsize);
//  }
//  Ipp32f ctabf[32], ctabf16[16];
//  Ipp32f tabf[32][32], tabf16[16][32];
//  Ipp16s tabs[32][32], tabs16[16][32];
//  IppiFFTSpec_C_32fc* pFFTSpec;
//  int FFTsize;
//  Ipp8u* FFTbuff; // move to each thread later
//} tab;

//Ipp32f MVSearchPh(const Ipp8u* src, Ipp32s sstep, const Ipp8u* blk, Ipp32s height,
//                Ipp32s l, Ipp32s t, Ipp32s r, Ipp32s b, Ipp32s* px, Ipp32s* py)
//{
//  Ipp32fc fsrc[FFSZ*FFSZ], fref[FFSZ*FFSZ];
//  Ipp32s x, y, x2, y2;
//  Ipp32s xoff, yoff;
//  IppiSize roi = {FFSZ, FFSZ};
//  IppStatus sts;
//
//  xoff = (l < -FFBRD) ? ((r>FFBRD)?-FFBRD:-2*FFBRD+r) : l;
//  yoff = (t < -FFBRD) ? ((b>FFBRD)?-FFBRD:-2*FFBRD+b) : t;
//
//  for(y=0; y<FFSZ; y++) {
//    for(x=0; x<FFSZ; x++) {
//      fsrc[y*FFSZ+x].im = fref[y*FFSZ+x].im = 0;
//      fsrc[y*FFSZ+x].re = tab.tabf[y][x] * src[(y+yoff)*sstep + x + xoff];
//      fref[y*FFSZ+x].re = tab.tabf[y][x] * blk[(y+yoff)*sstep + x + xoff];
//    }
//  }
//
//
//  //for(y=0; y<t+FFBRD; y++)
//  //  for(x=0; x<FFSZ; x++) fsrc[y*FFSZ+x].re = fref[y*FFSZ+x].re = 0;
//  //for(y2=FFSZ-1; y2>=b+FFSZ-FFBRD; y2--)
//  //  for(x=0; x<FFSZ; x++) fsrc[y2*FFSZ+x].re = fref[y2*FFSZ+x].re = 0;
//  //for( ; y<=y2; y++) {
//  //  for(x=0; x<l+FFBRD; x++) fsrc[y*FFSZ+x].re = fref[y*FFSZ+x].re = 0;
//  //  for(x2=FFSZ-1; x2>=r+FFSZ-FFBRD; x2--) fsrc[y*FFSZ+x2].re = fref[y*FFSZ+x2].re = 0;
//  //  for( ; x<=x2; x++) {
//  //    fsrc[y*FFSZ+x].re = tab.tabf[y][x] * src[(y-FFBRD)*sstep + x - FFBRD];
//  //    fref[y*FFSZ+x].re = tab.tabf[y][x] * blk[(y-FFBRD)*sstep + x - FFBRD];
//  //  }
//  //}
//
//  ippiFFTFwd_CToC_32fc_C1IR(fsrc, sizeof(fsrc[0])*FFSZ,
//    tab.pFFTSpec, tab.FFTbuff);
//  ippiFFTFwd_CToC_32fc_C1IR(fref, sizeof(fref[0])*FFSZ,
//    tab.pFFTSpec, tab.FFTbuff);
//
//  for(y=0; y<FFSZ; y++)
//    for(x=0; x<FFSZ; x++) {
//      Ipp32f r0, i0, r1, i1, rr, ii, imod;
//      r0 = fsrc[y*FFSZ+x].re;
//      i0 = fsrc[y*FFSZ+x].im;
//      r1 = fref[y*FFSZ+x].re;
//      i1 = fref[y*FFSZ+x].im;
//      rr = r0*r1 + i0*i1;
//      ii = i0*r1 - r0*i1;
//      imod = sqrtf(sqrtf(rr*rr + ii*ii));
//      if(imod > 0) {
//        fref[y*FFSZ+x].re = rr / imod;
//        fref[y*FFSZ+x].im = ii / imod;
//      } else {
//        fref[y*FFSZ+x].re = 0; //1 | 0?
//        fref[y*FFSZ+x].im = 0;
//      }
//    }
//
//  ippiFFTInv_CToC_32fc_C1IR(fref, sizeof(fref[0])*FFSZ,
//    tab.pFFTSpec, tab.FFTbuff);
//
//  Ipp32f ccor[FFSZ*FFSZ], fres;
//  ippiMagnitude_32fc32f_C1R(fref, sizeof(fref[0])*FFSZ,
//    ccor, sizeof(ccor[0])*FFSZ, roi);
//  sts = ippiMaxIndx_32f_C1R(ccor, sizeof(ccor[0])*FFSZ, roi, &fres, px, py);
//
//  if(*px >= FFSZ2) {
//    *px -= FFSZ;
//    if(*px < l)
//      *px = 0;
//  } else if(*px > r)
//    *px = r;
//  if(*py >= FFSZ2) {
//    *py -= FFSZ;
//    if(*py < t)
//      *py = 0;
//  } else if(*py > b)
//    *py = b;
//
//  *px *= 2;
//  *py *= 2;
//
//  return fres;
//}

//Ipp32f MVSearch(const Ipp8u* src, Ipp32s sstep, const Ipp8u* blk, Ipp32s height,
//                Ipp32s l, Ipp32s t, Ipp32s r, Ipp32s b, Ipp32s* px, Ipp32s* py)
//{
//  Ipp32f ccor[256*256], fres;
//  IppiSize rois = {r-l+16, b-t+height};
//  IppiSize roit = {16, height};
//  IppiSize roic = {r-l, b-t};
//  Ipp32s bstep = sizeof(ccor[0]) * (r-l);
//  Ipp32s del = 0;
//  Ipp32s ave = 0;
//  Ipp32s x2, y2;
//  Ipp32f f2;
//  IppStatus sts;
//
//  if(bstep * (b-t) > sizeof(ccor)) {
//    del = sizeof(ccor) / bstep - (b-t) + 1;
//    rois.height -= del;
//    roic.height -= del;
//    src += (del/2) * sstep;
//  }
//
//  ippiCrossCorrValid_Norm_8u32f_C1R(src, sstep, rois, blk, sstep, roit, ccor, bstep);
//  sts = ippiMaxIndx_32f_C1R(ccor, bstep, roic, &fres, px, py);
//
//  if(sts == ippStsNoErr){
//    f2 = ccor[-l - (t+del/2)*roic.width];
//    ccor[*px + *py*roic.width] = 0;
//    //if ((1-f2)*.9 < 1-fres) {
//    if (f2+.02 > fres && *px!=0 && *py!=0) {
//      *px = 0;
//      *py = 0;
//      return f2;
//    }
//    ippiMaxIndx_32f_C1R(ccor, bstep, roic, &f2, &x2, &y2);
//    if((1-f2)*.9 < 1-fres) {
//      if (abs(*px-x2) <= 1 && abs(*py-y2) <= 1) {
//        ave = 1;
//      }
//    }
//  } else {
//    *px = 0;
//    *py = 0;
//  }
//
//  if(ave == 1) {
//    *px += x2;
//    *py += y2;
//    fres = (fres + f2) / 2;
//  } else {
//    *px *= 2;
//    *py *= 2;
//  }
//
//  *px += 2*l;
//  *py += 2*t + 2*(del/2);
//
//  return fres;
//}
#endif // UMC_RESTRICTED_CODE

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
