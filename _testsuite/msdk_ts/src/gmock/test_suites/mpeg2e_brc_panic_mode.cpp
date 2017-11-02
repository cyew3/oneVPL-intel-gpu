/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

File Name: mpeg2e_brc_panic_mode.cpp

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_struct.h"

namespace mpeg2e_brc_panic_mode
{
    enum
    {
        MFX_PAR = 1,
        EXT_COD3
    };

    struct tc_struct
    {
        mfxStatus   queryStatus;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_MPEG2) {}

        ~TestSuite() {}

        int RunTest(unsigned int id);

        static const unsigned int n_cases;

        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
#if (defined(LINUX32) || defined(LINUX64))
        {/*00*/ MFX_ERR_NONE,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*01*/ MFX_ERR_NONE,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*02*/ MFX_ERR_NONE,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*03*/ MFX_ERR_NONE,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*04*/ MFX_ERR_NONE,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*05*/ MFX_ERR_NONE,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*06*/ MFX_ERR_UNSUPPORTED,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*07*/ MFX_ERR_NONE,
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*08*/ MFX_ERR_UNSUPPORTED, /* ICQ is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*09*/ MFX_ERR_UNSUPPORTED, /* ICQ is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*10*/ MFX_ERR_UNSUPPORTED, /* VCM is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*11*/ MFX_ERR_UNSUPPORTED, /* VCM is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*12*/ MFX_ERR_UNSUPPORTED, /* LA is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*13*/ MFX_ERR_UNSUPPORTED, /* LA is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*14*/ MFX_ERR_UNSUPPORTED, /* LA ICQ is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*15*/ MFX_ERR_UNSUPPORTED, /* LA ICQ is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*16*/ MFX_ERR_UNSUPPORTED, /* LA EXT is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*17*/ MFX_ERR_UNSUPPORTED, /* LA EXT is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*18*/ MFX_ERR_UNSUPPORTED, /* LA HRD is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD},
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*19*/ MFX_ERR_UNSUPPORTED, /* LA HRD is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD},
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} },

        {/*20*/ MFX_ERR_UNSUPPORTED, /* QVBR is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }} },
        {/*21*/ MFX_ERR_UNSUPPORTED, /* QVBR is unsupported in MPEG2e */
            {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
                { EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }} }
#endif
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        m_par.mfx.CodecId = MFX_CODEC_MPEG2;

        MFXInit();

        SETPARS(&m_par, MFX_PAR);
        mfxExtCodingOption3 &co3 = m_par;

        SETPARS(&co3,   EXT_COD3);
        set_brc_params(&m_par);

        g_tsStatus.expect(tc.queryStatus);

        Query();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(mpeg2e_brc_panic_mode);
}
