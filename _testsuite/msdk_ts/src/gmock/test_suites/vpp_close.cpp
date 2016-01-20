#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_close
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP(){ }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        VALID = 1,
        INVALID
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
    {/* 0*/ MFX_ERR_NONE, VALID},
    {/* 1*/ MFX_ERR_INVALID_HANDLE, INVALID}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];

    if (tc.mode == VALID)
    {
        MFXInit();
        Init(m_session, m_pPar);
        SetHandle();
    }
    else
    {
        m_session = 0;
    }

    g_tsStatus.expect(tc.sts);
    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_close);

}
