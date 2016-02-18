#include "ts_decoder.h"
#include "ts_struct.h"

namespace mpeg2d_no_i_frames
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;


private:

    struct tc_struct
    {
        std::string desc;
        std::string stream;
        mfxU32 nframes;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {"Issue: MPEG2 decoder skips P frames if no I frame at the beginning", "forBehaviorTest/customer_issues/mpeg2_no_i_frames.mpg", 1586}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    g_tsLog << "Test description: " << tc.desc << "\n";

    MFXInit();

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    DecodeFrames(tc.nframes, true);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2d_no_i_frames);
}
