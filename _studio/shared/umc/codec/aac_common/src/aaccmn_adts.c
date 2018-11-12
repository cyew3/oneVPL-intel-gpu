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
#if defined (UMC_ENABLE_AAC_AUDIO_DECODER)

#include "aaccmn_adts.h"
#include "bstream.h"
#include "aaccmn_const.h"

Ipp32s
dec_adts_fixed_header(sAdts_fixed_header* pHeader,sBitsreamBuffer* BS)
{
  Ipp32s syncword;

  syncword = Getbits(BS,12);
  if (syncword != 0x0FFF)
  {
    return 1;
  }

  pHeader->ID =  Getbits(BS,1);
  pHeader->Layer = Getbits(BS,2);
  pHeader->protection_absent = Getbits(BS,1);
  pHeader->Profile = Getbits(BS,2);
  pHeader->sampling_frequency_index = Getbits(BS,4);
  pHeader->private_bit = Getbits(BS,1);
  pHeader->channel_configuration = Getbits(BS,3);
  pHeader->original_copy = Getbits(BS,1);
  pHeader->Home = Getbits(BS,1);
//  pHeader->Emphasis = Getbits(BS,2);
  return 0;
}

Ipp32s
dec_adts_variable_header(sAdts_variable_header* pHeader,sBitsreamBuffer* BS)
{
  pHeader->copyright_identification_bit = Getbits(BS,1);
  pHeader->copyright_identification_start =  Getbits(BS,1);
  pHeader->aac_frame_length = Getbits(BS,13);
  pHeader->adts_buffer_fullness = Getbits(BS,11);
  pHeader->no_raw_data_blocks_in_frame = Getbits(BS,2);

  return 0;
}

/***********************************************************************

    Unpack function(s) (support(s) alternative bitstream representation)

***********************************************************************/

Ipp32s
unpack_adts_fixed_header(sAdts_fixed_header* p_header,Ipp8u **pp_bitstream, Ipp32s *p_offset)
{
  Ipp32s syncword;

  syncword = get_bits(pp_bitstream,p_offset,12);
  if (syncword != 0x0FFF)
  {
    return 1;
  }

  p_header->ID =  get_bits(pp_bitstream,p_offset,1);
  p_header->Layer = get_bits(pp_bitstream,p_offset,2);
  p_header->protection_absent = get_bits(pp_bitstream,p_offset,1);
  p_header->Profile = get_bits(pp_bitstream,p_offset,2);
  p_header->sampling_frequency_index = get_bits(pp_bitstream,p_offset,4);
  p_header->private_bit = get_bits(pp_bitstream,p_offset,1);
  p_header->channel_configuration = get_bits(pp_bitstream,p_offset,3);
  p_header->original_copy = get_bits(pp_bitstream,p_offset,1);
  p_header->Home = get_bits(pp_bitstream,p_offset,1);
//  p_header->Emphasis = get_bits(pp_bitstream,p_offset,2);
  return 0;
}

Ipp32s
unpack_adts_variable_header(sAdts_variable_header* p_header,Ipp8u **pp_bitstream, Ipp32s *p_offset)
{
  p_header->copyright_identification_bit = get_bits(pp_bitstream,p_offset,1);
  p_header->copyright_identification_start =  get_bits(pp_bitstream,p_offset,1);
  p_header->aac_frame_length = get_bits(pp_bitstream,p_offset,13);
  p_header->adts_buffer_fullness = get_bits(pp_bitstream,p_offset,11);
  p_header->no_raw_data_blocks_in_frame = get_bits(pp_bitstream,p_offset,2);

  return 0;
}

static Ipp32s g_adts_profile_table[4][2] =
{
    /*MPEG-2  ID == 1*/ /*MPEG-4  ID == 0*/
    {AOT_AAC_MAIN,      AOT_AAC_MAIN},
    {AOT_AAC_LC,        AOT_AAC_LC },
    {AOT_AAC_SSR,       AOT_AAC_SSR },
    {AOT_UNDEF,         AOT_AAC_LTP }
};

Ipp32s
get_audio_object_type_by_adts_header(sAdts_fixed_header* p_header)
{
    Ipp32s res;

    if (p_header->ID == 1)
    {
        res = g_adts_profile_table[p_header->Profile][0];
    }
    else
    {
        res = g_adts_profile_table[p_header->Profile][1];
    }

    return res;
}

#else

#ifdef _MSVC_LANG
#pragma warning( disable: 4206 )
#endif

#endif //UMC_ENABLE_XXX
