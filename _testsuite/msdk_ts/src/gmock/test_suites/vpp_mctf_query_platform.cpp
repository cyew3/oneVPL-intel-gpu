/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2018 Intel Corporation. All Rights Reserved.

File Name: vpp_mctf_query_platform.cpp

\* ****************************************************************************** */
#include "ts_vpp.h"
#include "ts_struct.h"

namespace mctf_query_platform
{
    enum
    {
        MFX_PAR = 0,
    };

    enum MODE
    {
        // it only checks platform
        MCTF_CHECK_PLATFORM = 0,
        MCTF_CHECK_QUERY = 1
    };

    class TestSuite : tsVideoVPP
    {
    public:
        TestSuite() : tsVideoVPP(true) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
//        static const mfxU32 n_par = 5;
        static const mfxU32 n_codenames = 20;


        struct tc_struct
        {
            //! \brief Test mode
            mfxStatus expected_for_listed;
            mfxStatus expected_for_unlisted;
            // names of supported platforms from
            // mfxcommon.h, PlatformCodeName enum
            mfxU16 exp_codenames[n_codenames];
            struct f_pair
            {
                //! Number of the params set (if there is more than one in a test case)
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/* 0*/ MFX_ERR_NONE, MFX_ERR_UNSUPPORTED, { MFX_PLATFORM_BROADWELL, MFX_PLATFORM_SKYLAKE, MFX_PLATFORM_KABYLAKE, MFX_PLATFORM_CANNONLAKE, 0xFF },
            {
                { MFX_PAR,  &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
            }
        },

        // if no MCTF, then no reason to return MFX_ERR_UNSUPPORTED
        {/* 1*/ MFX_ERR_NONE, MFX_ERR_NONE,{ MFX_PLATFORM_BROADWELL, MFX_PLATFORM_SKYLAKE, MFX_PLATFORM_KABYLAKE, MFX_PLATFORM_CANNONLAKE, 0xFF },
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START
        const tc_struct& tc = test_case[id];

        //int
        MFXInit();
        SETPARS(&m_par, MFX_PAR);

        //test call
        mfxPlatform platform;
        g_tsStatus.check(MFXVideoCORE_QueryPlatform(m_session, &platform));
        mfxStatus status = MFX_ERR_NONE;

        mfxExtVppMctf* mctf_buf = (mfxExtVppMctf*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_MCTF);
        if (mctf_buf)
        {
            status = tc.expected_for_unlisted;
            for (mfxU32 i = 0; i < n_codenames; ++i)
            {
                if (platform.CodeName == tc.exp_codenames[i])
                {
                    status = tc.expected_for_listed;
                    break;
                }
            }
        }
        g_tsStatus.expect(status);
        //check
        Query();
        TS_END
            return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vpp_mctf_query_platform);

}
