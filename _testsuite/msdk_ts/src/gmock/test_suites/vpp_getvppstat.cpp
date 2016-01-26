#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_getvppstat
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    enum
    {
        MFX_PAR = 1,
        NULL_PAR,
        NULL_SESSION,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NULL_PTR, NULL_PAR },
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
    {/*02*/ MFX_ERR_NONE, 0 },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.AsyncDepth = 1;

    SetHandle();

    Init(m_session, m_pPar);

    mfxSession m_session_tmp = m_session;

    if (tc.mode == NULL_PAR)
        m_pStat = 0;
    else if (tc.mode == NULL_SESSION)
        m_session = 0;

    g_tsStatus.expect(tc.sts);
    GetVPPStat(m_session, m_pStat);
    g_tsStatus.expect(MFX_ERR_NONE);

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_getvppstat);

}
