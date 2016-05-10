/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace avcd_get_decode_stat
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC) { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    private:

    enum
    {
        MFX_PAR = 1,
        NULL_SESSION,
        SESSION_NOT_INITIALIZED,
        NULL_STAT,
        FAILED_IN_INIT,
        CHECK,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxF32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, CHECK},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NOT_INITIALIZED, SESSION_NOT_INITIALIZED},
    {/*03*/ MFX_ERR_NULL_PTR, NULL_STAT},
    {/*04*/ MFX_ERR_NOT_INITIALIZED, FAILED_IN_INIT,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];

    MFXInit();
    if(m_uid)
        Load();

    std::string stream = "forBehaviorTest/foreman_cif.h264";

    const char* sname = g_tsStreamPool.Get(stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1024);
    m_bs_processor = &reader;

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

    mfxSession m_session_tmp = m_session;

    mfxU32 n = 1;
    if (tc.mode == CHECK) n = 10;

    if (tc.mode == NULL_SESSION)
        m_session = 0;
    else
    {
        DecodeHeader();

        SETPARS(m_pPar, MFX_PAR);

        if (tc.mode == FAILED_IN_INIT)
        {
            g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
            Init(m_session, m_pPar);
        }
        else
        {
            Init();
            DecodeFrames(n);
        }

    }

    if (tc.mode == NULL_STAT)
        m_pStat = 0;
    else if (tc.mode == SESSION_NOT_INITIALIZED)
        Close();

    g_tsStatus.expect(tc.sts);
    GetDecodeStat();

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;
    else if (tc.mode == CHECK)
        EXPECT_NE(m_pStat->NumFrame, 0u) << "ERROR: Num decoded frames in decode stat = 0 \n";

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_get_decode_stat);

}

// Legacy test: behavior_h264_decode_suite, b_AVC_DECODE_GetDecodeStat, b_AVC_DECODE_GetDecodeStat_Check