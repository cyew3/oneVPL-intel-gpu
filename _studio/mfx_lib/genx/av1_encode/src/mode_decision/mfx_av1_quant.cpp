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
#include "mfx_av1_tables.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_dispatcher.h"

namespace H265Enc {

    static const int16_t dc_qlookup[QINDEX_RANGE] = {
        4,    8,    8,    9,    10,  11,  12,  12,  13,  14,  15,   16,   17,   18,
        19,   19,   20,   21,   22,  23,  24,  25,  26,  26,  27,   28,   29,   30,
        31,   32,   32,   33,   34,  35,  36,  37,  38,  38,  39,   40,   41,   42,
        43,   43,   44,   45,   46,  47,  48,  48,  49,  50,  51,   52,   53,   53,
        54,   55,   56,   57,   57,  58,  59,  60,  61,  62,  62,   63,   64,   65,
        66,   66,   67,   68,   69,  70,  70,  71,  72,  73,  74,   74,   75,   76,
        77,   78,   78,   79,   80,  81,  81,  82,  83,  84,  85,   85,   87,   88,
        90,   92,   93,   95,   96,  98,  99,  101, 102, 104, 105,  107,  108,  110,
        111,  113,  114,  116,  117, 118, 120, 121, 123, 125, 127,  129,  131,  134,
        136,  138,  140,  142,  144, 146, 148, 150, 152, 154, 156,  158,  161,  164,
        166,  169,  172,  174,  177, 180, 182, 185, 187, 190, 192,  195,  199,  202,
        205,  208,  211,  214,  217, 220, 223, 226, 230, 233, 237,  240,  243,  247,
        250,  253,  257,  261,  265, 269, 272, 276, 280, 284, 288,  292,  296,  300,
        304,  309,  313,  317,  322, 326, 330, 335, 340, 344, 349,  354,  359,  364,
        369,  374,  379,  384,  389, 395, 400, 406, 411, 417, 423,  429,  435,  441,
        447,  454,  461,  467,  475, 482, 489, 497, 505, 513, 522,  530,  539,  549,
        559,  569,  579,  590,  602, 614, 626, 640, 654, 668, 684,  700,  717,  736,
        755,  775,  796,  819,  843, 869, 896, 925, 955, 988, 1022, 1058, 1098, 1139,
        1184, 1232, 1282, 1336,
    };

    static const int16_t ac_qlookup[QINDEX_RANGE] = {
        4,    8,    9,    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
        20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
        33,   34,   35,   36,   37,   38,   39,   40,   41,   42,   43,   44,   45,
        46,   47,   48,   49,   50,   51,   52,   53,   54,   55,   56,   57,   58,
        59,   60,   61,   62,   63,   64,   65,   66,   67,   68,   69,   70,   71,
        72,   73,   74,   75,   76,   77,   78,   79,   80,   81,   82,   83,   84,
        85,   86,   87,   88,   89,   90,   91,   92,   93,   94,   95,   96,   97,
        98,   99,   100,  101,  102,  104,  106,  108,  110,  112,  114,  116,  118,
        120,  122,  124,  126,  128,  130,  132,  134,  136,  138,  140,  142,  144,
        146,  148,  150,  152,  155,  158,  161,  164,  167,  170,  173,  176,  179,
        182,  185,  188,  191,  194,  197,  200,  203,  207,  211,  215,  219,  223,
        227,  231,  235,  239,  243,  247,  251,  255,  260,  265,  270,  275,  280,
        285,  290,  295,  300,  305,  311,  317,  323,  329,  335,  341,  347,  353,
        359,  366,  373,  380,  387,  394,  401,  408,  416,  424,  432,  440,  448,
        456,  465,  474,  483,  492,  501,  510,  520,  530,  540,  550,  560,  571,
        582,  593,  604,  615,  627,  639,  651,  663,  676,  689,  702,  715,  729,
        743,  757,  771,  786,  801,  816,  832,  848,  864,  881,  898,  915,  933,
        951,  969,  988,  1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151, 1173, 1196,
        1219, 1243, 1267, 1292, 1317, 1343, 1369, 1396, 1423, 1451, 1479, 1508, 1537,
        1567, 1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
    };

