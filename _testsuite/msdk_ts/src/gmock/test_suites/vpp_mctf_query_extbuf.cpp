/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.

File Name: vpp_mctf_query_extbuf.cpp
\* ****************************************************************************** */

/*!
\file vpp_mctf_query_extbuf.cpp
\brief Gmock test vpp_mctf_query_extbuf

Description:
This suite tests  VPP, MCTF\n

1 test case for check unsupported on Windows

*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"

#if !defined(_WIN32)
#include <cpuid.h>
#endif


/*! \brief Main test name space */
namespace vpp_mctf_qery_extbuf
{

    enum
    {
        MFX_PAR = 0,
    };

    //! \brief Enum with test modes
    enum MODE
    {
        MCTF_STANDARD
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! \brief Test mode
        MODE mode;
        //! \brief Query expected reurn status
        mfxStatus expected;

        /*! \brief Structure contains params for some fields of VPP */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        }set_par[MAX_NPARS];
        //! \brief Array for DoNotUse algs (if required)
        mfxU32 dnu[4];
    };

    //!\brief Main test class
    class TestSuite :tsVideoVPP
    {
    public:
        //! \brief A constructor 
        TestSuite() : tsVideoVPP(true) {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };


    const        tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MCTF_STANDARD, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR,  &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
            { MFX_PAR,  &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
        },{}
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        MFXInit();
        auto& tc = test_case[id];
        SETPARS(&m_par, MFX_PAR);
        mfxExtVPPDoNotUse* buf;

        mfxStatus adjusted_status = tc.expected;
        bool sse41_support = false;
#if defined(_WIN32) || defined(_WIN64)
        mfxI32 info[4], mask = (1 << 19);    // SSE41
        __cpuidex(info, 0x1, 0);
        sse41_support = ((info[2] & mask) == mask);
#else
        sse41_support = ((__builtin_cpu_supports("sse4.1")));
#endif
        bool bMCTFDoNotUse(false);
        switch (tc.mode) {
        case MODE::MCTF_STANDARD:
            buf = (mfxExtVPPDoNotUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);

            if (buf != nullptr) {
                buf->AlgList = new mfxU32[buf->NumAlg];
                memcpy(buf->AlgList, tc.dnu, sizeof(mfxU32)*buf->NumAlg);
                for (mfxU32 i = 0; i < buf->NumAlg; ++i)
                {
                    if (MFX_EXTBUFF_VPP_MCTF == buf->AlgList[i])
                    {
                        bMCTFDoNotUse = true;
                        break;
                    }
                }
            }

            break;
        default:
            break;
        }
        g_tsStatus.expect(adjusted_status);
        Query();

        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_mctf_query_extbuf);
}