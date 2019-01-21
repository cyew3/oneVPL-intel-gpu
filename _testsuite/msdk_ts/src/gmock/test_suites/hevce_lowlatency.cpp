/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_lowlatency
{
    const int MFX_PAR = 1;

    enum TYPE {
        QUERY = 0x1,
        INIT = 0x2,
        ENCODE = 0x4,

        HUGE_SIZE_4K = 0x8,
        HUGE_SIZE_8K = 0x10,
    };

    struct tc_struct {
        struct status {
            mfxStatus query;
            mfxStatus init;
            mfxStatus encode;
        } sts;
        mfxU32 type;
        struct tctrl {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, public tsBitstreamProcessor, public tsParserHEVC {
    public:
        TestSuite() :
            tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC()
        {
            m_bs_processor = this;
            set_trace_level(0);
        }
        ~TestSuite() { }
        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);
        int RunTest(tc_struct tc, unsigned int fourcc_id);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        static const unsigned int n_cases;
        static const tc_struct test_case[];
    };

    mfxStatus TestSuite::ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
    {
        mfxStatus sts = MFX_ERR_NONE;
        SetBuffer0(bs);

        if (bs.FrameType & MFX_FRAMETYPE_B)
        {
            sts = MFX_ERR_ABORTED;
        }

        return sts;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE, {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR }
        } },

        {/*01*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT, {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP }
        } },

        {/*02*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT, {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR }
        } },

        {/*03*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_OFF }
        } },

        {/*04*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_PYRAMID }
        } },

        {/*05*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.PRefType, MFX_P_REF_SIMPLE }
        } },

        {/*06*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON },
            { MFX_PAR, &tsStruct::mfxExtAvcTemporalLayers.Layer[0].Scale, 1 } //mfxExtAvcTemporalLayers == mfxExtHevcTemporalLayers
        } },

        {/*07*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON },
            { MFX_PAR, &tsStruct::mfxExtAvcTemporalLayers.Layer[0].Scale, 1 }, //mfxExtAvcTemporalLayers == mfxExtHevcTemporalLayers
            { MFX_PAR, &tsStruct::mfxExtAvcTemporalLayers.Layer[1].Scale, 2 } //mfxExtAvcTemporalLayers == mfxExtHevcTemporalLayers
        } },

        {/*08*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON }
        } },

        {/*09*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | HUGE_SIZE_4K,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4096 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2160 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  4096 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  2160 }
        } },

        {/*10*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | HUGE_SIZE_8K,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  8192 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  8192 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  4096 }
        } }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id) {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id) {
        TS_START;

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }

        if (g_tsImpl == MFX_IMPL_HARDWARE) {
            if (g_tsHWtype < MFX_HW_CNL) {
                g_tsLog << "SKIPPED for this platform\n";
                return 0;
            }
        }

        if ((g_tsHWtype < MFX_HW_CNL) && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: Platform less CNL are NOT supported on VDENC!\n\n\n";
            throw tsSKIP;
        }

        mfxStatus sts = MFX_ERR_NONE;

        MFXInit();
        set_brc_params(&m_par);
        Load();

        bool unsupportedCase = (tc.type & HUGE_SIZE_8K) && (g_tsHWtype <= MFX_HW_CNL && m_par.mfx.LowPower != MFX_CODINGOPTION_ON);

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        if (tc.type & QUERY) {
            SETPARS(m_par, MFX_PAR);

            if (unsupportedCase)
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            else
                g_tsStatus.expect(tc.sts.query);

            Query();

            if (unsupportedCase)
                throw tsSKIP;
        }

        if (tc.type & INIT) {
            SETPARS(m_par, MFX_PAR);
            g_tsStatus.expect(tc.sts.init);
            sts = Init();
        }

        if (tc.type & ENCODE) {
            if (!(tc.type & (HUGE_SIZE_4K | HUGE_SIZE_8K) && (g_tsConfig.sim))) {
                SETPARS(m_par, MFX_PAR);
                g_tsStatus.expect(tc.sts.encode);
                EncodeFrames(50);
            }
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_420_nv12_lowlatency,  RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_lowlatency, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_lowlatency,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_lowlatency, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_lowlatency,  RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_lowlatency, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
}