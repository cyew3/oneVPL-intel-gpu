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

#include "mfx_av1_defs.h"
#include "mfx_av1_dispatcher.h"
#include "mfx_av1_dispatcher_proto.h"
#include "mfx_av1_dispatcher_fptr.h"

namespace VP9PP {
    void SetTargetAVX2(int32_t isAV1);
    void SetTargetPX(int32_t isAV1);

    int initDispatcher(int32_t cpuFeature, int32_t isAV1)
    {
        if (cpuFeature == CPU_FEAT_AUTO) {
            // GetPlatformType
            //uint32_t cpuIdInfoRegs[4];
            //uint64_t featuresMask;
            //int sts = ippGetCpuFeatures(&featuresMask, cpuIdInfoRegs);
            //if (0 != sts)
            //    return sts;

            //cpuFeature = CPU_FEAT_PX;
            //if (featuresMask & (uint64_t)(ippCPUID_AVX2)) // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
            //    cpuFeature = CPU_FEAT_AVX2;

            cpuFeature = CPU_FEAT_AVX2;
        }

        switch (cpuFeature) {
        default:
        case CPU_FEAT_PX:   SetTargetPX(isAV1);   break;
        case CPU_FEAT_AVX2: {SetTargetAVX2(isAV1); break;}
        }

        return 0;
    }

