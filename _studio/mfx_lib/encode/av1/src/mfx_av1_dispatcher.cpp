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

#include "mfx_common.h"

#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_defs.h"
#include "mfx_av1_dispatcher.h"
#include "mfx_av1_dispatcher_proto.h"
#include "mfx_av1_dispatcher_fptr.h"

namespace AV1PP {

    void SetTargetAVX2();
#if ENABLE_PX_CODE
    void SetTargetPX();
#endif
    IppStatus initDispatcher(int32_t cpuFeature)
    {
        if (cpuFeature == CPU_FEAT_AUTO) {
            // GetPlatformType
            uint32_t cpuIdInfoRegs[4];
            uint64_t featuresMask;
            IppStatus sts = ippGetCpuFeatures(&featuresMask, cpuIdInfoRegs);
            if (ippStsNoErr != sts)
                return sts;

            cpuFeature = CPU_FEAT_PX;
            if (featuresMask & (uint64_t)(ippCPUID_AVX2)) // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
                cpuFeature = CPU_FEAT_AVX2;
        }

        switch (cpuFeature) {
        default:
#if ENABLE_PX_CODE
        case CPU_FEAT_PX:   SetTargetPX();   break;
#endif
        case CPU_FEAT_AVX2: {SetTargetAVX2(); break; }
        }

        return ippStsNoErr;
    }

#if ENABLE_PX_CODE
    void SetTargetPX()
    {
        using namespace AV1Enc;


        // luma intra pred [txSize] [haveLeft] [haveAbove] [mode]
        predict_intra_av1_fptr_arr[TX_4X4][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 0, 0>;
        predict_intra_av1_fptr_arr[TX_4X4][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 0, 1>;
        predict_intra_av1_fptr_arr[TX_4X4][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 1, 0>;
        predict_intra_av1_fptr_arr[TX_4X4][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 1, 1>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 0, 0>;
        predict_intra_av1_fptr_arr[TX_8X8][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 0, 1>;
        predict_intra_av1_fptr_arr[TX_8X8][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 1, 0>;
        predict_intra_av1_fptr_arr[TX_8X8][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 1, 1>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 0, 0>;
        predict_intra_av1_fptr_arr[TX_16X16][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 0, 1>;
        predict_intra_av1_fptr_arr[TX_16X16][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 1, 0>;
        predict_intra_av1_fptr_arr[TX_16X16][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 1, 1>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 0, 0>;
        predict_intra_av1_fptr_arr[TX_32X32][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 0, 1>;
        predict_intra_av1_fptr_arr[TX_32X32][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 1, 0>;
        predict_intra_av1_fptr_arr[TX_32X32][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 1, 1>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][V_PRED] = predict_intra_av1_px<TX_4X4, V_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][H_PRED] = predict_intra_av1_px<TX_4X4, H_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D45_PRED] = predict_intra_av1_px<TX_4X4, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D135_PRED] = predict_intra_av1_px<TX_4X4, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D117_PRED] = predict_intra_av1_px<TX_4X4, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D153_PRED] = predict_intra_av1_px<TX_4X4, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D207_PRED] = predict_intra_av1_px<TX_4X4, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D63_PRED] = predict_intra_av1_px<TX_4X4, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_4X4, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_4X4, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_4X4, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][PAETH_PRED] = predict_intra_av1_px<TX_4X4, PAETH_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][V_PRED] = predict_intra_av1_px<TX_8X8, V_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][H_PRED] = predict_intra_av1_px<TX_8X8, H_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D45_PRED] = predict_intra_av1_px<TX_8X8, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D135_PRED] = predict_intra_av1_px<TX_8X8, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D117_PRED] = predict_intra_av1_px<TX_8X8, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D153_PRED] = predict_intra_av1_px<TX_8X8, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D207_PRED] = predict_intra_av1_px<TX_8X8, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D63_PRED] = predict_intra_av1_px<TX_8X8, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_8X8, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_8X8, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_8X8, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][PAETH_PRED] = predict_intra_av1_px<TX_8X8, PAETH_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][V_PRED] = predict_intra_av1_px<TX_16X16, V_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][H_PRED] = predict_intra_av1_px<TX_16X16, H_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D45_PRED] = predict_intra_av1_px<TX_16X16, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D135_PRED] = predict_intra_av1_px<TX_16X16, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D117_PRED] = predict_intra_av1_px<TX_16X16, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D153_PRED] = predict_intra_av1_px<TX_16X16, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D207_PRED] = predict_intra_av1_px<TX_16X16, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D63_PRED] = predict_intra_av1_px<TX_16X16, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_16X16, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_16X16, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_16X16, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][PAETH_PRED] = predict_intra_av1_px<TX_16X16, PAETH_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][V_PRED] = predict_intra_av1_px<TX_32X32, V_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][H_PRED] = predict_intra_av1_px<TX_32X32, H_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D45_PRED] = predict_intra_av1_px<TX_32X32, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D135_PRED] = predict_intra_av1_px<TX_32X32, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D117_PRED] = predict_intra_av1_px<TX_32X32, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D153_PRED] = predict_intra_av1_px<TX_32X32, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D207_PRED] = predict_intra_av1_px<TX_32X32, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D63_PRED] = predict_intra_av1_px<TX_32X32, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_PRED] = predict_intra_av1_px<TX_32X32, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_av1_px<TX_32X32, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_av1_px<TX_32X32, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][PAETH_PRED] = predict_intra_av1_px<TX_32X32, PAETH_PRED>;
        for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
            for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
                predict_intra_av1_fptr_arr[txSize][0][1][mode] =
                    predict_intra_av1_fptr_arr[txSize][1][0][mode] =
                    predict_intra_av1_fptr_arr[txSize][1][1][mode] = predict_intra_av1_fptr_arr[txSize][0][0][mode];
            }
        }
        // nv12 intra pred [txSize] [haveLeft] [haveAbove] [mode]
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_4X4, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_4X4, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][1][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_4X4, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][1][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_4X4, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_8X8, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_8X8, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][1][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_8X8, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][1][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_8X8, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_16X16, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_16X16, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][1][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_16X16, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][1][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_16X16, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_32X32, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_32X32, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][1][0][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_32X32, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][1][1][DC_PRED] = predict_intra_nv12_dc_av1_px<TX_32X32, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][V_PRED] = predict_intra_nv12_av1_px<TX_4X4, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][H_PRED] = predict_intra_nv12_av1_px<TX_4X4, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D45_PRED] = predict_intra_nv12_av1_px<TX_4X4, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D135_PRED] = predict_intra_nv12_av1_px<TX_4X4, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D117_PRED] = predict_intra_nv12_av1_px<TX_4X4, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D153_PRED] = predict_intra_nv12_av1_px<TX_4X4, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D207_PRED] = predict_intra_nv12_av1_px<TX_4X4, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D63_PRED] = predict_intra_nv12_av1_px<TX_4X4, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_px<TX_4X4, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_px<TX_4X4, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_px<TX_4X4, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][PAETH_PRED] = predict_intra_nv12_av1_px<TX_4X4, PAETH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][V_PRED] = predict_intra_nv12_av1_px<TX_8X8, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][H_PRED] = predict_intra_nv12_av1_px<TX_8X8, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D45_PRED] = predict_intra_nv12_av1_px<TX_8X8, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D135_PRED] = predict_intra_nv12_av1_px<TX_8X8, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D117_PRED] = predict_intra_nv12_av1_px<TX_8X8, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D153_PRED] = predict_intra_nv12_av1_px<TX_8X8, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D207_PRED] = predict_intra_nv12_av1_px<TX_8X8, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D63_PRED] = predict_intra_nv12_av1_px<TX_8X8, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_px<TX_8X8, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_px<TX_8X8, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_px<TX_8X8, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][PAETH_PRED] = predict_intra_nv12_av1_px<TX_8X8, PAETH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][V_PRED] = predict_intra_nv12_av1_px<TX_16X16, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][H_PRED] = predict_intra_nv12_av1_px<TX_16X16, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D45_PRED] = predict_intra_nv12_av1_px<TX_16X16, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D135_PRED] = predict_intra_nv12_av1_px<TX_16X16, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D117_PRED] = predict_intra_nv12_av1_px<TX_16X16, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D153_PRED] = predict_intra_nv12_av1_px<TX_16X16, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D207_PRED] = predict_intra_nv12_av1_px<TX_16X16, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D63_PRED] = predict_intra_nv12_av1_px<TX_16X16, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_px<TX_16X16, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_px<TX_16X16, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_px<TX_16X16, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][PAETH_PRED] = predict_intra_nv12_av1_px<TX_16X16, PAETH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][V_PRED] = predict_intra_nv12_av1_px<TX_32X32, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][H_PRED] = predict_intra_nv12_av1_px<TX_32X32, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D45_PRED] = predict_intra_nv12_av1_px<TX_32X32, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D135_PRED] = predict_intra_nv12_av1_px<TX_32X32, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D117_PRED] = predict_intra_nv12_av1_px<TX_32X32, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D153_PRED] = predict_intra_nv12_av1_px<TX_32X32, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D207_PRED] = predict_intra_nv12_av1_px<TX_32X32, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D63_PRED] = predict_intra_nv12_av1_px<TX_32X32, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_px<TX_32X32, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_px<TX_32X32, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_px<TX_32X32, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][PAETH_PRED] = predict_intra_nv12_av1_px<TX_32X32, PAETH_PRED>;
        for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
            for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
                predict_intra_nv12_av1_fptr_arr[txSize][0][1][mode] =
                    predict_intra_nv12_av1_fptr_arr[txSize][1][0][mode] =
                    predict_intra_nv12_av1_fptr_arr[txSize][1][1][mode] = predict_intra_nv12_av1_fptr_arr[txSize][0][0][mode];
            }
        }

        predict_intra_palette_fptr_arr[TX_4X4  ] = predict_intra_palette_px<TX_4X4  >;
        predict_intra_palette_fptr_arr[TX_8X8  ] = predict_intra_palette_px<TX_8X8  >;
        predict_intra_palette_fptr_arr[TX_16X16] = predict_intra_palette_px<TX_16X16>;
        predict_intra_palette_fptr_arr[TX_32X32] = predict_intra_palette_px<TX_32X32>;

        ftransform_av1_fptr_arr[TX_4X4][DCT_DCT] = ftransform_av1_px<TX_4X4, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_4X4][ADST_DCT] = ftransform_av1_px<TX_4X4, ADST_DCT >;
        ftransform_av1_fptr_arr[TX_4X4][DCT_ADST] = ftransform_av1_px<TX_4X4, DCT_ADST >;
        ftransform_av1_fptr_arr[TX_4X4][ADST_ADST] = ftransform_av1_px<TX_4X4, ADST_ADST>;
        ftransform_av1_fptr_arr[TX_8X8][DCT_DCT] = ftransform_av1_px<TX_8X8, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_8X8][ADST_DCT] = ftransform_av1_px<TX_8X8, ADST_DCT >;
        ftransform_av1_fptr_arr[TX_8X8][DCT_ADST] = ftransform_av1_px<TX_8X8, DCT_ADST >;
        ftransform_av1_fptr_arr[TX_8X8][ADST_ADST] = ftransform_av1_px<TX_8X8, ADST_ADST>;
        ftransform_av1_fptr_arr[TX_16X16][DCT_DCT] = ftransform_vp9_px<TX_16X16, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_16X16][ADST_DCT] = ftransform_vp9_px<TX_16X16, ADST_DCT >;
        ftransform_av1_fptr_arr[TX_16X16][DCT_ADST] = ftransform_vp9_px<TX_16X16, DCT_ADST >;
        ftransform_av1_fptr_arr[TX_16X16][ADST_ADST] = ftransform_vp9_px<TX_16X16, ADST_ADST>;
        ftransform_av1_fptr_arr[TX_32X32][DCT_DCT] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_32X32][ADST_DCT] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_32X32][DCT_ADST] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_32X32][ADST_ADST] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
        // hbd
        ftransform_av1_hbd_fptr_arr[TX_4X4][DCT_DCT] = ftransform_av1_px<TX_4X4, DCT_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_4X4][ADST_DCT] = ftransform_av1_px<TX_4X4, ADST_DCT, int >;
        ftransform_av1_hbd_fptr_arr[TX_4X4][DCT_ADST] = ftransform_av1_px<TX_4X4, DCT_ADST, int >;
        ftransform_av1_hbd_fptr_arr[TX_4X4][ADST_ADST] = ftransform_av1_px<TX_4X4, ADST_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_8X8][DCT_DCT] = ftransform_av1_px<TX_8X8, DCT_DCT, int  >;
        ftransform_av1_hbd_fptr_arr[TX_8X8][ADST_DCT] = ftransform_av1_px<TX_8X8, ADST_DCT, int >;
        ftransform_av1_hbd_fptr_arr[TX_8X8][DCT_ADST] = ftransform_av1_px<TX_8X8, DCT_ADST, int >;
        ftransform_av1_hbd_fptr_arr[TX_8X8][ADST_ADST] = ftransform_av1_px<TX_8X8, ADST_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_16X16][DCT_DCT] = ftransform_vp9_px<TX_16X16, DCT_DCT, int  >;
        ftransform_av1_hbd_fptr_arr[TX_16X16][ADST_DCT] = ftransform_vp9_px<TX_16X16, ADST_DCT, int >;
        ftransform_av1_hbd_fptr_arr[TX_16X16][DCT_ADST] = ftransform_vp9_px<TX_16X16, DCT_ADST, int >;
        ftransform_av1_hbd_fptr_arr[TX_16X16][ADST_ADST] = ftransform_vp9_px<TX_16X16, ADST_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_32X32][DCT_DCT] = ftransform_av1_px<TX_32X32, DCT_DCT, int  >;
        ftransform_av1_hbd_fptr_arr[TX_32X32][ADST_DCT] = ftransform_av1_px<TX_32X32, DCT_DCT, int  >;
        ftransform_av1_hbd_fptr_arr[TX_32X32][DCT_ADST] = ftransform_av1_px<TX_32X32, DCT_DCT, int  >;
        ftransform_av1_hbd_fptr_arr[TX_32X32][ADST_ADST] = ftransform_av1_px<TX_32X32, DCT_DCT, int  >;
        // inverse transform (for chroma) [txSize][txType]
        itransform_av1_fptr_arr[TX_4X4][DCT_DCT] = itransform_av1_px<TX_4X4, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_4X4][ADST_DCT] = itransform_av1_px<TX_4X4, ADST_DCT >;
        itransform_av1_fptr_arr[TX_4X4][DCT_ADST] = itransform_av1_px<TX_4X4, DCT_ADST >;
        itransform_av1_fptr_arr[TX_4X4][ADST_ADST] = itransform_av1_px<TX_4X4, ADST_ADST>;
        itransform_av1_fptr_arr[TX_8X8][DCT_DCT] = itransform_av1_px<TX_8X8, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_8X8][ADST_DCT] = itransform_av1_px<TX_8X8, ADST_DCT >;
        itransform_av1_fptr_arr[TX_8X8][DCT_ADST] = itransform_av1_px<TX_8X8, DCT_ADST >;
        itransform_av1_fptr_arr[TX_8X8][ADST_ADST] = itransform_av1_px<TX_8X8, ADST_ADST>;
        itransform_av1_fptr_arr[TX_16X16][DCT_DCT] = itransform_av1_px<TX_16X16, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_16X16][ADST_DCT] = itransform_av1_px<TX_16X16, ADST_DCT >;
        itransform_av1_fptr_arr[TX_16X16][DCT_ADST] = itransform_av1_px<TX_16X16, DCT_ADST >;
        itransform_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_av1_px<TX_16X16, ADST_ADST>;
        itransform_av1_fptr_arr[TX_32X32][DCT_DCT] = itransform_av1_px<TX_32X32, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_32X32][ADST_DCT] = itransform_av1_px<TX_32X32, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_32X32][DCT_ADST] = itransform_av1_px<TX_32X32, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_av1_px<TX_32X32, DCT_DCT  >;
        // inverse transform and addition (for luma) [txSize][txType]
        itransform_add_av1_fptr_arr[TX_4X4][DCT_DCT] = itransform_add_av1_px<TX_4X4, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_4X4][ADST_DCT] = itransform_add_av1_px<TX_4X4, ADST_DCT >;
        itransform_add_av1_fptr_arr[TX_4X4][DCT_ADST] = itransform_add_av1_px<TX_4X4, DCT_ADST >;
        itransform_add_av1_fptr_arr[TX_4X4][ADST_ADST] = itransform_add_av1_px<TX_4X4, ADST_ADST>;
        itransform_add_av1_fptr_arr[TX_8X8][DCT_DCT] = itransform_add_av1_px<TX_8X8, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_8X8][ADST_DCT] = itransform_add_av1_px<TX_8X8, ADST_DCT >;
        itransform_add_av1_fptr_arr[TX_8X8][DCT_ADST] = itransform_add_av1_px<TX_8X8, DCT_ADST >;
        itransform_add_av1_fptr_arr[TX_8X8][ADST_ADST] = itransform_add_av1_px<TX_8X8, ADST_ADST>;
        itransform_add_av1_fptr_arr[TX_16X16][DCT_DCT] = itransform_add_av1_px<TX_16X16, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_16X16][ADST_DCT] = itransform_add_av1_px<TX_16X16, ADST_DCT >;
        itransform_add_av1_fptr_arr[TX_16X16][DCT_ADST] = itransform_add_av1_px<TX_16X16, DCT_ADST >;
        itransform_add_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_add_av1_px<TX_16X16, ADST_ADST>;
        itransform_add_av1_fptr_arr[TX_32X32][DCT_DCT] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_32X32][ADST_DCT] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_32X32][DCT_ADST] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;


        // quantization [txSize]
        quant_fptr_arr[TX_4X4] = quant_px<TX_4X4, short>;
        quant_fptr_arr[TX_8X8] = quant_px<TX_8X8, short>;
        quant_fptr_arr[TX_16X16] = quant_px<TX_16X16, short>;
        quant_fptr_arr[TX_32X32] = quant_px<TX_32X32, short>;
        // hbd
        quant_hbd_fptr_arr[TX_4X4] = quant_px<TX_4X4, int>;
        quant_hbd_fptr_arr[TX_8X8] = quant_px<TX_8X8, int>;
        quant_hbd_fptr_arr[TX_16X16] = quant_px<TX_16X16, int>;
        quant_hbd_fptr_arr[TX_32X32] = quant_px<TX_32X32, int>;
        dequant_fptr_arr[TX_4X4] = dequant_px<TX_4X4, short>;
        dequant_fptr_arr[TX_8X8] = dequant_px<TX_8X8, short>;
        dequant_fptr_arr[TX_16X16] = dequant_px<TX_16X16, short>;
        dequant_fptr_arr[TX_32X32] = dequant_px<TX_32X32, short>;
        //hbd
        dequant_hbd_fptr_arr[TX_4X4] = dequant_px<TX_4X4, int>;
        dequant_hbd_fptr_arr[TX_8X8] = dequant_px<TX_8X8, int>;
        dequant_hbd_fptr_arr[TX_16X16] = dequant_px<TX_16X16, int>;
        dequant_hbd_fptr_arr[TX_32X32] = dequant_px<TX_32X32, int>;
        quant_dequant_fptr_arr[TX_4X4] = quant_dequant_px<TX_4X4>;
        quant_dequant_fptr_arr[TX_8X8] = quant_dequant_px<TX_8X8>;
        quant_dequant_fptr_arr[TX_16X16] = quant_dequant_px<TX_16X16>;
        quant_dequant_fptr_arr[TX_32X32] = quant_dequant_px<TX_32X32>;
        // average [log2(width/4)]
        average_fptr_arr[0] = average_px<4>;
        average_fptr_arr[1] = average_px<8>;
        average_fptr_arr[2] = average_px<16>;
        average_fptr_arr[3] = average_px<32>;
        average_fptr_arr[4] = average_px<64>;
        average_pitch64_fptr_arr[0] = average_pitch64_px<4>;
        average_pitch64_fptr_arr[1] = average_pitch64_px<8>;
        average_pitch64_fptr_arr[2] = average_pitch64_px<16>;
        average_pitch64_fptr_arr[3] = average_pitch64_px<32>;
        average_pitch64_fptr_arr[4] = average_pitch64_px<64>;


        // luma inter prediction [log2(width/4)] [dx!=0] [dy!=0]
        interp_av1_single_ref_fptr_arr = interp_av1_single_ref_fptr_arr_px;
        interp_av1_first_ref_fptr_arr = interp_av1_first_ref_fptr_arr_px;
        interp_av1_second_ref_fptr_arr = interp_av1_second_ref_fptr_arr_px;
        // luma inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0]
        interp_pitch64_av1_single_ref_fptr_arr = interp_pitch64_av1_single_ref_fptr_arr_px;
        interp_pitch64_av1_first_ref_fptr_arr = interp_pitch64_av1_first_ref_fptr_arr_px;
        interp_pitch64_av1_second_ref_fptr_arr = interp_pitch64_av1_second_ref_fptr_arr_px;
        // nv12 inter prediction [log2(width/4)] [dx!=0] [dy!=0]
        interp_nv12_av1_single_ref_fptr_arr = interp_nv12_av1_single_ref_fptr_arr_px;
        interp_nv12_av1_first_ref_fptr_arr = interp_nv12_av1_first_ref_fptr_arr_px;
        interp_nv12_av1_second_ref_fptr_arr = interp_nv12_av1_second_ref_fptr_arr_px;
        // nv12 inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0]
        interp_nv12_pitch64_av1_single_ref_fptr_arr = interp_nv12_pitch64_av1_single_ref_fptr_arr_px;
        interp_nv12_pitch64_av1_first_ref_fptr_arr = interp_nv12_pitch64_av1_first_ref_fptr_arr_px;
        interp_nv12_pitch64_av1_second_ref_fptr_arr = interp_nv12_pitch64_av1_second_ref_fptr_arr_px;

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        // luma inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0] [avg]
        interp_pitch64_fptr_arr[0][0][0][0] = interp_pitch64_px<4, 0, 0, 0>;
        interp_pitch64_fptr_arr[0][0][0][1] = interp_pitch64_px<4, 0, 0, 1>;
        interp_pitch64_fptr_arr[0][0][1][0] = interp_pitch64_px<4, 0, 1, 0>;
        interp_pitch64_fptr_arr[0][0][1][1] = interp_pitch64_px<4, 0, 1, 1>;
        interp_pitch64_fptr_arr[0][1][0][0] = interp_pitch64_px<4, 1, 0, 0>;
        interp_pitch64_fptr_arr[0][1][0][1] = interp_pitch64_px<4, 1, 0, 1>;
        interp_pitch64_fptr_arr[0][1][1][0] = interp_pitch64_px<4, 1, 1, 0>;
        interp_pitch64_fptr_arr[0][1][1][1] = interp_pitch64_px<4, 1, 1, 1>;
        interp_pitch64_fptr_arr[1][0][0][0] = interp_pitch64_px<8, 0, 0, 0>;
        interp_pitch64_fptr_arr[1][0][0][1] = interp_pitch64_px<8, 0, 0, 1>;
        interp_pitch64_fptr_arr[1][0][1][0] = interp_pitch64_px<8, 0, 1, 0>;
        interp_pitch64_fptr_arr[1][0][1][1] = interp_pitch64_px<8, 0, 1, 1>;
        interp_pitch64_fptr_arr[1][1][0][0] = interp_pitch64_px<8, 1, 0, 0>;
        interp_pitch64_fptr_arr[1][1][0][1] = interp_pitch64_px<8, 1, 0, 1>;
        interp_pitch64_fptr_arr[1][1][1][0] = interp_pitch64_px<8, 1, 1, 0>;
        interp_pitch64_fptr_arr[1][1][1][1] = interp_pitch64_px<8, 1, 1, 1>;
        interp_pitch64_fptr_arr[2][0][0][0] = interp_pitch64_px<16, 0, 0, 0>;
        interp_pitch64_fptr_arr[2][0][0][1] = interp_pitch64_px<16, 0, 0, 1>;
        interp_pitch64_fptr_arr[2][0][1][0] = interp_pitch64_px<16, 0, 1, 0>;
        interp_pitch64_fptr_arr[2][0][1][1] = interp_pitch64_px<16, 0, 1, 1>;
        interp_pitch64_fptr_arr[2][1][0][0] = interp_pitch64_px<16, 1, 0, 0>;
        interp_pitch64_fptr_arr[2][1][0][1] = interp_pitch64_px<16, 1, 0, 1>;
        interp_pitch64_fptr_arr[2][1][1][0] = interp_pitch64_px<16, 1, 1, 0>;
        interp_pitch64_fptr_arr[2][1][1][1] = interp_pitch64_px<16, 1, 1, 1>;
        interp_pitch64_fptr_arr[3][0][0][0] = interp_pitch64_px<32, 0, 0, 0>;
        interp_pitch64_fptr_arr[3][0][0][1] = interp_pitch64_px<32, 0, 0, 1>;
        interp_pitch64_fptr_arr[3][0][1][0] = interp_pitch64_px<32, 0, 1, 0>;
        interp_pitch64_fptr_arr[3][0][1][1] = interp_pitch64_px<32, 0, 1, 1>;
        interp_pitch64_fptr_arr[3][1][0][0] = interp_pitch64_px<32, 1, 0, 0>;
        interp_pitch64_fptr_arr[3][1][0][1] = interp_pitch64_px<32, 1, 0, 1>;
        interp_pitch64_fptr_arr[3][1][1][0] = interp_pitch64_px<32, 1, 1, 0>;
        interp_pitch64_fptr_arr[3][1][1][1] = interp_pitch64_px<32, 1, 1, 1>;
        interp_pitch64_fptr_arr[4][0][0][0] = interp_pitch64_px<64, 0, 0, 0>;
        interp_pitch64_fptr_arr[4][0][0][1] = interp_pitch64_px<64, 0, 0, 1>;
        interp_pitch64_fptr_arr[4][0][1][0] = interp_pitch64_px<64, 0, 1, 0>;
        interp_pitch64_fptr_arr[4][0][1][1] = interp_pitch64_px<64, 0, 1, 1>;
        interp_pitch64_fptr_arr[4][1][0][0] = interp_pitch64_px<64, 1, 0, 0>;
        interp_pitch64_fptr_arr[4][1][0][1] = interp_pitch64_px<64, 1, 0, 1>;
        interp_pitch64_fptr_arr[4][1][1][0] = interp_pitch64_px<64, 1, 1, 0>;
        interp_pitch64_fptr_arr[4][1][1][1] = interp_pitch64_px<64, 1, 1, 1>;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

        // diff
        diff_nxn_fptr_arr[0] = diff_nxn_px<4>;
        diff_nxn_fptr_arr[1] = diff_nxn_px<8>;
        diff_nxn_fptr_arr[2] = diff_nxn_px<16>;
        diff_nxn_fptr_arr[3] = diff_nxn_px<32>;
        diff_nxn_fptr_arr[4] = diff_nxn_px<64>;
        diff_nxn_p64_p64_pw_fptr_arr[0] = diff_nxn_p64_p64_pw_px<4>;
        diff_nxn_p64_p64_pw_fptr_arr[1] = diff_nxn_p64_p64_pw_px<8>;
        diff_nxn_p64_p64_pw_fptr_arr[2] = diff_nxn_p64_p64_pw_px<16>;
        diff_nxn_p64_p64_pw_fptr_arr[3] = diff_nxn_p64_p64_pw_px<32>;
        diff_nxn_p64_p64_pw_fptr_arr[4] = diff_nxn_p64_p64_pw_px<64>;
        diff_nxm_fptr_arr[0] = diff_nxm_px<4>;
        diff_nxm_fptr_arr[1] = diff_nxm_px<8>;
        diff_nxm_fptr_arr[2] = diff_nxm_px<16>;
        diff_nxm_fptr_arr[3] = diff_nxm_px<32>;
        diff_nxm_fptr_arr[4] = diff_nxm_px<64>;
        diff_nxm_p64_p64_pw_fptr_arr[0] = diff_nxm_p64_p64_pw_px<4>;
        diff_nxm_p64_p64_pw_fptr_arr[1] = diff_nxm_p64_p64_pw_px<8>;
        diff_nxm_p64_p64_pw_fptr_arr[2] = diff_nxm_p64_p64_pw_px<16>;
        diff_nxm_p64_p64_pw_fptr_arr[3] = diff_nxm_p64_p64_pw_px<32>;
        diff_nxm_p64_p64_pw_fptr_arr[4] = diff_nxm_p64_p64_pw_px<64>;
        diff_nv12_fptr_arr[0] = diff_nv12_px<4>;
        diff_nv12_fptr_arr[1] = diff_nv12_px<8>;
        diff_nv12_fptr_arr[2] = diff_nv12_px<16>;
        diff_nv12_fptr_arr[3] = diff_nv12_px<32>;
        // hbd
        diff_nv12_hbd_fptr_arr[0] = diff_nv12_px<4>;
        diff_nv12_hbd_fptr_arr[1] = diff_nv12_px<8>;
        diff_nv12_hbd_fptr_arr[2] = diff_nv12_px<16>;
        diff_nv12_hbd_fptr_arr[3] = diff_nv12_px<32>;
        diff_nv12_p64_p64_pw_fptr_arr[0] = diff_nv12_p64_p64_pw_px<4>;
        diff_nv12_p64_p64_pw_fptr_arr[1] = diff_nv12_p64_p64_pw_px<8>;
        diff_nv12_p64_p64_pw_fptr_arr[2] = diff_nv12_p64_p64_pw_px<16>;
        diff_nv12_p64_p64_pw_fptr_arr[3] = diff_nv12_p64_p64_pw_px<32>;
        // satd
        satd_4x4_fptr = satd_4x4_px;
        satd_4x4_pitch64_fptr = satd_4x4_pitch64_px;
        satd_4x4_pitch64_both_fptr = satd_4x4_pitch64_both_px;
        satd_8x8_fptr = satd_8x8_px;
        satd_8x8_pitch64_fptr = satd_8x8_pitch64_px;
        satd_8x8_pitch64_both_fptr = satd_8x8_pitch64_both_px;
        satd_4x4_pair_fptr = satd_4x4_pair_px;
        satd_4x4_pair_pitch64_fptr = satd_4x4_pair_pitch64_px;
        satd_4x4_pair_pitch64_both_fptr = satd_4x4_pair_pitch64_both_px;
        satd_8x8_pair_fptr = satd_8x8_pair_px;
        satd_8x8_pair_pitch64_fptr = satd_8x8_pair_pitch64_px;
        satd_8x8_pair_pitch64_both_fptr = satd_8x8_pair_pitch64_both_px;
        satd_8x8x2_fptr = satd_8x8x2_px;
        satd_8x8x2_pitch64_fptr = satd_8x8x2_pitch64_px;
        satd_fptr_arr[0][0] = satd_px<4, 4>;
        satd_fptr_arr[0][1] = satd_px<4, 8>;
        satd_fptr_arr[0][2] = NULL;
        satd_fptr_arr[0][3] = NULL;
        satd_fptr_arr[0][4] = NULL;
        satd_fptr_arr[1][0] = satd_px<8, 4>;
        satd_fptr_arr[1][1] = satd_px<8, 8>;
        satd_fptr_arr[1][2] = satd_px<8, 16>;
        satd_fptr_arr[1][3] = NULL;
        satd_fptr_arr[1][4] = NULL;
        satd_fptr_arr[2][0] = NULL;
        satd_fptr_arr[2][1] = satd_px<16, 8>;
        satd_fptr_arr[2][2] = satd_px<16, 16>;
        satd_fptr_arr[2][3] = satd_px<16, 32>;
        satd_fptr_arr[2][4] = NULL;
        satd_fptr_arr[3][0] = NULL;
        satd_fptr_arr[3][1] = NULL;
        satd_fptr_arr[3][2] = satd_px<32, 16>;
        satd_fptr_arr[3][3] = satd_px<32, 32>;
        satd_fptr_arr[3][4] = satd_px<32, 64>;
        satd_fptr_arr[4][0] = NULL;
        satd_fptr_arr[4][1] = NULL;
        satd_fptr_arr[4][2] = NULL;
        satd_fptr_arr[4][3] = satd_px<64, 32>;
        satd_fptr_arr[4][4] = satd_px<64, 64>;
        satd_pitch64_fptr_arr[0][0] = satd_pitch64_px<4, 4>;
        satd_pitch64_fptr_arr[0][1] = satd_pitch64_px<4, 8>;
        satd_pitch64_fptr_arr[0][2] = NULL;
        satd_pitch64_fptr_arr[0][3] = NULL;
        satd_pitch64_fptr_arr[0][4] = NULL;
        satd_pitch64_fptr_arr[1][0] = satd_pitch64_px<8, 4>;
        satd_pitch64_fptr_arr[1][1] = satd_pitch64_px<8, 8>;
        satd_pitch64_fptr_arr[1][2] = satd_pitch64_px<8, 16>;
        satd_pitch64_fptr_arr[1][3] = NULL;
        satd_pitch64_fptr_arr[1][4] = NULL;
        satd_pitch64_fptr_arr[2][0] = NULL;
        satd_pitch64_fptr_arr[2][1] = satd_pitch64_px<16, 8>;
        satd_pitch64_fptr_arr[2][2] = satd_pitch64_px<16, 16>;
        satd_pitch64_fptr_arr[2][3] = satd_pitch64_px<16, 32>;
        satd_pitch64_fptr_arr[2][4] = NULL;
        satd_pitch64_fptr_arr[3][0] = NULL;
        satd_pitch64_fptr_arr[3][1] = NULL;
        satd_pitch64_fptr_arr[3][2] = satd_pitch64_px<32, 16>;
        satd_pitch64_fptr_arr[3][3] = satd_pitch64_px<32, 32>;
        satd_pitch64_fptr_arr[3][4] = satd_pitch64_px<32, 64>;
        satd_pitch64_fptr_arr[4][0] = NULL;
        satd_pitch64_fptr_arr[4][1] = NULL;
        satd_pitch64_fptr_arr[4][2] = NULL;
        satd_pitch64_fptr_arr[4][3] = satd_pitch64_px<64, 32>;
        satd_pitch64_fptr_arr[4][4] = satd_pitch64_px<64, 64>;
        satd_pitch64_both_fptr_arr[0][0] = satd_pitch64_both_px<4, 4>;
        satd_pitch64_both_fptr_arr[0][1] = satd_pitch64_both_px<4, 8>;
        satd_pitch64_both_fptr_arr[0][2] = NULL;
        satd_pitch64_both_fptr_arr[0][3] = NULL;
        satd_pitch64_both_fptr_arr[0][4] = NULL;
        satd_pitch64_both_fptr_arr[1][0] = satd_pitch64_both_px<8, 4>;
        satd_pitch64_both_fptr_arr[1][1] = satd_pitch64_both_px<8, 8>;
        satd_pitch64_both_fptr_arr[1][2] = satd_pitch64_both_px<8, 16>;
        satd_pitch64_both_fptr_arr[1][3] = NULL;
        satd_pitch64_both_fptr_arr[1][4] = NULL;
        satd_pitch64_both_fptr_arr[2][0] = NULL;
        satd_pitch64_both_fptr_arr[2][1] = satd_pitch64_both_px<16, 8>;
        satd_pitch64_both_fptr_arr[2][2] = satd_pitch64_both_px<16, 16>;
        satd_pitch64_both_fptr_arr[2][3] = satd_pitch64_both_px<16, 32>;
        satd_pitch64_both_fptr_arr[2][4] = NULL;
        satd_pitch64_both_fptr_arr[3][0] = NULL;
        satd_pitch64_both_fptr_arr[3][1] = NULL;
        satd_pitch64_both_fptr_arr[3][2] = satd_pitch64_both_px<32, 16>;
        satd_pitch64_both_fptr_arr[3][3] = satd_pitch64_both_px<32, 32>;
        satd_pitch64_both_fptr_arr[3][4] = satd_pitch64_both_px<32, 64>;
        satd_pitch64_both_fptr_arr[4][0] = NULL;
        satd_pitch64_both_fptr_arr[4][1] = NULL;
        satd_pitch64_both_fptr_arr[4][2] = NULL;
        satd_pitch64_both_fptr_arr[4][3] = satd_pitch64_both_px<64, 32>;
        satd_pitch64_both_fptr_arr[4][4] = satd_pitch64_both_px<64, 64>;
        satd_with_const_pitch64_fptr_arr[0][0] = satd_with_const_pitch64_px<4, 4>;
        satd_with_const_pitch64_fptr_arr[0][1] = satd_with_const_pitch64_px<4, 8>;
        satd_with_const_pitch64_fptr_arr[0][2] = NULL;
        satd_with_const_pitch64_fptr_arr[0][3] = NULL;
        satd_with_const_pitch64_fptr_arr[0][4] = NULL;
        satd_with_const_pitch64_fptr_arr[1][0] = satd_with_const_pitch64_px<8, 4>;
        satd_with_const_pitch64_fptr_arr[1][1] = satd_with_const_pitch64_px<8, 8>;
        satd_with_const_pitch64_fptr_arr[1][2] = satd_with_const_pitch64_px<8, 16>;
        satd_with_const_pitch64_fptr_arr[1][3] = NULL;
        satd_with_const_pitch64_fptr_arr[1][4] = NULL;
        satd_with_const_pitch64_fptr_arr[2][0] = NULL;
        satd_with_const_pitch64_fptr_arr[2][1] = satd_with_const_pitch64_px<16, 8>;
        satd_with_const_pitch64_fptr_arr[2][2] = satd_with_const_pitch64_px<16, 16>;
        satd_with_const_pitch64_fptr_arr[2][3] = satd_with_const_pitch64_px<16, 32>;
        satd_with_const_pitch64_fptr_arr[2][4] = NULL;
        satd_with_const_pitch64_fptr_arr[3][0] = NULL;
        satd_with_const_pitch64_fptr_arr[3][1] = NULL;
        satd_with_const_pitch64_fptr_arr[3][2] = satd_with_const_pitch64_px<32, 16>;
        satd_with_const_pitch64_fptr_arr[3][3] = satd_with_const_pitch64_px<32, 32>;
        satd_with_const_pitch64_fptr_arr[3][4] = satd_with_const_pitch64_px<32, 64>;
        satd_with_const_pitch64_fptr_arr[4][0] = NULL;
        satd_with_const_pitch64_fptr_arr[4][1] = NULL;
        satd_with_const_pitch64_fptr_arr[4][2] = NULL;
        satd_with_const_pitch64_fptr_arr[4][3] = satd_with_const_pitch64_px<64, 32>;
        satd_with_const_pitch64_fptr_arr[4][4] = satd_with_const_pitch64_px<64, 64>;
        // sse
        sse_fptr_arr[0][0] = sse_px<4, 4>;
        sse_fptr_arr[0][1] = sse_px<4, 8>;
        sse_fptr_arr[0][2] = NULL;
        sse_fptr_arr[0][3] = NULL;
        sse_fptr_arr[0][4] = NULL;
        sse_fptr_arr[1][0] = sse_px<8, 4>;
        sse_fptr_arr[1][1] = sse_px<8, 8>;
        sse_fptr_arr[1][2] = sse_px<8, 16>;
        sse_fptr_arr[1][3] = NULL;
        sse_fptr_arr[1][4] = NULL;
        sse_fptr_arr[2][0] = sse_px<16, 4>;
        sse_fptr_arr[2][1] = sse_px<16, 8>;
        sse_fptr_arr[2][2] = sse_px<16, 16>;
        sse_fptr_arr[2][3] = sse_px<16, 32>;
        sse_fptr_arr[2][4] = NULL;
        sse_fptr_arr[3][0] = NULL;
        sse_fptr_arr[3][1] = sse_px<32, 8>;
        sse_fptr_arr[3][2] = sse_px<32, 16>;
        sse_fptr_arr[3][3] = sse_px<32, 32>;
        sse_fptr_arr[3][4] = sse_px<32, 64>;
        sse_fptr_arr[4][0] = NULL;
        sse_fptr_arr[4][1] = NULL;
        sse_fptr_arr[4][2] = sse_px<64, 16>;
        sse_fptr_arr[4][3] = sse_px<64, 32>;
        sse_fptr_arr[4][4] = sse_px<64, 64>;
        sse_p64_pw_fptr_arr[0][0] = sse_p64_pw_px<4, 4>;
        sse_p64_pw_fptr_arr[0][1] = sse_p64_pw_px<4, 8>;
        sse_p64_pw_fptr_arr[0][2] = NULL;
        sse_p64_pw_fptr_arr[0][3] = NULL;
        sse_p64_pw_fptr_arr[0][4] = NULL;
        sse_p64_pw_fptr_arr[1][0] = sse_p64_pw_px<8, 4>;
        sse_p64_pw_fptr_arr[1][1] = sse_p64_pw_px<8, 8>;
        sse_p64_pw_fptr_arr[1][2] = sse_p64_pw_px<8, 16>;
        sse_p64_pw_fptr_arr[1][3] = NULL;
        sse_p64_pw_fptr_arr[1][4] = NULL;
        sse_p64_pw_fptr_arr[2][0] = sse_p64_pw_px<16, 4>;
        sse_p64_pw_fptr_arr[2][1] = sse_p64_pw_px<16, 8>;
        sse_p64_pw_fptr_arr[2][2] = sse_p64_pw_px<16, 16>;
        sse_p64_pw_fptr_arr[2][3] = sse_p64_pw_px<16, 32>;
        sse_p64_pw_fptr_arr[2][4] = NULL;
        sse_p64_pw_fptr_arr[3][0] = NULL;
        sse_p64_pw_fptr_arr[3][1] = sse_p64_pw_px<32, 8>;
        sse_p64_pw_fptr_arr[3][2] = sse_p64_pw_px<32, 16>;
        sse_p64_pw_fptr_arr[3][3] = sse_p64_pw_px<32, 32>;
        sse_p64_pw_fptr_arr[3][4] = sse_p64_pw_px<32, 64>;
        sse_p64_pw_fptr_arr[4][0] = NULL;
        sse_p64_pw_fptr_arr[4][1] = NULL;
        sse_p64_pw_fptr_arr[4][2] = sse_p64_pw_px<64, 16>;
        sse_p64_pw_fptr_arr[4][3] = sse_p64_pw_px<64, 32>;
        sse_p64_pw_fptr_arr[4][4] = sse_p64_pw_px<64, 64>;

        sse_p64_pw_uv_fptr_arr[0][0] = sse_p64_pw_uv_px<4, 4>;
        sse_p64_pw_uv_fptr_arr[0][1] = sse_p64_pw_uv_px<4, 8>;
        sse_p64_pw_uv_fptr_arr[0][2] = NULL;
        sse_p64_pw_uv_fptr_arr[0][3] = NULL;
        sse_p64_pw_uv_fptr_arr[0][4] = NULL;
        sse_p64_pw_uv_fptr_arr[1][0] = sse_p64_pw_uv_px<8, 4>;
        sse_p64_pw_uv_fptr_arr[1][1] = sse_p64_pw_uv_px<8, 8>;
        sse_p64_pw_uv_fptr_arr[1][2] = sse_p64_pw_uv_px<8, 16>;
        sse_p64_pw_uv_fptr_arr[1][3] = NULL;
        sse_p64_pw_uv_fptr_arr[1][4] = NULL;
        sse_p64_pw_uv_fptr_arr[2][0] = sse_p64_pw_uv_px<16, 4>;
        sse_p64_pw_uv_fptr_arr[2][1] = sse_p64_pw_uv_px<16, 8>;
        sse_p64_pw_uv_fptr_arr[2][2] = sse_p64_pw_uv_px<16, 16>;
        sse_p64_pw_uv_fptr_arr[2][3] = sse_p64_pw_uv_px<16, 32>;
        sse_p64_pw_uv_fptr_arr[2][4] = NULL;
        sse_p64_pw_uv_fptr_arr[3][0] = NULL;
        sse_p64_pw_uv_fptr_arr[3][1] = sse_p64_pw_uv_px<32, 8>;
        sse_p64_pw_uv_fptr_arr[3][2] = sse_p64_pw_uv_px<32, 16>;
        sse_p64_pw_uv_fptr_arr[3][3] = sse_p64_pw_uv_px<32, 32>;
        sse_p64_pw_uv_fptr_arr[3][4] = sse_p64_pw_uv_px<32, 64>;
        sse_p64_pw_uv_fptr_arr[4][0] = NULL;
        sse_p64_pw_uv_fptr_arr[4][1] = NULL;
        sse_p64_pw_uv_fptr_arr[4][2] = sse_p64_pw_uv_px<64, 16>;
        sse_p64_pw_uv_fptr_arr[4][3] = sse_p64_pw_uv_px<64, 32>;
        sse_p64_pw_uv_fptr_arr[4][4] = sse_p64_pw_uv_px<64, 64>;

        sse_p64_p64_fptr_arr[0][0] = sse_p64_p64_px<4, 4>;
        sse_p64_p64_fptr_arr[0][1] = sse_p64_p64_px<4, 8>;
        sse_p64_p64_fptr_arr[0][2] = NULL;
        sse_p64_p64_fptr_arr[0][3] = NULL;
        sse_p64_p64_fptr_arr[0][4] = NULL;
        sse_p64_p64_fptr_arr[1][0] = sse_p64_p64_px<8, 4>;
        sse_p64_p64_fptr_arr[1][1] = sse_p64_p64_px<8, 8>;
        sse_p64_p64_fptr_arr[1][2] = sse_p64_p64_px<8, 16>;
        sse_p64_p64_fptr_arr[1][3] = NULL;
        sse_p64_p64_fptr_arr[1][4] = NULL;
        sse_p64_p64_fptr_arr[2][0] = sse_p64_p64_px<16, 4>;
        sse_p64_p64_fptr_arr[2][1] = sse_p64_p64_px<16, 8>;
        sse_p64_p64_fptr_arr[2][2] = sse_p64_p64_px<16, 16>;
        sse_p64_p64_fptr_arr[2][3] = sse_p64_p64_px<16, 32>;
        sse_p64_p64_fptr_arr[2][4] = NULL;
        sse_p64_p64_fptr_arr[3][0] = NULL;
        sse_p64_p64_fptr_arr[3][1] = sse_p64_p64_px<32, 8>;
        sse_p64_p64_fptr_arr[3][2] = sse_p64_p64_px<32, 16>;
        sse_p64_p64_fptr_arr[3][3] = sse_p64_p64_px<32, 32>;
        sse_p64_p64_fptr_arr[3][4] = sse_p64_p64_px<32, 64>;
        sse_p64_p64_fptr_arr[4][0] = NULL;
        sse_p64_p64_fptr_arr[4][1] = NULL;
        sse_p64_p64_fptr_arr[4][2] = sse_p64_p64_px<64, 16>;
        sse_p64_p64_fptr_arr[4][3] = sse_p64_p64_px<64, 32>;
        sse_p64_p64_fptr_arr[4][4] = sse_p64_p64_px<64, 64>;
        sse_flexh_fptr_arr[0] = sse_px<4>;
        sse_flexh_fptr_arr[1] = sse_px<8>;
        sse_flexh_fptr_arr[2] = sse_px<16>;
        sse_flexh_fptr_arr[3] = sse_px<32>;
        sse_flexh_fptr_arr[4] = sse_px<64>;
        sse_cont_fptr/*_arr[4]*/ = sse_cont_px;
        ssz_cont_fptr/*_arr[4]*/ = ssz_cont_px;
        // sad
        sad_special_fptr_arr[0][0] = sad_special_px<4, 4>;
        sad_special_fptr_arr[0][1] = sad_special_px<4, 8>;
        sad_special_fptr_arr[0][2] = NULL;
        sad_special_fptr_arr[0][3] = NULL;
        sad_special_fptr_arr[0][4] = NULL;
        sad_special_fptr_arr[1][0] = sad_special_px<8, 4>;
        sad_special_fptr_arr[1][1] = sad_special_px<8, 8>;
        sad_special_fptr_arr[1][2] = sad_special_px<8, 16>;
        sad_special_fptr_arr[1][3] = NULL;
        sad_special_fptr_arr[1][4] = NULL;
        sad_special_fptr_arr[2][0] = NULL;
        sad_special_fptr_arr[2][1] = sad_special_px<16, 8>;
        sad_special_fptr_arr[2][2] = sad_special_px<16, 16>;
        sad_special_fptr_arr[2][3] = sad_special_px<16, 32>;
        sad_special_fptr_arr[2][4] = NULL;
        sad_special_fptr_arr[3][0] = NULL;
        sad_special_fptr_arr[3][1] = NULL;
        sad_special_fptr_arr[3][2] = sad_special_px<32, 16>;
        sad_special_fptr_arr[3][3] = sad_special_px<32, 32>;
        sad_special_fptr_arr[3][4] = sad_special_px<32, 64>;
        sad_special_fptr_arr[4][0] = NULL;
        sad_special_fptr_arr[4][1] = NULL;
        sad_special_fptr_arr[4][2] = NULL;
        sad_special_fptr_arr[4][3] = sad_special_px<64, 32>;
        sad_special_fptr_arr[4][4] = sad_special_px<64, 64>;
        sad_general_fptr_arr[0][0] = sad_general_px<4, 4>;
        sad_general_fptr_arr[0][1] = sad_general_px<4, 8>;
        sad_general_fptr_arr[0][2] = NULL;
        sad_general_fptr_arr[0][3] = NULL;
        sad_general_fptr_arr[0][4] = NULL;
        sad_general_fptr_arr[1][0] = sad_general_px<8, 4>;
        sad_general_fptr_arr[1][1] = sad_general_px<8, 8>;
        sad_general_fptr_arr[1][2] = sad_general_px<8, 16>;
        sad_general_fptr_arr[1][3] = NULL;
        sad_general_fptr_arr[1][4] = NULL;
        sad_general_fptr_arr[2][0] = NULL;
        sad_general_fptr_arr[2][1] = sad_general_px<16, 8>;
        sad_general_fptr_arr[2][2] = sad_general_px<16, 16>;
        sad_general_fptr_arr[2][3] = sad_general_px<16, 32>;
        sad_general_fptr_arr[2][4] = NULL;
        sad_general_fptr_arr[3][0] = NULL;
        sad_general_fptr_arr[3][1] = NULL;
        sad_general_fptr_arr[3][2] = sad_general_px<32, 16>;
        sad_general_fptr_arr[3][3] = sad_general_px<32, 32>;
        sad_general_fptr_arr[3][4] = sad_general_px<32, 64>;
        sad_general_fptr_arr[4][0] = NULL;
        sad_general_fptr_arr[4][1] = NULL;
        sad_general_fptr_arr[4][2] = NULL;
        sad_general_fptr_arr[4][3] = sad_general_px<64, 32>;
        sad_general_fptr_arr[4][4] = sad_general_px<64, 64>;
        sad_store8x8_fptr_arr[0] = nullptr;
        sad_store8x8_fptr_arr[1] = sad_store8x8_px<8>;
        sad_store8x8_fptr_arr[2] = sad_store8x8_px<16>;
        sad_store8x8_fptr_arr[3] = sad_store8x8_px<32>;
        sad_store8x8_fptr_arr[4] = sad_store8x8_px<64>;
        // deblocking
        lpf_horizontal_4_fptr = lpf_horizontal_4_av1_px;
        lpf_horizontal_8_fptr = lpf_horizontal_8_av1_px;
        lpf_horizontal_edge_16_fptr = lpf_horizontal_14_av1_px;
        lpf_vertical_4_fptr = lpf_vertical_4_av1_px;
        lpf_vertical_8_fptr = lpf_vertical_8_av1_px;
        lpf_vertical_16_fptr = lpf_vertical_14_av1_px;

        lpf_fptr_arr[VERT_EDGE][0] = lpf_vertical_4_av1_px;
        lpf_fptr_arr[VERT_EDGE][1] = lpf_vertical_8_av1_px;
        lpf_fptr_arr[VERT_EDGE][2] = lpf_vertical_14_av1_px;
        lpf_fptr_arr[VERT_EDGE][3] = lpf_vertical_6_av1_px;
        lpf_fptr_arr[HORZ_EDGE][0] = lpf_horizontal_4_av1_px;
        lpf_fptr_arr[HORZ_EDGE][1] = lpf_horizontal_8_av1_px;
        lpf_fptr_arr[HORZ_EDGE][2] = lpf_horizontal_14_av1_px;
        lpf_fptr_arr[HORZ_EDGE][3] = lpf_horizontal_6_av1_px;

        // cdef
        cdef_find_dir_fptr = cdef_find_dir_px;
        cdef_filter_block_u8_fptr_arr[0] = cdef_filter_block_8x8_px<uint8_t>;
        cdef_filter_block_u8_fptr_arr[1] = cdef_filter_block_4x4_nv12_px<uint8_t>;
        cdef_filter_block_u16_fptr_arr[0] = cdef_filter_block_8x8_px<uint16_t>;
        cdef_filter_block_u16_fptr_arr[1] = cdef_filter_block_4x4_nv12_px<uint16_t>;
        cdef_estimate_block_fptr_arr[0] = cdef_estimate_block_8x8_px;
        cdef_estimate_block_fptr_arr[1] = cdef_estimate_block_4x4_nv12_px;
        cdef_estimate_block_sec0_fptr_arr[0] = cdef_estimate_block_8x8_sec0_px;
        cdef_estimate_block_sec0_fptr_arr[1] = cdef_estimate_block_4x4_nv12_sec0_px;
        cdef_estimate_block_pri0_fptr_arr[0] = cdef_estimate_block_8x8_pri0_px;
        cdef_estimate_block_pri0_fptr_arr[1] = cdef_estimate_block_4x4_nv12_pri0_px;
        cdef_estimate_block_all_sec_fptr_arr[0] = cdef_estimate_block_8x8_all_sec_px;
        cdef_estimate_block_all_sec_fptr_arr[1] = cdef_estimate_block_4x4_nv12_all_sec_px;
        cdef_estimate_block_2pri_fptr_arr[0] = cdef_estimate_block_8x8_2pri_px;
        cdef_estimate_block_2pri_fptr_arr[1] = cdef_estimate_block_4x4_nv12_2pri_px;
        // etc
        diff_dc_fptr = NULL;//diff_dc_px;
        adds_nv12_fptr = adds_nv12_px;
        search_best_block8x8_fptr = search_best_block8x8_px;
        compute_rscs_fptr = compute_rscs_px;
        compute_rscs_4x4_fptr = compute_rscs_4x4_px;
        compute_rscs_diff_fptr = compute_rscs_diff_px;
        diff_histogram_fptr = diff_histogram_px;

        cfl_subtract_average_fptr_arr[TX_4X4] = subtract_average_px<4, 4>;
        cfl_subtract_average_fptr_arr[TX_8X8] = subtract_average_px<8, 8>;
        cfl_subtract_average_fptr_arr[TX_16X16] = subtract_average_px<16, 16>;
        cfl_subtract_average_fptr_arr[TX_32X32] = subtract_average_px<32, 32>;
        cfl_subtract_average_fptr_arr[TX_64X64] = nullptr;
        cfl_subtract_average_fptr_arr[TX_4X8] = subtract_average_px<4, 8>;
        cfl_subtract_average_fptr_arr[TX_8X4] = subtract_average_px<8, 4>;
        cfl_subtract_average_fptr_arr[TX_8X16] = subtract_average_px<8, 16>;
        cfl_subtract_average_fptr_arr[TX_16X8] = subtract_average_px<16, 8>;
        cfl_subtract_average_fptr_arr[TX_16X32] = subtract_average_px<16, 32>;
        cfl_subtract_average_fptr_arr[TX_32X16] = subtract_average_px<32, 16>;
        cfl_subtract_average_fptr_arr[TX_32X64] = nullptr;
        cfl_subtract_average_fptr_arr[TX_64X32] = nullptr;
        cfl_subtract_average_fptr_arr[TX_4X16] = subtract_average_px<4, 16>;
        cfl_subtract_average_fptr_arr[TX_16X4] = subtract_average_px<16, 4>;
        cfl_subtract_average_fptr_arr[TX_8X32] = subtract_average_px<8, 32>;
        cfl_subtract_average_fptr_arr[TX_32X8] = subtract_average_px<32, 8>;
        cfl_subtract_average_fptr_arr[TX_16X64] = nullptr;
        cfl_subtract_average_fptr_arr[TX_64X16] = nullptr;

        cfl_subsample_420_u8_fptr_arr[TX_4X4] = cfl_luma_subsampling_420_u8_px<4, 4>;
        cfl_subsample_420_u8_fptr_arr[TX_8X8] = cfl_luma_subsampling_420_u8_px<8, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_16X16] = cfl_luma_subsampling_420_u8_px<16, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_32X32] = cfl_luma_subsampling_420_u8_px<32, 32>;
        cfl_subsample_420_u8_fptr_arr[TX_64X64] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_4X8] = cfl_luma_subsampling_420_u8_px<4, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_8X4] = cfl_luma_subsampling_420_u8_px<8, 4>;
        cfl_subsample_420_u8_fptr_arr[TX_8X16] = cfl_luma_subsampling_420_u8_px<8, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_16X8] = cfl_luma_subsampling_420_u8_px<16, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_16X32] = cfl_luma_subsampling_420_u8_px<16, 32>;
        cfl_subsample_420_u8_fptr_arr[TX_32X16] = cfl_luma_subsampling_420_u8_px<32, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_32X64] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_64X32] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_4X16] = cfl_luma_subsampling_420_u8_px<4, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_16X4] = cfl_luma_subsampling_420_u8_px<16, 4>;
        cfl_subsample_420_u8_fptr_arr[TX_8X32] = cfl_luma_subsampling_420_u8_px<8, 32>;
        cfl_subsample_420_u8_fptr_arr[TX_32X8] = cfl_luma_subsampling_420_u8_px<32, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_16X64] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_64X16] = nullptr;

        cfl_subsample_420_u16_fptr_arr[TX_4X4] = cfl_luma_subsampling_420_u16_px<4, 4>;
        cfl_subsample_420_u16_fptr_arr[TX_8X8] = cfl_luma_subsampling_420_u16_px<8, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_16X16] = cfl_luma_subsampling_420_u16_px<16, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_32X32] = cfl_luma_subsampling_420_u16_px<32, 32>;
        cfl_subsample_420_u16_fptr_arr[TX_64X64] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_4X8] = cfl_luma_subsampling_420_u16_px<4, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_8X4] = cfl_luma_subsampling_420_u16_px<8, 4>;
        cfl_subsample_420_u16_fptr_arr[TX_8X16] = cfl_luma_subsampling_420_u16_px<8, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_16X8] = cfl_luma_subsampling_420_u16_px<16, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_16X32] = cfl_luma_subsampling_420_u16_px<16, 32>;
        cfl_subsample_420_u16_fptr_arr[TX_32X16] = cfl_luma_subsampling_420_u16_px<32, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_32X64] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_64X32] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_4X16] = cfl_luma_subsampling_420_u16_px<4, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_16X4] = cfl_luma_subsampling_420_u16_px<16, 4>;
        cfl_subsample_420_u16_fptr_arr[TX_8X32] = cfl_luma_subsampling_420_u16_px<8, 32>;
        cfl_subsample_420_u16_fptr_arr[TX_32X8] = cfl_luma_subsampling_420_u16_px<32, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_16X64] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_64X16] = nullptr;

        //avx2, only 32x32, 32x16, 32x8, other ssse3
        cfl_predict_nv12_u8_fptr_arr[TX_4X4] = cfl_predict_nv12_px<uint8_t, 4, 4>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X8] = cfl_predict_nv12_px<uint8_t, 8, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X16] = cfl_predict_nv12_px<uint8_t, 16, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X32] = cfl_predict_nv12_px<uint8_t, 32, 32>;
        cfl_predict_nv12_u8_fptr_arr[TX_64X64] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_4X8] = cfl_predict_nv12_px<uint8_t, 4, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X4] = cfl_predict_nv12_px<uint8_t, 8, 4>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X16] = cfl_predict_nv12_px<uint8_t, 8, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X8] = cfl_predict_nv12_px<uint8_t, 16, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X32] = cfl_predict_nv12_px<uint8_t, 16, 32>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X16] = cfl_predict_nv12_px<uint8_t, 32, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X64] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_64X32] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_4X16] = cfl_predict_nv12_px<uint8_t, 4, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X4] = cfl_predict_nv12_px<uint8_t, 16, 4>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X32] = cfl_predict_nv12_px<uint8_t, 8, 32>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X8] = cfl_predict_nv12_px<uint8_t, 32, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X64] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_64X16] = nullptr;

        //avx2, only 32x32, 32x16, 32x8, other ssse3
        cfl_predict_nv12_u16_fptr_arr[TX_4X4] = cfl_predict_nv12_px<uint16_t, 4, 4>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X8] = cfl_predict_nv12_px<uint16_t, 8, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X16] = cfl_predict_nv12_px<uint16_t, 16, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X32] = cfl_predict_nv12_px<uint16_t, 32, 32>;
        cfl_predict_nv12_u16_fptr_arr[TX_64X64] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_4X8] = cfl_predict_nv12_px<uint16_t, 4, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X4] = cfl_predict_nv12_px<uint16_t, 8, 4>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X16] = cfl_predict_nv12_px<uint16_t, 8, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X8] = cfl_predict_nv12_px<uint16_t, 16, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X32] = cfl_predict_nv12_px<uint16_t, 16, 32>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X16] = cfl_predict_nv12_px<uint16_t, 32, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X64] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_64X32] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_4X16] = cfl_predict_nv12_px<uint16_t, 4, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X4] = cfl_predict_nv12_px<uint16_t, 16, 4>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X32] = cfl_predict_nv12_px<uint16_t, 8, 32>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X8] = cfl_predict_nv12_px<uint16_t, 32, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X64] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_64X16] = nullptr;
    }
