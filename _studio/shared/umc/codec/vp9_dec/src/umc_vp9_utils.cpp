/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#include "umc_structures.h"
#include "umc_vp9_utils.h"
#include "umc_vp9_frame.h"

namespace UMC_VP9_DECODER
{
    static
    const mfxI16 dc_qlookup[QINDEX_RANGE] =
    {
        4,       8,    8,    9,   10,   11,   12,   12,
        13,     14,   15,   16,   17,   18,   19,   19,
        20,     21,   22,   23,   24,   25,   26,   26,
        27,     28,   29,   30,   31,   32,   32,   33,
        34,     35,   36,   37,   38,   38,   39,   40,
        41,     42,   43,   43,   44,   45,   46,   47,
        48,     48,   49,   50,   51,   52,   53,   53,
        54,     55,   56,   57,   57,   58,   59,   60,
        61,     62,   62,   63,   64,   65,   66,   66,
        67,     68,   69,   70,   70,   71,   72,   73,
        74,     74,   75,   76,   77,   78,   78,   79,
        80,     81,   81,   82,   83,   84,   85,   85,
        87,     88,   90,   92,   93,   95,   96,   98,
        99,    101,  102,  104,  105,  107,  108,  110,
        111,   113,  114,  116,  117,  118,  120,  121,
        123,   125,  127,  129,  131,  134,  136,  138,
        140,   142,  144,  146,  148,  150,  152,  154,
        156,   158,  161,  164,  166,  169,  172,  174,
        177,   180,  182,  185,  187,  190,  192,  195,
        199,   202,  205,  208,  211,  214,  217,  220,
        223,   226,  230,  233,  237,  240,  243,  247,
        250,   253,  257,  261,  265,  269,  272,  276,
        280,   284,  288,  292,  296,  300,  304,  309,
        313,   317,  322,  326,  330,  335,  340,  344,
        349,   354,  359,  364,  369,  374,  379,  384,
        389,   395,  400,  406,  411,  417,  423,  429,
        435,   441,  447,  454,  461,  467,  475,  482,
        489,   497,  505,  513,  522,  530,  539,  549,
        559,   569,  579,  590,  602,  614,  626,  640,
        654,   668,  684,  700,  717,  736,  755,  775,
        796,   819,  843,  869,  896,  925,  955,  988,
        1022, 1058, 1098, 1139, 1184, 1232, 1282, 1336,
    };

    static
    const mfxI16 ac_qlookup[QINDEX_RANGE] =
    {
        4,       8,    9,   10,   11,   12,   13,   14,
        15,     16,   17,   18,   19,   20,   21,   22,
        23,     24,   25,   26,   27,   28,   29,   30,
        31,     32,   33,   34,   35,   36,   37,   38,
        39,     40,   41,   42,   43,   44,   45,   46,
        47,     48,   49,   50,   51,   52,   53,   54,
        55,     56,   57,   58,   59,   60,   61,   62,
        63,     64,   65,   66,   67,   68,   69,   70,
        71,     72,   73,   74,   75,   76,   77,   78,
        79,     80,   81,   82,   83,   84,   85,   86,
        87,     88,   89,   90,   91,   92,   93,   94,
        95,     96,   97,   98,   99,  100,  101,  102,
        104,   106,  108,  110,  112,  114,  116,  118,
        120,   122,  124,  126,  128,  130,  132,  134,
        136,   138,  140,  142,  144,  146,  148,  150,
        152,   155,  158,  161,  164,  167,  170,  173,
        176,   179,  182,  185,  188,  191,  194,  197,
        200,   203,  207,  211,  215,  219,  223,  227,
        231,   235,  239,  243,  247,  251,  255,  260,
        265,   270,  275,  280,  285,  290,  295,  300,
        305,   311,  317,  323,  329,  335,  341,  347,
        353,   359,  366,  373,  380,  387,  394,  401,
        408,   416,  424,  432,  440,  448,  456,  465,
        474,   483,  492,  501,  510,  520,  530,  540,
        550,   560,  571,  582,  593,  604,  615,  627,
        639,   651,  663,  676,  689,  702,  715,  729,
        743,   757,  771,  786,  801,  816,  832,  848,
        864,   881,  898,  915,  933,  951,  969,  988,
        1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151,
        1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
        1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567,
        1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
    };

    inline
    Ipp32s clamp(Ipp32s value, Ipp32s low, Ipp32s high)
    {
        return
            value < low ? low : (value > high ? high : value);
    }

    inline
    Ipp16s vp9_dc_quant(Ipp32s qindex, Ipp32s delta)
    {
        return
            dc_qlookup[clamp(qindex + delta, 0, MAXQ)];
    }

    inline
    Ipp16s vp9_ac_quant(Ipp32s qindex, Ipp32s delta)
    {
        return
            ac_qlookup[clamp(qindex + delta, 0, MAXQ)];
    }

    Ipp32s GetQIndex(VP9Segmentation const & seg, Ipp8u segmentId, Ipp32s baseQIndex)
    {
        if (!IsSegFeatureActive(seg, segmentId, SEG_LVL_ALT_Q))
            return baseQIndex;
        else
        {
            const Ipp32s data = GetSegData(seg, segmentId, UMC_VP9_DECODER::SEG_LVL_ALT_Q);
            return
                 seg.absDelta == SEGMENT_ABSDATA ?
                 data :  // Abs value
                 clamp(baseQIndex + data, 0, MAXQ);  // Delta value
        }
    }

    void InitDequantizer(VP9DecoderFrame* info)
    {
        for (mfxU16 q = 0; q < UMC_VP9_DECODER::QINDEX_RANGE; ++q)
        {
            info->yDequant[q][0] = vp9_dc_quant(q, info->y_dc_delta_q);
            info->yDequant[q][1] = vp9_ac_quant(q, 0);

            info->uvDequant[q][0] = vp9_dc_quant(q, info->uv_dc_delta_q);
            info->uvDequant[q][1] = vp9_ac_quant(q, info->uv_ac_delta_q);
        }
    }

    UMC::ColorFormat GetUMCColorFormat_VP9(VP9DecoderFrame const* info)
    {
        if (info->color_space == SRGB)
            //only 4:4:4 chroma sampling is allowed for SRGB
            return UMC::RGB24;

#if 0
        switch (info->subsamplingX * info->subsamplingY)
        {
            case 0:  return info->subsamplingX ? UMC::YUV422 : UMC::YUV444;
            case 1:  return UMC::YUV420;
            default: return UMC::NONE;
        }
#else
        if (!info->subsamplingX)
        {
            VM_ASSERT(info->subsamplingY == 0);

            //4:4:4
            switch (info->bit_depth)
            {
                case  8: return UMC::YUV444;
                case 10: return UMC::Y410;
                case 12: return UMC::Y416;
                default: return UMC::NONE;
            }
        }
        else if (info->subsamplingY)
        {
            //4:2:0
            switch (info->bit_depth)
            {
                case  8: return UMC::YUV420;
                case 10: return UMC::P010;
                case 12: return UMC::P016;
                default: return UMC::NONE;
            }
        }
        else
        {
            //4:2:2
            switch (info->bit_depth)
            {
                case  8: return UMC::YUV422;
                case 10: return UMC::P210;
                case 12: return UMC::P216;
                default: return UMC::NONE;
            }
        }
#endif

    }
} //UMC_VP9_DECODER

#endif //__UMC_VP9_UTILS_H_
