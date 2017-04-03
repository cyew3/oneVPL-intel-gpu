/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: query_platform.cpp

\* ****************************************************************************** */
#include "ts_session.h"
#include "ts_struct.h"

namespace query_platform
{

mfxStatus CheckPlatform(mfxPlatform platform)
{
    switch (platform.CodeName)
    {
        case MFX_PLATFORM_SANDYBRIDGE:
            {
                return ((g_tsHWtype == MFX_HW_SNB) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_IVYBRIDGE:
            {
                return ((g_tsHWtype == MFX_HW_IVB) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_HASWELL:
            {
                return ((g_tsHWtype == MFX_HW_HSW) || (g_tsHWtype == MFX_HW_HSW_ULT) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_BAYTRAIL:
            {
                return ((g_tsHWtype == MFX_HW_VLV) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_BROADWELL:
            {
                return ((g_tsHWtype == MFX_HW_BDW) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_CHERRYTRAIL:
            {
                return ((g_tsHWtype == MFX_HW_CHV) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_SKYLAKE:
            {
                return ((g_tsHWtype == MFX_HW_SKL) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_APOLLOLAKE:
            {
                return ((g_tsHWtype == MFX_HW_APL) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_KABYLAKE:
            {
                return ((g_tsHWtype == MFX_HW_KBL) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_GEMINILAKE:
            {
                return ((g_tsHWtype == MFX_HW_GLK) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_CANNONLAKE:
            {
                return ((g_tsHWtype == MFX_HW_CNL) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_ICELAKE:
            {
                return ((g_tsHWtype == MFX_HW_ICL) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        case MFX_PLATFORM_TIGERLAKE:
            {
                return ((g_tsHWtype == MFX_HW_TGL) || (g_tsHWtype == MFX_HW_UNKNOWN)) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
            }
        default:
            {
                return MFX_ERR_UNKNOWN;
            }
    }
}

class TestSuite : tsSession
{
public:
    TestSuite() : tsSession() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 5;


    struct tc_struct
    {
        mfxStatus sts;
        mfxU16 exp_codename;
        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_INVALID_HANDLE, 0xff},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START
    const tc_struct& tc = test_case[id];

    //int
    tsSession session(g_tsImpl, g_tsVersion);
    session.MFXInit();

    //test call
    mfxPlatform platform;
    g_tsStatus.check(MFXVideoCORE_QueryPlatform(session.m_session, &platform));

    //check
    g_tsStatus.check(CheckPlatform(platform));

    TS_END
    return 0;
}

TS_REG_TEST_SUITE_CLASS(query_platform);

}