#endif

    void SetTargetAVX2() {
        using namespace AV1Enc;

        // luma intra pred [txSize] [haveLeft] [haveAbove] [mode]

        predict_intra_av1_fptr_arr[TX_4X4][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 0, 0>;
        predict_intra_av1_fptr_arr[TX_4X4][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 0, 1>;
        predict_intra_av1_fptr_arr[TX_4X4][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 1, 0>;
        predict_intra_av1_fptr_arr[TX_4X4][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 1, 1>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 0, 0>;
        predict_intra_av1_fptr_arr[TX_8X8][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 0, 1>;
        predict_intra_av1_fptr_arr[TX_8X8][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 1, 0>;
        predict_intra_av1_fptr_arr[TX_8X8][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 1, 1>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 0, 0>;
        predict_intra_av1_fptr_arr[TX_16X16][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 0, 1>;
        predict_intra_av1_fptr_arr[TX_16X16][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 1, 0>;
        predict_intra_av1_fptr_arr[TX_16X16][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 1, 1>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 0, 0>;
        predict_intra_av1_fptr_arr[TX_32X32][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 0, 1>;
        predict_intra_av1_fptr_arr[TX_32X32][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 1, 0>;
        predict_intra_av1_fptr_arr[TX_32X32][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 1, 1>;
        {
            // if (10 bits required)
            predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 0, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_4X4][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 0, 1>;
            predict_intra_av1_hbd_fptr_arr[TX_4X4][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 1, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_4X4][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_4X4, 1, 1>;
            predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 0, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_8X8][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 0, 1>;
            predict_intra_av1_hbd_fptr_arr[TX_8X8][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 1, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_8X8][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_8X8, 1, 1>;
            predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 0, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_16X16][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 0, 1>;
            predict_intra_av1_hbd_fptr_arr[TX_16X16][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 1, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_16X16][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_16X16, 1, 1>;
            predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 0, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_32X32][0][1][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 0, 1>;
            predict_intra_av1_hbd_fptr_arr[TX_32X32][1][0][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 1, 0>;
            predict_intra_av1_hbd_fptr_arr[TX_32X32][1][1][DC_PRED] = predict_intra_dc_av1_px<TX_32X32, 1, 1>;
        }
        predict_intra_av1_fptr_arr[TX_4X4][0][0][V_PRED] = predict_intra_av1_avx2<TX_4X4, V_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][H_PRED] = predict_intra_av1_avx2<TX_4X4, H_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D45_PRED] = predict_intra_av1_avx2<TX_4X4, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D135_PRED] = predict_intra_av1_avx2<TX_4X4, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D117_PRED] = predict_intra_av1_avx2<TX_4X4, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D153_PRED] = predict_intra_av1_avx2<TX_4X4, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D207_PRED] = predict_intra_av1_avx2<TX_4X4, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][D63_PRED] = predict_intra_av1_avx2<TX_4X4, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_4X4][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_4X4, PAETH_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][V_PRED] = predict_intra_av1_avx2<TX_8X8, V_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][H_PRED] = predict_intra_av1_avx2<TX_8X8, H_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D45_PRED] = predict_intra_av1_avx2<TX_8X8, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D135_PRED] = predict_intra_av1_avx2<TX_8X8, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D117_PRED] = predict_intra_av1_avx2<TX_8X8, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D153_PRED] = predict_intra_av1_avx2<TX_8X8, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D207_PRED] = predict_intra_av1_avx2<TX_8X8, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][D63_PRED] = predict_intra_av1_avx2<TX_8X8, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_8X8][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_8X8, PAETH_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][V_PRED] = predict_intra_av1_avx2<TX_16X16, V_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][H_PRED] = predict_intra_av1_avx2<TX_16X16, H_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D45_PRED] = predict_intra_av1_avx2<TX_16X16, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D135_PRED] = predict_intra_av1_avx2<TX_16X16, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D117_PRED] = predict_intra_av1_avx2<TX_16X16, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D153_PRED] = predict_intra_av1_avx2<TX_16X16, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D207_PRED] = predict_intra_av1_avx2<TX_16X16, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][D63_PRED] = predict_intra_av1_avx2<TX_16X16, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_16X16][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_16X16, PAETH_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][V_PRED] = predict_intra_av1_avx2<TX_32X32, V_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][H_PRED] = predict_intra_av1_avx2<TX_32X32, H_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D45_PRED] = predict_intra_av1_avx2<TX_32X32, D45_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D135_PRED] = predict_intra_av1_avx2<TX_32X32, D135_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D117_PRED] = predict_intra_av1_avx2<TX_32X32, D117_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D153_PRED] = predict_intra_av1_avx2<TX_32X32, D153_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D207_PRED] = predict_intra_av1_avx2<TX_32X32, D207_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][D63_PRED] = predict_intra_av1_avx2<TX_32X32, D63_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_V_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_H_PRED>;
        predict_intra_av1_fptr_arr[TX_32X32][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_32X32, PAETH_PRED>;
        for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
            for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
                predict_intra_av1_fptr_arr[txSize][0][1][mode] =
                    predict_intra_av1_fptr_arr[txSize][1][0][mode] =
                    predict_intra_av1_fptr_arr[txSize][1][1][mode] = predict_intra_av1_fptr_arr[txSize][0][0][mode];
            }
        }

        //====================================================================================================
        //     10 bit
        //====================================================================================================
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 0, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 0, 1>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 1, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_4X4, 1, 1>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 0, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 0, 1>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 1, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_8X8, 1, 1>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 0, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 0, 1>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 1, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_16X16, 1, 1>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 0, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 0, 1>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][1][0][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 1, 0>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][1][1][DC_PRED] = predict_intra_dc_av1_avx2<TX_32X32, 1, 1>;

        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][V_PRED] = predict_intra_av1_avx2<TX_4X4, V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][H_PRED] = predict_intra_av1_avx2<TX_4X4, H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][D45_PRED] = predict_intra_av1_avx2<TX_4X4, D45_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][D135_PRED] = predict_intra_av1_avx2<TX_4X4, D135_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][D117_PRED] = predict_intra_av1_avx2<TX_4X4, D117_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][D153_PRED] = predict_intra_av1_avx2<TX_4X4, D153_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][D207_PRED] = predict_intra_av1_avx2<TX_4X4, D207_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][D63_PRED] = predict_intra_av1_avx2<TX_4X4, D63_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_4X4, SMOOTH_H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_4X4][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_4X4, PAETH_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][V_PRED] = predict_intra_av1_avx2<TX_8X8, V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][H_PRED] = predict_intra_av1_avx2<TX_8X8, H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][D45_PRED] = predict_intra_av1_avx2<TX_8X8, D45_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][D135_PRED] = predict_intra_av1_avx2<TX_8X8, D135_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][D117_PRED] = predict_intra_av1_avx2<TX_8X8, D117_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][D153_PRED] = predict_intra_av1_avx2<TX_8X8, D153_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][D207_PRED] = predict_intra_av1_avx2<TX_8X8, D207_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][D63_PRED] = predict_intra_av1_avx2<TX_8X8, D63_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_8X8, SMOOTH_H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_8X8][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_8X8, PAETH_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][V_PRED] = predict_intra_av1_avx2<TX_16X16, V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][H_PRED] = predict_intra_av1_avx2<TX_16X16, H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][D45_PRED] = predict_intra_av1_avx2<TX_16X16, D45_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][D135_PRED] = predict_intra_av1_avx2<TX_16X16, D135_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][D117_PRED] = predict_intra_av1_avx2<TX_16X16, D117_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][D153_PRED] = predict_intra_av1_avx2<TX_16X16, D153_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][D207_PRED] = predict_intra_av1_avx2<TX_16X16, D207_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][D63_PRED] = predict_intra_av1_avx2<TX_16X16, D63_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_16X16, SMOOTH_H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_16X16][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_16X16, PAETH_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][V_PRED] = predict_intra_av1_avx2<TX_32X32, V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][H_PRED] = predict_intra_av1_avx2<TX_32X32, H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][D45_PRED] = predict_intra_av1_avx2<TX_32X32, D45_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][D135_PRED] = predict_intra_av1_avx2<TX_32X32, D135_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][D117_PRED] = predict_intra_av1_avx2<TX_32X32, D117_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][D153_PRED] = predict_intra_av1_avx2<TX_32X32, D153_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][D207_PRED] = predict_intra_av1_avx2<TX_32X32, D207_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][D63_PRED] = predict_intra_av1_avx2<TX_32X32, D63_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][SMOOTH_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_V_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_av1_avx2<TX_32X32, SMOOTH_H_PRED>;
        predict_intra_av1_hbd_fptr_arr[TX_32X32][0][0][PAETH_PRED] = predict_intra_av1_avx2<TX_32X32, PAETH_PRED>;
        for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
            for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
                predict_intra_av1_hbd_fptr_arr[txSize][0][1][mode] =
                    predict_intra_av1_hbd_fptr_arr[txSize][1][0][mode] =
                    predict_intra_av1_hbd_fptr_arr[txSize][1][1][mode] = predict_intra_av1_hbd_fptr_arr[txSize][0][0][mode];
            }
        }
        // nv12 intra pred [txSize] [haveLeft] [haveAbove] [mode]
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 0, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 0, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 1, 0>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 1, 1>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_4X4][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, PAETH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_8X8][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, PAETH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_16X16][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, PAETH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D45_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D135_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D117_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D153_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D207_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D63_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_fptr_arr[TX_32X32][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, PAETH_PRED>;
        for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
            for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
                predict_intra_nv12_av1_fptr_arr[txSize][0][1][mode] =
                    predict_intra_nv12_av1_fptr_arr[txSize][1][0][mode] =
                    predict_intra_nv12_av1_fptr_arr[txSize][1][1][mode] = predict_intra_nv12_av1_fptr_arr[txSize][0][0][mode];
            }
        }

        // 10 bit
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 0, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 0, 1>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 1, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_4X4, 1, 1>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 0, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 0, 1>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 1, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_8X8, 1, 1>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 0, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 0, 1>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 1, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_16X16, 1, 1>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 0, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 0, 1>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][1][0][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 1, 0>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][1][1][DC_PRED] = predict_intra_nv12_dc_av1_avx2<TX_32X32, 1, 1>;

        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D45_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D135_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D117_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D153_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D207_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, D63_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, SMOOTH_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_4X4][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_4X4, PAETH_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D45_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D135_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D117_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D153_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D207_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, D63_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, SMOOTH_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_8X8][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_8X8, PAETH_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D45_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D135_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D117_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D153_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D207_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, D63_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_16X16][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_16X16, PAETH_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][V_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][H_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][D45_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D45_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][D135_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D135_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][D117_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D117_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][D153_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D153_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][D207_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D207_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][D63_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, D63_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][SMOOTH_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][SMOOTH_V_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_V_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][SMOOTH_H_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_H_PRED>;
        predict_intra_nv12_av1_hbd_fptr_arr[TX_32X32][0][0][PAETH_PRED] = predict_intra_nv12_av1_avx2<TX_32X32, PAETH_PRED>;
        for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
            for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
                predict_intra_nv12_av1_hbd_fptr_arr[txSize][0][1][mode] =
                    predict_intra_nv12_av1_hbd_fptr_arr[txSize][1][0][mode] =
                    predict_intra_nv12_av1_hbd_fptr_arr[txSize][1][1][mode] = predict_intra_nv12_av1_hbd_fptr_arr[txSize][0][0][mode];
            }
        }



        predict_intra_palette_fptr_arr[TX_4X4  ] = predict_intra_palette_avx2<TX_4X4  >;
        predict_intra_palette_fptr_arr[TX_8X8  ] = predict_intra_palette_avx2<TX_8X8  >;
        predict_intra_palette_fptr_arr[TX_16X16] = predict_intra_palette_avx2<TX_16X16>;
        predict_intra_palette_fptr_arr[TX_32X32] = predict_intra_palette_avx2<TX_32X32>;

        // forward transform [txSize][txType]
        ftransform_av1_fptr_arr[TX_4X4][DCT_DCT] = ftransform_av1_avx2<TX_4X4, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_4X4][ADST_DCT] = ftransform_av1_avx2<TX_4X4, ADST_DCT >;
        ftransform_av1_fptr_arr[TX_4X4][DCT_ADST] = ftransform_av1_avx2<TX_4X4, DCT_ADST >;
        ftransform_av1_fptr_arr[TX_4X4][ADST_ADST] = ftransform_av1_avx2<TX_4X4, ADST_ADST>;
        ftransform_av1_fptr_arr[TX_4X4][IDTX] = ftransform_av1_avx2<TX_4X4, IDTX>;
        ftransform_av1_fptr_arr[TX_8X8][DCT_DCT] = ftransform_av1_avx2<TX_8X8, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_8X8][ADST_DCT] = ftransform_av1_avx2<TX_8X8, ADST_DCT >;
        ftransform_av1_fptr_arr[TX_8X8][DCT_ADST] = ftransform_av1_avx2<TX_8X8, DCT_ADST >;
        ftransform_av1_fptr_arr[TX_8X8][ADST_ADST] = ftransform_av1_avx2<TX_8X8, ADST_ADST>;
        ftransform_av1_fptr_arr[TX_8X8][IDTX] = ftransform_av1_avx2<TX_8X8, IDTX>;
        ftransform_av1_fptr_arr[TX_16X16][DCT_DCT] = ftransform_vp9_avx2<TX_16X16, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_16X16][ADST_DCT] = ftransform_vp9_avx2<TX_16X16, ADST_DCT >;
        ftransform_av1_fptr_arr[TX_16X16][DCT_ADST] = ftransform_vp9_avx2<TX_16X16, DCT_ADST >;
        ftransform_av1_fptr_arr[TX_16X16][ADST_ADST] = ftransform_vp9_avx2<TX_16X16, ADST_ADST>;
        ftransform_av1_fptr_arr[TX_16X16][IDTX] = ftransform_av1_avx2<TX_16X16, IDTX>;
        ftransform_av1_fptr_arr[TX_32X32][DCT_DCT] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_32X32][ADST_DCT] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_32X32][DCT_ADST] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_32X32][ADST_ADST] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
        ftransform_av1_fptr_arr[TX_32X32][IDTX] = ftransform_av1_avx2<TX_32X32, IDTX>;
        // hbd
        ftransform_av1_hbd_fptr_arr[TX_4X4][DCT_DCT] = ftransform_av1_px<TX_4X4, DCT_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_4X4][ADST_DCT] = ftransform_av1_px<TX_4X4, ADST_DCT, int >;
        ftransform_av1_hbd_fptr_arr[TX_4X4][DCT_ADST] = ftransform_av1_px<TX_4X4, DCT_ADST, int >;
        ftransform_av1_hbd_fptr_arr[TX_4X4][ADST_ADST] = ftransform_av1_px<TX_4X4, ADST_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_8X8][DCT_DCT] = ftransform_av1_px<TX_8X8, DCT_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_8X8][ADST_DCT] = ftransform_av1_px<TX_8X8, ADST_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_8X8][DCT_ADST] = ftransform_av1_px<TX_8X8, DCT_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_8X8][ADST_ADST] = ftransform_av1_px<TX_8X8, ADST_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_16X16][DCT_DCT] = ftransform_av1_px<TX_16X16, DCT_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_16X16][ADST_DCT] = ftransform_av1_px<TX_16X16, ADST_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_16X16][DCT_ADST] = ftransform_av1_px<TX_16X16, DCT_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_16X16][ADST_ADST] = ftransform_av1_px<TX_16X16, ADST_ADST, int>;
        ftransform_av1_hbd_fptr_arr[TX_32X32][DCT_DCT] = ftransform_av1_px<TX_32X32, DCT_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_32X32][ADST_DCT] = ftransform_av1_px<TX_32X32, DCT_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_32X32][DCT_ADST] = ftransform_av1_px<TX_32X32, DCT_DCT, int>;
        ftransform_av1_hbd_fptr_arr[TX_32X32][ADST_ADST] = ftransform_av1_px<TX_32X32, DCT_DCT, int>;
        //inverse transform (for chroma) [txSize][txType]
        itransform_av1_fptr_arr[TX_4X4][DCT_DCT] = itransform_av1_avx2<TX_4X4, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_4X4][ADST_DCT] = itransform_av1_avx2<TX_4X4, ADST_DCT >;
        itransform_av1_fptr_arr[TX_4X4][DCT_ADST] = itransform_av1_avx2<TX_4X4, DCT_ADST >;
        itransform_av1_fptr_arr[TX_4X4][ADST_ADST] = itransform_av1_avx2<TX_4X4, ADST_ADST>;
        itransform_av1_fptr_arr[TX_4X4][IDTX] = itransform_av1_avx2<TX_4X4, IDTX>;
        itransform_av1_fptr_arr[TX_8X8][DCT_DCT] = itransform_av1_avx2<TX_8X8, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_8X8][ADST_DCT] = itransform_av1_avx2<TX_8X8, ADST_DCT >;
        itransform_av1_fptr_arr[TX_8X8][DCT_ADST] = itransform_av1_avx2<TX_8X8, DCT_ADST >;
        itransform_av1_fptr_arr[TX_8X8][ADST_ADST] = itransform_av1_avx2<TX_8X8, ADST_ADST>;
        itransform_av1_fptr_arr[TX_8X8][IDTX] = itransform_av1_avx2<TX_8X8, IDTX>;
        itransform_av1_fptr_arr[TX_16X16][DCT_DCT] = itransform_av1_avx2<TX_16X16, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_16X16][ADST_DCT] = itransform_av1_avx2<TX_16X16, ADST_DCT >;
        itransform_av1_fptr_arr[TX_16X16][DCT_ADST] = itransform_av1_avx2<TX_16X16, DCT_ADST >;
        itransform_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_av1_avx2<TX_16X16, ADST_ADST>;
        itransform_av1_fptr_arr[TX_16X16][IDTX] = itransform_av1_avx2<TX_16X16, IDTX>;
        itransform_av1_fptr_arr[TX_32X32][DCT_DCT] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_32X32][ADST_DCT] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_32X32][DCT_ADST] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_av1_fptr_arr[TX_32X32][IDTX] = itransform_av1_avx2<TX_32X32, IDTX>;

        // inverse transform and addition (for luma) [txSize][txType]
        itransform_add_av1_fptr_arr[TX_4X4][DCT_DCT] = itransform_add_av1_avx2<TX_4X4, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_4X4][ADST_DCT] = itransform_add_av1_avx2<TX_4X4, ADST_DCT >;
        itransform_add_av1_fptr_arr[TX_4X4][DCT_ADST] = itransform_add_av1_avx2<TX_4X4, DCT_ADST >;
        itransform_add_av1_fptr_arr[TX_4X4][ADST_ADST] = itransform_add_av1_avx2<TX_4X4, ADST_ADST>;
        itransform_add_av1_fptr_arr[TX_4X4][IDTX] = itransform_add_av1_avx2<TX_4X4, IDTX>;
        itransform_add_av1_fptr_arr[TX_8X8][DCT_DCT] = itransform_add_av1_avx2<TX_8X8, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_8X8][ADST_DCT] = itransform_add_av1_avx2<TX_8X8, ADST_DCT >;
        itransform_add_av1_fptr_arr[TX_8X8][DCT_ADST] = itransform_add_av1_avx2<TX_8X8, DCT_ADST >;
        itransform_add_av1_fptr_arr[TX_8X8][ADST_ADST] = itransform_add_av1_avx2<TX_8X8, ADST_ADST>;
        itransform_add_av1_fptr_arr[TX_8X8][IDTX] = itransform_add_av1_avx2<TX_8X8, IDTX>;
        itransform_add_av1_fptr_arr[TX_16X16][DCT_DCT] = itransform_add_av1_avx2<TX_16X16, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_16X16][ADST_DCT] = itransform_add_av1_avx2<TX_16X16, ADST_DCT >;
        itransform_add_av1_fptr_arr[TX_16X16][DCT_ADST] = itransform_add_av1_avx2<TX_16X16, DCT_ADST >;
        itransform_add_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_add_av1_avx2<TX_16X16, ADST_ADST>;
        itransform_add_av1_fptr_arr[TX_16X16][IDTX] = itransform_add_av1_avx2<TX_16X16, IDTX>;
        itransform_add_av1_fptr_arr[TX_32X32][DCT_DCT] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_32X32][ADST_DCT] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_32X32][DCT_ADST] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
        itransform_add_av1_fptr_arr[TX_32X32][IDTX] = itransform_add_av1_avx2<TX_32X32, IDTX>;

        itransform_add_av1_hbd_fptr_arr[TX_4X4][DCT_DCT]   = itransform_add_av1_hbd_px<TX_4X4, DCT_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_4X4][ADST_DCT]  = itransform_add_av1_hbd_px<TX_4X4, ADST_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_4X4][DCT_ADST]  = itransform_add_av1_hbd_px<TX_4X4, DCT_ADST, short>;
        itransform_add_av1_hbd_fptr_arr[TX_4X4][ADST_ADST] = itransform_add_av1_hbd_px<TX_4X4, ADST_ADST, short>;
        itransform_add_av1_hbd_fptr_arr[TX_4X4][IDTX]      = itransform_add_av1_hbd_px<TX_4X4, IDTX, short>;
        itransform_add_av1_hbd_fptr_arr[TX_8X8][DCT_DCT]   = itransform_add_av1_hbd_px<TX_8X8, DCT_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_8X8][ADST_DCT]  = itransform_add_av1_hbd_px<TX_8X8, ADST_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_8X8][DCT_ADST]  = itransform_add_av1_hbd_px<TX_8X8, DCT_ADST, short>;
        itransform_add_av1_hbd_fptr_arr[TX_8X8][ADST_ADST] = itransform_add_av1_hbd_px<TX_8X8, ADST_ADST, short>;
        itransform_add_av1_hbd_fptr_arr[TX_8X8][IDTX]      = itransform_add_av1_hbd_px<TX_8X8, IDTX, short>;
        itransform_add_av1_hbd_fptr_arr[TX_16X16][DCT_DCT] = itransform_add_av1_hbd_px<TX_16X16, DCT_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_16X16][ADST_DCT]= itransform_add_av1_hbd_px<TX_16X16, ADST_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_16X16][DCT_ADST]= itransform_add_av1_hbd_px<TX_16X16, DCT_ADST, short>;
        itransform_add_av1_hbd_fptr_arr[TX_16X16][ADST_ADST] = itransform_add_av1_hbd_px<TX_16X16, ADST_ADST, short>;
        itransform_add_av1_hbd_fptr_arr[TX_16X16][IDTX]    = itransform_add_av1_hbd_px<TX_16X16, IDTX, short>;
        itransform_add_av1_hbd_fptr_arr[TX_32X32][DCT_DCT] = itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_32X32][ADST_DCT]= itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_32X32][DCT_ADST]= itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_32X32][ADST_ADST] = itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, short>;
        itransform_add_av1_hbd_fptr_arr[TX_32X32][IDTX]    = itransform_add_av1_hbd_px<TX_32X32, IDTX, short>;

        itransform_add_av1_hbd_hbd_fptr_arr[TX_4X4][DCT_DCT]  = itransform_add_av1_hbd_px<TX_4X4, DCT_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_4X4][ADST_DCT] = itransform_add_av1_hbd_px<TX_4X4, ADST_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_4X4][DCT_ADST] = itransform_add_av1_hbd_px<TX_4X4, DCT_ADST, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_4X4][ADST_ADST] = itransform_add_av1_hbd_px<TX_4X4, ADST_ADST, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_4X4][IDTX] = itransform_add_av1_hbd_px<TX_4X4, IDTX, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_8X8][DCT_DCT]  = itransform_add_av1_hbd_px<TX_8X8, DCT_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_8X8][ADST_DCT] = itransform_add_av1_hbd_px<TX_8X8, ADST_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_8X8][DCT_ADST] = itransform_add_av1_hbd_px<TX_8X8, DCT_ADST, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_8X8][ADST_ADST] = itransform_add_av1_hbd_px<TX_8X8, ADST_ADST, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_8X8][IDTX] = itransform_add_av1_hbd_px<TX_8X8, IDTX, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_16X16][DCT_DCT]  = itransform_add_av1_hbd_px<TX_16X16, DCT_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_16X16][ADST_DCT] = itransform_add_av1_hbd_px<TX_16X16, ADST_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_16X16][DCT_ADST] = itransform_add_av1_hbd_px<TX_16X16, DCT_ADST, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_16X16][ADST_ADST] = itransform_add_av1_hbd_px<TX_16X16, ADST_ADST, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_16X16][IDTX] = itransform_add_av1_hbd_px<TX_16X16, IDTX, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_32X32][DCT_DCT]  = itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_32X32][ADST_DCT] = itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_32X32][DCT_ADST] = itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_32X32][ADST_ADST] = itransform_add_av1_hbd_px<TX_32X32, DCT_DCT, int>;
        itransform_add_av1_hbd_hbd_fptr_arr[TX_32X32][IDTX] = itransform_add_av1_hbd_px<TX_32X32, IDTX, int>;

        // quantization [txSize]
        quant_fptr_arr[TX_4X4] = quant_avx2<TX_4X4>;
        quant_fptr_arr[TX_8X8] = quant_avx2<TX_8X8>;
        quant_fptr_arr[TX_16X16] = quant_avx2<TX_16X16>;
        quant_fptr_arr[TX_32X32] = quant_avx2<TX_32X32>;
        // hbd
        quant_hbd_fptr_arr[TX_4X4] = quant_px<TX_4X4, int>;
        quant_hbd_fptr_arr[TX_8X8] = quant_px<TX_8X8, int>;
        quant_hbd_fptr_arr[TX_16X16] = quant_px<TX_16X16, int>;
        quant_hbd_fptr_arr[TX_32X32] = quant_px<TX_32X32, int>;

        dequant_fptr_arr[TX_4X4] = dequant_avx2<TX_4X4>;
        dequant_fptr_arr[TX_8X8] = dequant_avx2<TX_8X8>;
        dequant_fptr_arr[TX_16X16] = dequant_avx2<TX_16X16>;
        dequant_fptr_arr[TX_32X32] = dequant_avx2<TX_32X32>;
        //hbd
        dequant_hbd_fptr_arr[TX_4X4] = dequant_px<TX_4X4, int>;
        dequant_hbd_fptr_arr[TX_8X8] = dequant_px<TX_8X8, int>;
        dequant_hbd_fptr_arr[TX_16X16] = dequant_px<TX_16X16, int>;
        dequant_hbd_fptr_arr[TX_32X32] = dequant_px<TX_32X32, int>;

        quant_dequant_fptr_arr[TX_4X4] = quant_dequant_avx2<TX_4X4>;
        quant_dequant_fptr_arr[TX_8X8] = quant_dequant_avx2<TX_8X8>;
        quant_dequant_fptr_arr[TX_16X16] = quant_dequant_avx2<TX_16X16>;
        quant_dequant_fptr_arr[TX_32X32] = quant_dequant_avx2<TX_32X32>;
        // average [log2(width/4)]
        average_fptr_arr[0] = average_avx2<4>;
        average_fptr_arr[1] = average_avx2<8>;
        average_fptr_arr[2] = average_avx2<16>;
        average_fptr_arr[3] = average_avx2<32>;
        average_fptr_arr[4] = average_avx2<64>;
        average_pitch64_fptr_arr[0] = average_pitch64_avx2<4>;
        average_pitch64_fptr_arr[1] = average_pitch64_avx2<8>;
        average_pitch64_fptr_arr[2] = average_pitch64_avx2<16>;
        average_pitch64_fptr_arr[3] = average_pitch64_avx2<32>;
        average_pitch64_fptr_arr[4] = average_pitch64_avx2<64>;

        // luma inter prediction [log2(width/4)] [dx!=0] [dy!=0]
        interp_av1_single_ref_fptr_arr = interp_av1_single_ref_fptr_arr_avx2;
        interp_av1_first_ref_fptr_arr = interp_av1_first_ref_fptr_arr_avx2;
        interp_av1_second_ref_fptr_arr = interp_av1_second_ref_fptr_arr_avx2;
        // luma inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0]
        interp_pitch64_av1_single_ref_fptr_arr = interp_pitch64_av1_single_ref_fptr_arr_avx2;
        interp_pitch64_av1_single_ref_hbd_fptr_arr = interp_pitch64_av1_single_ref_hbd_fptr_arr_px;

        interp_pitch64_av1_first_ref_fptr_arr = interp_pitch64_av1_first_ref_fptr_arr_avx2;
        interp_pitch64_av1_first_ref_hbd_fptr_arr = interp_pitch64_av1_first_ref_hbd_fptr_arr_px;

        interp_pitch64_av1_second_ref_fptr_arr = interp_pitch64_av1_second_ref_fptr_arr_avx2;
        interp_pitch64_av1_second_ref_hbd_fptr_arr = interp_pitch64_av1_second_ref_hbd_fptr_arr_px;
        // nv12 inter prediction [log2(width/4)] [dx!=0] [dy!=0]
        interp_nv12_av1_single_ref_fptr_arr = interp_nv12_av1_single_ref_fptr_arr_avx2;
        interp_nv12_av1_first_ref_fptr_arr = interp_nv12_av1_first_ref_fptr_arr_avx2;
        interp_nv12_av1_second_ref_fptr_arr = interp_nv12_av1_second_ref_fptr_arr_avx2;
        // nv12 inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0]
        interp_nv12_pitch64_av1_single_ref_fptr_arr = interp_nv12_pitch64_av1_single_ref_fptr_arr_avx2;
        interp_nv12_pitch64_av1_single_ref_hbd_fptr_arr = interp_nv12_pitch64_av1_single_ref_hbd_fptr_arr_px;
        interp_nv12_pitch64_av1_first_ref_fptr_arr = interp_nv12_pitch64_av1_first_ref_fptr_arr_avx2;
        interp_nv12_pitch64_av1_first_ref_hbd_fptr_arr = interp_nv12_pitch64_av1_first_ref_hbd_fptr_arr_px;
        interp_nv12_pitch64_av1_second_ref_fptr_arr = interp_nv12_pitch64_av1_second_ref_fptr_arr_avx2;
        interp_nv12_pitch64_av1_second_ref_hbd_fptr_arr = interp_nv12_pitch64_av1_second_ref_hbd_fptr_arr_px;

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        interp_pitch64_fptr_arr[0][0][0][0] = interp_pitch64_avx2<4, 0, 0, 0>;
        interp_pitch64_fptr_arr[0][0][0][1] = interp_pitch64_avx2<4, 0, 0, 1>;
        interp_pitch64_fptr_arr[0][0][1][0] = interp_pitch64_avx2<4, 0, 1, 0>;
        interp_pitch64_fptr_arr[0][0][1][1] = interp_pitch64_avx2<4, 0, 1, 1>;
        interp_pitch64_fptr_arr[0][1][0][0] = interp_pitch64_avx2<4, 1, 0, 0>;
        interp_pitch64_fptr_arr[0][1][0][1] = interp_pitch64_avx2<4, 1, 0, 1>;
        interp_pitch64_fptr_arr[0][1][1][0] = interp_pitch64_avx2<4, 1, 1, 0>;
        interp_pitch64_fptr_arr[0][1][1][1] = interp_pitch64_avx2<4, 1, 1, 1>;
        interp_pitch64_fptr_arr[1][0][0][0] = interp_pitch64_avx2<8, 0, 0, 0>;
        interp_pitch64_fptr_arr[1][0][0][1] = interp_pitch64_avx2<8, 0, 0, 1>;
        interp_pitch64_fptr_arr[1][0][1][0] = interp_pitch64_avx2<8, 0, 1, 0>;
        interp_pitch64_fptr_arr[1][0][1][1] = interp_pitch64_avx2<8, 0, 1, 1>;
        interp_pitch64_fptr_arr[1][1][0][0] = interp_pitch64_avx2<8, 1, 0, 0>;
        interp_pitch64_fptr_arr[1][1][0][1] = interp_pitch64_avx2<8, 1, 0, 1>;
        interp_pitch64_fptr_arr[1][1][1][0] = interp_pitch64_avx2<8, 1, 1, 0>;
        interp_pitch64_fptr_arr[1][1][1][1] = interp_pitch64_avx2<8, 1, 1, 1>;
        interp_pitch64_fptr_arr[2][0][0][0] = interp_pitch64_avx2<16, 0, 0, 0>;
        interp_pitch64_fptr_arr[2][0][0][1] = interp_pitch64_avx2<16, 0, 0, 1>;
        interp_pitch64_fptr_arr[2][0][1][0] = interp_pitch64_avx2<16, 0, 1, 0>;
        interp_pitch64_fptr_arr[2][0][1][1] = interp_pitch64_avx2<16, 0, 1, 1>;
        interp_pitch64_fptr_arr[2][1][0][0] = interp_pitch64_avx2<16, 1, 0, 0>;
        interp_pitch64_fptr_arr[2][1][0][1] = interp_pitch64_avx2<16, 1, 0, 1>;
        interp_pitch64_fptr_arr[2][1][1][0] = interp_pitch64_avx2<16, 1, 1, 0>;
        interp_pitch64_fptr_arr[2][1][1][1] = interp_pitch64_avx2<16, 1, 1, 1>;
        interp_pitch64_fptr_arr[3][0][0][0] = interp_pitch64_avx2<32, 0, 0, 0>;
        interp_pitch64_fptr_arr[3][0][0][1] = interp_pitch64_avx2<32, 0, 0, 1>;
        interp_pitch64_fptr_arr[3][0][1][0] = interp_pitch64_avx2<32, 0, 1, 0>;
        interp_pitch64_fptr_arr[3][0][1][1] = interp_pitch64_avx2<32, 0, 1, 1>;
        interp_pitch64_fptr_arr[3][1][0][0] = interp_pitch64_avx2<32, 1, 0, 0>;
        interp_pitch64_fptr_arr[3][1][0][1] = interp_pitch64_avx2<32, 1, 0, 1>;
        interp_pitch64_fptr_arr[3][1][1][0] = interp_pitch64_avx2<32, 1, 1, 0>;
        interp_pitch64_fptr_arr[3][1][1][1] = interp_pitch64_avx2<32, 1, 1, 1>;
        interp_pitch64_fptr_arr[4][0][0][0] = interp_pitch64_avx2<64, 0, 0, 0>;
        interp_pitch64_fptr_arr[4][0][0][1] = interp_pitch64_avx2<64, 0, 0, 1>;
        interp_pitch64_fptr_arr[4][0][1][0] = interp_pitch64_avx2<64, 0, 1, 0>;
        interp_pitch64_fptr_arr[4][0][1][1] = interp_pitch64_avx2<64, 0, 1, 1>;
        interp_pitch64_fptr_arr[4][1][0][0] = interp_pitch64_avx2<64, 1, 0, 0>;
        interp_pitch64_fptr_arr[4][1][0][1] = interp_pitch64_avx2<64, 1, 0, 1>;
        interp_pitch64_fptr_arr[4][1][1][0] = interp_pitch64_avx2<64, 1, 1, 0>;
        interp_pitch64_fptr_arr[4][1][1][1] = interp_pitch64_avx2<64, 1, 1, 1>;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

        // diff
        diff_nxn_fptr_arr[0] = diff_nxn_avx2<4>;
        diff_nxn_fptr_arr[1] = diff_nxn_avx2<8>;
        diff_nxn_fptr_arr[2] = diff_nxn_avx2<16>;
        diff_nxn_fptr_arr[3] = diff_nxn_avx2<32>;
        diff_nxn_fptr_arr[4] = diff_nxn_avx2<64>;


        diff_nxn_hbd_fptr_arr[0] = diff_nxn_hbd_px<4>;
        diff_nxn_hbd_fptr_arr[1] = diff_nxn_hbd_px<8>;
        diff_nxn_hbd_fptr_arr[2] = diff_nxn_hbd_px<16>;
        diff_nxn_hbd_fptr_arr[3] = diff_nxn_hbd_px<32>;
        diff_nxn_hbd_fptr_arr[4] = diff_nxn_hbd_px<64>;


        diff_nxn_p64_p64_pw_fptr_arr[0] = diff_nxn_p64_p64_pw_avx2<4>;
        diff_nxn_p64_p64_pw_fptr_arr[1] = diff_nxn_p64_p64_pw_avx2<8>;
        diff_nxn_p64_p64_pw_fptr_arr[2] = diff_nxn_p64_p64_pw_avx2<16>;
        diff_nxn_p64_p64_pw_fptr_arr[3] = diff_nxn_p64_p64_pw_avx2<32>;
        diff_nxn_p64_p64_pw_fptr_arr[4] = diff_nxn_p64_p64_pw_avx2<64>;

        diff_nxn_p64_p64_pw_hbd_fptr_arr[0] = diff_nxn_p64_p64_pw_px<4>;
        diff_nxn_p64_p64_pw_hbd_fptr_arr[1] = diff_nxn_p64_p64_pw_px<8>;
        diff_nxn_p64_p64_pw_hbd_fptr_arr[2] = diff_nxn_p64_p64_pw_px<16>;
        diff_nxn_p64_p64_pw_hbd_fptr_arr[3] = diff_nxn_p64_p64_pw_px<32>;
        diff_nxn_p64_p64_pw_hbd_fptr_arr[4] = diff_nxn_p64_p64_pw_px<64>;


        diff_nxm_fptr_arr[0] = diff_nxm_avx2<4>;
        diff_nxm_fptr_arr[1] = diff_nxm_avx2<8>;
        diff_nxm_fptr_arr[2] = diff_nxm_avx2<16>;
        diff_nxm_fptr_arr[3] = diff_nxm_avx2<32>;
        diff_nxm_fptr_arr[4] = diff_nxm_avx2<64>;
        diff_nxm_p64_p64_pw_fptr_arr[0] = diff_nxm_p64_p64_pw_avx2<4>;
        diff_nxm_p64_p64_pw_fptr_arr[1] = diff_nxm_p64_p64_pw_avx2<8>;
        diff_nxm_p64_p64_pw_fptr_arr[2] = diff_nxm_p64_p64_pw_avx2<16>;
        diff_nxm_p64_p64_pw_fptr_arr[3] = diff_nxm_p64_p64_pw_avx2<32>;
        diff_nxm_p64_p64_pw_fptr_arr[4] = diff_nxm_p64_p64_pw_avx2<64>;
        diff_nv12_fptr_arr[0] = diff_nv12_avx2<4>;
        diff_nv12_fptr_arr[1] = diff_nv12_avx2<8>;
        diff_nv12_fptr_arr[2] = diff_nv12_avx2<16>;
        diff_nv12_fptr_arr[3] = diff_nv12_avx2<32>;
        // hbd
        diff_nv12_hbd_fptr_arr[0] = diff_nv12_px<4>;
        diff_nv12_hbd_fptr_arr[1] = diff_nv12_px<8>;
        diff_nv12_hbd_fptr_arr[2] = diff_nv12_px<16>;
        diff_nv12_hbd_fptr_arr[3] = diff_nv12_px<32>;

        diff_nv12_p64_p64_pw_fptr_arr[0] = diff_nv12_p64_p64_pw_avx2<4>;
        diff_nv12_p64_p64_pw_fptr_arr[1] = diff_nv12_p64_p64_pw_avx2<8>;
        diff_nv12_p64_p64_pw_fptr_arr[2] = diff_nv12_p64_p64_pw_avx2<16>;
        diff_nv12_p64_p64_pw_fptr_arr[3] = diff_nv12_p64_p64_pw_avx2<32>;
        // hbd
        diff_nv12_p64_p64_pw_hbd_fptr_arr[0] = diff_nv12_p64_p64_pw_px<4>;
        diff_nv12_p64_p64_pw_hbd_fptr_arr[1] = diff_nv12_p64_p64_pw_px<8>;
        diff_nv12_p64_p64_pw_hbd_fptr_arr[2] = diff_nv12_p64_p64_pw_px<16>;
        diff_nv12_p64_p64_pw_hbd_fptr_arr[3] = diff_nv12_p64_p64_pw_px<32>;
        // satd
        satd_4x4_fptr = satd_4x4_avx2;
        satd_4x4_pitch64_fptr = satd_4x4_pitch64_avx2;
        satd_4x4_pitch64_both_fptr = satd_4x4_pitch64_both_avx2;
        satd_8x8_fptr = satd_8x8_avx2;
        satd_8x8_pitch64_fptr = satd_8x8_pitch64_avx2;
        satd_8x8_pitch64_both_fptr = satd_8x8_pitch64_both_avx2;
        satd_4x4_pair_fptr = satd_4x4_pair_avx2;
        satd_4x4_pair_pitch64_fptr = satd_4x4_pair_pitch64_avx2;
        satd_4x4_pair_pitch64_both_fptr = satd_4x4_pair_pitch64_both_avx2;
        satd_8x8_pair_fptr = satd_8x8_pair_avx2;
        satd_8x8_pair_pitch64_fptr = satd_8x8_pair_pitch64_avx2;
        satd_8x8_pair_pitch64_both_fptr = satd_8x8_pair_pitch64_both_avx2;
        satd_8x8x2_fptr = satd_8x8x2_avx2;
        satd_8x8x2_pitch64_fptr = satd_8x8x2_pitch64_avx2;
        satd_fptr_arr[0][0] = satd_avx2<4, 4>;
        satd_fptr_arr[0][1] = satd_avx2<4, 8>;
        satd_fptr_arr[0][2] = NULL;
        satd_fptr_arr[0][3] = NULL;
        satd_fptr_arr[0][4] = NULL;
        satd_fptr_arr[1][0] = satd_avx2<8, 4>;
        satd_fptr_arr[1][1] = satd_avx2<8, 8>;
        satd_fptr_arr[1][2] = satd_avx2<8, 16>;
        satd_fptr_arr[1][3] = NULL;
        satd_fptr_arr[1][4] = NULL;
        satd_fptr_arr[2][0] = NULL;
        satd_fptr_arr[2][1] = satd_avx2<16, 8>;
        satd_fptr_arr[2][2] = satd_avx2<16, 16>;
        satd_fptr_arr[2][3] = satd_avx2<16, 32>;
        satd_fptr_arr[2][4] = NULL;
        satd_fptr_arr[3][0] = NULL;
        satd_fptr_arr[3][1] = NULL;
        satd_fptr_arr[3][2] = satd_avx2<32, 16>;
        satd_fptr_arr[3][3] = satd_avx2<32, 32>;
        satd_fptr_arr[3][4] = satd_avx2<32, 64>;
        satd_fptr_arr[4][0] = NULL;
        satd_fptr_arr[4][1] = NULL;
        satd_fptr_arr[4][2] = NULL;
        satd_fptr_arr[4][3] = satd_avx2<64, 32>;
        satd_fptr_arr[4][4] = satd_avx2<64, 64>;
        satd_pitch64_fptr_arr[0][0] = satd_pitch64_avx2<4, 4>;
        satd_pitch64_fptr_arr[0][1] = satd_pitch64_avx2<4, 8>;
        satd_pitch64_fptr_arr[0][2] = NULL;
        satd_pitch64_fptr_arr[0][3] = NULL;
        satd_pitch64_fptr_arr[0][4] = NULL;
        satd_pitch64_fptr_arr[1][0] = satd_pitch64_avx2<8, 4>;
        satd_pitch64_fptr_arr[1][1] = satd_pitch64_avx2<8, 8>;
        satd_pitch64_fptr_arr[1][2] = satd_pitch64_avx2<8, 16>;
        satd_pitch64_fptr_arr[1][3] = NULL;
        satd_pitch64_fptr_arr[1][4] = NULL;
        satd_pitch64_fptr_arr[2][0] = NULL;
        satd_pitch64_fptr_arr[2][1] = satd_pitch64_avx2<16, 8>;
        satd_pitch64_fptr_arr[2][2] = satd_pitch64_avx2<16, 16>;
        satd_pitch64_fptr_arr[2][3] = satd_pitch64_avx2<16, 32>;
        satd_pitch64_fptr_arr[2][4] = NULL;
        satd_pitch64_fptr_arr[3][0] = NULL;
        satd_pitch64_fptr_arr[3][1] = NULL;
        satd_pitch64_fptr_arr[3][2] = satd_pitch64_avx2<32, 16>;
        satd_pitch64_fptr_arr[3][3] = satd_pitch64_avx2<32, 32>;
        satd_pitch64_fptr_arr[3][4] = satd_pitch64_avx2<32, 64>;
        satd_pitch64_fptr_arr[4][0] = NULL;
        satd_pitch64_fptr_arr[4][1] = NULL;
        satd_pitch64_fptr_arr[4][2] = NULL;
        satd_pitch64_fptr_arr[4][3] = satd_pitch64_avx2<64, 32>;
        satd_pitch64_fptr_arr[4][4] = satd_pitch64_avx2<64, 64>;
        satd_pitch64_both_fptr_arr[0][0] = satd_pitch64_both_avx2<4, 4>;
        satd_pitch64_both_fptr_arr[0][1] = satd_pitch64_both_avx2<4, 8>;
        satd_pitch64_both_fptr_arr[0][2] = NULL;
        satd_pitch64_both_fptr_arr[0][3] = NULL;
        satd_pitch64_both_fptr_arr[0][4] = NULL;
        satd_pitch64_both_fptr_arr[1][0] = satd_pitch64_both_avx2<8, 4>;
        satd_pitch64_both_fptr_arr[1][1] = satd_pitch64_both_avx2<8, 8>;
        satd_pitch64_both_fptr_arr[1][2] = satd_pitch64_both_avx2<8, 16>;
        satd_pitch64_both_fptr_arr[1][3] = NULL;
        satd_pitch64_both_fptr_arr[1][4] = NULL;
        satd_pitch64_both_fptr_arr[2][0] = NULL;
        satd_pitch64_both_fptr_arr[2][1] = satd_pitch64_both_avx2<16, 8>;
        satd_pitch64_both_fptr_arr[2][2] = satd_pitch64_both_avx2<16, 16>;
        satd_pitch64_both_fptr_arr[2][3] = satd_pitch64_both_avx2<16, 32>;
        satd_pitch64_both_fptr_arr[2][4] = NULL;
        satd_pitch64_both_fptr_arr[3][0] = NULL;
        satd_pitch64_both_fptr_arr[3][1] = NULL;
        satd_pitch64_both_fptr_arr[3][2] = satd_pitch64_both_avx2<32, 16>;
        satd_pitch64_both_fptr_arr[3][3] = satd_pitch64_both_avx2<32, 32>;
        satd_pitch64_both_fptr_arr[3][4] = satd_pitch64_both_avx2<32, 64>;
        satd_pitch64_both_fptr_arr[4][0] = NULL;
        satd_pitch64_both_fptr_arr[4][1] = NULL;
        satd_pitch64_both_fptr_arr[4][2] = NULL;
        satd_pitch64_both_fptr_arr[4][3] = satd_pitch64_both_avx2<64, 32>;
        satd_pitch64_both_fptr_arr[4][4] = satd_pitch64_both_avx2<64, 64>;
        satd_with_const_pitch64_fptr_arr[0][0] = satd_with_const_pitch64_avx2<4, 4>;
        satd_with_const_pitch64_fptr_arr[0][1] = satd_with_const_pitch64_avx2<4, 8>;
        satd_with_const_pitch64_fptr_arr[0][2] = NULL;
        satd_with_const_pitch64_fptr_arr[0][3] = NULL;
        satd_with_const_pitch64_fptr_arr[0][4] = NULL;
        satd_with_const_pitch64_fptr_arr[1][0] = satd_with_const_pitch64_avx2<8, 4>;
        satd_with_const_pitch64_fptr_arr[1][1] = satd_with_const_pitch64_avx2<8, 8>;
        satd_with_const_pitch64_fptr_arr[1][2] = satd_with_const_pitch64_avx2<8, 16>;
        satd_with_const_pitch64_fptr_arr[1][3] = NULL;
        satd_with_const_pitch64_fptr_arr[1][4] = NULL;
        satd_with_const_pitch64_fptr_arr[2][0] = NULL;
        satd_with_const_pitch64_fptr_arr[2][1] = satd_with_const_pitch64_avx2<16, 8>;
        satd_with_const_pitch64_fptr_arr[2][2] = satd_with_const_pitch64_avx2<16, 16>;
        satd_with_const_pitch64_fptr_arr[2][3] = satd_with_const_pitch64_avx2<16, 32>;
        satd_with_const_pitch64_fptr_arr[2][4] = NULL;
        satd_with_const_pitch64_fptr_arr[3][0] = NULL;
        satd_with_const_pitch64_fptr_arr[3][1] = NULL;
        satd_with_const_pitch64_fptr_arr[3][2] = satd_with_const_pitch64_avx2<32, 16>;
        satd_with_const_pitch64_fptr_arr[3][3] = satd_with_const_pitch64_avx2<32, 32>;
        satd_with_const_pitch64_fptr_arr[3][4] = satd_with_const_pitch64_avx2<32, 64>;
        satd_with_const_pitch64_fptr_arr[4][0] = NULL;
        satd_with_const_pitch64_fptr_arr[4][1] = NULL;
        satd_with_const_pitch64_fptr_arr[4][2] = NULL;
        satd_with_const_pitch64_fptr_arr[4][3] = satd_with_const_pitch64_avx2<64, 32>;
        satd_with_const_pitch64_fptr_arr[4][4] = satd_with_const_pitch64_avx2<64, 64>;
        // sse
        sse_fptr_arr[0][0] = sse_avx2<4, 4>;
        sse_fptr_arr[0][1] = sse_avx2<4, 8>;
        sse_fptr_arr[0][2] = NULL;
        sse_fptr_arr[0][3] = NULL;
        sse_fptr_arr[0][4] = NULL;
        sse_fptr_arr[1][0] = sse_avx2<8, 4>;
        sse_fptr_arr[1][1] = sse_avx2<8, 8>;
        sse_fptr_arr[1][2] = sse_avx2<8, 16>;
        sse_fptr_arr[1][3] = NULL;
        sse_fptr_arr[1][4] = NULL;
        sse_fptr_arr[2][0] = sse_avx2<16, 4>;
        sse_fptr_arr[2][1] = sse_avx2<16, 8>;
        sse_fptr_arr[2][2] = sse_avx2<16, 16>;
        sse_fptr_arr[2][3] = sse_avx2<16, 32>;
        sse_fptr_arr[2][4] = NULL;
        sse_fptr_arr[3][0] = NULL;
        sse_fptr_arr[3][1] = sse_avx2<32, 8>;
        sse_fptr_arr[3][2] = sse_avx2<32, 16>;
        sse_fptr_arr[3][3] = sse_avx2<32, 32>;
        sse_fptr_arr[3][4] = sse_avx2<32, 64>;
        sse_fptr_arr[4][0] = NULL;
        sse_fptr_arr[4][1] = NULL;
        sse_fptr_arr[4][2] = sse_avx2<64, 16>;
        sse_fptr_arr[4][3] = sse_avx2<64, 32>;
        sse_fptr_arr[4][4] = sse_avx2<64, 64>;

        sse_hbd_fptr_arr[0][0] = sse_px<4, 4>;
        sse_hbd_fptr_arr[0][1] = sse_px<4, 8>;
        sse_hbd_fptr_arr[0][2] = NULL;
        sse_hbd_fptr_arr[0][3] = NULL;
        sse_hbd_fptr_arr[0][4] = NULL;
        sse_hbd_fptr_arr[1][0] = sse_px<8, 4>;
        sse_hbd_fptr_arr[1][1] = sse_px<8, 8>;
        sse_hbd_fptr_arr[1][2] = sse_px<8, 16>;
        sse_hbd_fptr_arr[1][3] = NULL;
        sse_hbd_fptr_arr[1][4] = NULL;
        sse_hbd_fptr_arr[2][0] = sse_px<16, 4>;
        sse_hbd_fptr_arr[2][1] = sse_px<16, 8>;
        sse_hbd_fptr_arr[2][2] = sse_px<16, 16>;
        sse_hbd_fptr_arr[2][3] = sse_px<16, 32>;
        sse_hbd_fptr_arr[2][4] = NULL;
        sse_hbd_fptr_arr[3][0] = NULL;
        sse_hbd_fptr_arr[3][1] = sse_px<32, 8>;
        sse_hbd_fptr_arr[3][2] = sse_px<32, 16>;
        sse_hbd_fptr_arr[3][3] = sse_px<32, 32>;
        sse_hbd_fptr_arr[3][4] = sse_px<32, 64>;
        sse_hbd_fptr_arr[4][0] = NULL;
        sse_hbd_fptr_arr[4][1] = NULL;
        sse_hbd_fptr_arr[4][2] = sse_px<64, 16>;
        sse_hbd_fptr_arr[4][3] = sse_px<64, 32>;
        sse_hbd_fptr_arr[4][4] = sse_px<64, 64>;

        sse_p64_pw_fptr_arr[0][0] = sse_p64_pw_avx2<4, 4>;
        sse_p64_pw_fptr_arr[0][1] = sse_p64_pw_avx2<4, 8>;
        sse_p64_pw_fptr_arr[0][2] = NULL;
        sse_p64_pw_fptr_arr[0][3] = NULL;
        sse_p64_pw_fptr_arr[0][4] = NULL;
        sse_p64_pw_fptr_arr[1][0] = sse_p64_pw_avx2<8, 4>;
        sse_p64_pw_fptr_arr[1][1] = sse_p64_pw_avx2<8, 8>;
        sse_p64_pw_fptr_arr[1][2] = sse_p64_pw_avx2<8, 16>;
        sse_p64_pw_fptr_arr[1][3] = NULL;
        sse_p64_pw_fptr_arr[1][4] = NULL;
        sse_p64_pw_fptr_arr[2][0] = sse_p64_pw_avx2<16, 4>;
        sse_p64_pw_fptr_arr[2][1] = sse_p64_pw_avx2<16, 8>;
        sse_p64_pw_fptr_arr[2][2] = sse_p64_pw_avx2<16, 16>;
        sse_p64_pw_fptr_arr[2][3] = sse_p64_pw_avx2<16, 32>;
        sse_p64_pw_fptr_arr[2][4] = NULL;
        sse_p64_pw_fptr_arr[3][0] = NULL;
        sse_p64_pw_fptr_arr[3][1] = sse_p64_pw_avx2<32, 8>;
        sse_p64_pw_fptr_arr[3][2] = sse_p64_pw_avx2<32, 16>;
        sse_p64_pw_fptr_arr[3][3] = sse_p64_pw_avx2<32, 32>;
        sse_p64_pw_fptr_arr[3][4] = sse_p64_pw_avx2<32, 64>;
        sse_p64_pw_fptr_arr[4][0] = NULL;
        sse_p64_pw_fptr_arr[4][1] = NULL;
        sse_p64_pw_fptr_arr[4][2] = sse_p64_pw_avx2<64, 16>;
        sse_p64_pw_fptr_arr[4][3] = sse_p64_pw_avx2<64, 32>;
        sse_p64_pw_fptr_arr[4][4] = sse_p64_pw_avx2<64, 64>;

        sse_p64_pw_uv_fptr_arr[0][0] = sse_p64_pw_uv_avx2<4, 4>;
        sse_p64_pw_uv_fptr_arr[0][1] = sse_p64_pw_uv_avx2<4, 8>;
        sse_p64_pw_uv_fptr_arr[0][2] = NULL;
        sse_p64_pw_uv_fptr_arr[0][3] = NULL;
        sse_p64_pw_uv_fptr_arr[0][4] = NULL;
        sse_p64_pw_uv_fptr_arr[1][0] = sse_p64_pw_uv_avx2<8, 4>;
        sse_p64_pw_uv_fptr_arr[1][1] = sse_p64_pw_uv_avx2<8, 8>;
        sse_p64_pw_uv_fptr_arr[1][2] = sse_p64_pw_uv_avx2<8, 16>;
        sse_p64_pw_uv_fptr_arr[1][3] = NULL;
        sse_p64_pw_uv_fptr_arr[1][4] = NULL;
        sse_p64_pw_uv_fptr_arr[2][0] = sse_p64_pw_uv_avx2<16, 4>;
        sse_p64_pw_uv_fptr_arr[2][1] = sse_p64_pw_uv_avx2<16, 8>;
        sse_p64_pw_uv_fptr_arr[2][2] = sse_p64_pw_uv_avx2<16, 16>;
        sse_p64_pw_uv_fptr_arr[2][3] = sse_p64_pw_uv_avx2<16, 32>;
        sse_p64_pw_uv_fptr_arr[2][4] = NULL;
        sse_p64_pw_uv_fptr_arr[3][0] = NULL;
        sse_p64_pw_uv_fptr_arr[3][1] = sse_p64_pw_uv_avx2<32, 8>;
        sse_p64_pw_uv_fptr_arr[3][2] = sse_p64_pw_uv_avx2<32, 16>;
        sse_p64_pw_uv_fptr_arr[3][3] = sse_p64_pw_uv_avx2<32, 32>;
        sse_p64_pw_uv_fptr_arr[3][4] = sse_p64_pw_uv_avx2<32, 64>;
        sse_p64_pw_uv_fptr_arr[4][0] = NULL;
        sse_p64_pw_uv_fptr_arr[4][1] = NULL;
        sse_p64_pw_uv_fptr_arr[4][2] = sse_p64_pw_uv_avx2<64, 16>;
        sse_p64_pw_uv_fptr_arr[4][3] = sse_p64_pw_uv_avx2<64, 32>;
        sse_p64_pw_uv_fptr_arr[4][4] = sse_p64_pw_uv_avx2<64, 64>;


        sse_p64_p64_fptr_arr[0][0] = sse_p64_p64_avx2<4, 4>;
        sse_p64_p64_fptr_arr[0][1] = sse_p64_p64_avx2<4, 8>;
        sse_p64_p64_fptr_arr[0][2] = NULL;
        sse_p64_p64_fptr_arr[0][3] = NULL;
        sse_p64_p64_fptr_arr[0][4] = NULL;
        sse_p64_p64_fptr_arr[1][0] = sse_p64_p64_avx2<8, 4>;
        sse_p64_p64_fptr_arr[1][1] = sse_p64_p64_avx2<8, 8>;
        sse_p64_p64_fptr_arr[1][2] = sse_p64_p64_avx2<8, 16>;
        sse_p64_p64_fptr_arr[1][3] = NULL;
        sse_p64_p64_fptr_arr[1][4] = NULL;
        sse_p64_p64_fptr_arr[2][0] = sse_p64_p64_avx2<16, 4>;
        sse_p64_p64_fptr_arr[2][1] = sse_p64_p64_avx2<16, 8>;
        sse_p64_p64_fptr_arr[2][2] = sse_p64_p64_avx2<16, 16>;
        sse_p64_p64_fptr_arr[2][3] = sse_p64_p64_avx2<16, 32>;
        sse_p64_p64_fptr_arr[2][4] = NULL;
        sse_p64_p64_fptr_arr[3][0] = NULL;
        sse_p64_p64_fptr_arr[3][1] = sse_p64_p64_avx2<32, 8>;
        sse_p64_p64_fptr_arr[3][2] = sse_p64_p64_avx2<32, 16>;
        sse_p64_p64_fptr_arr[3][3] = sse_p64_p64_avx2<32, 32>;
        sse_p64_p64_fptr_arr[3][4] = sse_p64_p64_avx2<32, 64>;
        sse_p64_p64_fptr_arr[4][0] = NULL;
        sse_p64_p64_fptr_arr[4][1] = NULL;
        sse_p64_p64_fptr_arr[4][2] = sse_p64_p64_avx2<64, 16>;
        sse_p64_p64_fptr_arr[4][3] = sse_p64_p64_avx2<64, 32>;
        sse_p64_p64_fptr_arr[4][4] = sse_p64_p64_avx2<64, 64>;


        sse_p64_p64_hbd_fptr_arr[0][0] = sse_p64_p64_px<4, 4>;
        sse_p64_p64_hbd_fptr_arr[0][1] = sse_p64_p64_px<4, 8>;
        sse_p64_p64_hbd_fptr_arr[0][2] = NULL;
        sse_p64_p64_hbd_fptr_arr[0][3] = NULL;
        sse_p64_p64_hbd_fptr_arr[0][4] = NULL;
        sse_p64_p64_hbd_fptr_arr[1][0] = sse_p64_p64_px<8, 4>;
        sse_p64_p64_hbd_fptr_arr[1][1] = sse_p64_p64_px<8, 8>;
        sse_p64_p64_hbd_fptr_arr[1][2] = sse_p64_p64_px<8, 16>;
        sse_p64_p64_hbd_fptr_arr[1][3] = NULL;
        sse_p64_p64_hbd_fptr_arr[1][4] = NULL;
        sse_p64_p64_hbd_fptr_arr[2][0] = sse_p64_p64_px<16, 4>;
        sse_p64_p64_hbd_fptr_arr[2][1] = sse_p64_p64_px<16, 8>;
        sse_p64_p64_hbd_fptr_arr[2][2] = sse_p64_p64_px<16, 16>;
        sse_p64_p64_hbd_fptr_arr[2][3] = sse_p64_p64_px<16, 32>;
        sse_p64_p64_hbd_fptr_arr[2][4] = NULL;
        sse_p64_p64_hbd_fptr_arr[3][0] = NULL;
        sse_p64_p64_hbd_fptr_arr[3][1] = sse_p64_p64_px<32, 8>;
        sse_p64_p64_hbd_fptr_arr[3][2] = sse_p64_p64_px<32, 16>;
        sse_p64_p64_hbd_fptr_arr[3][3] = sse_p64_p64_px<32, 32>;
        sse_p64_p64_hbd_fptr_arr[3][4] = sse_p64_p64_px<32, 64>;
        sse_p64_p64_hbd_fptr_arr[4][0] = NULL;
        sse_p64_p64_hbd_fptr_arr[4][1] = NULL;
        sse_p64_p64_hbd_fptr_arr[4][2] = sse_p64_p64_px<64, 16>;
        sse_p64_p64_hbd_fptr_arr[4][3] = sse_p64_p64_px<64, 32>;
        sse_p64_p64_hbd_fptr_arr[4][4] = sse_p64_p64_px<64, 64>;


        sse_flexh_fptr_arr[0] = sse_avx2<4>;
        sse_flexh_fptr_arr[1] = sse_avx2<8>;
        sse_flexh_fptr_arr[2] = sse_avx2<16>;
        sse_flexh_fptr_arr[3] = sse_avx2<32>;
        sse_flexh_fptr_arr[4] = sse_avx2<64>;
        sse_cont_fptr/*_arr[4]*/ = sse_cont_avx2;
        ssz_cont_fptr/*_arr[4]*/ = ssz_cont_avx2;
        // sad
        sad_special_fptr_arr[0][0] = sad_special_avx2<4, 4>;
        sad_special_fptr_arr[0][1] = sad_special_avx2<4, 8>;
        sad_special_fptr_arr[0][2] = NULL;
        sad_special_fptr_arr[0][3] = NULL;
        sad_special_fptr_arr[0][4] = NULL;
        sad_special_fptr_arr[1][0] = sad_special_avx2<8, 4>;
        sad_special_fptr_arr[1][1] = sad_special_avx2<8, 8>;
        sad_special_fptr_arr[1][2] = sad_special_avx2<8, 16>;
        sad_special_fptr_arr[1][3] = NULL;
        sad_special_fptr_arr[1][4] = NULL;
        sad_special_fptr_arr[2][0] = NULL;
        sad_special_fptr_arr[2][1] = sad_special_avx2<16, 8>;
        sad_special_fptr_arr[2][2] = sad_special_avx2<16, 16>;
        sad_special_fptr_arr[2][3] = sad_special_avx2<16, 32>;
        sad_special_fptr_arr[2][4] = NULL;
        sad_special_fptr_arr[3][0] = NULL;
        sad_special_fptr_arr[3][1] = NULL;
        sad_special_fptr_arr[3][2] = sad_special_avx2<32, 16>;
        sad_special_fptr_arr[3][3] = sad_special_avx2<32, 32>;
        sad_special_fptr_arr[3][4] = sad_special_avx2<32, 64>;
        sad_special_fptr_arr[4][0] = NULL;
        sad_special_fptr_arr[4][1] = NULL;
        sad_special_fptr_arr[4][2] = NULL;
        sad_special_fptr_arr[4][3] = sad_special_avx2<64, 32>;
        sad_special_fptr_arr[4][4] = sad_special_avx2<64, 64>;
        sad_general_fptr_arr[0][0] = sad_general_avx2<4, 4>;
        sad_general_fptr_arr[0][1] = sad_general_avx2<4, 8>;
        sad_general_fptr_arr[0][2] = NULL;
        sad_general_fptr_arr[0][3] = NULL;
        sad_general_fptr_arr[0][4] = NULL;
        sad_general_fptr_arr[1][0] = sad_general_avx2<8, 4>;
        sad_general_fptr_arr[1][1] = sad_general_avx2<8, 8>;
        sad_general_fptr_arr[1][2] = sad_general_avx2<8, 16>;
        sad_general_fptr_arr[1][3] = NULL;
        sad_general_fptr_arr[1][4] = NULL;
        sad_general_fptr_arr[2][0] = NULL;
        sad_general_fptr_arr[2][1] = sad_general_avx2<16, 8>;
        sad_general_fptr_arr[2][2] = sad_general_avx2<16, 16>;
        sad_general_fptr_arr[2][3] = sad_general_avx2<16, 32>;
        sad_general_fptr_arr[2][4] = NULL;
        sad_general_fptr_arr[3][0] = NULL;
        sad_general_fptr_arr[3][1] = NULL;
        sad_general_fptr_arr[3][2] = sad_general_avx2<32, 16>;
        sad_general_fptr_arr[3][3] = sad_general_avx2<32, 32>;
        sad_general_fptr_arr[3][4] = sad_general_avx2<32, 64>;
        sad_general_fptr_arr[4][0] = NULL;
        sad_general_fptr_arr[4][1] = NULL;
        sad_general_fptr_arr[4][2] = NULL;
        sad_general_fptr_arr[4][3] = sad_general_avx2<64, 32>;
        sad_general_fptr_arr[4][4] = sad_general_avx2<64, 64>;
        sad_store8x8_fptr_arr[0] = nullptr;
        sad_store8x8_fptr_arr[1] = sad_store8x8_avx2<8>;
        sad_store8x8_fptr_arr[2] = sad_store8x8_avx2<16>;
        sad_store8x8_fptr_arr[3] = sad_store8x8_avx2<32>;
        sad_store8x8_fptr_arr[4] = sad_store8x8_avx2<64>;

        // deblocking
        lpf_fptr_arr[VERT_EDGE][0] = lpf_vertical_4_8u_av1_sse2;
        lpf_fptr_arr[VERT_EDGE][1] = lpf_vertical_8_8u_av1_sse2;
        lpf_fptr_arr[VERT_EDGE][2] = lpf_vertical_14_8u_av1_avx2;
        lpf_fptr_arr[VERT_EDGE][3] = lpf_vertical_6_8u_av1_sse2;
        lpf_fptr_arr[HORZ_EDGE][0] = lpf_horizontal_4_8u_av1_sse2;
        lpf_fptr_arr[HORZ_EDGE][1] = lpf_horizontal_8_8u_av1_sse2;
        lpf_fptr_arr[HORZ_EDGE][2] = lpf_horizontal_14_8u_av1_sse2;
        lpf_fptr_arr[HORZ_EDGE][3] = lpf_horizontal_6_8u_av1_sse2;

        lpf_hbd_fptr_arr[VERT_EDGE][0] = lpf_vertical_4_av1_px;
        lpf_hbd_fptr_arr[VERT_EDGE][1] = lpf_vertical_8_av1_px;
        lpf_hbd_fptr_arr[VERT_EDGE][2] = lpf_vertical_14_av1_px;
        lpf_hbd_fptr_arr[VERT_EDGE][3] = lpf_vertical_6_av1_px;
        lpf_hbd_fptr_arr[HORZ_EDGE][0] = lpf_horizontal_4_av1_px;
        lpf_hbd_fptr_arr[HORZ_EDGE][1] = lpf_horizontal_8_av1_px;
        lpf_hbd_fptr_arr[HORZ_EDGE][2] = lpf_horizontal_14_av1_px;
        lpf_hbd_fptr_arr[HORZ_EDGE][3] = lpf_horizontal_6_av1_px;

        // cdef
        cdef_find_dir_fptr = cdef_find_dir_avx2;
        cdef_filter_block_u8_fptr_arr[0] = cdef_filter_block_8x8_avx2<uint8_t>;
        cdef_filter_block_u8_fptr_arr[1] = cdef_filter_block_4x4_nv12_avx2<uint8_t>;
        cdef_filter_block_u16_fptr_arr[0] = cdef_filter_block_8x8_avx2<uint16_t>;
        cdef_filter_block_u16_fptr_arr[1] = cdef_filter_block_4x4_nv12_avx2<uint16_t>;
        cdef_estimate_block_fptr_arr[0] = cdef_estimate_block_8x8_avx2;
        cdef_estimate_block_fptr_arr[1] = cdef_estimate_block_4x4_nv12_avx2;

        cdef_estimate_block_hbd_fptr_arr[0] = cdef_estimate_block_8x8_px;
        cdef_estimate_block_hbd_fptr_arr[1] = cdef_estimate_block_4x4_nv12_px;

        cdef_estimate_block_sec0_fptr_arr[0] = cdef_estimate_block_8x8_sec0_avx2;
        cdef_estimate_block_sec0_fptr_arr[1] = cdef_estimate_block_4x4_nv12_sec0_avx2;

        cdef_estimate_block_sec0_hbd_fptr_arr[0] = cdef_estimate_block_8x8_sec0_px;
        cdef_estimate_block_sec0_hbd_fptr_arr[1] = cdef_estimate_block_4x4_nv12_sec0_px;

        cdef_estimate_block_pri0_fptr_arr[0] = cdef_estimate_block_8x8_pri0_avx2;
        cdef_estimate_block_pri0_fptr_arr[1] = cdef_estimate_block_4x4_nv12_pri0_avx2;
        cdef_estimate_block_all_sec_fptr_arr[0] = cdef_estimate_block_8x8_all_sec_avx2;
        cdef_estimate_block_all_sec_fptr_arr[1] = cdef_estimate_block_4x4_nv12_all_sec_avx2;

        cdef_estimate_block_all_sec_hbd_fptr_arr[0] = cdef_estimate_block_8x8_all_sec_px;
        cdef_estimate_block_all_sec_hbd_fptr_arr[1] = cdef_estimate_block_4x4_nv12_all_sec_px;

        cdef_estimate_block_2pri_fptr_arr[0] = cdef_estimate_block_8x8_2pri_avx2;
        cdef_estimate_block_2pri_fptr_arr[1] = cdef_estimate_block_4x4_nv12_2pri_avx2;
        // etc
        diff_dc_fptr = NULL; //diff_dc_px;
        adds_nv12_fptr = adds_nv12_avx2;
        adds_nv12_hbd_fptr = adds_nv12_hbd_px;
        search_best_block8x8_fptr = search_best_block8x8_avx2;
        compute_rscs_fptr = compute_rscs_avx2;
        compute_rscs_4x4_fptr = compute_rscs_4x4_avx2;
        compute_rscs_diff_fptr = compute_rscs_diff_avx2;
        diff_histogram_fptr = diff_histogram_px;

        cfl_subtract_average_fptr_arr[TX_4X4] = subtract_average_sse2<4,4>;
        cfl_subtract_average_fptr_arr[TX_8X8] = subtract_average_sse2<8,8>;
        cfl_subtract_average_fptr_arr[TX_16X16] = subtract_average_avx2<16, 16>;
        cfl_subtract_average_fptr_arr[TX_32X32] = subtract_average_avx2<32, 32>;
        cfl_subtract_average_fptr_arr[TX_64X64] = nullptr;
        cfl_subtract_average_fptr_arr[TX_4X8] = subtract_average_sse2<4, 8>;
        cfl_subtract_average_fptr_arr[TX_8X4] = subtract_average_sse2<8, 4>;
        cfl_subtract_average_fptr_arr[TX_8X16] = subtract_average_sse2<8, 16>;
        cfl_subtract_average_fptr_arr[TX_16X8] = subtract_average_avx2<16, 8>;
        cfl_subtract_average_fptr_arr[TX_16X32] = subtract_average_avx2<16, 32>;
        cfl_subtract_average_fptr_arr[TX_32X16] = subtract_average_avx2<32, 16>;
        cfl_subtract_average_fptr_arr[TX_32X64] = nullptr;
        cfl_subtract_average_fptr_arr[TX_64X32] = nullptr;
        cfl_subtract_average_fptr_arr[TX_4X16] = subtract_average_sse2<4, 16>;
        cfl_subtract_average_fptr_arr[TX_16X4] = subtract_average_avx2<16, 4>;
        cfl_subtract_average_fptr_arr[TX_8X32] = subtract_average_sse2<8, 32>;
        cfl_subtract_average_fptr_arr[TX_32X8] = subtract_average_avx2<32, 8>;
        cfl_subtract_average_fptr_arr[TX_16X64] = nullptr;
        cfl_subtract_average_fptr_arr[TX_64X16] = nullptr;

        //avx2, only 32x32, 32x16, 32x8, other ssse3
        cfl_subsample_420_u8_fptr_arr[TX_4X4] = cfl_luma_subsampling_420_u8_ssse3<4, 4>;
        cfl_subsample_420_u8_fptr_arr[TX_8X8] = cfl_luma_subsampling_420_u8_ssse3<8, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_16X16] = cfl_luma_subsampling_420_u8_ssse3<16, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_32X32] = cfl_luma_subsampling_420_u8_avx2<32, 32>;
        cfl_subsample_420_u8_fptr_arr[TX_64X64] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_4X8] = cfl_luma_subsampling_420_u8_ssse3<4, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_8X4] = cfl_luma_subsampling_420_u8_ssse3<8, 4>;
        cfl_subsample_420_u8_fptr_arr[TX_8X16] = cfl_luma_subsampling_420_u8_ssse3<8, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_16X8] = cfl_luma_subsampling_420_u8_ssse3<16, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_16X32] = cfl_luma_subsampling_420_u8_ssse3<16, 32>;
        cfl_subsample_420_u8_fptr_arr[TX_32X16] = cfl_luma_subsampling_420_u8_avx2<32, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_32X64] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_64X32] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_4X16] = cfl_luma_subsampling_420_u8_ssse3<4, 16>;
        cfl_subsample_420_u8_fptr_arr[TX_16X4] = cfl_luma_subsampling_420_u8_ssse3<16, 4>;
        cfl_subsample_420_u8_fptr_arr[TX_8X32] = cfl_luma_subsampling_420_u8_ssse3<8, 32>;
        cfl_subsample_420_u8_fptr_arr[TX_32X8] = cfl_luma_subsampling_420_u8_avx2<32, 8>;
        cfl_subsample_420_u8_fptr_arr[TX_16X64] = nullptr;
        cfl_subsample_420_u8_fptr_arr[TX_64X16] = nullptr;

        //avx2, only 32x32, 32x16, 32x8, other ssse3
        cfl_subsample_420_u16_fptr_arr[TX_4X4] = cfl_luma_subsampling_420_u16_ssse3<4, 4>;
        cfl_subsample_420_u16_fptr_arr[TX_8X8] = cfl_luma_subsampling_420_u16_ssse3<8, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_16X16] = cfl_luma_subsampling_420_u16_ssse3<16, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_32X32] = cfl_luma_subsampling_420_u16_avx2<32, 32>;
        cfl_subsample_420_u16_fptr_arr[TX_64X64] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_4X8] = cfl_luma_subsampling_420_u16_ssse3<4, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_8X4] = cfl_luma_subsampling_420_u16_ssse3<8, 4>;
        cfl_subsample_420_u16_fptr_arr[TX_8X16] = cfl_luma_subsampling_420_u16_ssse3<8, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_16X8] = cfl_luma_subsampling_420_u16_ssse3<16, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_16X32] = cfl_luma_subsampling_420_u16_ssse3<16, 32>;
        cfl_subsample_420_u16_fptr_arr[TX_32X16] = cfl_luma_subsampling_420_u16_avx2<32, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_32X64] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_64X32] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_4X16] = cfl_luma_subsampling_420_u16_ssse3<4, 16>;
        cfl_subsample_420_u16_fptr_arr[TX_16X4] = cfl_luma_subsampling_420_u16_ssse3<16, 4>;
        cfl_subsample_420_u16_fptr_arr[TX_8X32] = cfl_luma_subsampling_420_u16_ssse3<8, 32>;
        cfl_subsample_420_u16_fptr_arr[TX_32X8] = cfl_luma_subsampling_420_u16_avx2<32, 8>;
        cfl_subsample_420_u16_fptr_arr[TX_16X64] = nullptr;
        cfl_subsample_420_u16_fptr_arr[TX_64X16] = nullptr;

        //avx2, only 32x32, 32x16, 32x8, other ssse3
        cfl_predict_nv12_u8_fptr_arr[TX_4X4] = cfl_predict_nv12_ssse3<uint8_t, 4, 4>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X8] = cfl_predict_nv12_ssse3<uint8_t, 8, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X16] = cfl_predict_nv12_avx2<uint8_t, 16, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X32] = cfl_predict_nv12_avx2<uint8_t, 32, 32>;
        cfl_predict_nv12_u8_fptr_arr[TX_64X64] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_4X8] = cfl_predict_nv12_ssse3<uint8_t, 4, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X4] = cfl_predict_nv12_ssse3<uint8_t, 8, 4>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X16] = cfl_predict_nv12_ssse3<uint8_t, 8, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X8] = cfl_predict_nv12_avx2<uint8_t, 16, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X32] = cfl_predict_nv12_avx2<uint8_t, 16, 32>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X16] = cfl_predict_nv12_avx2<uint8_t, 32, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X64] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_64X32] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_4X16] = cfl_predict_nv12_ssse3<uint8_t, 4, 16>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X4] = cfl_predict_nv12_avx2<uint8_t, 16, 4>;
        cfl_predict_nv12_u8_fptr_arr[TX_8X32] = cfl_predict_nv12_ssse3<uint8_t, 8, 32>;
        cfl_predict_nv12_u8_fptr_arr[TX_32X8] = cfl_predict_nv12_avx2<uint8_t, 32, 8>;
        cfl_predict_nv12_u8_fptr_arr[TX_16X64] = nullptr;
        cfl_predict_nv12_u8_fptr_arr[TX_64X16] = nullptr;

        //avx2, only 32x32, 32x16, 32x8, other ssse3
        cfl_predict_nv12_u16_fptr_arr[TX_4X4] = cfl_predict_nv12_ssse3<uint16_t, 4, 4>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X8] = cfl_predict_nv12_ssse3<uint16_t, 8, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X16] = cfl_predict_nv12_avx2<uint16_t, 16, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X32] = cfl_predict_nv12_avx2<uint16_t, 32, 32>;
        cfl_predict_nv12_u16_fptr_arr[TX_64X64] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_4X8] = cfl_predict_nv12_ssse3<uint16_t, 4, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X4] = cfl_predict_nv12_ssse3<uint16_t, 8, 4>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X16] = cfl_predict_nv12_ssse3<uint16_t, 8, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X8] = cfl_predict_nv12_ssse3<uint16_t, 16, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X32] = cfl_predict_nv12_ssse3<uint16_t, 16, 32>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X16] = cfl_predict_nv12_avx2<uint16_t, 32, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X64] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_64X32] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_4X16] = cfl_predict_nv12_ssse3<uint16_t, 4, 16>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X4] = cfl_predict_nv12_ssse3<uint16_t, 16, 4>;
        cfl_predict_nv12_u16_fptr_arr[TX_8X32] = cfl_predict_nv12_ssse3<uint16_t, 8, 32>;
        cfl_predict_nv12_u16_fptr_arr[TX_32X8] = cfl_predict_nv12_avx2<uint16_t, 32, 8>;
        cfl_predict_nv12_u16_fptr_arr[TX_16X64] = nullptr;
        cfl_predict_nv12_u16_fptr_arr[TX_64X16] = nullptr;
    }

} // AV1PP

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
