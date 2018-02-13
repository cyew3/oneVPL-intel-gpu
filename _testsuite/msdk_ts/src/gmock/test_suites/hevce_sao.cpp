/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"

//#define DEBUG_STREAM "hevce_sao.265"

namespace hevce_sao
{
    using namespace BS_HEVC2;

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
        mfxU16  m_saoInit;
        mfxU16  m_slicesWithSAO;
        const mfxU16* m_saoRT;
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
            m_slicesWithSAO = 0;
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
                mfxU16 saoRT = (m_saoRT[slice.POC] && m_saoInit != MFX_SAO_DISABLE) ? m_saoRT[slice.POC] : m_saoInit;

                TS_TRACE(slice.POC);
                TS_TRACE(!!slice.sps->sample_adaptive_offset_enabled_flag);
                TS_TRACE(!!slice.sao_luma_flag);
                TS_TRACE(!!slice.sao_chroma_flag);

                EXPECT_EQ(m_saoInit != MFX_SAO_DISABLE, !!slice.sps->sample_adaptive_offset_enabled_flag);

                if (saoRT != MFX_SAO_UNKNOWN)
                {
                    EXPECT_EQ(!!(saoRT & MFX_SAO_ENABLE_LUMA), !!slice.sao_luma_flag);
                    EXPECT_EQ(!!(saoRT & MFX_SAO_ENABLE_CHROMA), !!slice.sao_chroma_flag);
                }
                
                if (!slice.sao_luma_flag && !slice.sao_chroma_flag)
                    continue;

                m_slicesWithSAO++;

                mfxF64 AppliedSAOPercent[3] = {};
                mfxF64 nLCU = 0;

