#include "ts_decoder.h"
#include "ts_struct.h"

namespace screen_capture_close
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_CAPTURE, true, MFX_MAKEFOURCC('C','A','P','T')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFXCLOSE = 1,
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
    {/* 0*/ MFX_ERR_NONE, 0},
    {/* 1*/ MFX_ERR_NONE, MFXCLOSE}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    if (g_tsWinVersion >= MFX_WIN_VER_W10)
    {
        m_sw_fallback = true;
    }
    MFXInit();
    Load();

    AllocSurfaces();

    Init(m_session, m_pPar);

    mfxStatus sts = MFX_ERR_NONE;
    while (1)
    {
        sts = DecodeFrameAsync();

        if (g_tsStatus.get() == MFX_ERR_MORE_DATA ||
            g_tsStatus.get() == MFX_ERR_MORE_SURFACE)
            continue;
        else if (g_tsStatus.get() == MFX_ERR_NONE)
        {
            mfxSyncPoint* null_ptr = 0;
            EXPECT_NE(null_ptr, m_pSyncPoint);
            break;
        }
        else
        {
            g_tsStatus.check();
            break;
        }
    }

    // free surfaces locked by test
    for(std::map<mfxSyncPoint,mfxFrameSurface1*>::iterator it = m_surf_out.begin(); it != m_surf_out.end(); ++it)
    {
        if ((*it).second->Data.Locked)
            (*it).second->Data.Locked--; // access surface and decrease locked counter
    }

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    if (tc.mode == MFXCLOSE)
    {
        MFXClose();
        m_initialized = false; // calling Decoder's close in test destructor on closed session causes seg.fault
    }
    else
        Close();

    for(std::map<mfxSyncPoint,mfxFrameSurface1*>::iterator it = m_surf_out.begin(); it != m_surf_out.end(); ++it)
    {
        if (0 != (*it).second->Data.Locked)
        {
            g_tsLog << "ERROR: there is Locked OUT surface after Close()\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(screen_capture_close);

}
