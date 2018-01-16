/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

File Name: vpp_mctf.cpp

\* ****************************************************************************** */

/*!
\file vpp_mctf.cpp
\brief Gmock test vpp_mctf

Description:
This suite tests VPP initialization\n
35 test cases with the following differences:
- Valid, invalid or already inited session\n
- Filters\n
- Video or system IOPattern\n
- Output status

Algorithm:
*/

//#include "ts_encoder.h"
//#include "mfxstructures.h"

#include "ts_struct.h"

namespace vpp_mctf
{

    class TestSuite
    {
    public:
        TestSuite() {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;
    private:
        tsExtBufType<mfxVideoParam> m_par;
        struct tc_struct
        {
            mfxStatus sts;
        };
        static const tc_struct test_case[];
    };
    const TestSuite::tc_struct TestSuite::test_case[] =
    {

        {/* 0*/ MFX_ERR_NONE },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        m_par.AddExtBuffer<mfxExtVppMctf>(0);

        mfxExtVppMctf *extBuffer = m_par;
        g_tsLog << "ExtBuffer dump:\n";
        g_tsLog << extBuffer;

        TS_END;

        return 0;

    }
    TS_REG_TEST_SUITE_CLASS(test_mfxExtVppMctf);
}
