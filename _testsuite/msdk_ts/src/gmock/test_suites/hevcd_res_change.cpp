#include "ts_decoder.h"

namespace hevcd_res_change
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 3;

    struct tc_struct
    {
        const std::string stream;
        mfxU32 n_frames;
        mfxStatus sts;
    };

    static const tc_struct test_case[][max_num_ctrl];
};

const TestSuite::tc_struct TestSuite::test_case[][max_num_ctrl] = 
{
    {/* 0*/ {"foster_720x576_dpb6.h265"},       {"news_352x288_dpb6.h265"},         {}},
    {/* 1*/ {"news_352x288_dpb6.h265"},         {"foster_720x576_dpb6.h265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}, {}},
    {/* 2*/ {"foster_720x576_dpb6.h265", 5},    {"news_352x288_dpb6.h265", 5},      {}},
    {/* 3*/ {"foster_720x576_dpb6.h265"},       {"news_352x288_dpb6.h265"},         {"matrix_1920x1088_dpb6.h265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 4*/ {"matrix_1920x1088_dpb6.h265"},     {"foster_720x576_dpb6.h265"},       {"news_352x288_dpb6.h265"}},
    {/* 5*/ {"matrix_1920x1088_dpb6.h265", 5},  {"foster_720x576_dpb6.h265", 5},    {"news_352x288_dpb6.h265", 5}},
    {/* 6*/ {"matrix_1920x1088_dpb6.h265"},     {"news_352x288_dpb6.h265"},         {"foster_720x576_dpb6.h265"}},
    {/* 7*/ {"matrix_1920x1088_dpb6.h265", 5},  {"news_352x288_dpb6.h265", 5},      {"foster_720x576_dpb6.h265", 5}},
    {/* 8*/ {"matrix_1920x1088_dpb6.h265", 5},  {"foster_720x576_dpb6.h265", 5},    {"matrix_1920x1088_dpb6.h265", 5}},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct[max_num_ctrl]);

const char* stream_path = "forBehaviorTest/dif_resol/";

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tcs = test_case[id];
    for (auto& tc : tcs) 
        g_tsStreamPool.Get(tc.stream, stream_path);

    g_tsStreamPool.Reg();
    m_use_memid = true;

    for (auto& tc : tcs) 
    {
        if("" == tc.stream)
            break;

        tsBitstreamReader r(g_tsStreamPool.Get(tc.stream), 100000);
        m_bs_processor = &r;
        
        m_bitstream.DataLength = 0;
        DecodeHeader();

        if(m_initialized)
        {
            mfxU32 nSurfPreReset = GetAllocator()->cnt_surf();

            g_tsStatus.expect(tc.sts);
            Reset();

            EXPECT_EQ(nSurfPreReset, GetAllocator()->cnt_surf()) << "expected no surface allocation at Reset";

            if(g_tsStatus.get() < 0)
                break;
        }
        else
        {
            SetPar4_DecodeFrameAsync();
        }

        DecodeFrames(tc.n_frames);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevcd_res_change);
}