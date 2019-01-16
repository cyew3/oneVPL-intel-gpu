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

typedef unsigned char uint8_t;

namespace VP9PP {
    template <int size, int haveLeft, int haveAbove> void predict_intra_dc_av1_px  (const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int);
    template <int size, int haveLeft, int haveAbove> void predict_intra_dc_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int);
    template <int size, int mode> void predict_intra_av1_px  (const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int delta, int upTop, int upLeft);
    template <int size, int mode> void predict_intra_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int delta, int upTop, int upLeft);

    template <int w, int h> int satd_px  (const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2);
    template <int w, int h> int satd_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2);
    template <int w, int h> int sad_general_px  (const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    template <int w, int h> int sad_general_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
};
