/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

#include "mfx_vp9_dec_decode_utils.h"

namespace MfxVP9Decode
{
    void GetTileNBits(const mfxI32 miCols, mfxI32 & minLog2TileCols, mfxI32 & maxLog2TileCols)
    {
        const mfxI32 sbCols = ALIGN_POWER_OF_TWO(miCols, MI_BLOCK_SIZE_LOG2) >> MI_BLOCK_SIZE_LOG2;
        mfxI32 minLog2 = 0, maxLog2 = 0;

        while ((sbCols >> maxLog2) >= MIN_TILE_WIDTH_B64)
            ++maxLog2;
        --maxLog2;
        if (maxLog2 < 0)
            maxLog2 = 0;

        while ((MAX_TILE_WIDTH_B64 << minLog2) < sbCols)
            ++minLog2;

        assert(minLog2 <= maxLog2);

        minLog2TileCols = minLog2;
        maxLog2TileCols = maxLog2;
    }

    void GetFrameSize(InputBitstream *pBs, VP9FrameInfo & info)
    {
        info.width = pBs->GetBits(16) + 1;
        info.height = pBs->GetBits(16) + 1;

        GetDisplaySize(pBs, info);
    }

    void GetFrameSizeWithRefs(InputBitstream *pBs, VP9FrameInfo & info)
    {
        bool bFound = false;
        for (mfxU8 i = 0; i < REFS_PER_FRAME; ++i)
        {
            if (pBs->GetBit())
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            info.width = pBs->GetBits(16) + 1;
            info.height = pBs->GetBits(16) + 1;
        }

        GetDisplaySize(pBs, info);
    }

    void GetDisplaySize(InputBitstream *pBs, VP9FrameInfo & info)
    {
        info.displayWidth = info.width;
        info.displayHeight = info.height;

        if (pBs->GetBit())
        {
            info.displayWidth = pBs->GetBits(16) + 1;
            info.displayHeight = pBs->GetBits(16) + 1;
        }
    }

    void SetupLoopFilter(InputBitstream *pBs, Loopfilter & loopfilter)
    {
        loopfilter.filterLevel = pBs->GetBits(6);
        loopfilter.sharpnessLevel = pBs->GetBits(3);

        loopfilter.modeRefDeltaUpdate = 0;

        mfxI8 value = 0;

        loopfilter.modeRefDeltaEnabled = (mfxU8)pBs->GetBit();
        if (loopfilter.modeRefDeltaEnabled)
        {
            loopfilter.modeRefDeltaUpdate = (mfxU8)pBs->GetBit();
            if (loopfilter.modeRefDeltaUpdate)
            {
                mfxU32 i;

                for (i = 0; i < MAX_REF_LF_DELTAS; i++)
                {
                    if (pBs->GetBit())
                    {
                        value = (mfxI8)pBs->GetBits(6);
                        loopfilter.refDeltas[i] = pBs->GetBit() ? -value : value;
                    }
                }

                for (i = 0; i < MAX_MODE_LF_DELTAS; i++)
                {
                    if (pBs->GetBit())
                    {
                        value = (mfxI8)pBs->GetBits(6);
                        loopfilter.modeDeltas[i] = pBs->GetBit() ? -value : value;
                    }
                }
            }
        }
#if 0
        mfxU32 i;
        for (i = 0; i < MAX_REF_LF_DELTAS; i++)
            printf("ref_deltas[%d] %d\n", i, loopfilter.refDeltas[i]);

        for (i = 0; i < MAX_MODE_LF_DELTAS; i++)
            printf("mode_deltas[%d] %d\n", i, loopfilter.modeDeltas[i]);
#endif
    }

    void ClearAllSegFeatures(VP9Segmentation & seg)
    {
        memset(&seg.featureData, 0, sizeof(seg.featureData));
        memset(&seg.featureMask, 0, sizeof(seg.featureMask));
    }

    bool IsSegFeatureActive(VP9Segmentation const & seg,
                                   mfxU8 segmentId,
                                   SEG_LVL_FEATURES featureId)
    {
        return seg.enabled && (seg.featureMask[segmentId] & (1 << featureId));
    }

    void EnableSegFeature(VP9Segmentation & seg,
                                 mfxU8 segmentId,
                                 SEG_LVL_FEATURES featureId)
    {
        seg.featureMask[segmentId] |= 1 << featureId;
    }

    mfxU8 IsSegFeatureSigned(SEG_LVL_FEATURES featureId)
    {
        return SEG_FEATURE_DATA_SIGNED[featureId];
    }

    void SetSegData(VP9Segmentation & seg, mfxU8 segmentId,
                           SEG_LVL_FEATURES featureId, mfxI32 seg_data)
    {
        assert(seg_data <= SEG_FEATURE_DATA_MAX[featureId]);
        if (seg_data < 0)
        {
            assert(SEG_FEATURE_DATA_SIGNED[featureId]);
            assert(-seg_data <= SEG_FEATURE_DATA_MAX[featureId]);
        }

        seg.featureData[segmentId][featureId] = seg_data;
    }

