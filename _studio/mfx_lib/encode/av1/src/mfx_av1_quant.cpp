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

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_defs.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_dispatcher.h"
#include "mfx_av1_scan.h"

#include <algorithm>

namespace AV1Enc {

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

    static const int16_t dc_qlookup_10[QINDEX_RANGE] = {
  4,    9,    10,   13,   15,   17,   20,   22,   25,   28,   31,   34,   37,
  40,   43,   47,   50,   53,   57,   60,   64,   68,   71,   75,   78,   82,
  86,   90,   93,   97,   101,  105,  109,  113,  116,  120,  124,  128,  132,
  136,  140,  143,  147,  151,  155,  159,  163,  166,  170,  174,  178,  182,
  185,  189,  193,  197,  200,  204,  208,  212,  215,  219,  223,  226,  230,
  233,  237,  241,  244,  248,  251,  255,  259,  262,  266,  269,  273,  276,
  280,  283,  287,  290,  293,  297,  300,  304,  307,  310,  314,  317,  321,
  324,  327,  331,  334,  337,  343,  350,  356,  362,  369,  375,  381,  387,
  394,  400,  406,  412,  418,  424,  430,  436,  442,  448,  454,  460,  466,
  472,  478,  484,  490,  499,  507,  516,  525,  533,  542,  550,  559,  567,
  576,  584,  592,  601,  609,  617,  625,  634,  644,  655,  666,  676,  687,
  698,  708,  718,  729,  739,  749,  759,  770,  782,  795,  807,  819,  831,
  844,  856,  868,  880,  891,  906,  920,  933,  947,  961,  975,  988,  1001,
  1015, 1030, 1045, 1061, 1076, 1090, 1105, 1120, 1137, 1153, 1170, 1186, 1202,
  1218, 1236, 1253, 1271, 1288, 1306, 1323, 1342, 1361, 1379, 1398, 1416, 1436,
  1456, 1476, 1496, 1516, 1537, 1559, 1580, 1601, 1624, 1647, 1670, 1692, 1717,
  1741, 1766, 1791, 1817, 1844, 1871, 1900, 1929, 1958, 1990, 2021, 2054, 2088,
  2123, 2159, 2197, 2236, 2276, 2319, 2363, 2410, 2458, 2508, 2561, 2616, 2675,
  2737, 2802, 2871, 2944, 3020, 3102, 3188, 3280, 3375, 3478, 3586, 3702, 3823,
  3953, 4089, 4236, 4394, 4559, 4737, 4929, 5130, 5347,
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

    static const int16_t ac_qlookup_10[QINDEX_RANGE] = {
  4,    9,    11,   13,   16,   18,   21,   24,   27,   30,   33,   37,   40,
  44,   48,   51,   55,   59,   63,   67,   71,   75,   79,   83,   88,   92,
  96,   100,  105,  109,  114,  118,  122,  127,  131,  136,  140,  145,  149,
  154,  158,  163,  168,  172,  177,  181,  186,  190,  195,  199,  204,  208,
  213,  217,  222,  226,  231,  235,  240,  244,  249,  253,  258,  262,  267,
  271,  275,  280,  284,  289,  293,  297,  302,  306,  311,  315,  319,  324,
  328,  332,  337,  341,  345,  349,  354,  358,  362,  367,  371,  375,  379,
  384,  388,  392,  396,  401,  409,  417,  425,  433,  441,  449,  458,  466,
  474,  482,  490,  498,  506,  514,  523,  531,  539,  547,  555,  563,  571,
  579,  588,  596,  604,  616,  628,  640,  652,  664,  676,  688,  700,  713,
  725,  737,  749,  761,  773,  785,  797,  809,  825,  841,  857,  873,  889,
  905,  922,  938,  954,  970,  986,  1002, 1018, 1038, 1058, 1078, 1098, 1118,
  1138, 1158, 1178, 1198, 1218, 1242, 1266, 1290, 1314, 1338, 1362, 1386, 1411,
  1435, 1463, 1491, 1519, 1547, 1575, 1603, 1631, 1663, 1695, 1727, 1759, 1791,
  1823, 1859, 1895, 1931, 1967, 2003, 2039, 2079, 2119, 2159, 2199, 2239, 2283,
  2327, 2371, 2415, 2459, 2507, 2555, 2603, 2651, 2703, 2755, 2807, 2859, 2915,
  2971, 3027, 3083, 3143, 3203, 3263, 3327, 3391, 3455, 3523, 3591, 3659, 3731,
  3803, 3876, 3952, 4028, 4104, 4184, 4264, 4348, 4432, 4516, 4604, 4692, 4784,
  4876, 4972, 5068, 5168, 5268, 5372, 5476, 5584, 5692, 5804, 5916, 6032, 6148,
  6268, 6388, 6512, 6640, 6768, 6900, 7036, 7172, 7312,
    };

    int16_t vp9_dc_quant(int qindex, int delta, int32_t bitDepth) {
//#if CONFIG_AV1_HIGHBITDEPTH
        switch (bitDepth) {
        case 8: return dc_qlookup[Saturate(0, MAXQ, qindex + delta)];
        case 10: return dc_qlookup_10[Saturate(0, MAXQ, qindex + delta)];
        //case VPX_BITS_12: return dc_qlookup_12[clamp(qindex + delta, 0, MAXQ)];
        default:
            assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
            return -1;
        }
//#else
//        (void)bitDepth;
//        return dc_qlookup[Saturate(0, MAXQ, qindex + delta)];
//#endif
    }

    int32_t vp9_dc_qindex(int qdc) {
        int32_t qindex;
        for (qindex =0; qindex < MAXQ; qindex++) {
            if (dc_qlookup[qindex] > qdc) break;
        }
        return qindex;
    }

    int32_t vp9_ac_qindex(int qac) {
        int32_t qindex;
        for (qindex = 0; qindex < MAXQ; qindex++) {
            if (ac_qlookup[qindex] > qac) break;
        }
        return qindex;
    }

    int16_t vp9_ac_quant(int qindex, int delta, int32_t bitDepth) {
//#if CONFIG_AV1_HIGHBITDEPTH
        switch (bitDepth) {
        case 8: return ac_qlookup[Saturate(0, MAXQ, qindex + delta)];
        case 10: return ac_qlookup_10[Saturate(0, MAXQ, qindex + delta)];
        //case VPX_BITS_12: return ac_qlookup_12[clamp(qindex + delta, 0, MAXQ)];
        default:
            assert(0 && "bit_depth should be VPX_BITS_8, VPX_BITS_10 or VPX_BITS_12");
            return -1;
        }
//#else
//        (void)bitDepth;
//        return ac_qlookup[Saturate(0, MAXQ, qindex + delta)];
//#endif
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

    //template class AV1CU<uint8_t>;
    //template class AV1CU<uint16_t>;

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

    void InitQuantizer(AV1VideoParam &par)
    {
        int32_t ybd = par.bitDepthLuma;
        int32_t uvbd = par.bitDepthChroma;
        for (int32_t q = 0; q < QINDEX_RANGE; q++) {
            const int16_t yquant[2] = { vp9_dc_quant(q, 0, ybd), vp9_ac_quant(q, 0, ybd) };
            const int16_t uvquant[2] = { vp9_dc_quant(q, 0, uvbd), vp9_ac_quant(q, 0, uvbd) };

#ifdef ADAPTIVE_DEADZONE
            const int32_t qzbinFactor = 64;
            const int32_t qzbinFactorUv = 64;
#else
            const int32_t qzbinFactor = (q == 0) ? 64 : (yquant[0] < 148 ? 84 : 80);
            const int32_t qzbinFactorUv = (q == 0) ? 64 : (yquant[0] < 148 ? 84 : 80);
#endif
            const int32_t qroundingFactor = (q == 0) ? 64 : 48;
            const int32_t qroundingFactorFp[2] = { (q == 0) ? 64 : 48, (q == 0) ? 64 : 42 };

            for (int32_t i = 0; i < 2; i++) {
                InvertQuant(&par.qparamY[q].quant[i], &par.qparamY[q].quantShift[i], yquant[i]);
                par.qparamY[q].quantFp[i] = int16_t((1 << 16) / yquant[i]);
                par.qparamY[q].roundFp[i] = int16_t((qroundingFactorFp[i] * yquant[i]) >> 7);
                par.qparamY[q].zbin[i] = int16_t((qzbinFactor * yquant[i] + 64) >> 7);
                par.qparamY[q].round[i] = int16_t((qroundingFactor * yquant[i]) >> 7);
                par.qparamY[q].dequant[i] = yquant[i];

                InvertQuant(&par.qparamUv[q].quant[i], &par.qparamUv[q].quantShift[i], uvquant[i]);
                par.qparamUv[q].quantFp[i] = int16_t((1 << 16) / uvquant[i]);
                par.qparamUv[q].roundFp[i] = int16_t((qroundingFactorFp[i] * uvquant[i]) >> 7);
                par.qparamUv[q].zbin[i] = int16_t((qzbinFactorUv * uvquant[i] + 64) >> 7);
                par.qparamUv[q].round[i] = int16_t((qroundingFactor * uvquant[i]) >> 7);
                par.qparamUv[q].dequant[i] = uvquant[i];
            }
        }
    }

    void InitQuantizer10bit(AV1VideoParam &par)
    {
        int32_t ybd = 10;// par.bitDepthLuma;
        int32_t uvbd = 10;// par.bitDepthChroma;
        for (int32_t q = 0; q < QINDEX_RANGE; q++) {
            const int16_t yquant[2] = { vp9_dc_quant(q, 0, ybd), vp9_ac_quant(q, 0, ybd) };
            const int16_t uvquant[2] = { vp9_dc_quant(q, 0, uvbd), vp9_ac_quant(q, 0, uvbd) };

#ifdef ADAPTIVE_DEADZONE
            const int32_t qzbinFactor = 64;
            const int32_t qzbinFactorUv = 64;
#else
            const int32_t qzbinFactor = (q == 0) ? 64 : (yquant[0] < 148 ? 84 : 80);
            const int32_t qzbinFactorUv = (q == 0) ? 64 : (yquant[0] < 148 ? 84 : 80);
#endif
            const int32_t qroundingFactor = (q == 0) ? 64 : 48;
            const int32_t qroundingFactorFp[2] = { (q == 0) ? 64 : 48, (q == 0) ? 64 : 42 };

            for (int32_t i = 0; i < 2; i++) {
                InvertQuant(&par.qparamY10[q].quant[i], &par.qparamY10[q].quantShift[i], yquant[i]);
                par.qparamY10[q].quantFp[i] = int16_t((1 << 16) / yquant[i]);
                par.qparamY10[q].roundFp[i] = int16_t((qroundingFactorFp[i] * yquant[i]) >> 7);
                par.qparamY10[q].zbin[i] = int16_t((qzbinFactor * yquant[i] + 64) >> 7);
                par.qparamY10[q].round[i] = int16_t((qroundingFactor * yquant[i]) >> 7);
                par.qparamY10[q].dequant[i] = yquant[i];

                InvertQuant(&par.qparamUv10[q].quant[i], &par.qparamUv10[q].quantShift[i], uvquant[i]);
                par.qparamUv10[q].quantFp[i] = int16_t((1 << 16) / uvquant[i]);
                par.qparamUv10[q].roundFp[i] = int16_t((qroundingFactorFp[i] * uvquant[i]) >> 7);
                par.qparamUv10[q].zbin[i] = int16_t((qzbinFactorUv * uvquant[i] + 64) >> 7);
                par.qparamUv10[q].round[i] = int16_t((qroundingFactor * uvquant[i]) >> 7);
                par.qparamUv10[q].dequant[i] = uvquant[i];
            }
        }
    }

#ifdef ADAPTIVE_DEADZONE
    static inline int clamp(int value, int low, int high) {
        return value < low ? low : (value > high ? high : value);
    }

//#define USE_INTEGER_ADAPTATION

    void adaptDzQuant(const int16_t *coeff_ptr, const int16_t fqpar[10], int32_t txSize, float *roundFAdj)
    {
        const int16_t *zbin_ptr = fqpar;
        const int16_t *round_ptr = fqpar + 2;
        const int16_t *quant_ptr = fqpar + 4;
        const int16_t *quant_shift_ptr = fqpar + 6;
        const int16_t *scales = fqpar + 8;

        const int n_coeffs = 16 << (txSize << 1);

        int16_t zbin[2] = { zbin_ptr[0], zbin_ptr[1] };
        int16_t round[2] = { round_ptr[0], round_ptr[1] };
        int16_t quant_shift[2] = { quant_shift_ptr[0], quant_shift_ptr[1] };
        int shift = 0;
        if (txSize == 3) {
            zbin[0] = (zbin[0] + 1) >> 1;
            zbin[1] = (zbin[1] + 1) >> 1;
            round[0] = (round[0] + 1) >> 1;
            round[1] = (round[1] + 1) >> 1;
            quant_shift[0] = quant_shift[0] << 1;
            quant_shift[1] = quant_shift[1] << 1;
            shift = 1;
        }

        int coeff = coeff_ptr[0];
        int coeff_sign = (coeff >> 31);
        int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

        if (abs_coeff >= zbin[0]) {
            int tmp = clamp(abs_coeff + round[0], INT16_MIN, INT16_MAX);
            tmp = ((((tmp * quant_ptr[0]) >> 16) + tmp) * quant_shift[0]) >> 16;  // quantization
            int rec = (tmp * scales[0]) >> shift;
            if (rec) {
#ifdef USE_INTEGER_ADAPTATION
                int diff = abs_coeff - rec;
                int diff_sign = (diff >> 31);

                diff = (diff ^ diff_sign) - diff_sign;
                diff <<= 2;

                diff = ((((diff * quant_ptr[0]) >> 16) + diff) * quant_shift_ptr[0]) >> 16;  // quantization
                diff = (diff ^ diff_sign) - diff_sign;

                roundFAdj[0] += 0.004f*(float)(diff);
#else
                float diff = float(abs_coeff - rec);
                roundFAdj[0] += 0.016f*(diff / (float)scales[0]);
#endif
            }
        }

        for (int i = 1; i < n_coeffs; i++) {
            coeff = coeff_ptr[i];
            coeff_sign = (coeff >> 31);
            abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

            if (abs_coeff >= zbin[1]) {
                int tmp = clamp(abs_coeff + round[1], INT16_MIN, INT16_MAX);
                tmp = ((((tmp * quant_ptr[1]) >> 16) + tmp) * quant_shift[1]) >> 16;  // quantization
                int rec = (tmp * scales[1]) >> shift;
                if (rec) {
#ifdef USE_INTEGER_ADAPTATION
                    int diff = (abs_coeff - rec);
                    int diff_sign = (diff >> 31);

                    diff = (diff ^ diff_sign) - diff_sign;
                    diff <<= 2;

                    diff = ((((diff * quant_ptr[1]) >> 16) + diff) * quant_shift_ptr[1]) >> 16;  // quantization
                    diff = (diff ^ diff_sign) - diff_sign;

                    roundFAdj[1] += 0.001f*(float)(diff);
#else
                    float diff = float(abs_coeff - rec);
                    roundFAdj[1] += 0.004f*(diff / (float)scales[1]);
#endif
                }
            }
        }
    }

    template<typename TCoeffType> void adaptDz(const TCoeffType *coeff_ptr, const TCoeffType *reccoeff_ptr, const QuantParam &qpar, int32_t txSize, float *roundFAdj, int32_t eob)
    {
        const int16_t *scales = qpar.dequant;

        const int n_coeffs = 16 << (txSize << 1);

        int32_t nnz = (eob <= 0) ? n_coeffs : eob;
        // call with eob equals 0 if num non zero unknown
        // do not call this function when eob is 0
        {
            int rec = reccoeff_ptr[0];
            if (rec) {
                int coeff = coeff_ptr[0];
                int coeff_sign = (coeff >> 31);
                int abs_coeff = (coeff ^ coeff_sign);
                rec = (rec ^ coeff_sign);
                float diff = float(abs_coeff - rec);
                roundFAdj[0] += 0.016f*(diff / (float)scales[0]);
                if (--nnz == 0) return;
            }
        }
        // Fast convergence with fewer coeffs
        nnz = std::min(nnz, 6);
        const float inv_scale = 0.016f / (float)scales[1];
        const int16_t *scan = av1_default_scan[txSize];
        for (int i = 1; i < n_coeffs; i++) {
            int rec = reccoeff_ptr[scan[i]];
            if (rec) {
                int coeff = coeff_ptr[scan[i]];
                int coeff_sign = (coeff >> 31);
                int abs_coeff = (coeff ^ coeff_sign);

                rec = (rec ^ coeff_sign);
                float diff = float(abs_coeff - rec);
                roundFAdj[1] += diff * inv_scale;
                if(--nnz == 0) return;
            }
        }
    }

    template void adaptDz(const short *coeff_ptr, const short *reccoeff_ptr, const QuantParam &qpar, int32_t txSize, float *roundFAdj, int32_t eob);
    template void adaptDz(const int *coeff_ptr, const int *reccoeff_ptr, const QuantParam &qpar, int32_t txSize, float *roundFAdj, int32_t eob);

#endif
} // namespace AV1Enc

#endif  // MFX_ENABLE_AV1_VIDEO_ENCODE
