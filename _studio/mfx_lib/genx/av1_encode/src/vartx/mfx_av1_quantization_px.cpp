// Copyright (c) 2014-2019 Intel Corporation
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

#include "assert.h"
#include "string.h"
#include "stdlib.h"

#include <stdint.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

namespace VP9PP
{
    typedef int16_t tran_low_t;

    static inline int clamp(int value, int low, int high) {
        return value < low ? low : (value > high ? high : value);
    }


    template <int txSize> int quant_px(const int16_t *coeff_ptr, int16_t *qcoeff_ptr, const int16_t fqpar[8])
    {
        const int16_t *zbin_ptr = fqpar;
        const int16_t *round_ptr = fqpar+2;
        const int16_t *quant_ptr = fqpar+4;
        const int16_t *quant_shift_ptr = fqpar+6;
        const int n_coeffs = 16<<(txSize<<1);
        memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
        int eob = 0;

        int16_t zbin[2] = { zbin_ptr[0], zbin_ptr[1] };
        int16_t round[2] = { round_ptr[0], round_ptr[1] };
        int16_t quant_shift[2] = { quant_shift_ptr[0], quant_shift_ptr[1] };
        if (txSize == 3) {
            zbin[0] = (zbin[0] + 1) >> 1;
            zbin[1] = (zbin[1] + 1) >> 1;
            round[0] = (round[0] + 1) >> 1;
            round[1] = (round[1] + 1) >> 1;
            quant_shift[0] = quant_shift[0] << 1;
            quant_shift[1] = quant_shift[1] << 1;
        }

        int coeff = coeff_ptr[0];
        int coeff_sign = (coeff >> 31);
        int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

        if (abs_coeff >= zbin[0]) {
            int tmp = clamp(abs_coeff + round[0], INT16_MIN, INT16_MAX);
            tmp = ((((tmp * quant_ptr[0]) >> 16) + tmp) * quant_shift[0]) >> 16;  // quantization
            qcoeff_ptr[0]  = (tmp ^ coeff_sign) - coeff_sign;
            eob = tmp!=0;
        }

        for (int i = 1; i < n_coeffs; i++) {
            coeff = coeff_ptr[i];
            coeff_sign = (coeff >> 31);
            abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

            if (abs_coeff >= zbin[1]) {
                int tmp = clamp(abs_coeff + round[1], INT16_MIN, INT16_MAX);
                tmp = ((((tmp * quant_ptr[1]) >> 16) + tmp) * quant_shift[1]) >> 16;  // quantization
                qcoeff_ptr[i]  = (tmp ^ coeff_sign) - coeff_sign;
                if (tmp)
                    eob++;
            }
        }
        return eob;
    }
    template int quant_px<0>(const int16_t*,int16_t*,const int16_t*);
    template int quant_px<1>(const int16_t*,int16_t*,const int16_t*);
    template int quant_px<2>(const int16_t*,int16_t*,const int16_t*);
    template int quant_px<3>(const int16_t*,int16_t*,const int16_t*);

    template <int txSize> void dequant_px(const int16_t* src, int16_t* dst, const int16_t *scales)
    {
        const int len = 16<<(txSize<<1);
        int shift = len == 1024 ? 1 : 0;

        int aval = abs((int)src[0]);        // remove sign
        int sign = src[0] >> 15;
        int coeffQ = (((aval * scales[0]) >> shift) ^ sign) - sign;
        // clipped when decoded ???
        dst[0] = (int16_t)Saturate(-32768, 32767, coeffQ);

#pragma ivdep
#pragma vector always
        for (int i = 1; i < len; i++) {
            int aval = abs((int)src[i]);        // remove sign
            int sign = src[i] >> 15;
            int coeffQ = (((aval * scales[1]) >> shift) ^ sign) - sign;
            // clipped when decoded ???
            dst[i] = (int16_t)Saturate(-32768, 32767, coeffQ);
        }
    }
    template void dequant_px<0>(const int16_t*,int16_t*,const int16_t*);
    template void dequant_px<1>(const int16_t*,int16_t*,const int16_t*);
    template void dequant_px<2>(const int16_t*,int16_t*,const int16_t*);
    template void dequant_px<3>(const int16_t*,int16_t*,const int16_t*);

    template <int txSize> int quant_dequant_px(int16_t *coef, int16_t *qcoef, const int16_t qpar[10])
    {
        int eob = quant_px<txSize>(coef, qcoef, qpar);
        dequant_px<txSize>(qcoef, coef, qpar + 8);
        return eob;
    }
    template int quant_dequant_px<0>(int16_t*,int16_t*,const int16_t*);
    template int quant_dequant_px<1>(int16_t*,int16_t*,const int16_t*);
    template int quant_dequant_px<2>(int16_t*,int16_t*,const int16_t*);
    template int quant_dequant_px<3>(int16_t*,int16_t*,const int16_t*);
};
