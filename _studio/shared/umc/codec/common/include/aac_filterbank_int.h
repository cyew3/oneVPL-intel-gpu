// Copyright (c) 2005-2018 Intel Corporation
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

#ifndef __SAACDEC_FILTERBANK_H
#define __SAACDEC_FILTERBANK_H

#include "ipps.h"
#include "ippac.h"
#include "aaccmn_const.h"
#include "aac_status.h"

struct MDCTFwdSpec_32s;
typedef struct MDCTFwdSpec_32s IppsMDCTFwdSpec_32s;

struct MDCTInvSpec_32s;
typedef struct MDCTInvSpec_32s IppsMDCTInvSpec_32s;

typedef struct {
  IppsMDCTInvSpec_32s *p_mdct_inv_long;
  IppsMDCTInvSpec_32s *p_mdct_inv_short;
  Ipp8u  *p_buffer_inv;

  IppsMDCTFwdSpec_32s *p_mdct_fwd_long;
  IppsMDCTFwdSpec_32s *p_mdct_fwd_short;
  Ipp8u  *p_buffer_fwd;
} sFilterbankInt;

#ifdef  __cplusplus
extern  "C" {
#endif

  AACStatus InitFilterbankInt(sFilterbankInt* pBlock, Ipp32s mode);
  void    FreeFilterbankInt(sFilterbankInt * p_data);
  void    FilterbankDecInt(sFilterbankInt * p_data, Ipp32s *p_in_spectrum,
                           Ipp32s *p_in_prev_samples, Ipp32s window_sequence,
                           Ipp32s window_shape, Ipp32s prev_window_shape,
                           Ipp32s *p_out_samples_1st, Ipp32s *p_out_samples_2nd);
  void    FilterbankEncInt(sFilterbankInt * p_data, Ipp32s *p_in_samples_1st_part,
                           Ipp32s *p_in_samples_2nd_part, Ipp32s window_sequence,
                           Ipp32s window_shape, Ipp32s prev_window_shape,
                           Ipp32s *p_out_spectrum);

#ifdef  __cplusplus
}
#endif

#ifndef PI
#define PI 3.14159265359f
#endif

#endif             // __SAACDEC_FILTERBANK_H
