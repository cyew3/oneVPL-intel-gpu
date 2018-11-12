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

#include "umc_defs.h"
#if defined (UMC_ENABLE_AAC_AUDIO_DECODER) || defined (UMC_ENABLE_MP4_SPLITTER)

#include "mp4cmn_pce.h"
#include "bstream.h"

Ipp32s dec_program_config_element(sProgram_config_element* p_data, sBitsreamBuffer* p_bs)
{
  Ipp32s i;

  p_data->element_instance_tag = Getbits(p_bs,4);
  p_data->object_type = Getbits(p_bs,2);
  p_data->sampling_frequency_index = Getbits(p_bs,4);

  p_data->num_front_channels = p_data->num_front_channel_elements = Getbits(p_bs,4);
  p_data->num_side_channels  = p_data->num_side_channel_elements = Getbits(p_bs,4);
  p_data->num_back_channels  = p_data->num_back_channel_elements = Getbits(p_bs,4);
  p_data->num_lfe_channels   = p_data->num_lfe_channel_elements = Getbits(p_bs,2);

  p_data->num_assoc_data_elements = Getbits(p_bs,3);
  p_data->num_valid_cc_elements = Getbits(p_bs,4);

  p_data->mono_mixdown_present = Getbits(p_bs,1);
  if (p_data->mono_mixdown_present == 1)
  {
    p_data->mono_miwdown_element_number = Getbits(p_bs,4);
  }

  p_data->stereo_mixdown_present = Getbits(p_bs,1);
  if (p_data->stereo_mixdown_present == 1)
  {
    p_data->stereo_miwdown_element_number = Getbits(p_bs,4);
  }

  p_data->matrix_mixdown_idx_present = Getbits(p_bs,1);
  if (p_data->matrix_mixdown_idx_present == 1)
  {
    p_data->matrix_mixdown_idx = Getbits(p_bs,2);
    p_data->pseudo_surround_enable = Getbits(p_bs,1);
  }

  for (i = 0; i < p_data->num_front_channel_elements; i++)
  {
    p_data->num_front_channels += p_data->front_element_is_cpe[i] = Getbits(p_bs,1);
    p_data->front_element_tag_select[i] = Getbits(p_bs,4);
  }

  for (i = 0; i < p_data->num_side_channel_elements; i++)
  {
    p_data->num_side_channels += p_data->side_element_is_cpe[i] = Getbits(p_bs,1);
    p_data->side_element_tag_select[i] = Getbits(p_bs,4);
  }

  for (i = 0; i < p_data->num_back_channel_elements; i++)
  {
    p_data->num_back_channels += p_data->back_element_is_cpe[i] = Getbits(p_bs,1);
    p_data->back_element_tag_select[i] = Getbits(p_bs,4);
  }

  for (i = 0; i < p_data->num_lfe_channel_elements; i++)
  {
    p_data->lfe_element_tag_select[i] = Getbits(p_bs,4);
  }

  for (i = 0; i < p_data->num_assoc_data_elements; i++)
  {
    p_data->assoc_data_element_tag_select[i] = Getbits(p_bs,4);
  }

  for (i = 0; i < p_data->num_valid_cc_elements; i++)
  {
    p_data->cc_element_is_ind_sw[i] = Getbits(p_bs,1);
    p_data->valid_cc_element_tag_select[i] = Getbits(p_bs,4);
  }

  Byte_alignment(p_bs);

  p_data->comment_field_bytes = Getbits(p_bs,8);
  for (i = 0; i < p_data->comment_field_bytes; i++)
  {
    p_data->comment_field_data[i] = (Ipp8s)Getbits(p_bs,8);
  }

  return p_data->element_instance_tag;
}

/***********************************************************************

    Unpack function(s) (support(s) alternative bitstream representation)

***********************************************************************/

