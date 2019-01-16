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

#include "memory.h"
#include "mfx_av1_transform_common.h"

namespace VP9PP {

    static const char inv_shift_4x4[2] = { 0, -4 };
    static const char inv_shift_8x8[2] = { -1, -4 };
    static const char inv_shift_16x16[2] = { -2, -4 };
    static const char inv_shift_32x32[2] = { -2, -4 };
    static const char inv_shift_64x64[2] = { -2, -4 };
    static const char inv_shift_4x8[2] = { 0, -4 };
    static const char inv_shift_8x4[2] = { 0, -4 };
    static const char inv_shift_8x16[2] = { -1, -4 };
    static const char inv_shift_16x8[2] = { -1, -4 };
    static const char inv_shift_16x32[2] = { -1, -4 };
    static const char inv_shift_32x16[2] = { -1, -4 };
    static const char inv_shift_32x64[2] = { -1, -4 };
    static const char inv_shift_64x32[2] = { -1, -4 };
    static const char inv_shift_4x16[2] = { -1, -4 };
    static const char inv_shift_16x4[2] = { -1, -4 };
    static const char inv_shift_8x32[2] = { -2, -4 };
    static const char inv_shift_32x8[2] = { -2, -4 };
    static const char inv_shift_16x64[2] = { -2, -4 };
    static const char inv_shift_64x16[2] = { -2, -4 };

    static const char *inv_txfm_shift_ls[TX_SIZES_ALL] = {
        inv_shift_4x4,   inv_shift_8x8,   inv_shift_16x16, inv_shift_32x32,
        inv_shift_64x64, inv_shift_4x8,   inv_shift_8x4,   inv_shift_8x16,
        inv_shift_16x8,  inv_shift_16x32, inv_shift_32x16, inv_shift_32x64,
        inv_shift_64x32, inv_shift_4x16,  inv_shift_16x4,  inv_shift_8x32,
        inv_shift_32x8,  inv_shift_16x64, inv_shift_64x16,
    };

};  // namesapce VP9PP
