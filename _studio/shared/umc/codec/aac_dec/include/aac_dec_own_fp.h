// Copyright (c) 2002-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __AAC_DEC_OWN_FP_H__
#define __AAC_DEC_OWN_FP_H__

#include "aac_filterbank_fp.h"
#include "aac_dec_ltp_fp.h"
#include "sbr_dec_struct.h"
#include "aaccmn_chmap.h"
#include "audio_codec_params.h"
#include "aac_dec_own.h"
#include "aac_dec_sbr_fp.h"
#include "als_dec.h"
#include "bsac_dec.h"

/* PARAMETRIC STEREO */
#include "aac_dec_ps_fp.h"

typedef struct {
  Ipp32f app_pqfbuf0[24];
  Ipp32f app_pqfbuf1[24];
  Ipp32f app_pqfbuf2[24];
  Ipp32f app_pqfbuf3[24];
} ownIppsIPQFState_32f;

struct _AACDec {
  AACDec_com     com;

  Ipp32f         m_prev_samples[CH_MAX + COUPL_CH_MAX][1024 * 2];
  Ipp32f         m_curr_samples[CH_MAX + COUPL_CH_MAX][1024 * 2];
  Ipp32f         m_ltp_buf[CH_MAX + COUPL_CH_MAX][3][1024 * 2];
  Ipp32f         m_spectrum_data[CH_MAX + COUPL_CH_MAX][1024];
  Ipp32f         m_gcOverlapBuffer[CH_MAX + COUPL_CH_MAX][4 * 512];
  Ipp32f*        m_ordered_samples[CH_MAX + COUPL_CH_MAX];
  Ipp32f         cc_gain[COUPL_CH_MAX][18][MAX_GROUP_NUMBER][MAX_SFB];
  s_tns_data     tns_data[CH_MAX];
  ownIppsIPQFState_32f SSR_IPQFState[CH_MAX + COUPL_CH_MAX];

  sSBRBlock      sbrBlock[CH_MAX];
  sSbrDecFilter  sbr_filter[CH_MAX];
  Ipp8u*         pWorkBuffer;
  /* it is only draft version. reason - memory usage has to be optimized */
  sPSDecState_32f psState;

  sFilterbank   m_filterbank;
  sLtp          m_ltp;

  IppsFDPState_32f *pFDPState[CH_MAX + COUPL_CH_MAX];
  ALSDec        alsState;
  BSACDec       *bsState;
};

/********************************************************************/
#define AACDEC_UPDATE_PTR(type, ptr,inc)  \
  if (ptr) {                              \
    ptr = (type *)((Ipp8u *)(ptr) + inc); \
  }
/********************************************************************/
#define AACDEC_UPDATE_PPTR(type, ptr,inc)  \
  if (ptr) {                               \
    ptr = (type **)((Ipp8u *)(ptr) + inc); \
  }
/********************************************************************/

#endif
