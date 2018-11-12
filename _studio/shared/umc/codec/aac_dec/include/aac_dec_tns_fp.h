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

#ifndef __SAACDEC_TNS_H
#define __SAACDEC_TNS_H

#include "aaccmn_const.h"
#include "aac_dec_own.h"

typedef struct
{
    Ipp32f   m_lpc[MAX_NUM_WINDOWS][MAX_FILT][TNS_MAX_ORDER];
    Ipp32s   m_start[MAX_NUM_WINDOWS][MAX_FILT];
    Ipp32s   m_size[MAX_NUM_WINDOWS][MAX_FILT];
    Ipp32s   m_order[MAX_NUM_WINDOWS][MAX_FILT];
    Ipp32s   m_inc[MAX_NUM_WINDOWS][MAX_FILT];

    Ipp32s   m_num_windows;
    Ipp32s   m_n_filt[MAX_NUM_WINDOWS];
    Ipp32s   m_tns_data_present;
} s_tns_data;

#ifdef  __cplusplus
extern "C" {
#endif

void ics_calc_tns_data(s_SE_Individual_channel_stream *p_stream, s_tns_data *p_data);
void ics_apply_tns_enc_I(s_tns_data* p_data, Ipp32f *p_spectrum);
void ics_apply_tns_dec_I(s_tns_data* p_data, Ipp32f *p_spectrum);

#ifdef  __cplusplus
}
#endif


#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef PI
#define PI 3.14159265359f
#endif

#endif//__SAACDEC_LTP_H
