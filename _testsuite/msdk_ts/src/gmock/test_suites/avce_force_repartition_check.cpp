/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.

File Name: avce_force_repartition_check.cpp
\* ****************************************************************************** */

/*!
\file avce_force_repartition_check.cpp
\brief Gmock test avce_force_repartition_check

Description:
This suite tests AVC encoder\n

Algorithm:
- Initializing MSDK lib
- Set suite params (AVC encoder params)
- Set case params
- Set expected Query() status
- Call Query()
- Check returned status
- Set case params
- Set expected Init() status
- Call Init()
- Check returned status
- Call GetVideoParam() if Init() returned a warning
- Check returned status and expected values
- Set case params
- Set expected Reset() status
- Call Reset()
- Check returned status
- Call GetVideoParam() if Reset () returned a warning
- Check returned status and expected values

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_force_repartition_check {

    enum
    {
        MFX_PAR   =  0,
        RESET_PAR  = 1,
        QUERY_EXP =  2,
        INIT_EXP  =  3,
        RESET_EXP =  4,
        ENC_EXP   =  5,
    };

    enum TYPE
    {
        QUERY = 1,
        INIT  = 2,
        RESET = 4,
        ENC   = 8,
    };

    struct tc_struct {
        mfxU32 type;
        //! Expected Query() status
        mfxStatus q_sts;
        //! Expected Init() status
        mfxStatus i_sts;
        //! Expected Reset() status
        mfxStatus r_sts;
        //! Expected EncodeFrames() status
        mfxStatus enc_sts;
        //! \brief Structure contains params for some fields of encoder
        struct f_pair {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const tsStruct::Field* f;
            //! Field value
            mfxI32 v;
        }set_par[MAX_NPARS];
    };

    //!\brief Main test class
    class TestSuite :tsVideoEncoder {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) { }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common for the entire test suite
        tsExtBufType<mfxVideoParam> initParams();
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType <mfxVideoParam> par;
        par.mfx.CodecId = MFX_CODEC_AVC;
        par.mfx.CodecLevel = MFX_LEVEL_AVC_4;
        par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.Width = 1920;
        par.mfx.FrameInfo.Height = 1088;
        par.mfx.FrameInfo.CropW = 1920;
        par.mfx.FrameInfo.CropH = 1088;
        par.mfx.TargetKbps = 10000;
        par.mfx.MaxKbps = 13000;
        par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        par.mfx.TargetUsage = MFX_TARGETUSAGE_1;
        par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        return par;
    }

    const tc_struct TestSuite::test_case[] =
    {
        // VALID PARAMS. RepartitionCheckEnable = 0 - Follow driver default settings
        {/*00*/ QUERY | INIT | ENC | RESET, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
            { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_UNKNOWN },
            { RESET_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_OFF },
            { QUERY_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_UNKNOWN },
            { INIT_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { RESET_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_OFF } }
        },
        // VALID PARAMS. RepartitionCheckEnable = 3 - Follow driver default settings
        {/*01*/  QUERY | INIT | ENC | RESET, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
            { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { RESET_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ON },
            { QUERY_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { INIT_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { RESET_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ON } }
        },
        // VALID PARAMS. RepartitionCheckEnable = 1 - Enable this feature totally for all cases
        {/*02*/  QUERY | INIT | ENC | RESET, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
            { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ON },
            { RESET_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_OFF },
            { QUERY_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ON },
            { INIT_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ON },
            { RESET_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_OFF } }
        },
        // VALID PARAMS. RepartitionCheckEnable = 2 - Disable this feature totally for all cases
        {/*03*/  QUERY | INIT | ENC | RESET, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
            { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_OFF },
            { RESET_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { QUERY_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_OFF },
            { INIT_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_OFF },
            { RESET_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE } }
        },
        // INVALID PARAMS. LowPower = ON and RepartitionCheckEnable != MFX_CODINGOPTION_UNKNOWN and RepartitionCheckEnable != MFX_CODINGOPTION_ADAPTIVE
        {/*04*/  QUERY | INIT | RESET, MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, MFX_ERR_NONE,
            { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ON },
            { RESET_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { QUERY_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_UNKNOWN },
            { RESET_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE } }
        },
        // INVALID PARAMS. RepartitionCheckEnable is out of valid range [MFX_CODINGOPTION_UNKNOWN; MFX_CODINGOPTION_ADAPTIVE]
        {/*05*/  QUERY | INIT | ENC | RESET, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_ERR_NONE,
            { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
            { MFX_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, 100 },
            { RESET_PAR, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { QUERY_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_UNKNOWN },
            { INIT_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE },
            { RESET_EXP, &tsStruct::mfxExtCodingOption3.RepartitionCheckEnable, MFX_CODINGOPTION_ADAPTIVE } }
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        auto& tc = test_case[id];

        m_par = initParams();

        MFXInit();

        if (tc.type & QUERY)
        {
            tsExtBufType<mfxVideoParam> out_par;
            SETPARS(&m_par, MFX_PAR);
            out_par = m_par;
            m_pParOut = &out_par;
            g_tsStatus.expect(tc.q_sts);
            Query();
            EXPECT_TRUE(COMPAREPARS(out_par, QUERY_EXP)) << "Error! Parameters mismatch.";
        }

        if (tc.type & INIT)
        {
            tsExtBufType<mfxVideoParam> out_par;
            SETPARS(&m_par, MFX_PAR);
            out_par = m_par;
            m_pParOut = &out_par;
            g_tsStatus.expect(tc.i_sts);
            Init();
            if (tc.i_sts >= MFX_ERR_NONE)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                GetVideoParam(m_session, &out_par);
                EXPECT_TRUE(COMPAREPARS(out_par, INIT_EXP)) << "Error! Parameters mismatch.";
            }
        }

        if (tc.type & ENC)
        {
            g_tsStatus.expect(tc.enc_sts);
            EncodeFrames(1);
        }

        if (tc.type & RESET)
        {
            tsExtBufType<mfxVideoParam> out_par;
            SETPARS(&m_par, RESET_PAR);
            out_par = m_par;
            m_pParOut = &out_par;
            g_tsStatus.expect(tc.r_sts);
            Reset();
            if (tc.r_sts >= MFX_ERR_NONE)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                GetVideoParam(m_session, &out_par);
                EXPECT_TRUE(COMPAREPARS(out_par, RESET_EXP)) << "Error! Parameters mismatch.";
            }
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_force_repartition_check);
}