Ipp32s
unpack_program_config_element(sProgram_config_element * p_data, Ipp8u **pp_bitstream, Ipp32s *p_offset)
{
  Ipp32s i;

//    get_bits(pp_bitstream,p_offset,

  p_data->element_instance_tag = get_bits(pp_bitstream,p_offset,4);
  p_data->object_type = get_bits(pp_bitstream,p_offset,2);
  p_data->sampling_frequency_index = get_bits(pp_bitstream,p_offset,4);

  p_data->num_front_channels = p_data->num_front_channel_elements = get_bits(pp_bitstream,p_offset,4);
  p_data->num_side_channels  = p_data->num_side_channel_elements = get_bits(pp_bitstream,p_offset,4);
  p_data->num_back_channels  = p_data->num_back_channel_elements = get_bits(pp_bitstream,p_offset,4);
  p_data->num_lfe_channels   = p_data->num_lfe_channel_elements = get_bits(pp_bitstream,p_offset,2);

  p_data->num_assoc_data_elements = get_bits(pp_bitstream,p_offset,3);
  p_data->num_valid_cc_elements = get_bits(pp_bitstream,p_offset,4);

  p_data->mono_mixdown_present = get_bits(pp_bitstream,p_offset,1);
  if (p_data->mono_mixdown_present == 1)
  {
    p_data->mono_miwdown_element_number = get_bits(pp_bitstream,p_offset,4);
  }

  p_data->stereo_mixdown_present = get_bits(pp_bitstream,p_offset,1);
  if (p_data->stereo_mixdown_present == 1)
  {
    p_data->stereo_miwdown_element_number = get_bits(pp_bitstream,p_offset,4);
  }

  p_data->matrix_mixdown_idx_present = get_bits(pp_bitstream,p_offset,1);
  if (p_data->matrix_mixdown_idx_present == 1)
  {
    p_data->matrix_mixdown_idx = get_bits(pp_bitstream,p_offset,2);
    p_data->pseudo_surround_enable = get_bits(pp_bitstream,p_offset,1);
  }

  for (i = 0; i < p_data->num_front_channel_elements; i++)
  {
    p_data->front_element_is_cpe[i] = get_bits(pp_bitstream,p_offset,1);
    p_data->front_element_tag_select[i] = get_bits(pp_bitstream,p_offset,4);
  }

  for (i = 0; i < p_data->num_side_channel_elements; i++)
  {
    p_data->side_element_is_cpe[i] = get_bits(pp_bitstream,p_offset,1);
    p_data->side_element_tag_select[i] = get_bits(pp_bitstream,p_offset,4);
  }

  for (i = 0; i < p_data->num_back_channel_elements; i++)
  {
    p_data->back_element_is_cpe[i] = get_bits(pp_bitstream,p_offset,1);
    p_data->back_element_tag_select[i] = get_bits(pp_bitstream,p_offset,4);
  }

  for (i = 0; i < p_data->num_lfe_channel_elements; i++)
  {
    p_data->lfe_element_tag_select[i] = get_bits(pp_bitstream,p_offset,4);
  }

  for (i = 0; i < p_data->num_assoc_data_elements; i++)
  {
    p_data->assoc_data_element_tag_select[i] = get_bits(pp_bitstream,p_offset,4);
  }

  for (i = 0; i < p_data->num_valid_cc_elements; i++)
  {
    p_data->cc_element_is_ind_sw[i] = get_bits(pp_bitstream,p_offset,1);
    p_data->valid_cc_element_tag_select[i] = get_bits(pp_bitstream,p_offset,4);
  }

  byte_alignment(pp_bitstream,p_offset);

  p_data->comment_field_bytes = get_bits(pp_bitstream,p_offset,8);
  for (i = 0; i < p_data->comment_field_bytes; i++)
  {
    p_data->comment_field_data[i] = (Ipp8s)get_bits(pp_bitstream,p_offset,8);
  }

  return p_data->element_instance_tag;
}

#endif //UMC_ENABLE_XXX
