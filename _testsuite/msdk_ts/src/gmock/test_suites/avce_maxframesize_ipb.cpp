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
            ENCODE
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU16 type;
            mfxU32 maxFrameSize;
            mfxU32 maxIFrameSize;
            mfxU32 maxPBFrameSize;
            mfxU32 out_maxFrameSize;
            mfxU32 out_maxIFrameSize;
            mfxU32 out_maxPBFrameSize;
        };

        static const tc_struct test_case[];

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*0*/ MFX_ERR_NONE, INIT, 0, 0, 0, 0, 0, 0 },
        {/*1*/ MFX_ERR_NONE, INIT, 1, 0, 0, 1, 0, 0 },
        {/*2*/ MFX_ERR_NONE, INIT, 0, 1, 0, 0, 1, 0 },
        {/*3*/ MFX_ERR_INVALID_VIDEO_PARAM, INIT, 0, 0, 1, 0, 0, 1 },
        {/*4*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, 1, 2, 3 , 2, 2, 3 },
        {/*5*/ MFX_ERR_NONE, ENCODE, 0, 0, 0 , 0, 0, 0 },
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
        }

        g_tsStatus.expect(tc.sts);

        if (tc.type == INIT)
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
            GetVideoParam(m_session, &out_par);

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
