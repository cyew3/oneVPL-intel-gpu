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

#if MFX_VERSION >= 1023

namespace avce_adaptivemaxframesize
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:

        enum TYPE
        {
            INIT = 0x1,
            QUERY =0x2,
            ENCODE =0x4
        };

        enum
        {
            MFXPAR = 1,
            EXT_COD2,
            EXT_COD2_EXPECTED,
            EXT_COD3,
            EXT_COD3_EXPECTED_QUERY,
            EXT_COD3_EXPECTED_INIT,
        };
        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, QUERY | INIT ,{
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeI, 1 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeP, 2 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_UNKNOWN },
            { EXT_COD3_EXPECTED_QUERY, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_UNKNOWN },
            { EXT_COD3_EXPECTED_INIT, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_OFF },
        } },

        // invalid tri-state
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT ,{
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeI, 1 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeP, 2 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, 0x1 },
            { EXT_COD3_EXPECTED_QUERY, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, 0 },
            { EXT_COD3_EXPECTED_INIT, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_OFF },
        } },

        // zero MaxFrameSizeP
        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT ,{
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeI, 1 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeP, 0 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_ON },
            { EXT_COD3_EXPECTED_QUERY, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, 0 },
            { EXT_COD3_EXPECTED_INIT, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_OFF },
        } },

        // zero LowPower ON
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT ,{
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeI, 1 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeP, 2 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_ON },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
            { EXT_COD3_EXPECTED_QUERY, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, 0 },
            { EXT_COD3_EXPECTED_INIT, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_OFF },
        } },

        #if defined(_WIN32) || defined(_WIN64)
        // Can be ON
        {/*04*/ MFX_ERR_NONE, QUERY | INIT ,{
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeI, 1 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeP, 2 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_ON },
            { EXT_COD3_EXPECTED_QUERY, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_ON },
            { EXT_COD3_EXPECTED_INIT, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_ON },
        } },
        #else // LINUX, IOTG , OPEN SOURCE
        // Can't be ON
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT ,{
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeI, 1 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.MaxFrameSizeP, 2 },
            { EXT_COD3, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_ON },
            { EXT_COD3_EXPECTED_QUERY, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, 0 },
            { EXT_COD3_EXPECTED_INIT, &tsStruct::mfxExtCodingOption3.AdaptiveMaxFrameSize, MFX_CODINGOPTION_OFF },
        } },
        #endif
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        mfxStatus sts = MFX_ERR_NONE;
        mfxExtBuffer* extOpt2;
        mfxExtBuffer* extOpt3;
        tsExtBufType<mfxVideoParam> out_par;

        MFXInit();
        m_session = tsSession::m_session;

        // set buffer mfxExtCodingOption2, mfxExtCodingOption3
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));
        extOpt2 = m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        extOpt3 = m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3);

        SETPARS(m_par, MFXPAR);
        SETPARS(extOpt2, EXT_COD2);
        SETPARS(extOpt3, EXT_COD3);

        mfxExtCodingOption3 extOpt3_expectation;

        g_tsStatus.expect(tc.sts);

        if (tc.type & QUERY)
        {
            SETPARS(&extOpt3_expectation, EXT_COD3_EXPECTED_QUERY);
            out_par = m_par;
            m_pParOut = &out_par;

            Query();

            extOpt3 = out_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3);
            EXPECT_EQ(extOpt3_expectation.AdaptiveMaxFrameSize, ((mfxExtCodingOption3*)extOpt3)->AdaptiveMaxFrameSize)
                << "ERROR: Expect AdaptiveMaxFrameSize = " << extOpt3_expectation.AdaptiveMaxFrameSize << ", but real AdaptiveMaxFrameSize = " << ((mfxExtCodingOption3*)extOpt3)->AdaptiveMaxFrameSize << "!\n";

        }

        g_tsStatus.expect(tc.sts);

        if (tc.type & INIT)
        {
            SETPARS(&extOpt3_expectation, EXT_COD3_EXPECTED_INIT);
            Init();

            GetVideoParam(m_session, &out_par);
            extOpt3 = out_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3);

            EXPECT_EQ(extOpt3_expectation.AdaptiveMaxFrameSize, ((mfxExtCodingOption3*)extOpt3)->AdaptiveMaxFrameSize)
                << "ERROR: Expect AdaptiveMaxFrameSize = " << extOpt3_expectation.AdaptiveMaxFrameSize << ", but real AdaptiveMaxFrameSize = " << ((mfxExtCodingOption3*)extOpt3)->AdaptiveMaxFrameSize << "!\n";
        }

        if (m_initialized)
        {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_adaptivemaxframesize);
}

#endif //if MFX_VERSION >= 1023