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

#pragma once

#include "mfx_av1_dispatcher_fptr.h"

namespace VP9PP {
    inline void predict_intra_av1(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int txSize, int haveLeft, int haveAbove,
                                  int mode, int delta, int upTop, int upLeft) {
        assert(txSize >= 0 && txSize <= 3);
        assert(haveLeft == 0 || haveLeft == 1);
        assert(haveAbove == 0 || haveAbove == 1);
        assert(mode >= 0 && mode <= 12);
        assert(delta >= -3 && delta <= 3);
        assert(predict_intra_av1_fptr_arr[txSize][haveLeft][haveAbove][mode] != NULL);
        predict_intra_av1_fptr_arr[txSize][haveLeft][haveAbove][mode](topPels, leftPels, dst, pitch, delta, upTop, upLeft);
    }

    inline int sad_general(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sad_general_fptr_arr[log2w][log2h] != NULL);
        return sad_general_fptr_arr[log2w][log2h](src1, pitch1, src2, pitch2);
    }
    inline int satd(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(satd_fptr_arr[log2w][log2h] != NULL);
        return satd_fptr_arr[log2w][log2h](src1, pitch1, src2, pitch2);
    }
};

