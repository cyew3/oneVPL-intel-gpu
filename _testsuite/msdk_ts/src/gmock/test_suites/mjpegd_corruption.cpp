#include "ts_decoder.h"
#include "ts_struct.h"

namespace mjpegd_corruption
{

class TestSuite : tsVideoDecoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_JPEG)
                , found_corruption(false)
    {
        m_surf_processor = this;
    }

    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
    bool found_corruption;

private:

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        std::string stream;
        mfxU8 nframes;
    };

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (0 != s.Data.Corrupted)
        {
            found_corruption = true;
        }
        return MFX_ERR_NONE;
    }

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, 0, {}, "forBehaviorTest/corrupted/foreman_cif_5_3.mjpeg", 4},
    {/* 1*/ MFX_ERR_NONE, 0, {}, "forBehaviorTest/corrupted/logitech_webcam_640x480_30fps_6_4.mjpeg", 5}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    std::string file = g_tsStreamPool.Get(tc.stream.c_str());
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(file.c_str(), 1024*1024*1024);
    m_bs_processor = &reader;

    mfxStatus sts = MFX_ERR_NONE;
    sts = MFXInit();

    sts = Init();

    sts = QueryIOSurf();

    sts = AllocSurfaces();

    sts = GetVideoParam();

    sts = DecodeFrames(tc.nframes);

    if (!found_corruption)
    {
        g_tsLog << "ERROR: Decoder didn't filled ppSurfOut.mfxFrameData.Corrupted for corrupted stream\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mjpegd_corruption);
};
