/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_struct.h"
#include <algorithm>

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

        enum FRAME_SIZE
        {
            ZERO = 0,
            AVG_FRAME_SIZE,
            AVG_FRAME_SIZE_PLUS1,
            AVG_FRAME_SIZE_PLUS2
        };

        mfxU32 GetMaxFrameSize(FRAME_SIZE type, mfxU32 avgFrameSizeInBytes)
        {
            switch (type)
            {
            case ZERO:
                return 0;
            case AVG_FRAME_SIZE:
                return avgFrameSizeInBytes;
            case AVG_FRAME_SIZE_PLUS1:
                return avgFrameSizeInBytes + 1;
            case AVG_FRAME_SIZE_PLUS2:
                return avgFrameSizeInBytes + 2;
            }
            return 0;
        }

        struct tc_struct
        {
            mfxStatus sts;
            mfxU16 type;
            FRAME_SIZE maxFrameSize;
            FRAME_SIZE maxIFrameSize;
            FRAME_SIZE maxPBFrameSize;
            FRAME_SIZE out_maxFrameSize;
            FRAME_SIZE out_maxIFrameSize;
            FRAME_SIZE out_maxPBFrameSize;
        };

        static const tc_struct test_case[];

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*0*/ MFX_ERR_NONE, INIT, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO },
        {/*1*/ MFX_ERR_NONE, INIT, AVG_FRAME_SIZE, ZERO, ZERO, AVG_FRAME_SIZE, ZERO, ZERO },
        {/*2*/ MFX_ERR_NONE, INIT, ZERO, AVG_FRAME_SIZE, ZERO, ZERO, AVG_FRAME_SIZE, ZERO },
        {/*3*/ MFX_ERR_INVALID_VIDEO_PARAM, INIT, ZERO, ZERO, AVG_FRAME_SIZE, ZERO, ZERO, ZERO },
        {/*4*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, AVG_FRAME_SIZE, AVG_FRAME_SIZE_PLUS1, AVG_FRAME_SIZE_PLUS2,
                                                       AVG_FRAME_SIZE_PLUS2, AVG_FRAME_SIZE_PLUS1, AVG_FRAME_SIZE_PLUS2 },
        {/*5*/ MFX_ERR_NONE, ENCODE, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO },
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

        // calculate AVG Frame Size
        mfxF64 rawDataBitrate = 12.0 * m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height *
            m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD;
        mfxU32 minTargetKbps = std::min(0xffffffff, mfxU32(rawDataBitrate / 1000 / 700));
        mfxF64 frameRate = mfxF64(m_par.mfx.FrameInfo.FrameRateExtN) / m_par.mfx.FrameInfo.FrameRateExtD;
        mfxU32 avgFrameSizeInBytes = mfxU32(minTargetKbps * 1000 / frameRate / 8);

        // set required parameters
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        m_par.mfx.TargetKbps = m_par.mfx.MaxKbps = minTargetKbps;

        // set buffer mfxExtCodingOption2, mfxExtCodingOption3
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));
        extOpt2 = m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        extOpt3 = m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3);
        if (extOpt2)
        {
            ((mfxExtCodingOption2*)extOpt2)->MaxFrameSize = GetMaxFrameSize(tc.maxFrameSize, avgFrameSizeInBytes);
        }
        if (extOpt3)
        {
            ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeI = GetMaxFrameSize(tc.maxIFrameSize, avgFrameSizeInBytes);
            ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeP = GetMaxFrameSize(tc.maxPBFrameSize, avgFrameSizeInBytes);
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
            if ((extOpt2) && (tc.maxFrameSize != ZERO))
            {
                EXPECT_EQ(GetMaxFrameSize(tc.out_maxFrameSize, avgFrameSizeInBytes), ((mfxExtCodingOption2*)extOpt2)->MaxFrameSize)
                    << "ERROR: Expect MaxFrameSize = " << GetMaxFrameSize(tc.out_maxFrameSize, avgFrameSizeInBytes) << ", but real MaxFarmeSize = " << ((mfxExtCodingOption2*)extOpt2)->MaxFrameSize << "!\n";
            }
            if (extOpt3)
            {
                if (tc.maxIFrameSize != ZERO)
                    EXPECT_EQ(GetMaxFrameSize(tc.out_maxIFrameSize, avgFrameSizeInBytes), ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeI)
                        << "ERROR: Expect MaxIFrameSize = " << GetMaxFrameSize(tc.out_maxIFrameSize, avgFrameSizeInBytes) << ", but real MaxIFarmeSize = " << ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeI << "!\n";
                if (tc.out_maxFrameSize != ZERO)
                    EXPECT_EQ(GetMaxFrameSize(tc.out_maxPBFrameSize, avgFrameSizeInBytes), ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeP)
                        << "ERROR: Expect MaxPBFrameSize = " << GetMaxFrameSize(tc.out_maxPBFrameSize, avgFrameSizeInBytes) << ", but real MaxPBFarmeSize = " << ((mfxExtCodingOption3*)extOpt3)->MaxFrameSizeP << "!\n";
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
