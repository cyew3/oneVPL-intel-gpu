/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"

//#define DEBUG_STREAM "hevce_wp.265"

using namespace BS_HEVC2;

namespace hevce_weight_prediction
{
    const mfxU32 nFrames = 24;
    const char* stream;

    void CorruptSurface(mfxFrameSurface1& _s)
    {
        mfxI32 logWC_luma = 6;
        mfxI32 w_luma = 64;
        mfxI32 o_luma = 25;

        tsFrameNV12 frame(_s.Data);
        for (mfxU32 i = 0; i < _s.Info.Height; i++)
            for (mfxU32 j = 0; j < _s.Info.Width; j++)
            {
                frame.Y(j, i) = ((frame.Y(j, i) * w_luma + (1 << (logWC_luma - 1))) >> logWC_luma) - o_luma;
            }
    }

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:

#ifdef DEBUG_STREAM
        tsBitstreamWriter m_bsw;
#endif
        BitstreamChecker()
            : tsParserHEVC2(PARSE_SSD)
#ifdef DEBUG_STREAM
            , m_bsw(DEBUG_STREAM)
#endif
        {
            set_trace_level(0);
        }
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            SetBuffer0(bs);

#ifdef DEBUG_STREAM
        m_bsw.ProcessBitstream(bs, nFrames);
#endif

        while (checked++ < nFrames)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (!IsHEVCSlice(pNALU->nal_unit_type))
                    continue;

                auto& slice = *pNALU->slice;

                if (slice.type == P + 5)
                {
                    EXPECT_EQ(slice.pps->weighted_pred_flag, MFX_WEIGHTED_PRED_EXPLICIT);
                }
            }
        }

        bs.DataLength = 0;

        BreakOnFailure();

        return MFX_ERR_NONE;
    }

    struct TestCase
    {
        mfxStatus sts;
        mfxU16 WeightPredP;
    };

    class SProc : public tsSurfaceProcessor
    {
    public:
        tsSurfaceProcessor & m_reader;
        tsVideoEncoder& m_enc;
        const TestCase& m_tc;
        mfxU32 fn;

        SProc(tsSurfaceProcessor& r, const TestCase& tc, tsVideoEncoder& enc)
            : m_reader(r)
            , m_enc(enc)
            , m_tc(tc)
        {
            m_max = m_reader.m_max;
            fn = 0;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            m_reader.ProcessSurface(s);

            if ((fn > 0) && ((fn % 2) != 0))
                CorruptSurface(s);

            fn++;

            return  MFX_ERR_NONE;
        }
    };

    const TestCase test_case[] =
    {
        /*
        {
        Expected Query/Init Status;
        WeightPredP; WeightPredB;
        }
        */
        /*0000*/{ MFX_ERR_NONE, MFX_WEIGHTED_PRED_EXPLICIT },

    };

    int test(unsigned int id, tsVideoEncoder& enc, const char* stream)
    {
        auto& tc = test_case[id];
        BitstreamChecker checker;
        tsRawReader reader(stream, enc.m_par.mfx.FrameInfo, nFrames);
        SProc sproc(reader, tc, enc);

        reader.m_disable_shift_hack = true;
        enc.m_filler = &sproc;
        enc.m_bs_processor = &checker;

        mfxExtCodingOption2& CO2 = enc.m_par;
        mfxExtCodingOption3& CO3 = enc.m_par;

        CO3.FadeDetection = MFX_CODINGOPTION_ON;
        CO3.WeightedPred = tc.WeightPredP;
        CO3.GPB = MFX_CODINGOPTION_ON;
        CO3.EnableQPOffset = MFX_CODINGOPTION_ON;

        if (g_tsHWtype < MFX_HW_CNL)
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            throw tsSKIP;
        }

        enc.MFXInit();
        enc.Load();

        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);
        g_tsStatus.check(enc.GetCaps(&caps, &capSize));
        if (caps.NoWeightedPred)
        {
            g_tsLog << "WARNING: Weighted prediction is unsupported!\n";
            throw tsSKIP;
        }

        g_tsStatus.expect(tc.sts);

        enc.Query();
        enc.Init();
        enc.GetVideoParam();

        enc.EncodeFrames(nFrames);

        enc.Close();

        return 0;
    }

    int testNV12(unsigned int id)
    {
        TS_START;

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        if (g_tsConfig.sim) {
            stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");
            enc.m_par.mfx.FrameInfo.Width = enc.m_par.mfx.FrameInfo.CropW = 176;
            enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = 144;
        }
        else {
            stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_1920x1088_30_content_1920x1080.yuv");
            enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 1920;
            enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 1088;
        }
        g_tsStreamPool.Reg();

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testAYUV(unsigned int id)
    {
        TS_START;

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;

        if (g_tsConfig.sim) {
            stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_ayuv.yuv");
            enc.m_par.mfx.FrameInfo.Width = enc.m_par.mfx.FrameInfo.CropW = 176;
            enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = 144;
        }
        else {
            stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_1920x1088_24_ayuv_30_content_1920x1080.yuv");
            enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 1920;
            enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 1088;
        }
        g_tsStreamPool.Reg();

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testP010(unsigned int id)
    {
        TS_START;

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;

        if (g_tsConfig.sim) {
            stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_p010.yuv");
            enc.m_par.mfx.FrameInfo.Width = enc.m_par.mfx.FrameInfo.CropW = 176;
            enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = 144;
        }
        else {
            stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_1920x1088_24_p010_shifted_30_content_1920x1080.yuv");
            enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 1920;
            enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 1088;
        }
        g_tsStreamPool.Reg();

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testY410(unsigned int id)
    {
        TS_START;

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;

        if (g_tsConfig.sim) {
            stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_y410.yuv");
            enc.m_par.mfx.FrameInfo.Width = enc.m_par.mfx.FrameInfo.CropW = 176;
            enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = 144;
        }
        else {
            stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_1920x1088_24_y410_30_content_1920x1080.yuv");
            enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 1920;
            enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 1088;
        }
        g_tsStreamPool.Reg();

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    TS_REG_TEST_SUITE(hevce_8b_420_nv12_weight_prediction, testNV12, sizeof(test_case) / sizeof(test_case[0]));
    TS_REG_TEST_SUITE(hevce_8b_444_ayuv_weight_prediction, testAYUV, sizeof(test_case) / sizeof(test_case[0]));
    TS_REG_TEST_SUITE(hevce_10b_420_p010_weight_prediction, testP010, sizeof(test_case) / sizeof(test_case[0]));
    TS_REG_TEST_SUITE(hevce_10b_444_y410_weight_prediction, testY410, sizeof(test_case) / sizeof(test_case[0]));
};