    mfxI32 GetSegData(VP9Segmentation const & seg,
                             mfxI32 segmentId,
                             SEG_LVL_FEATURES featureId)
    {
        return seg.featureData[segmentId][featureId];
    }

    static const mfxI16 dc_qlookup[QINDEX_RANGE] =
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

    static const mfxI16 ac_qlookup[QINDEX_RANGE] =
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

    mfxI16 vp9_dc_quant(mfxI32 qindex, mfxI32 delta)
    {
        return dc_qlookup[clamp(qindex + delta, 0, MAXQ)];
    }

    mfxI16 vp9_ac_quant(mfxI32 qindex, mfxI32 delta)
    {
        return ac_qlookup[clamp(qindex + delta, 0, MAXQ)];
    }

    void InitDequantizer(VP9FrameInfo & info)
    {
        for (mfxU16 q = 0; q < QINDEX_RANGE; ++q)
        {
            info.yDequant[q][0] = vp9_dc_quant(q, info.y_dc_delta_q);
            info.yDequant[q][1] = vp9_ac_quant(q, 0);

            info.uvDequant[q][0] = vp9_dc_quant(q, info.uv_dc_delta_q);
            info.uvDequant[q][1] = vp9_ac_quant(q, info.uv_ac_delta_q);
        }
    }

    mfxI32 GetQIndex(VP9Segmentation const & seg, mfxU8 segmentId, mfxI32 baseQIndex)
    {
        if (IsSegFeatureActive(seg, segmentId, SEG_LVL_ALT_Q))
        {
            const mfxI32 data = GetSegData(seg, segmentId, SEG_LVL_ALT_Q);
            return seg.absDelta == SEGMENT_ABSDATA ?
                                    data :  // Abs value
                                    clamp(baseQIndex + data, 0, MAXQ);  // Delta value
        }
        else
        {
            return baseQIndex;
        }
    }

    void SetupPastIndependence(VP9FrameInfo & info)
    {
        // Reset the segment feature data to the default stats:
        // Features disabled, 0, with delta coding (Default state).

        ClearAllSegFeatures(info.segmentation);
        info.segmentation.absDelta = SEGMENT_DELTADATA;
        
        // set_default_lf_deltas()
        info.lf.modeRefDeltaEnabled = 1;
        info.lf.modeRefDeltaUpdate = 1;

        info.lf.refDeltas[INTRA_FRAME] = 1;
        info.lf.refDeltas[LAST_FRAME] = 0;
        info.lf.refDeltas[GOLDEN_FRAME] = -1;
        info.lf.refDeltas[ALTREF_FRAME] = -1;

        info.lf.modeDeltas[0] = 0;
        info.lf.modeDeltas[1] = 0;
/*
        if (info.frameType == KEY_FRAME || info.errorResilientMode || cm->resetFrameContext == 3)
        {
            // Reset all frame contexts.
            for (mfxI32 i = 0; i < FRAME_CONTEXTS; ++i)
                cm->frame_contexts[i] = cm->fc;
            }
            else if (cm->reset_frame_context == 2)
            {
                // Reset only the frame context specified in the frame header.
                cm->frame_contexts[cm->frame_context_idx] = cm->fc;
            }
        }
*/
        memset(info.refFrameSignBias, 0, sizeof(info.refFrameSignBias));

        info.frameContextIdx = 0;
    }

InputBitstream::InputBitstream(
    mfxU8 const * buf,
    mfxU8 const * bufEnd)
: m_buf(buf)
, m_ptr(buf)
, m_bufEnd(bufEnd)
, m_bitOff(0)
{
}

mfxU32 InputBitstream::NumBitsRead() const
{
    return mfxU32(8 * (m_ptr - m_buf) + m_bitOff);
}

mfxU32 InputBitstream::NumBitsLeft() const
{
    return mfxU32(8 * (m_bufEnd - m_ptr) - m_bitOff);
}

mfxU32 InputBitstream::GetBit()
{
    if (m_ptr >= m_bufEnd)
        throw std::out_of_range("");

    mfxU32 bit = (*m_ptr >> (7 - m_bitOff)) & 1;

    if (++m_bitOff == 8)
    {
        ++m_ptr;
        m_bitOff = 0;
    }

    return bit;
}

mfxU32 InputBitstream::GetBits(mfxU32 nbits)
{
    mfxU32 bits = 0;
    for (; nbits > 0; --nbits)
    {
        bits <<= 1;
        bits |= GetBit();
    }

    return bits;
}

mfxU32 InputBitstream::GetUe()
{
    mfxU32 zeroes = 0;
    while (GetBit() == 0)
        ++zeroes;

    return zeroes == 0 ? 0 : ((1 << zeroes) | GetBits(zeroes)) - 1;
}

mfxI32 InputBitstream::GetSe()
{
    mfxU32 val = GetUe();
    mfxU32 sign = (val & 1);
    val = (val + 1) >> 1;
    return sign ? val : -mfxI32(val);
}

}; // namespace MfxVP9Decode

#endif // _MFX_VP9_DECODE_UTILS_H_
