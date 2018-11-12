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

#ifndef __AACCMN_ADIF_H
#define __AACCMN_ADIF_H

#include "mp4cmn_pce.h"

enum eAACCMN_ADIF_H
{
    ADIF_SIGNATURE   = 0x41444946,
    LEN_COPYRIGHT_ID = ((72/8)),
    MAX_PCE_NUM      = 16
};

typedef struct
{
  Ipp32u adif_id;
  Ipp32s copyright_id_present;
  Ipp8s  copyright_id[LEN_COPYRIGHT_ID+1];
  Ipp32s original_copy;
  Ipp32s home;
  Ipp32s bitstream_type;
  Ipp32u bitrate;
  Ipp32s num_program_config_elements;
  Ipp32u adif_buffer_fullness;

  sProgram_config_element pce[MAX_PCE_NUM];
} sAdif_header;

#ifdef  __cplusplus
extern "C" {
#endif

Ipp32s dec_adif_header(sAdif_header* p_adif_header,sBitsreamBuffer* p_bs);

Ipp32s unpack_adif_header(sAdif_header* p_data,Ipp8u **pp_bitstream, Ipp32s* p_offset);

#ifdef  __cplusplus
}
#endif

#endif//__AACCMN_ADIF_H
