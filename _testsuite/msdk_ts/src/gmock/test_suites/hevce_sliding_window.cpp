/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_sliding_window
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
        struct tc_struct
        {
            struct status {
                mfxStatus query;
                mfxStatus init;
                mfxStatus reset;
            } sts;
            mfxU32 type;
            struct tctrl {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        enum TYPE
        {
            INIT    = 1 << 1
            , RESET = 1 << 2
            , QUERY = 1 << 3
        };

        enum
        {
            MFXVPAR = 1
            , CO3
        };
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
    // Wrong WinBRCSize = 15, should be 30/1 = 30. Error expected
    {/*00*/ { MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET, {
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 15 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    }   },

    // WinBRCSize and other params are OK. No errors expected 
    {/*01*/ { MFX_ERR_NONE,  MFX_ERR_NONE, MFX_ERR_NONE },
    QUERY | INIT | RESET, {
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    }   },

    // FrameRates are not set. Not an error
    {/*02*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
    QUERY | INIT | RESET, {
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    }   },

    // WinBRCMaxAvgKbps < MaxKbps. Error expected
    {/*03*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 4500 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    } },

    // MaxKbps is not set. Error expected
    {/*04*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    } },

    // WinBRCMaxAvgKbps < TargetKbps. Error expected
    {/*05*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
    QUERY | INIT ,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 6000 }
    } },

    // WinBRCMaxAvgKbps is not set. Error expected
    {/*06*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    } },

#if !defined(LINUX_TARGET_PLATFORM_BXT) && !defined (LINUX_TARGET_PLATFORM_BXTMIN) && !defined (LINUX_TARGET_PLATFORM_CFL)

    // Wrong WinBRCSize = 15, should be 30/1 = 30. Error expected
    {/*07*/ { MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET, {
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 15 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    }   },

    // WinBRCSize and other params are OK. No errors expected 
    {/*08*/ { MFX_ERR_NONE,  MFX_ERR_NONE, MFX_ERR_NONE },
    QUERY | INIT | RESET, {
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    }   },

    // FrameRates are not set. Not an error
    {/*09*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
    QUERY | INIT | RESET, {
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    }   },

    // WinBRCMaxAvgKbps < MaxKbps. Error expected
    {/*10*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 4500 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    } },

    // MaxKbps is not set. Error expected
    {/*11*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    } },

    // WinBRCMaxAvgKbps < TargetKbps. Error expected
    {/*12*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
    QUERY | INIT ,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 6000 }
    } },

    // WinBRCMaxAvgKbps is not set. Error expected
    {/*13*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    QUERY | INIT | RESET,{
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCSize, 30 },
        { CO3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 0 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 5000 },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 }
    } },
#endif //!defined(LINUX_TARGET_PLATFORM_BXT) && !defined (LINUX_TARGET_PLATFORM_BXTMIN) && !defined (LINUX_TARGET_PLATFORM_CFL)
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        MFXInit();
        Load();
        mfxExtCodingOption3& ext3 = m_par;

        if (tc.type & QUERY)
        {
            SETPARS(m_par, MFXVPAR);
            SETPARS(&ext3, CO3);
            g_tsStatus.expect(tc.sts.query);
            Query();
        }

        if (tc.type & INIT)
        {
            SETPARS(m_par, MFXVPAR);
            SETPARS(&ext3, CO3);
            g_tsStatus.expect(tc.sts.init);
            Init();
        }

        if (tc.type & RESET)
        {
            SETPARS(m_par, MFXVPAR);
            SETPARS(&ext3, CO3);
            g_tsStatus.expect(tc.sts.reset);
            Reset();
        }

        TS_END;
        return 0;
    }

TS_REG_TEST_SUITE_CLASS(hevce_sliding_window);
}