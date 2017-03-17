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

namespace avce_maxframesize_ipb
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
            INIT = 0,
            QUERY,
            ENCODE
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU16 type;
            mfxU32 maxFrameSize;
            mfxU32 maxIFrameSize;
            mfxU32 maxPBFrameSize;
            mfxU16 AdaptiveMaxFrameSize;
            mfxU32 out_maxFrameSize;
            mfxU32 out_maxIFrameSize;
            mfxU32 out_maxPBFrameSize;
            mfxU16 out_AdaptiveMaxFrameSize;
        };

        static const tc_struct test_case[];

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*0*/ MFX_ERR_NONE, INIT, 0, 0, 0, 0, 0, 0, 0, MFX_CODINGOPTION_OFF },
        {/*1*/ MFX_ERR_NONE, INIT, 1, 0, 0, 0, 1, 0, 0, MFX_CODINGOPTION_OFF },
        {/*2*/ MFX_ERR_NONE, INIT, 0, 1, 0, 0, 0, 1, 0, MFX_CODINGOPTION_OFF },
        {/*3*/ MFX_ERR_INVALID_VIDEO_PARAM, INIT, 0, 0, 1, 0, 0, 0, 1, MFX_CODINGOPTION_OFF },
        {/*4*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, 1, 2, 3, 0 , 2, 2, 3, MFX_CODINGOPTION_OFF },
        {/*5*/ MFX_ERR_NONE, ENCODE, 0, 0, 0, 0 , 0, 0, 0, MFX_CODINGOPTION_OFF },
        //AdaptiveMaxFrameSize tests
#if MFX_VERSION >= 1023
        {/*6*/ MFX_ERR_NONE, QUERY, 0, 1, 2, MFX_CODINGOPTION_OFF , 0, 1, 2, MFX_CODINGOPTION_OFF }, // off ok
        {/*7*/ MFX_ERR_NONE, INIT, 0, 1, 2, MFX_CODINGOPTION_UNKNOWN , 0, 1, 2, MFX_CODINGOPTION_OFF }, // by default OFF
        {/*8*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, 0, 1, 2, 0x1 , 0, 1, 2, 0 }, // ivalid tri state
        {/*9*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, 0, 1, 2, 0x1 , 0, 1, 2, MFX_CODINGOPTION_OFF }, // // ivalid tri state - but Init set it to OFF
    #if defined(_WIN32) || defined(_WIN64)
        {/*10*/ MFX_ERR_NONE, QUERY, 0, 1, 2, MFX_CODINGOPTION_ON , 0, 1, 2, MFX_CODINGOPTION_ON }, // can be enabled
        {/*11*/ MFX_ERR_NONE, INIT, 0, 1, 2, MFX_CODINGOPTION_ON , 0, 1, 2, MFX_CODINGOPTION_ON }, // can be enabled
        {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, 0, 1, 0, MFX_CODINGOPTION_ON , 0, 1, 0, 0 }, // maxPFrameSize must be non zero
        {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, 0, 1, 0, MFX_CODINGOPTION_ON , 0, 1, 0, MFX_CODINGOPTION_OFF }, // maxPFrameSize must be non zero and but Init set it to OFF
    #else // LINUX, IOTG , OPEN SOURCE
        {/*10*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, 0, 1, 2, MFX_CODINGOPTION_ON , 0, 1, 2, 0 }, // can NOT be enabled
        {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, 0, 1, 2, MFX_CODINGOPTION_ON , 0, 1, 2, MFX_CODINGOPTION_OFF }, // can NOT  be enabled
    #endif
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
        if (extOpt2)
        {
            ((mfxExtCodingOption2*)extOpt2)->MaxFrameSize = tc.maxFrameSize;
        }
        if (extOpt3)
        {
            ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeI = tc.maxIFrameSize;
            ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeP = tc.maxPBFrameSize;
#if MFX_VERSION >= 1023
            ((mfxExtCodingOption3*)extOpt3)->AdaptiveMaxFrameSize = tc.AdaptiveMaxFrameSize;
#endif
        }

        g_tsStatus.expect(tc.sts);

        if (tc.type == QUERY)
        {
            out_par = m_par;
            m_pParOut = &out_par;
            Query();
        }
        else if (tc.type == INIT)
        {
            Init();
        }
        else
        {
            EncodeFrames(1);
        }

        if (tc.sts >= MFX_ERR_NONE)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            out_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
            out_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));

            if (tc.type != QUERY)
            {
                GetVideoParam(m_session, &out_par);
            }

            //check expOpt2, extOpt3
            extOpt2 = out_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
            extOpt3 = out_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3);
            if ((extOpt2) && (tc.maxFrameSize != 0))
            {
                EXPECT_EQ(tc.out_maxFrameSize, ((mfxExtCodingOption2*)extOpt2)->MaxFrameSize)
                    << "ERROR: Expect MaxFrameSize = " << tc.out_maxFrameSize << ", but real MaxFarmeSize = " << ((mfxExtCodingOption2*)extOpt2)->MaxFrameSize << "!\n";
            }
            if (extOpt3)
            {
                if (tc.maxIFrameSize != 0)
                    EXPECT_EQ(tc.out_maxIFrameSize, ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeI)
                        << "ERROR: Expect MaxIFrameSize = " << tc.out_maxIFrameSize << ", but real MaxIFarmeSize = " << ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeI << "!\n";
                if (tc.out_maxFrameSize != 0)
                    EXPECT_EQ(tc.out_maxPBFrameSize, ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeP)
                        << "ERROR: Expect MaxPBFrameSize = " << tc.out_maxPBFrameSize << ", but real MaxPBFarmeSize = " << ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeP << "!\n";
#if MFX_VERSION >= 1023
                EXPECT_EQ(tc.out_AdaptiveMaxFrameSize, ((mfxExtCodingOption3*)extOpt3)->AdaptiveMaxFrameSize)
                    << "ERROR: Expect AdaptiveMaxFrameSize = " << tc.out_AdaptiveMaxFrameSize << ", but real AdaptiveMaxFrameSize = " << ((mfxExtCodingOption3*)extOpt3)->AdaptiveMaxFrameSize << "!\n";
#endif
            }
        }

        if (m_initialized)
        {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_maxframesize_ipb);
}
