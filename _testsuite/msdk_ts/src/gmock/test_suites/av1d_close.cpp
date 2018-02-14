/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace av1_close
{
    class TestSuite : public tsVideoDecoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoDecoder(MFX_CODEC_AV1)
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

        //set default param
        g_tsStreamPool.Reg();
        tsBitstreamReaderIVF reader(sname, 1000000);
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
    { return "conformance/av1/from_fulsim/akiyo0_176x144_8b_420_LowLatency_qp12.ivf"; }

    /* 10 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
    { return "conformance/av1/from_fulsim/rain2_640x360_10b_420_LowLatency_qp12.ivf"; }

    template <unsigned fourcc>
    struct TestSuiteEx
        : public TestSuite
    {
        static
        int RunTest(unsigned int id)
        {
            const char* sname =
                g_tsStreamPool.Get(query_stream(id, std::integral_constant<unsigned, fourcc>{}));
            g_tsStreamPool.Reg();

            TestSuite suite;
            return suite.RunTest(id, sname);
        }
    };

    TS_REG_TEST_SUITE(av1d_8b_420_nv12_close,  TestSuiteEx<MFX_FOURCC_NV12>::RunTest, TestSuiteEx<MFX_FOURCC_NV12>::n_cases);
    TS_REG_TEST_SUITE(av1d_10b_420_p010_close, TestSuiteEx<MFX_FOURCC_P010>::RunTest, TestSuiteEx<MFX_FOURCC_P010>::n_cases);
};