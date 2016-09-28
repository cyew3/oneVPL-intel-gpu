/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include <stdint.h>

namespace hevce_maxframesize
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:

        enum TYPE
        {
            QUERY = 0x1,
            INIT = 0x2,
            ENCODE = 0x4,
            CHECK_EXPECTED = 0x8
        };

        enum
        {
            MFXPAR = 1,
            EXT_COD2,
            EXT_COD2_EXPECTED,
        };
        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 mode;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        void SetExpectations(const tc_struct& tc);
        mfxU32 expectedMaxFrameSize;
        mfxU32 actualMaxFrameSize;
    };

#define VBRPARAMETERS(Delay,Target,Max) \
    { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR }, \
    { MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, (Delay) },\
    { MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, (Target) },\
    { MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, (Max) },\
    { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, (30) },\
    { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, (1) }

#define STDVBRPARAMETERS() VBRPARAMETERS(400,1200,1500)

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, QUERY | INIT, {
            STDVBRPARAMETERS(),
            { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0 }
        } },
        {/*01*/ MFX_ERR_NONE, QUERY | INIT, {
            STDVBRPARAMETERS(),
            { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 5001 }
        } },
        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT | CHECK_EXPECTED | ENCODE, {
            STDVBRPARAMETERS(),
            { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 4999 },
            { EXT_COD2_EXPECTED, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 5000 },
        } }, // less than avgFrameSizeInBytes
        {/*03*/ MFX_ERR_NONE, QUERY | INIT | ENCODE, {
            STDVBRPARAMETERS(),
            { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxFrameSize, UINT32_MAX }, // Max UINT32 Value
        } }, // max value don't have any sence but we should not fail
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT | CHECK_EXPECTED , {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1200 },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, (30) },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, (1) },
            { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 5001 },
            { EXT_COD2_EXPECTED, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0 }, //MaxFrameSize is not supported in CBR
        } },
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT | CHECK_EXPECTED , {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.QPI, 26 },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.QPP, 26 },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 26 },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, (30) },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, (1) },
            { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 5001 },
            { EXT_COD2_EXPECTED, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0 }, //MaxFrameSize is not supported in CQP
        } },
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | INIT | CHECK_EXPECTED, {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1200 },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, (30) },
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, (1) },
            { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 5001 },
            { EXT_COD2_EXPECTED, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0 }, //MaxFrameSize is not supported in LA_EXT
        } },

    };



    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    mfxU32 GetMaxFrameSize(tsExtBufType<mfxVideoParam> &pmvp)
    {
        mfxExtCodingOption2& cod2 = pmvp;
        return cod2.MaxFrameSize;
    }

    void TestSuite::SetExpectations(const tc_struct& tc)
    {
        g_tsStatus.expect(tc.sts);
        SETPARS(m_par, MFXPAR);

        mfxExtCodingOption2& cod2 = m_par;
        SETPARS(&cod2, EXT_COD2);

        mfxExtCodingOption2 expected = {};
        SETPARS(&expected, EXT_COD2_EXPECTED);

        if (tc.mode & CHECK_EXPECTED)
        {
            // compare with expected value from TC
            expectedMaxFrameSize = expected.MaxFrameSize;
        }
        else
        {
            // unchanged
            expectedMaxFrameSize = cod2.MaxFrameSize;
        }

    }
    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        mfxStatus sts = MFX_ERR_NONE;
        tsExtBufType<mfxVideoParam> out_par;

        MFXInit();
        m_session = tsSession::m_session;

        if (!m_loaded)
        {
            Load();
        }

        if (tc.mode & QUERY)
        {
            SetExpectations(tc);
            out_par = m_par;
            m_pParOut = &out_par;
            Query();

            if (tc.sts >= MFX_ERR_NONE)
            {

                actualMaxFrameSize = GetMaxFrameSize(out_par);
                EXPECT_EQ(expectedMaxFrameSize, actualMaxFrameSize)
                    << "ERROR: QUERY EXPECT_EQ( expectedMaxFrameSize:" << expectedMaxFrameSize << ", actualMaxFrameSize:" << actualMaxFrameSize << ") --";

            }
        }
        if (tc.mode & INIT)
        {
            SetExpectations(tc);
            Init();
            if (tc.sts >= MFX_ERR_NONE)
            {
                out_par = m_par;
                GetVideoParam(m_session, &out_par);
                actualMaxFrameSize = GetMaxFrameSize(out_par);

                EXPECT_EQ(expectedMaxFrameSize, actualMaxFrameSize)
                    << "ERROR: INIT EXPECT_EQ( expectedMaxFrameSize:" << expectedMaxFrameSize << ", actualMaxFrameSize:" << actualMaxFrameSize << ") --";
            }
        }
        if (tc.mode & ENCODE)
        {
            SetExpectations(tc);
            g_tsStatus.expect(MFX_ERR_NONE);
            EncodeFrames(1);
        }


        if (m_initialized)
        {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_maxframesize);
}
