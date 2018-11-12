// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __SEARCH_H
#define __SEARCH_H

#include "aac_enc_own.h"

#define  MAX_NON_ESC_VALUE  12

#ifdef  __cplusplus
extern "C" {
#endif

Ipp32s best_codebooks_search(sEnc_individual_channel_stream* pStream,
                             Ipp16s *px_quant_unsigned,
                             Ipp16s *px_quant_signed,
                             Ipp16s *px_quant_unsigned_pred,
                             Ipp16s *px_quant_signed_pred);

void tree_build(Ipp32s sfb_bit_len[MAX_SFB][12],
                Ipp32s cb_trace[MAX_SFB][12],
                Ipp32s max_sfb,
                Ipp32s len_esc_value);
#if 1
void bit_count(sEnc_individual_channel_stream* pStream,
               Ipp16s *px_quant_unsigned,
               Ipp16s *px_quant_signed,
               Ipp32s *sfb_offset,
               Ipp32s sfb_bit_len[MAX_SFB][12]);
#endif
#ifdef  __cplusplus
}
#endif

#endif//__SEARCH_H