    int16_t vp9_dc_quant(int qindex, int delta, int32_t bitDepth) {
#if CONFIG_VP9_HIGHBITDEPTH
        switch (bit_depth) {
        case VPX_BITS_8: return dc_qlookup[clamp(qindex + delta, 0, MAXQ)];
        case VPX_BITS_10: return dc_qlookup_10[clamp(qindex + delta, 0, MAXQ)];
        case VPX_BITS_12: return dc_qlookup_12[clamp(qindex + delta, 0, MAXQ)];
        default:
            assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
            return -1;
        }
#else
        (void)bitDepth;
        return dc_qlookup[Saturate(0, MAXQ, qindex + delta)];
#endif
    }

    int16_t vp9_ac_quant(int qindex, int delta, int32_t bitDepth) {
#if CONFIG_VP9_HIGHBITDEPTH
        switch (bit_depth) {
        case VPX_BITS_8: return ac_qlookup[clamp(qindex + delta, 0, MAXQ)];
        case VPX_BITS_10: return ac_qlookup_10[clamp(qindex + delta, 0, MAXQ)];
        case VPX_BITS_12: return ac_qlookup_12[clamp(qindex + delta, 0, MAXQ)];
        default:
            assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
            return -1;
        }
#else
        (void)bitDepth;
        return ac_qlookup[Saturate(0, MAXQ, qindex + delta)];
#endif
    }

    template <class T>
    bool IsZero(const T *arr, int32_t size) {
        assert(reinterpret_cast<uint64_t>(arr) % sizeof(uint64_t) == 0);
        assert(size * sizeof(T) % sizeof(uint64_t) == 0);

        const uint64_t *arr64 = reinterpret_cast<const uint64_t *>(arr);
        const uint64_t *arr64end = reinterpret_cast<const uint64_t *>(arr + size);

        for (; arr64 != arr64end; arr64++)
            if (*arr64)
                return false;
        return true;
    }

    template class H265CU<uint8_t>;
    template class H265CU<uint16_t>;

    namespace {
        void InvertQuant(int16_t *quant, int16_t *shift, int d) {
            int32_t t = d;
            int32_t l;
            for (l = 0; t > 1; l++)
                t >>= 1;
            int32_t m = 1 + (1 << (16 + l)) / d;
            *quant = (int16_t)(m - (1 << 16));
            *shift = 1 << (16 - l);
        }
    } // namespace

    void InitQuantizer(H265VideoParam &par)
    {
        int32_t ybd = par.bitDepthLuma;
        int32_t uvbd = par.bitDepthChroma;
        for (int32_t q = 0; q < QINDEX_RANGE; q++) {
            const int32_t yquant[2] = { vp9_dc_quant(q, 0, ybd), vp9_ac_quant(q, 0, ybd) };
            const int32_t uvquant[2] = { vp9_dc_quant(q, 0, uvbd), vp9_ac_quant(q, 0, uvbd) };
            const int32_t qzbinFactor = (q == 0) ? 64 : (yquant[0] < 148 ? 84 : 80);
            const int32_t qroundingFactor = (q == 0) ? 64 : 48;
            const int32_t qroundingFactorFp[2] = { (q == 0) ? 64 : 48, (q == 0) ? 64 : 42 };

            for (int32_t i = 0; i < 2; i++) {
                InvertQuant(&par.qparamY[q].quant[i], &par.qparamY[q].quantShift[i], yquant[i]);
                //par.qparamY[q].quantFp[i] = (1 << 16) / yquant[i];
                //par.qparamY[q].roundFp[i] = (qroundingFactorFp[i] * yquant[i]) >> 7;
                par.qparamY[q].zbin[i] = (qzbinFactor * yquant[i] + 64) >> 7;
                par.qparamY[q].round[i] = (qroundingFactor * yquant[i]) >> 7;
                par.qparamY[q].dequant[i] = yquant[i];

                InvertQuant(&par.qparamUv[q].quant[i], &par.qparamUv[q].quantShift[i], uvquant[i]);
                //par.qparamUv[q].quantFp[i] = (1 << 16) / uvquant[i];
                //par.qparamUv[q].roundFp[i] = (qroundingFactorFp[i] * uvquant[i]) >> 7;
                par.qparamUv[q].zbin[i] = (qzbinFactor * uvquant[i] + 64) >> 7;
                par.qparamUv[q].round[i] = (qroundingFactor * uvquant[i]) >> 7;
                par.qparamUv[q].dequant[i] = uvquant[i];
            }
        }
    }
} // namespace H265Enc
