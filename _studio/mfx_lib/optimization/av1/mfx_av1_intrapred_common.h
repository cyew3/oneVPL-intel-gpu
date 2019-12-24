// Copyright (c) 2019 Intel Corporation
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
#include "limits.h"
#include "mfx_av1_opts_common.h"

namespace AV1PP {

    enum { TX_4X4 = 0, TX_8X8 = 1, TX_16X16 = 2, TX_32X32 = 3, TX_SIZES = 4 };

    enum {
        DC_PRED = 0, V_PRED = 1, H_PRED = 2, D45_PRED = 3, D135_PRED = 4, D117_PRED = 5,
        D153_PRED = 6, D207_PRED = 7, D63_PRED = 8, SMOOTH_PRED = 9,
        SMOOTH_V_PRED = 10, SMOOTH_H_PRED = 11, PAETH_PRED = 12
    };

    const uint8_t mode_to_angle_map[] = { 0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0 };

    const int16_t dr_intra_derivative[90] = {
        0,    0, 0,        //
        1023, 0, 0,        // 3, ...
        547,  0, 0,        // 6, ...
        372,  0, 0, 0, 0,  // 9, ...
        273,  0, 0,        // 14, ...
        215,  0, 0,        // 17, ...
        178,  0, 0,        // 20, ...
        151,  0, 0,        // 23, ... (113 & 203 are base angles)
        132,  0, 0,        // 26, ...
        116,  0, 0,        // 29, ...
        102,  0, 0, 0,     // 32, ...
        90,   0, 0,        // 36, ...
        80,   0, 0,        // 39, ...
        71,   0, 0,        // 42, ...
        64,   0, 0,        // 45, ... (45 & 135 are base angles)
        57,   0, 0,        // 48, ...
        51,   0, 0,        // 51, ...
        45,   0, 0, 0,     // 54, ...
        40,   0, 0,        // 58, ...
        35,   0, 0,        // 61, ...
        31,   0, 0,        // 64, ...
        27,   0, 0,        // 67, ... (67 & 157 are base angles)
        23,   0, 0,        // 70, ...
        19,   0, 0,        // 73, ...
        15,   0, 0, 0, 0,  // 76, ...
        11,   0, 0,        // 81, ...
        7,    0, 0,        // 84, ...
        3,    0, 0,        // 87, ...
    };

    // Get the shift (up-scaled by 256) in X w.r.t a unit change in Y.
    // If angle > 0 && angle < 90, dx = -((int)(256 / t));
    // If angle > 90 && angle < 180, dx = (int)(256 / t);
    // If angle > 180 && angle < 270, dx = 1;
    inline int get_dx(int angle)
    {
        if (angle > 0 && angle < 90) return dr_intra_derivative[angle];
        else if (angle > 90 && angle < 180) return dr_intra_derivative[180 - angle];
        else return 1; // In this case, we are not really going to use dx. We may return any value.
    }

    // Get the shift (up-scaled by 256) in Y w.r.t a unit change in X.
    // If angle > 0 && angle < 90, dy = 1;
    // If angle > 90 && angle < 180, dy = (int)(256 * t);
    // If angle > 180 && angle < 270, dy = -((int)(256 * t));
    inline int get_dy(int angle)
    {
        if (angle > 90 && angle < 180) return dr_intra_derivative[angle - 90];
        else if (angle > 180 && angle < 270) return dr_intra_derivative[270 - angle];
        else return 1; // In this case, we are not really going to use dy. We may return any value.
    }

    alignas(16) static const uint8_t smooth_weights_4[4] = {
        255, 149, 85, 64
    };

    alignas(16) static const uint8_t smooth_weights_8[8] = {
        255, 197, 146, 105, 73, 50, 37, 32
    };

    alignas(32) static const uint8_t smooth_weights_16[32] = {
        255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16,
        255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16
    };

    alignas(32) static const uint8_t smooth_weights_32[32] = {
        255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92,  83,  74,
        66,  59,  52,  45,  39,  34,  29,  25,  21,  17,  14,  12,  10,  9,   8,   8
    };

    alignas(32) static const uint8_t z2_blend_mask[] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
    };

};  // namesapce AV1PP
