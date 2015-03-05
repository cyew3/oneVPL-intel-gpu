#include "ts_decoder.h"
#include "ts_struct.h"

namespace screen_capture_decode_header
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_CAPTURE, true, MFX_MAKEFOURCC('C','A','P','T')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_UNSUPPORTED, 0}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    Load();

    g_tsStatus.expect(tc.sts);

    DecodeHeader(m_session, &m_bitstream, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(screen_capture_decode_header);

}