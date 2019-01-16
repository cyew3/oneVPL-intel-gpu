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

#include "mfx_av1_opts_common.h"

typedef int int32_t;

namespace VP9PP {

template <int plane, typename PixType>
void GetPredPelsAV1(const PixType *rec, int pitch, PixType *topPels, PixType *leftPels, int width,
                    int haveTop, int haveLeft, int pixTopRight, int pixBottomLeft);


template <typename PixType, int W = 32>
struct IntraPredPels {
private:
    static const int32_t OFFSET = 32 / sizeof(PixType);

    ALIGN_DECL(32) PixType topData [2 * W + OFFSET];
    ALIGN_DECL(32) PixType leftData[2 * W + OFFSET];

public:
    IntraPredPels() : top(topData + OFFSET), left(leftData + OFFSET) {}

    PixType * const top;
    PixType * const left;
};

};  // namesapce VP9PP
