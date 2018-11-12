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

#ifndef __MP4CMN_PCE_H__
#define __MP4CMN_PCE_H__

#include "bstream.h"

enum eMP4CMN_PCE_H
{
    LEN_COMMENT             = (1<<8),
    MAX_CHANNELS_ELEMENTS   = 16,
    MAX_ASSOC_DATA_ELEMENTS =  8,
    MAX_VALID_CC_ELEMENTS   = 16
};

typedef struct
{
  Ipp32s element_instance_tag;
  Ipp32s object_type;
  Ipp32s sampling_frequency_index;

  Ipp32s num_front_channel_elements;
  Ipp32s num_side_channel_elements;
  Ipp32s num_back_channel_elements;
  Ipp32s num_lfe_channel_elements;

  Ipp32s num_assoc_data_elements;
  Ipp32s num_valid_cc_elements;

  Ipp32s mono_mixdown_present;
  Ipp32s mono_miwdown_element_number;

  Ipp32s stereo_mixdown_present;
  Ipp32s stereo_miwdown_element_number;

  Ipp32s matrix_mixdown_idx_present;
  Ipp32s matrix_mixdown_idx;
  Ipp32s pseudo_surround_enable;

  Ipp32s front_element_is_cpe[MAX_CHANNELS_ELEMENTS];
  Ipp32s front_element_tag_select[MAX_CHANNELS_ELEMENTS];
  Ipp32s side_element_is_cpe[MAX_CHANNELS_ELEMENTS];
  Ipp32s side_element_tag_select[MAX_CHANNELS_ELEMENTS];
  Ipp32s back_element_is_cpe[MAX_CHANNELS_ELEMENTS];
  Ipp32s back_element_tag_select[MAX_CHANNELS_ELEMENTS];
  Ipp32s lfe_element_tag_select[MAX_CHANNELS_ELEMENTS];

  Ipp32s assoc_data_element_tag_select[MAX_ASSOC_DATA_ELEMENTS];
  Ipp32s cc_element_is_ind_sw[MAX_VALID_CC_ELEMENTS];
  Ipp32s valid_cc_element_tag_select[MAX_VALID_CC_ELEMENTS];

  Ipp32s comment_field_bytes;
  Ipp8s  comment_field_data[LEN_COMMENT];

  Ipp32s num_front_channels;
  Ipp32s num_side_channels;
  Ipp32s num_back_channels;
  Ipp32s num_lfe_channels;

} sProgram_config_element;

#ifdef  __cplusplus
extern "C" {
#endif

Ipp32s dec_program_config_element(sProgram_config_element * p_data,sBitsreamBuffer * p_bs);

/***********************************************************************

    Unpack function(s) (support(s) alternative bitstream representation)

***********************************************************************/

Ipp32s unpack_program_config_element(sProgram_config_element * p_data,Ipp8u **pp_bitstream, Ipp32s *p_offset);

#ifdef  __cplusplus
}
#endif

#endif//__MP4CMN_PCE_H__
