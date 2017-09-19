/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

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
        ENCODE = 0x4
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
        int RunTest(unsigned int id);
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
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        auto& tc = test_case[id];

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

        mfxStatus sts = MFX_ERR_NONE;

        MFXInit();
        set_brc_params(&m_par);
        Load();

        if (tc.type & QUERY) {
            SETPARS(m_par, MFX_PAR);
            g_tsStatus.expect(tc.sts.query);
            Query();
        }

        if (tc.type & INIT) {
            SETPARS(m_par, MFX_PAR);
            g_tsStatus.expect(tc.sts.init);
            sts = Init();
            }

        if (tc.type & ENCODE) {
            SETPARS(m_par, MFX_PAR);
            g_tsStatus.expect(tc.sts.encode);
            EncodeFrames(50);
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_lowlatency);
}