/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2020 Intel Corporation. All Rights Reserved.

File Name: hevce_load.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_load
{
    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        {

        }
        ~TestSuite() {}

        struct tc_struct
        {
            mfxStatus sts;
        };

        int RunTest(unsigned int id);

    private:

        static const tc_struct test_case[];
    };


    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        mfxStatus sts;
        bool errorExpected = false;
        const tc_struct& tc = test_case[id];

        MFXInit();

        if ((g_tsHWtype < MFX_HW_SKL) &&
            (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            Query();
            return 0;
        }

        if ((0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data))        ||
             0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_DP_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_DP_GACC.Data))) &&
             g_tsHWtype >= MFX_HW_TGL)
        {
            // We are trying to initialize HEVCe_GACC on Gen12+ platform, it is depricated - expect NOT_FOUND error in this case
            g_tsStatus.expect(MFX_ERR_NOT_FOUND);
            errorExpected = true;
        }
        else
        {
            g_tsStatus.expect(tc.sts);
        }

        sts = Load();

        if (errorExpected && sts == MFX_ERR_NOT_FOUND)
        {
            g_tsStatus.expect(MFX_ERR_UNKNOWN); //to prevent test failure on Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_load);
};