    void SetTargetPX(int32_t isAV1)
    {
        using namespace H265Enc;

        if (!isAV1) {
        } else {
            ftransform_av1_fptr_arr[TX_4X4  ][DCT_DCT  ] = ftransform_av1_px<TX_4X4,   DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_4X4  ][ADST_DCT ] = ftransform_av1_px<TX_4X4,   ADST_DCT >;
            ftransform_av1_fptr_arr[TX_4X4  ][DCT_ADST ] = ftransform_av1_px<TX_4X4,   DCT_ADST >;
            ftransform_av1_fptr_arr[TX_4X4  ][ADST_ADST] = ftransform_av1_px<TX_4X4,   ADST_ADST>;
            ftransform_av1_fptr_arr[TX_8X8  ][DCT_DCT  ] = ftransform_av1_px<TX_8X8,   DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_8X8  ][ADST_DCT ] = ftransform_av1_px<TX_8X8,   ADST_DCT >;
            ftransform_av1_fptr_arr[TX_8X8  ][DCT_ADST ] = ftransform_av1_px<TX_8X8,   DCT_ADST >;
            ftransform_av1_fptr_arr[TX_8X8  ][ADST_ADST] = ftransform_av1_px<TX_8X8,   ADST_ADST>;
            ftransform_av1_fptr_arr[TX_16X16][DCT_DCT  ] = ftransform_vp9_px<TX_16X16, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_16X16][ADST_DCT ] = ftransform_vp9_px<TX_16X16, ADST_DCT >;
            ftransform_av1_fptr_arr[TX_16X16][DCT_ADST ] = ftransform_vp9_px<TX_16X16, DCT_ADST >;
            ftransform_av1_fptr_arr[TX_16X16][ADST_ADST] = ftransform_vp9_px<TX_16X16, ADST_ADST>;
            ftransform_av1_fptr_arr[TX_32X32][DCT_DCT  ] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_32X32][ADST_DCT ] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_32X32][DCT_ADST ] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_32X32][ADST_ADST] = ftransform_av1_px<TX_32X32, DCT_DCT  >;
            // inverse transform (for chroma) [txSize][txType]
            itransform_av1_fptr_arr[TX_4X4  ][DCT_DCT  ] = itransform_av1_px<TX_4X4,   DCT_DCT  >;
            itransform_av1_fptr_arr[TX_4X4  ][ADST_DCT ] = itransform_av1_px<TX_4X4,   ADST_DCT >;
            itransform_av1_fptr_arr[TX_4X4  ][DCT_ADST ] = itransform_av1_px<TX_4X4,   DCT_ADST >;
            itransform_av1_fptr_arr[TX_4X4  ][ADST_ADST] = itransform_av1_px<TX_4X4,   ADST_ADST>;
            itransform_av1_fptr_arr[TX_8X8  ][DCT_DCT  ] = itransform_av1_px<TX_8X8,   DCT_DCT  >;
            itransform_av1_fptr_arr[TX_8X8  ][ADST_DCT ] = itransform_av1_px<TX_8X8,   ADST_DCT >;
            itransform_av1_fptr_arr[TX_8X8  ][DCT_ADST ] = itransform_av1_px<TX_8X8,   DCT_ADST >;
            itransform_av1_fptr_arr[TX_8X8  ][ADST_ADST] = itransform_av1_px<TX_8X8,   ADST_ADST>;
            itransform_av1_fptr_arr[TX_16X16][DCT_DCT  ] = itransform_av1_px<TX_16X16, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_16X16][ADST_DCT ] = itransform_av1_px<TX_16X16, ADST_DCT >;
            itransform_av1_fptr_arr[TX_16X16][DCT_ADST ] = itransform_av1_px<TX_16X16, DCT_ADST >;
            itransform_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_av1_px<TX_16X16, ADST_ADST>;
            itransform_av1_fptr_arr[TX_32X32][DCT_DCT  ] = itransform_av1_px<TX_32X32, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_32X32][ADST_DCT ] = itransform_av1_px<TX_32X32, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_32X32][DCT_ADST ] = itransform_av1_px<TX_32X32, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_av1_px<TX_32X32, DCT_DCT  >;
            // inverse transform and addition (for luma) [txSize][txType]
            itransform_add_av1_fptr_arr[TX_4X4  ][DCT_DCT  ] = itransform_add_av1_px<TX_4X4,   DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_4X4  ][ADST_DCT ] = itransform_add_av1_px<TX_4X4,   ADST_DCT >;
            itransform_add_av1_fptr_arr[TX_4X4  ][DCT_ADST ] = itransform_add_av1_px<TX_4X4,   DCT_ADST >;
            itransform_add_av1_fptr_arr[TX_4X4  ][ADST_ADST] = itransform_add_av1_px<TX_4X4,   ADST_ADST>;
            itransform_add_av1_fptr_arr[TX_8X8  ][DCT_DCT  ] = itransform_add_av1_px<TX_8X8,   DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_8X8  ][ADST_DCT ] = itransform_add_av1_px<TX_8X8,   ADST_DCT >;
            itransform_add_av1_fptr_arr[TX_8X8  ][DCT_ADST ] = itransform_add_av1_px<TX_8X8,   DCT_ADST >;
            itransform_add_av1_fptr_arr[TX_8X8  ][ADST_ADST] = itransform_add_av1_px<TX_8X8,   ADST_ADST>;
            itransform_add_av1_fptr_arr[TX_16X16][DCT_DCT  ] = itransform_add_av1_px<TX_16X16, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_16X16][ADST_DCT ] = itransform_add_av1_px<TX_16X16, ADST_DCT >;
            itransform_add_av1_fptr_arr[TX_16X16][DCT_ADST ] = itransform_add_av1_px<TX_16X16, DCT_ADST >;
            itransform_add_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_add_av1_px<TX_16X16, ADST_ADST>;
            itransform_add_av1_fptr_arr[TX_32X32][DCT_DCT  ] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_32X32][ADST_DCT ] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_32X32][DCT_ADST ] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_add_av1_px<TX_32X32, DCT_DCT  >;
        }
        // quantization [txSize]
        quant_fptr_arr[TX_4X4  ] = quant_px<TX_4X4>;
        quant_fptr_arr[TX_8X8  ] = quant_px<TX_8X8>;
        quant_fptr_arr[TX_16X16] = quant_px<TX_16X16>;
        quant_fptr_arr[TX_32X32] = quant_px<TX_32X32>;
        dequant_fptr_arr[TX_4X4  ] = dequant_px<TX_4X4>;
        dequant_fptr_arr[TX_8X8  ] = dequant_px<TX_8X8>;
        dequant_fptr_arr[TX_16X16] = dequant_px<TX_16X16>;
        dequant_fptr_arr[TX_32X32] = dequant_px<TX_32X32>;
        quant_dequant_fptr_arr[TX_4X4  ] = quant_dequant_px<TX_4X4>;
        quant_dequant_fptr_arr[TX_8X8  ] = quant_dequant_px<TX_8X8>;
        quant_dequant_fptr_arr[TX_16X16] = quant_dequant_px<TX_16X16>;
        quant_dequant_fptr_arr[TX_32X32] = quant_dequant_px<TX_32X32>;
        if (isAV1) {
            // luma inter prediction [log2(width/4)] [dx!=0] [dy!=0]
            interp_av1_single_ref_fptr_arr = interp_av1_single_ref_fptr_arr_px;
            interp_av1_first_ref_fptr_arr  = interp_av1_first_ref_fptr_arr_px;
            interp_av1_second_ref_fptr_arr = interp_av1_second_ref_fptr_arr_px;
            // luma inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0]
            interp_pitch64_av1_single_ref_fptr_arr = interp_pitch64_av1_single_ref_fptr_arr_px;
            interp_pitch64_av1_first_ref_fptr_arr  = interp_pitch64_av1_first_ref_fptr_arr_px;
            interp_pitch64_av1_second_ref_fptr_arr = interp_pitch64_av1_second_ref_fptr_arr_px;

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
        } else {
        }
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
        diff_nv12_p64_p64_pw_fptr_arr[0] = diff_nv12_p64_p64_pw_px<4>;
        diff_nv12_p64_p64_pw_fptr_arr[1] = diff_nv12_p64_p64_pw_px<8>;
        diff_nv12_p64_p64_pw_fptr_arr[2] = diff_nv12_p64_p64_pw_px<16>;
        diff_nv12_p64_p64_pw_fptr_arr[3] = diff_nv12_p64_p64_pw_px<32>;
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
    }
    void SetTargetAVX2(int32_t isAV1) {
        using namespace H265Enc;

        if (!isAV1) {
        } else {
            // forward transform [txSize][txType]
            ftransform_av1_fptr_arr[TX_4X4  ][DCT_DCT  ] = ftransform_av1_avx2<TX_4X4,   DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_4X4  ][ADST_DCT ] = ftransform_av1_avx2<TX_4X4,   ADST_DCT >;
            ftransform_av1_fptr_arr[TX_4X4  ][DCT_ADST ] = ftransform_av1_avx2<TX_4X4,   DCT_ADST >;
            ftransform_av1_fptr_arr[TX_4X4  ][ADST_ADST] = ftransform_av1_avx2<TX_4X4,   ADST_ADST>;
            ftransform_av1_fptr_arr[TX_8X8  ][DCT_DCT  ] = ftransform_av1_avx2<TX_8X8,   DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_8X8  ][ADST_DCT ] = ftransform_av1_avx2<TX_8X8,   ADST_DCT >;
            ftransform_av1_fptr_arr[TX_8X8  ][DCT_ADST ] = ftransform_av1_avx2<TX_8X8,   DCT_ADST >;
            ftransform_av1_fptr_arr[TX_8X8  ][ADST_ADST] = ftransform_av1_avx2<TX_8X8,   ADST_ADST>;
            ftransform_av1_fptr_arr[TX_16X16][DCT_DCT  ] = ftransform_vp9_avx2<TX_16X16, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_16X16][ADST_DCT ] = ftransform_vp9_avx2<TX_16X16, ADST_DCT >;
            ftransform_av1_fptr_arr[TX_16X16][DCT_ADST ] = ftransform_vp9_avx2<TX_16X16, DCT_ADST >;
            ftransform_av1_fptr_arr[TX_16X16][ADST_ADST] = ftransform_vp9_avx2<TX_16X16, ADST_ADST>;
            ftransform_av1_fptr_arr[TX_32X32][DCT_DCT  ] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_32X32][ADST_DCT ] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_32X32][DCT_ADST ] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
            ftransform_av1_fptr_arr[TX_32X32][ADST_ADST] = ftransform_av1_avx2<TX_32X32, DCT_DCT  >;
            // forward transform (for chroma) [txSize][txType]
            itransform_av1_fptr_arr[TX_4X4  ][DCT_DCT  ] = itransform_av1_avx2<TX_4X4,   DCT_DCT  >;
            itransform_av1_fptr_arr[TX_4X4  ][ADST_DCT ] = itransform_av1_avx2<TX_4X4,   ADST_DCT >;
            itransform_av1_fptr_arr[TX_4X4  ][DCT_ADST ] = itransform_av1_avx2<TX_4X4,   DCT_ADST >;
            itransform_av1_fptr_arr[TX_4X4  ][ADST_ADST] = itransform_av1_avx2<TX_4X4,   ADST_ADST>;
            itransform_av1_fptr_arr[TX_8X8  ][DCT_DCT  ] = itransform_av1_avx2<TX_8X8,   DCT_DCT  >;
            itransform_av1_fptr_arr[TX_8X8  ][ADST_DCT ] = itransform_av1_avx2<TX_8X8,   ADST_DCT >;
            itransform_av1_fptr_arr[TX_8X8  ][DCT_ADST ] = itransform_av1_avx2<TX_8X8,   DCT_ADST >;
            itransform_av1_fptr_arr[TX_8X8  ][ADST_ADST] = itransform_av1_avx2<TX_8X8,   ADST_ADST>;
            itransform_av1_fptr_arr[TX_16X16][DCT_DCT  ] = itransform_av1_avx2<TX_16X16, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_16X16][ADST_DCT ] = itransform_av1_avx2<TX_16X16, ADST_DCT >;
            itransform_av1_fptr_arr[TX_16X16][DCT_ADST ] = itransform_av1_avx2<TX_16X16, DCT_ADST >;
            itransform_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_av1_avx2<TX_16X16, ADST_ADST>;
            itransform_av1_fptr_arr[TX_32X32][DCT_DCT  ] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_32X32][ADST_DCT ] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_32X32][DCT_ADST ] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
            itransform_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_av1_avx2<TX_32X32, DCT_DCT  >;
            // forward transform and addition (for luma) [txSize][txType]
            itransform_add_av1_fptr_arr[TX_4X4  ][DCT_DCT  ] = itransform_add_av1_avx2<TX_4X4,   DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_4X4  ][ADST_DCT ] = itransform_add_av1_avx2<TX_4X4,   ADST_DCT >;
            itransform_add_av1_fptr_arr[TX_4X4  ][DCT_ADST ] = itransform_add_av1_avx2<TX_4X4,   DCT_ADST >;
            itransform_add_av1_fptr_arr[TX_4X4  ][ADST_ADST] = itransform_add_av1_avx2<TX_4X4,   ADST_ADST>;
            itransform_add_av1_fptr_arr[TX_8X8  ][DCT_DCT  ] = itransform_add_av1_avx2<TX_8X8,   DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_8X8  ][ADST_DCT ] = itransform_add_av1_avx2<TX_8X8,   ADST_DCT >;
            itransform_add_av1_fptr_arr[TX_8X8  ][DCT_ADST ] = itransform_add_av1_avx2<TX_8X8,   DCT_ADST >;
            itransform_add_av1_fptr_arr[TX_8X8  ][ADST_ADST] = itransform_add_av1_avx2<TX_8X8,   ADST_ADST>;
            itransform_add_av1_fptr_arr[TX_16X16][DCT_DCT  ] = itransform_add_av1_avx2<TX_16X16, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_16X16][ADST_DCT ] = itransform_add_av1_avx2<TX_16X16, ADST_DCT >;
            itransform_add_av1_fptr_arr[TX_16X16][DCT_ADST ] = itransform_add_av1_avx2<TX_16X16, DCT_ADST >;
            itransform_add_av1_fptr_arr[TX_16X16][ADST_ADST] = itransform_add_av1_avx2<TX_16X16, ADST_ADST>;
            itransform_add_av1_fptr_arr[TX_32X32][DCT_DCT  ] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_32X32][ADST_DCT ] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_32X32][DCT_ADST ] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
            itransform_add_av1_fptr_arr[TX_32X32][ADST_ADST] = itransform_add_av1_avx2<TX_32X32, DCT_DCT  >;
        }
        // quantization [txSize]
        quant_fptr_arr[TX_4X4  ] = quant_avx2<TX_4X4>;
        quant_fptr_arr[TX_8X8  ] = quant_avx2<TX_8X8>;
        quant_fptr_arr[TX_16X16] = quant_avx2<TX_16X16>;
        quant_fptr_arr[TX_32X32] = quant_avx2<TX_32X32>;
        dequant_fptr_arr[TX_4X4  ] = dequant_avx2<TX_4X4>;
        dequant_fptr_arr[TX_8X8  ] = dequant_avx2<TX_8X8>;
        dequant_fptr_arr[TX_16X16] = dequant_avx2<TX_16X16>;
        dequant_fptr_arr[TX_32X32] = dequant_avx2<TX_32X32>;
        quant_dequant_fptr_arr[TX_4X4  ] = quant_dequant_avx2<TX_4X4>;
        quant_dequant_fptr_arr[TX_8X8  ] = quant_dequant_avx2<TX_8X8>;
        quant_dequant_fptr_arr[TX_16X16] = quant_dequant_avx2<TX_16X16>;
        quant_dequant_fptr_arr[TX_32X32] = quant_dequant_avx2<TX_32X32>;
        if (isAV1) {
            // luma inter prediction [log2(width/4)] [dx!=0] [dy!=0]
            interp_av1_single_ref_fptr_arr = interp_av1_single_ref_fptr_arr_avx2;
            interp_av1_first_ref_fptr_arr  = interp_av1_first_ref_fptr_arr_avx2;
            interp_av1_second_ref_fptr_arr = interp_av1_second_ref_fptr_arr_avx2;
            // luma inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0]
            interp_pitch64_av1_single_ref_fptr_arr = interp_pitch64_av1_single_ref_fptr_arr_avx2;
            interp_pitch64_av1_first_ref_fptr_arr  = interp_pitch64_av1_first_ref_fptr_arr_avx2;
            interp_pitch64_av1_second_ref_fptr_arr = interp_pitch64_av1_second_ref_fptr_arr_avx2;
            // nv12 inter prediction [log2(width/4)] [dx!=0] [dy!=0]
            interp_nv12_av1_single_ref_fptr_arr = interp_nv12_av1_single_ref_fptr_arr_avx2;
            interp_nv12_av1_first_ref_fptr_arr  = interp_nv12_av1_first_ref_fptr_arr_avx2;
            interp_nv12_av1_second_ref_fptr_arr = interp_nv12_av1_second_ref_fptr_arr_avx2;
            // nv12 inter prediction with pitchDst=64 [log2(width/4)] [dx!=0] [dy!=0]
            interp_nv12_pitch64_av1_single_ref_fptr_arr = interp_nv12_pitch64_av1_single_ref_fptr_arr_avx2;
            interp_nv12_pitch64_av1_first_ref_fptr_arr  = interp_nv12_pitch64_av1_first_ref_fptr_arr_avx2;
            interp_nv12_pitch64_av1_second_ref_fptr_arr = interp_nv12_pitch64_av1_second_ref_fptr_arr_avx2;

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
        } else {
        }
        // diff
        diff_nxn_fptr_arr[0] = diff_nxn_avx2<4>;
        diff_nxn_fptr_arr[1] = diff_nxn_avx2<8>;
        diff_nxn_fptr_arr[2] = diff_nxn_avx2<16>;
        diff_nxn_fptr_arr[3] = diff_nxn_avx2<32>;
        diff_nxn_fptr_arr[4] = diff_nxn_avx2<64>;
        diff_nxn_p64_p64_pw_fptr_arr[0] = diff_nxn_p64_p64_pw_avx2<4>;
        diff_nxn_p64_p64_pw_fptr_arr[1] = diff_nxn_p64_p64_pw_avx2<8>;
        diff_nxn_p64_p64_pw_fptr_arr[2] = diff_nxn_p64_p64_pw_avx2<16>;
        diff_nxn_p64_p64_pw_fptr_arr[3] = diff_nxn_p64_p64_pw_avx2<32>;
        diff_nxn_p64_p64_pw_fptr_arr[4] = diff_nxn_p64_p64_pw_avx2<64>;
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
        diff_nv12_p64_p64_pw_fptr_arr[0] = diff_nv12_p64_p64_pw_avx2<4>;
        diff_nv12_p64_p64_pw_fptr_arr[1] = diff_nv12_p64_p64_pw_avx2<8>;
        diff_nv12_p64_p64_pw_fptr_arr[2] = diff_nv12_p64_p64_pw_avx2<16>;
        diff_nv12_p64_p64_pw_fptr_arr[3] = diff_nv12_p64_p64_pw_avx2<32>;
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
        sse_flexh_fptr_arr[0] = sse_avx2<4>;
        sse_flexh_fptr_arr[1] = sse_avx2<8>;
        sse_flexh_fptr_arr[2] = sse_avx2<16>;
        sse_flexh_fptr_arr[3] = sse_avx2<32>;
        sse_flexh_fptr_arr[4] = sse_avx2<64>;
        sse_cont_fptr/*_arr[4]*/ = sse_cont_avx2;
        ssz_cont_fptr/*_arr[4]*/ = ssz_cont_avx2;
    }

} // VP9PP
