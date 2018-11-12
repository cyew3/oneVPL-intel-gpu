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

#ifndef __SAACDEC_LTP_H
#define __SAACDEC_LTP_H

#include "aac_filterbank_fp.h"
#include "aac_dec_tns_fp.h"
#include "aac_dec_own.h"

typedef struct
{
    Ipp32f* p_samples_1st_part;
    Ipp32f* p_samples_2nd_part;
    Ipp32f* p_samples_3rd_part;
    Ipp32s  prev_windows_shape;

    sFilterbank* p_filterbank_data;
    s_tns_data*  p_tns_data;

} sLtp;

#ifdef  __cplusplus
extern "C" {
#endif

void ics_apply_ltp_I(sLtp* p_data, s_SE_Individual_channel_stream * p_stream, Ipp32f * p_spectrum);

#ifdef  __cplusplus
}
#endif

#ifndef PI
#define PI 3.14159265359f
#endif

#endif//__SAACDEC_LTP_H
