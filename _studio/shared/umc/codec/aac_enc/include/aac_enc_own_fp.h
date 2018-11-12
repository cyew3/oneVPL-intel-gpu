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

#ifndef __AAC_ENC_OWN_FP_H__
#define __AAC_ENC_OWN_FP_H__

#include "aaccmn_const.h"
#include "aac_enc_psychoacoustic_fp.h"
#include "aac_filterbank_fp.h"
#include "aac_enc_quantization_fp.h"
#include "aac_enc_own.h"
/* HEAAC profile*/
#include "sbr_enc_api_fp.h"

#include "ipps.h"

struct _AACEnc {
  Ipp32f**                 m_buff_pointers;
  Ipp32f**                 ltp_buff;
  Ipp32f**                 ltp_overlap;
  IppsFFTSpec_R_32f*       corrFft;
  Ipp8u*                   corrBuff;

  sPsychoacousticBlock*    psychoacoustic_block;
  sPsychoacousticBlockCom  psychoacoustic_block_com;
  sFilterbank              filterbank_block;

  /*AYA: HEAAC profile*/
  sSBREncState*             sbrState;
  ownFilterSpec_SBR_C_32fc* pQMFSpec;

  Ipp32f                    minSNRLong[MAX_SFB];
  Ipp32f                    minSNRShort[MAX_SFB_SHORT];

  AACEnc_com com;
};

#endif
