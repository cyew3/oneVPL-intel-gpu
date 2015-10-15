#include "ts_fei.h"
#include "ts_struct.h"

namespace preenc_init
{

class TestSuite : tsPreENC
{
public:
    TestSuite() : tsPreENC() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 6;

    enum
    {
        NULL_SESSION = 1,
        NULL_PARAMS
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
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
    //{/* 0*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    //{/* 1*/ MFX_ERR_NONE, NULL_PARAMS},
    {/* 2*/ MFX_ERR_NONE},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if (tc.mode != NULL_SESSION)
    {
        MFXInit();
    }

    bool hw_surf = m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;
    if (hw_surf)
        SetFrameAllocator(m_session, m_pVAHandle);

    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(preenc_init);

}

