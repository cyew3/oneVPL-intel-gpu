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

#ifndef __AACCMN_CCE_H
#define __AACCMN_CCE_H

#include "ippdefs.h"

typedef struct
{
  Ipp32s  element_instance_tag;

  Ipp32s ind_sw_cce_flag;         /// 1 bit
  Ipp32s num_coupled_elements;    /// 3 bits
  Ipp32s cc_target_is_cpe[9];     /// 1 bit
  Ipp32s cc_target_tag_select[9]; /// 4 bits
  Ipp32s cc_l[9];                 /// 1 bit   if (cc_target_is_cpe[9];)
  Ipp32s cc_r[9];                 /// 1 bit   if (cc_target_is_cpe[9];)
  Ipp32s cc_domain;               /// 1 bit
  Ipp32s gain_element_sign;       /// 1 bit
  Ipp32s gain_element_scale;      /// 2 bit

  Ipp32s common_gain_element_present[10];///
  Ipp32s common_gain_element[10];

//  s_SE_Individual_channel_stream  stream;

  Ipp32s num_gain_element_lists; ///

} sCoupling_channel_element;

#ifdef  __cplusplus
extern "C" {
#endif

Ipp32s unpack_coupling_channel_element(sCoupling_channel_element* p_data,Ipp8u** pp_bs,
                                       Ipp32s* p_offset,Ipp32s audio_object_type,
                                       Ipp32s sampling_frequency_index);

#ifdef  __cplusplus
}
#endif

#endif//__AACCMN_CCE_H
