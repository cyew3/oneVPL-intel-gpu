/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace hevcd_close
{
    class TestSuite : public tsVideoDecoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC)
        {

        }
        ~TestSuite() {}
        int RunTest(unsigned int id, const char* sname);

    private:

        enum
        {
            MFX_PAR,
            NULL_SESSION,
            NOT_INIT,
            FAILED_INIT,
            CLOSED,
            NONE
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

        {/*00*/ MFX_ERR_NONE, NONE },
        {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*02*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT },
        {/*03*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT },
        {/*04*/ MFX_ERR_NOT_INITIALIZED, CLOSED },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id, const char* sname)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        MFXInit();
        Load();

        //set default param
        g_tsStreamPool.Reg();
        tsBitstreamReader reader(sname, 100000);
        m_bs_processor = &reader;

        SETPARS(m_pPar, MFX_PAR);

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            Init();
            DecodeFrames(1);
        }

        if (tc.type == CLOSED)
        {
            Close();
        }

        g_tsStatus.expect(tc.sts);

        // Call test function
        if (tc.type == NULL_SESSION)
        {
            Close(NULL);
            g_tsStatus.expect(MFX_ERR_NONE);
            Close();
        }
        else
        {
            Close();
        }

        TS_END;
        return 0;
    }

    template <unsigned fourcc>
    char const* query_stream(unsigned int, std::integral_constant<unsigned, fourcc>);

    /* 8 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
    { return "conformance/hevc/itu/RPS_B_qualcomm_5.bit"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_YUY2>)
    { return "conformance/hevc/422format/inter_422_8.bin"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>)
    { return "conformance/hevc/10bit/GENERAL_8b_444_RExt_Sony_1.bit"; }

    /* 10 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
    { return "conformance/hevc/10bit/WP_A_MAIN10_Toshiba_3.bit"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y210>)
    { return "conformance/hevc/10bit/inter_422_10.bin"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>)
    { return "conformance/hevc/StressBitstreamEncode/rext444_10b/Stress_HEVC_Rext444_10bHT62_432x240_30fps_302_inter_stress_2.2.hevc"; }

    /* 12 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P016>)
    { return "conformance/hevc/12bit/420format/GENERAL_12b_420_RExt_Sony_1.bit"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y216>)
    { return "conformance/hevc/12bit/422format/GENERAL_12b_422_RExt_Sony_1.bit"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y416>)
    { return "conformance/hevc/12bit/444format/GENERAL_12b_444_RExt_Sony_1.bit"; }

    template <unsigned fourcc, unsigned profile>
    char const* query_stream(unsigned int id, std::integral_constant<unsigned, fourcc>, std::integral_constant<unsigned, profile>)
    { return query_stream(id, std::integral_constant<unsigned, fourcc>{}); }

    /* SCC */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_NV12>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
    { return "conformance/hevc/scc/scc-main/020_main_palette_all_lf.hevc"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
    { return "conformance/hevc/scc/scc-main10/020_main10_palette_all_lf.hevc"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
    { return "conformance/hevc/scc/scc-main444/020_main444_palette_all_lf.hevc"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
    { return "conformance/hevc/scc/scc-main444_10/020_main444_10_palette_all_lf.hevc"; }

    template <unsigned fourcc, unsigned profile = MFX_PROFILE_UNKNOWN>
    struct TestSuiteEx
        : public TestSuite
    {
        static
        int RunTest(unsigned int id)
        {
            const char* sname =
                g_tsStreamPool.Get(query_stream(id, std::integral_constant<unsigned, fourcc>{}, std::integral_constant<unsigned, profile>{}));
            g_tsStreamPool.Reg();

            TestSuite suite;
            return suite.RunTest(id, sname);
        }
    };

    TS_REG_TEST_SUITE(hevcd_close,     TestSuiteEx<MFX_FOURCC_NV12>::RunTest, TestSuiteEx<MFX_FOURCC_NV12>::n_cases);
    TS_REG_TEST_SUITE(hevcd_422_close, TestSuiteEx<MFX_FOURCC_YUY2>::RunTest, TestSuiteEx<MFX_FOURCC_YUY2>::n_cases);
    TS_REG_TEST_SUITE(hevcd_444_close, TestSuiteEx<MFX_FOURCC_AYUV>::RunTest, TestSuiteEx<MFX_FOURCC_AYUV>::n_cases);

    TS_REG_TEST_SUITE(hevc10d_close,     TestSuiteEx<MFX_FOURCC_P010>::RunTest, TestSuiteEx<MFX_FOURCC_P010>::n_cases);
    TS_REG_TEST_SUITE(hevc10d_422_close, TestSuiteEx<MFX_FOURCC_Y210>::RunTest, TestSuiteEx<MFX_FOURCC_Y210>::n_cases);
    TS_REG_TEST_SUITE(hevc10d_444_close, TestSuiteEx<MFX_FOURCC_Y410>::RunTest, TestSuiteEx<MFX_FOURCC_Y410>::n_cases);

    TS_REG_TEST_SUITE(hevc12d_420_p016_close, TestSuiteEx<MFX_FOURCC_P016>::RunTest, TestSuiteEx<MFX_FOURCC_P016>::n_cases);
    TS_REG_TEST_SUITE(hevc12d_422_y216_close, TestSuiteEx<MFX_FOURCC_Y216>::RunTest, TestSuiteEx<MFX_FOURCC_Y216>::n_cases);
    TS_REG_TEST_SUITE(hevc12d_444_y416_close, TestSuiteEx<MFX_FOURCC_Y416>::RunTest, TestSuiteEx<MFX_FOURCC_Y416>::n_cases);

    TS_REG_TEST_SUITE(hevcd_420_nv12_scc_close,   (TestSuiteEx<MFX_FOURCC_NV12, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuiteEx<MFX_FOURCC_NV12, MFX_PROFILE_HEVC_SCC>::n_cases));
    TS_REG_TEST_SUITE(hevcd_444_ayuv_scc_close,   (TestSuiteEx<MFX_FOURCC_AYUV, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuiteEx<MFX_FOURCC_AYUV, MFX_PROFILE_HEVC_SCC>::n_cases));
    TS_REG_TEST_SUITE(hevc10d_420_p010_scc_close, (TestSuiteEx<MFX_FOURCC_P010, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuiteEx<MFX_FOURCC_P010, MFX_PROFILE_HEVC_SCC>::n_cases));
    TS_REG_TEST_SUITE(hevc10d_444_y410_scc_close, (TestSuiteEx<MFX_FOURCC_Y410, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuiteEx<MFX_FOURCC_Y410, MFX_PROFILE_HEVC_SCC>::n_cases));
};