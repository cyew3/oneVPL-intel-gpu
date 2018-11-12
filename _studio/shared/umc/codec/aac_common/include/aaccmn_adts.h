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

#ifndef __AACCMN_ADTS_H
#define __AACCMN_ADTS_H

#include "bstream.h"

typedef struct
{
  Ipp32s ID;
  Ipp32s Layer;
  Ipp32s protection_absent;
  Ipp32s Profile;
  Ipp32s sampling_frequency_index;
  Ipp32s private_bit;
  Ipp32s channel_configuration;
  Ipp32s original_copy;
  Ipp32s Home;
  Ipp32s Emphasis;

} sAdts_fixed_header;

typedef struct
{
  Ipp32s copyright_identification_bit;
  Ipp32s copyright_identification_start;
  Ipp32s aac_frame_length;
  Ipp32s adts_buffer_fullness;
  Ipp32s no_raw_data_blocks_in_frame;

} sAdts_variable_header;

#ifdef  __cplusplus
extern "C" {
#endif

Ipp32s  dec_adts_fixed_header(sAdts_fixed_header* pHeader,sBitsreamBuffer* BS);
Ipp32s  dec_adts_variable_header(sAdts_variable_header* pHeader,sBitsreamBuffer* BS);

Ipp32s unpack_adts_fixed_header(sAdts_fixed_header* p_header,Ipp8u **pp_bitstream, Ipp32s *p_offset);
Ipp32s unpack_adts_variable_header(sAdts_variable_header* p_header,Ipp8u **pp_bitstream, Ipp32s *p_offset);

Ipp32s get_audio_object_type_by_adts_header(sAdts_fixed_header* p_header);

#ifdef  __cplusplus
}
#endif

#endif//__AACCMN_ADTS_H
