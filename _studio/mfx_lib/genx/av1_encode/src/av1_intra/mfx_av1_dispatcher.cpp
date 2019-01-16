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

#include "mfx_av1_dispatcher_proto.h"
#include "mfx_av1_dispatcher_fptr.h"

typedef int int32_t;

namespace VP9PP {
    void SetTargetAVX2(int32_t isAV1);
    void SetTargetPX(int32_t isAV1);

    enum { TX_4X4, TX_8X8, TX_16X16, TX_32X32 };
    enum { DC_PRED=0, V_PRED=1, H_PRED=2, D45_PRED=3, D135_PRED=4, D117_PRED=5, D153_PRED=6, D207_PRED=7, D63_PRED=8,
           SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12, AV1_INTRA_MODES=PAETH_PRED+1 };

    void SetTargetPX(int32_t isAV1)
    {
        if (isAV1) {
            // luma intra pred [txSize] [haveLeft] [haveAbove] [mode]
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_4X4,0,0>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_4X4,0,1>;
            predict_intra_av1_fptr_arr[TX_4X4  ][1][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_4X4,1,0>;
            predict_intra_av1_fptr_arr[TX_4X4  ][1][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_4X4,1,1>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_8X8,0,0>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_8X8,0,1>;
            predict_intra_av1_fptr_arr[TX_8X8  ][1][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_8X8,1,0>;
            predict_intra_av1_fptr_arr[TX_8X8  ][1][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_8X8,1,1>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_16X16,0,0>;
            predict_intra_av1_fptr_arr[TX_16X16][0][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_16X16,0,1>;
            predict_intra_av1_fptr_arr[TX_16X16][1][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_16X16,1,0>;
            predict_intra_av1_fptr_arr[TX_16X16][1][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_16X16,1,1>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_32X32,0,0>;
            predict_intra_av1_fptr_arr[TX_32X32][0][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_32X32,0,1>;
            predict_intra_av1_fptr_arr[TX_32X32][1][0][DC_PRED    ] = predict_intra_dc_av1_px<TX_32X32,1,0>;
            predict_intra_av1_fptr_arr[TX_32X32][1][1][DC_PRED    ] = predict_intra_dc_av1_px<TX_32X32,1,1>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][V_PRED     ] = predict_intra_av1_px<TX_4X4, V_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][H_PRED     ] = predict_intra_av1_px<TX_4X4, H_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D45_PRED   ] = predict_intra_av1_px<TX_4X4, D45_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D135_PRED  ] = predict_intra_av1_px<TX_4X4, D135_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D117_PRED  ] = predict_intra_av1_px<TX_4X4, D117_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D153_PRED  ] = predict_intra_av1_px<TX_4X4, D153_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D207_PRED  ] = predict_intra_av1_px<TX_4X4, D207_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D63_PRED   ] = predict_intra_av1_px<TX_4X4, D63_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_4X4, SMOOTH_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_4X4, SMOOTH_V_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_4X4, SMOOTH_H_PRED>;
            predict_intra_av1_fptr_arr[TX_4X4  ][0][0][PAETH_PRED ] = predict_intra_av1_px<TX_4X4, PAETH_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][V_PRED     ] = predict_intra_av1_px<TX_8X8, V_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][H_PRED     ] = predict_intra_av1_px<TX_8X8, H_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D45_PRED   ] = predict_intra_av1_px<TX_8X8, D45_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D135_PRED  ] = predict_intra_av1_px<TX_8X8, D135_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D117_PRED  ] = predict_intra_av1_px<TX_8X8, D117_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D153_PRED  ] = predict_intra_av1_px<TX_8X8, D153_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D207_PRED  ] = predict_intra_av1_px<TX_8X8, D207_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D63_PRED   ] = predict_intra_av1_px<TX_8X8, D63_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_8X8, SMOOTH_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_8X8, SMOOTH_V_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_8X8, SMOOTH_H_PRED>;
            predict_intra_av1_fptr_arr[TX_8X8  ][0][0][PAETH_PRED ] = predict_intra_av1_px<TX_8X8, PAETH_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][V_PRED     ] = predict_intra_av1_px<TX_16X16, V_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][H_PRED     ] = predict_intra_av1_px<TX_16X16, H_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][D45_PRED   ] = predict_intra_av1_px<TX_16X16, D45_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][D135_PRED  ] = predict_intra_av1_px<TX_16X16, D135_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][D117_PRED  ] = predict_intra_av1_px<TX_16X16, D117_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][D153_PRED  ] = predict_intra_av1_px<TX_16X16, D153_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][D207_PRED  ] = predict_intra_av1_px<TX_16X16, D207_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][D63_PRED   ] = predict_intra_av1_px<TX_16X16, D63_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_16X16, SMOOTH_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_16X16, SMOOTH_V_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_16X16, SMOOTH_H_PRED>;
            predict_intra_av1_fptr_arr[TX_16X16][0][0][PAETH_PRED ] = predict_intra_av1_px<TX_16X16, PAETH_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][V_PRED     ] = predict_intra_av1_px<TX_32X32, V_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][H_PRED     ] = predict_intra_av1_px<TX_32X32, H_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][D45_PRED   ] = predict_intra_av1_px<TX_32X32, D45_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][D135_PRED  ] = predict_intra_av1_px<TX_32X32, D135_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][D117_PRED  ] = predict_intra_av1_px<TX_32X32, D117_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][D153_PRED  ] = predict_intra_av1_px<TX_32X32, D153_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][D207_PRED  ] = predict_intra_av1_px<TX_32X32, D207_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][D63_PRED   ] = predict_intra_av1_px<TX_32X32, D63_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_32X32, SMOOTH_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_32X32, SMOOTH_V_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_32X32, SMOOTH_H_PRED>;
            predict_intra_av1_fptr_arr[TX_32X32][0][0][PAETH_PRED ] = predict_intra_av1_px<TX_32X32, PAETH_PRED>;
            for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
                for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
                    predict_intra_av1_fptr_arr[txSize][0][1][mode] =
                    predict_intra_av1_fptr_arr[txSize][1][0][mode] =
                    predict_intra_av1_fptr_arr[txSize][1][1][mode] = predict_intra_av1_fptr_arr[txSize][0][0][mode];
                }
            }
        } else {
        }
        satd_fptr_arr[0][0] = satd_px<4, 4>;
        satd_fptr_arr[0][1] = satd_px<4, 8>;
        satd_fptr_arr[0][2] = nullptr;
        satd_fptr_arr[0][3] = nullptr;
        satd_fptr_arr[0][4] = nullptr;
        satd_fptr_arr[1][0] = satd_px<8, 4>;
        satd_fptr_arr[1][1] = satd_px<8, 8>;
        satd_fptr_arr[1][2] = satd_px<8, 16>;
        satd_fptr_arr[1][3] = nullptr;
        satd_fptr_arr[1][4] = nullptr;
        satd_fptr_arr[2][0] = nullptr;
        satd_fptr_arr[2][1] = satd_px<16,8>;
        satd_fptr_arr[2][2] = satd_px<16,16>;
        satd_fptr_arr[2][3] = satd_px<16,32>;
        satd_fptr_arr[2][4] = nullptr;
        satd_fptr_arr[3][0] = nullptr;
        satd_fptr_arr[3][1] = nullptr;
        satd_fptr_arr[3][2] = satd_px<32,16>;
        satd_fptr_arr[3][3] = satd_px<32,32>;
        satd_fptr_arr[3][4] = satd_px<32,64>;
        satd_fptr_arr[4][0] = nullptr;
        satd_fptr_arr[4][1] = nullptr;
        satd_fptr_arr[4][2] = nullptr;
        satd_fptr_arr[4][3] = satd_px<64,32>;
        satd_fptr_arr[4][4] = satd_px<64,64>;
        sad_general_fptr_arr[0][0] = sad_general_px<4, 4>;
        sad_general_fptr_arr[0][1] = sad_general_px<4, 8>;
        sad_general_fptr_arr[0][2] = nullptr;
        sad_general_fptr_arr[0][3] = nullptr;
        sad_general_fptr_arr[0][4] = nullptr;
        sad_general_fptr_arr[1][0] = sad_general_px<8, 4>;
        sad_general_fptr_arr[1][1] = sad_general_px<8, 8>;
        sad_general_fptr_arr[1][2] = sad_general_px<8, 16>;
        sad_general_fptr_arr[1][3] = nullptr;
        sad_general_fptr_arr[1][4] = nullptr;
        sad_general_fptr_arr[2][0] = nullptr;
        sad_general_fptr_arr[2][1] = sad_general_px<16, 8>;
        sad_general_fptr_arr[2][2] = sad_general_px<16, 16>;
        sad_general_fptr_arr[2][3] = sad_general_px<16, 32>;
        sad_general_fptr_arr[2][4] = nullptr;
        sad_general_fptr_arr[3][0] = nullptr;
        sad_general_fptr_arr[3][1] = nullptr;
        sad_general_fptr_arr[3][2] = sad_general_px<32, 16>;
        sad_general_fptr_arr[3][3] = sad_general_px<32, 32>;
        sad_general_fptr_arr[3][4] = sad_general_px<32, 64>;
        sad_general_fptr_arr[4][0] = nullptr;
        sad_general_fptr_arr[4][1] = nullptr;
        sad_general_fptr_arr[4][2] = nullptr;
        sad_general_fptr_arr[4][3] = sad_general_px<64, 32>;
        sad_general_fptr_arr[4][4] = sad_general_px<64, 64>;
    }
    void SetTargetAVX2(int32_t isAV1) {
        // luma intra pred [txSize] [haveLeft] [haveAbove] [mode]
        //if (isAV1) {
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_4X4,0,0>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_4X4,0,1>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][1][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_4X4,1,0>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][1][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_4X4,1,1>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_8X8,0,0>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_8X8,0,1>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][1][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_8X8,1,0>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][1][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_8X8,1,1>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_16X16,0,0>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_16X16,0,1>;
        //    predict_intra_av1_fptr_arr[TX_16X16][1][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_16X16,1,0>;
        //    predict_intra_av1_fptr_arr[TX_16X16][1][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_16X16,1,1>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_32X32,0,0>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_32X32,0,1>;
        //    predict_intra_av1_fptr_arr[TX_32X32][1][0][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_32X32,1,0>;
        //    predict_intra_av1_fptr_arr[TX_32X32][1][1][DC_PRED      ] = predict_intra_dc_av1_avx2<TX_32X32,1,1>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][V_PRED       ] = predict_intra_av1_avx2<TX_4X4, V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][H_PRED       ] = predict_intra_av1_avx2<TX_4X4, H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D45_PRED     ] = predict_intra_av1_avx2<TX_4X4, D45_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D135_PRED    ] = predict_intra_av1_avx2<TX_4X4, D135_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D117_PRED    ] = predict_intra_av1_avx2<TX_4X4, D117_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D153_PRED    ] = predict_intra_av1_avx2<TX_4X4, D153_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D207_PRED    ] = predict_intra_av1_avx2<TX_4X4, D207_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][D63_PRED     ] = predict_intra_av1_avx2<TX_4X4, D63_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][SMOOTH_PRED  ] = predict_intra_av1_avx2<TX_4X4, SMOOTH_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_4X4  ][0][0][PAETH_PRED   ] = predict_intra_av1_avx2<TX_4X4, PAETH_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][V_PRED       ] = predict_intra_av1_avx2<TX_8X8, V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][H_PRED       ] = predict_intra_av1_avx2<TX_8X8, H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D45_PRED     ] = predict_intra_av1_avx2<TX_8X8, D45_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D135_PRED    ] = predict_intra_av1_avx2<TX_8X8, D135_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D117_PRED    ] = predict_intra_av1_avx2<TX_8X8, D117_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D153_PRED    ] = predict_intra_av1_avx2<TX_8X8, D153_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D207_PRED    ] = predict_intra_av1_avx2<TX_8X8, D207_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][D63_PRED     ] = predict_intra_av1_avx2<TX_8X8, D63_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][SMOOTH_PRED  ] = predict_intra_av1_avx2<TX_8X8, SMOOTH_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_8X8  ][0][0][PAETH_PRED   ] = predict_intra_av1_avx2<TX_8X8, PAETH_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][V_PRED       ] = predict_intra_av1_avx2<TX_16X16, V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][H_PRED       ] = predict_intra_av1_avx2<TX_16X16, H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][D45_PRED     ] = predict_intra_av1_avx2<TX_16X16, D45_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][D135_PRED    ] = predict_intra_av1_avx2<TX_16X16, D135_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][D117_PRED    ] = predict_intra_av1_avx2<TX_16X16, D117_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][D153_PRED    ] = predict_intra_av1_avx2<TX_16X16, D153_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][D207_PRED    ] = predict_intra_av1_avx2<TX_16X16, D207_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][D63_PRED     ] = predict_intra_av1_avx2<TX_16X16, D63_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_PRED  ] = predict_intra_av1_avx2<TX_16X16, SMOOTH_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_16X16][0][0][PAETH_PRED   ] = predict_intra_av1_avx2<TX_16X16, PAETH_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][V_PRED       ] = predict_intra_av1_avx2<TX_32X32, V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][H_PRED       ] = predict_intra_av1_avx2<TX_32X32, H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][D45_PRED     ] = predict_intra_av1_avx2<TX_32X32, D45_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][D135_PRED    ] = predict_intra_av1_avx2<TX_32X32, D135_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][D117_PRED    ] = predict_intra_av1_avx2<TX_32X32, D117_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][D153_PRED    ] = predict_intra_av1_avx2<TX_32X32, D153_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][D207_PRED    ] = predict_intra_av1_avx2<TX_32X32, D207_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][D63_PRED     ] = predict_intra_av1_avx2<TX_32X32, D63_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_PRED  ] = predict_intra_av1_avx2<TX_32X32, SMOOTH_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_V_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_H_PRED>;
        //    predict_intra_av1_fptr_arr[TX_32X32][0][0][PAETH_PRED   ] = predict_intra_av1_avx2<TX_32X32, PAETH_PRED>;
        //    for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
        //        for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
        //            predict_intra_av1_fptr_arr[txSize][0][1][mode] =
        //            predict_intra_av1_fptr_arr[txSize][1][0][mode] =
        //            predict_intra_av1_fptr_arr[txSize][1][1][mode] = predict_intra_av1_fptr_arr[txSize][0][0][mode];
        //        }
        //    }
        //} else {
        //}
        //satd_fptr_arr[0][0] = satd_avx2<4, 4>;
        //satd_fptr_arr[0][1] = satd_avx2<4, 8>;
        //satd_fptr_arr[0][2] = nullptr;
        //satd_fptr_arr[0][3] = nullptr;
        //satd_fptr_arr[0][4] = nullptr;
        //satd_fptr_arr[1][0] = satd_avx2<8, 4>;
        //satd_fptr_arr[1][1] = satd_avx2<8, 8>;
        //satd_fptr_arr[1][2] = satd_avx2<8, 16>;
        //satd_fptr_arr[1][3] = nullptr;
        //satd_fptr_arr[1][4] = nullptr;
        //satd_fptr_arr[2][0] = nullptr;
        //satd_fptr_arr[2][1] = satd_avx2<16,8>;
        //satd_fptr_arr[2][2] = satd_avx2<16,16>;
        //satd_fptr_arr[2][3] = satd_avx2<16,32>;
        //satd_fptr_arr[2][4] = nullptr;
        //satd_fptr_arr[3][0] = nullptr;
        //satd_fptr_arr[3][1] = nullptr;
        //satd_fptr_arr[3][2] = satd_avx2<32,16>;
        //satd_fptr_arr[3][3] = satd_avx2<32,32>;
        //satd_fptr_arr[3][4] = satd_avx2<32,64>;
        //satd_fptr_arr[4][0] = nullptr;
        //satd_fptr_arr[4][1] = nullptr;
        //satd_fptr_arr[4][2] = nullptr;
        //satd_fptr_arr[4][3] = satd_avx2<64,32>;
        //satd_fptr_arr[4][4] = satd_avx2<64,64>;
        //sad_general_fptr_arr[0][0] = sad_general_avx2<4, 4>;
        //sad_general_fptr_arr[0][1] = sad_general_avx2<4, 8>;
        //sad_general_fptr_arr[0][2] = nullptr;
        //sad_general_fptr_arr[0][3] = nullptr;
        //sad_general_fptr_arr[0][4] = nullptr;
        //sad_general_fptr_arr[1][0] = sad_general_avx2<8, 4>;
        //sad_general_fptr_arr[1][1] = sad_general_avx2<8, 8>;
        //sad_general_fptr_arr[1][2] = sad_general_avx2<8, 16>;
        //sad_general_fptr_arr[1][3] = nullptr;
        //sad_general_fptr_arr[1][4] = nullptr;
        //sad_general_fptr_arr[2][0] = nullptr;
        //sad_general_fptr_arr[2][1] = sad_general_avx2<16, 8>;
        //sad_general_fptr_arr[2][2] = sad_general_avx2<16, 16>;
        //sad_general_fptr_arr[2][3] = sad_general_avx2<16, 32>;
        //sad_general_fptr_arr[2][4] = nullptr;
        //sad_general_fptr_arr[3][0] = nullptr;
        //sad_general_fptr_arr[3][1] = nullptr;
        //sad_general_fptr_arr[3][2] = sad_general_avx2<32, 16>;
        //sad_general_fptr_arr[3][3] = sad_general_avx2<32, 32>;
        //sad_general_fptr_arr[3][4] = sad_general_avx2<32, 64>;
        //sad_general_fptr_arr[4][0] = nullptr;
        //sad_general_fptr_arr[4][1] = nullptr;
        //sad_general_fptr_arr[4][2] = nullptr;
        //sad_general_fptr_arr[4][3] = sad_general_avx2<64, 32>;
        //sad_general_fptr_arr[4][4] = sad_general_avx2<64, 64>;
    }

} // VP9PP

