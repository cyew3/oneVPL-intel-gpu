#include "ts_encoder.h"
#include "ts_struct.h"

namespace fei_encode_close
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true) {}
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

    MFXInit();

    bool hw_surf = m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;

    SetFrameAllocator(m_session, m_pVAHandle);
    /*!!! FEI work in Sync mode only!
     * this limitation should be removed from future*/
    m_par.AsyncDepth = 1;

    Init(m_session, m_pPar);

    AllocSurfaces();
    AllocBitstream();

    mfxStatus sts = MFX_ERR_NONE;
    while (1)
    {
        sts = EncodeFrameAsync();

        if (g_tsStatus.get() == MFX_ERR_MORE_DATA ||
            g_tsStatus.get() == MFX_ERR_MORE_SURFACE)
            continue;
        else if (g_tsStatus.get() == MFX_ERR_NONE)
        {
            int syncPoint = 0xDEADBEAF;
            if (0 == m_pSyncPoint)
                EXPECT_EQ(0, syncPoint);
            SyncOperation();
            break;
        }
        else
        {
            g_tsStatus.check();
            break;
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    if (tc.mode == MFXCLOSE)
    {
        MFXClose();
        m_initialized = false;
    }
    else
        Close();

    /*for (mfxU32 i = 0; i < m_pSurf)
    {
        EXPECT_EQ(0, m_pSurfPoolIn->GetSurface(i)->Data.Locked);
    }
    for (mfxU32 i = 0; i < m_pSurfPoolOut->PoolSize(); i++)
    {
        EXPECT_EQ(0, m_pSurfPoolOut->GetSurface(i)->Data.Locked);
    }*/


    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_close);

}