                for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next)
                {
                    AppliedSAOPercent[0] += (pCTU->sao[0].type_idx != SAO_TYPE::NOT_APPLIED);
                    AppliedSAOPercent[1] += (pCTU->sao[1].type_idx != SAO_TYPE::NOT_APPLIED);
                    AppliedSAOPercent[2] += (pCTU->sao[2].type_idx != SAO_TYPE::NOT_APPLIED);
                    nLCU++;
                }

                AppliedSAOPercent[0] /= nLCU;
                AppliedSAOPercent[1] /= nLCU;
                AppliedSAOPercent[2] /= nLCU;
                AppliedSAOPercent[0] *= 100;
                AppliedSAOPercent[1] *= 100;
                AppliedSAOPercent[2] *= 100;

                TS_TRACE(nLCU);
                TS_TRACE(AppliedSAOPercent[0]);
                TS_TRACE(AppliedSAOPercent[1]);
                TS_TRACE(AppliedSAOPercent[2]);

                if (slice.sao_luma_flag)
                {
                    EXPECT_GT(AppliedSAOPercent[0], 20);
                }
                else
                {
                    EXPECT_EQ(0, AppliedSAOPercent[0]);
                }

                if (slice.sao_chroma_flag)
                {
                    EXPECT_GT(AppliedSAOPercent[1], 10);
                    EXPECT_GT(AppliedSAOPercent[2], 10);
                }
                else
                {
                    EXPECT_EQ(0, AppliedSAOPercent[1]);
                    EXPECT_EQ(0, AppliedSAOPercent[2]);
                }
            }
        }

        bs.DataLength = 0;

        BreakOnFailure();

        return MFX_ERR_NONE;
    }

    struct TestCase
    {
        mfxU16 TUInit;
        mfxU16 SaoInit;
        mfxU16 TUReset;
        mfxU16 SaoReset;
        mfxU16 SaoRT[5];
    };

    class SProc : public tsSurfaceProcessor
    {
    public:
        tsSurfaceProcessor& m_reader;
        tsVideoEncoder& m_enc;
        const TestCase& m_tc;

        SProc(tsSurfaceProcessor& r, const TestCase& tc, tsVideoEncoder& enc)
            : m_reader(r)
            , m_enc(enc)
            , m_tc(tc)
        {
            m_max = m_reader.m_max;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            auto& ctrl = *m_enc.m_ctrl_next.New();

            mfxExtHEVCParam& hp = ctrl;
            hp = (mfxExtHEVCParam&)m_enc.m_par;
            hp.SampleAdaptiveOffset = m_tc.SaoRT[m_cur-1];

            return m_reader.ProcessSurface(s);
        }
    };

    enum eSAO
    {
          sD = MFX_SAO_UNKNOWN
        , s0 = MFX_SAO_DISABLE
        , sY = MFX_SAO_ENABLE_LUMA
        , sC = MFX_SAO_ENABLE_CHROMA
    };

    const TestCase TCList[] =
    {
        { 1, sD,       4, sD,{} },
        { 1, sD,       7, sD,{} },
        { 4, sD,       1, sD,{} },
        { 4, sD,       7, sD,{} },
        { 7, sD,       1, sD,{} },
        { 7, sD,       4, sD,{} },
        { 7, sY,       4, sD,{} },
        { 4, sD,       4, s0,         {0, sY, 0, sC, sY | sC} },
        { 4, s0,       4, sY,         {s0, 0, 0, sC, 0} },
        { 4, sY,       4, sC | sY,    {} },
        { 4, sC,       4, sY,         {} },
        { 4, sY | sC,  4, s0,         {} },
    };

    bool isUnsupported(tsExtBufType<mfxVideoParam>& par)
    {
        mfxExtCodingOption2* CO2 = par;
        mfxExtCodingOption3* CO3 = par;

        return (g_tsHWtype < MFX_HW_CNL)
            //|| LCUSize == 16
            || par.mfx.TargetUsage == 7
            || (CO3 && CO3->WeightedPred == MFX_WEIGHTED_PRED_EXPLICIT)
            || (CO3 && CO3->WeightedBiPred == MFX_WEIGHTED_PRED_EXPLICIT)
            || (   (g_tsHWtype == MFX_HW_CNL)
                && (   (CO3 && CO3->TargetBitDepthLuma == 10)
                    || (par.mfx.LowPower == 16 && CO2 && CO2->MaxSliceSize)))
            ;
    }

    const mfxU32 nFrames = sizeof(TCList[0].SaoRT) / sizeof(TCList[0].SaoRT[0]);

    int test(unsigned int id, tsVideoEncoder& enc, const char* stream)
    {
        auto& tc = TCList[id];
        mfxExtHEVCParam& hp = enc.m_par;
        BitstreamChecker checker;
        tsRawReader reader(stream, enc.m_par.mfx.FrameInfo, nFrames);
        SProc sproc(reader, tc, enc);

        reader.m_disable_shift_hack = true; // w/a stupid hack in tsRawReader for P010
        enc.m_filler = &sproc;
        enc.m_bs_processor = &checker;

        enc.m_par.mfx.TargetUsage = tc.TUInit;
        hp.SampleAdaptiveOffset = tc.SaoInit;

        bool unsupported = isUnsupported(enc.m_par);

        enc.MFXInit();
        enc.Load();

        if (unsupported && (hp.SampleAdaptiveOffset & (MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA)))
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Query();

        if (unsupported && tc.SaoInit != MFX_SAO_DISABLE)
        {
            EXPECT_EQ(MFX_SAO_UNKNOWN, hp.SampleAdaptiveOffset);
        }
        else
        {
            EXPECT_EQ(tc.SaoInit, hp.SampleAdaptiveOffset);
        }

        if (unsupported && (hp.SampleAdaptiveOffset & (MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA)))
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Init();
        enc.GetVideoParam();

        if (unsupported)
        {
            EXPECT_EQ(MFX_SAO_DISABLE, hp.SampleAdaptiveOffset);
        }
        else if (tc.SaoInit)
        {
            EXPECT_EQ(tc.SaoInit, hp.SampleAdaptiveOffset);
        }
        else if (enc.m_par.mfx.TargetUsage == 1)
        {
            EXPECT_EQ((MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA), hp.SampleAdaptiveOffset);
        }
        else if (enc.m_par.mfx.TargetUsage == 4)
        {
            EXPECT_EQ(MFX_SAO_UNKNOWN, hp.SampleAdaptiveOffset);
        }
        else if (enc.m_par.mfx.TargetUsage == 7)
        {
            EXPECT_EQ(MFX_SAO_DISABLE, hp.SampleAdaptiveOffset);
        }

        BreakOnFailure();

        checker.m_saoInit = hp.SampleAdaptiveOffset;
        checker.m_saoRT = tc.SaoRT;

        enc.EncodeFrames(nFrames);

        TS_TRACE(checker.m_slicesWithSAO);
        checker.m_slicesWithSAO = 0;

        mfxExtEncoderResetOption& ro = enc.m_par;
        mfxU16 saoInit = hp.SampleAdaptiveOffset;
        mfxU16 saoReset = tc.SaoReset ? tc.SaoReset : saoInit;
        enc.m_par.mfx.TargetUsage = tc.TUReset;
        hp.SampleAdaptiveOffset = tc.SaoReset;
        unsupported = isUnsupported(enc.m_par);
        ro.StartNewSequence = MFX_CODINGOPTION_OFF;
        bool idrRequired = ((saoInit == MFX_SAO_DISABLE) != (saoReset == MFX_SAO_DISABLE))
            || ((saoInit != MFX_SAO_DISABLE) && unsupported);

        if (tc.TUReset != tc.TUInit)
        {
            ro.StartNewSequence = MFX_CODINGOPTION_ON;
            idrRequired = true;
        }

        if (unsupported && (hp.SampleAdaptiveOffset & (MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA)))
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Query();

        if (unsupported && tc.SaoReset != MFX_SAO_DISABLE)
        {
            EXPECT_EQ(MFX_SAO_UNKNOWN, hp.SampleAdaptiveOffset);
        }
        else
        {
            EXPECT_EQ(tc.SaoReset, hp.SampleAdaptiveOffset);
        }

    l_reset:
        if (idrRequired && ro.StartNewSequence == MFX_CODINGOPTION_OFF && !unsupported)
            g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        else if (unsupported && (hp.SampleAdaptiveOffset & (MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA)))
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Reset();

        if (idrRequired && ro.StartNewSequence == MFX_CODINGOPTION_OFF)
        {
            ro.StartNewSequence = MFX_CODINGOPTION_ON;
            goto l_reset;
        }

        enc.GetVideoParam();

        if (unsupported)
        {
            EXPECT_EQ(MFX_SAO_DISABLE, hp.SampleAdaptiveOffset);
        }
        else if (saoReset)
        {
            EXPECT_EQ(saoReset, hp.SampleAdaptiveOffset);
        }
        else if (enc.m_par.mfx.TargetUsage == 1)
        {
            EXPECT_EQ((MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA), hp.SampleAdaptiveOffset);
        }
        else if (enc.m_par.mfx.TargetUsage == 4)
        {
            EXPECT_EQ(MFX_SAO_UNKNOWN, hp.SampleAdaptiveOffset);
        }
        else if (enc.m_par.mfx.TargetUsage == 7)
        {
            EXPECT_EQ(MFX_SAO_DISABLE, hp.SampleAdaptiveOffset);
        }

        BreakOnFailure();

        sproc.m_cur = 0;
        reader.m_cur = 0;

        checker.m_saoInit = hp.SampleAdaptiveOffset;
        checker.m_saoRT = tc.SaoRT - nFrames * (ro.StartNewSequence == MFX_CODINGOPTION_OFF);

        enc.EncodeFrames(nFrames);

        TS_TRACE(checker.m_slicesWithSAO);

        return 0;
    }

    int testNV12(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testYUY2(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV8bit422/Kimono1_352x288_24_yuy2.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testAYUV(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV8bit444/Kimono1_352x288_24_ayuv.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testP010(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV10bit420/Kimono1_352x288_24_p010_shifted.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }


    int testY210(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV10bit422/Kimono1_352x288_24_y210.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testY410(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV10bit444/Kimono1_352x288_24_y410.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testP012(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV16bit420/FruitStall_176x144_240_p016.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 12;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 12;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 176;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 144;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 24;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }


    int testY212(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV16bit422/FruitStall_176x144_240_y216.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 12;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 12;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 176;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 144;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 24;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testY412(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV16bit444/FruitStall_176x144_240_y416.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 12;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 12;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 176;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 144;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 24;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    TS_REG_TEST_SUITE(hevce_8b_420_nv12_sao, testNV12, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_8b_422_yuy2_sao, testYUY2, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_8b_444_ayuv_sao, testAYUV, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_10b_420_p010_sao, testP010, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_10b_422_y210_sao, testY210, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_10b_444_y410_sao, testY410, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_12b_420_p016_sao, testP012, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_12b_422_y216_sao, testY212, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_12b_444_y416_sao, testY412, sizeof(TCList) / sizeof(TCList[0]));
};
