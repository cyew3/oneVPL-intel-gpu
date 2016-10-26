//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __SAACENC_TNS_H
#define __SAACENC_TNS_H

#include "ippdefs.h"

#define TNS_MAX_ORDER 20
#define TNS_MAX_BANDS 51

#define TNS_GAIN 1.4
#define TNS_COEFF_THRESHOLD 0.1
#define TNS_COEFF_RES 4
#define TNS_RES_OFFSET 3
#define TNS_MAX_FILT  4

#ifndef MAX_NUM_WINDOWS
#define MAX_NUM_WINDOWS 8
#endif

typedef struct{
  Ipp32s w_number;
  Ipp32s is_window_long;

  Ipp32s n_filt[MAX_NUM_WINDOWS];
  Ipp32s coef_res[MAX_NUM_WINDOWS];
  Ipp32s length[MAX_NUM_WINDOWS][TNS_MAX_FILT];
  Ipp32s order[MAX_NUM_WINDOWS][TNS_MAX_FILT];
  Ipp32s direction[MAX_NUM_WINDOWS][TNS_MAX_FILT];
  Ipp32s coef_compress[MAX_NUM_WINDOWS][TNS_MAX_FILT];
  Ipp32s coef[MAX_NUM_WINDOWS][TNS_MAX_FILT][TNS_MAX_ORDER];
} sTns_data;

typedef struct
{
  Ipp32s*    p_tns_data_present;
  sTns_data* p_tns_data;

  Ipp32s*    p_num_sfb;
  Ipp32s*    sfb_offset;

  Ipp32s     tns_max_order_long;
  Ipp32s     tns_max_order_short;

  Ipp32s     tns_max_band_long;
  Ipp32s     tns_max_band_short;

  Ipp32s     tns_min_band_long;
  Ipp32s     tns_min_band_short;

//Ipp32s  tns_max_order;
//Ipp32s  tns_max_bands;

  Ipp32f     *in;

} sTnsBlock;

#ifdef  __cplusplus
extern "C" {
#endif

void InitTns(sTnsBlock* pBlock,
             Ipp32s profile,
             Ipp32s sampling_frequency_index);
void FreeTns(sTnsBlock* pBlock);
void Tns(sTnsBlock* pBlock,
         Ipp32s win_seq);

#ifdef  __cplusplus
}
#endif

#ifndef PI
#define PI 3.14159265359f
#endif

#ifndef AAC_PROFILES_ENUM
#define AAC_PROFILES_ENUM

enum AAC_PROFILES {
  EN_AAC_PROFILE_MAIN,
  EN_AAC_PROFILE_LC,
  EN_AAC_PROFILE_SSR
};

#endif//AAC_PROFILES_ENUM

#ifndef WINDOWS_SEQUENCES_DEF
#define WINDOWS_SEQUENCES_DEF

#define ONLY_LONG_SEQUENCE    0
#define LONG_START_SEQUENCE   1
#define EIGHT_SHORT_SEQUNECE  2
#define LONG_STOP_SEQUENCE    3

#endif//WINDOWS_SEQUENCES_DEF

#ifndef MIN_MAX_DEF
#define MIN_MAX_DEF

#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))

#endif//MIN_MAX_DEF

#endif//__SAACENC_TNS_H
