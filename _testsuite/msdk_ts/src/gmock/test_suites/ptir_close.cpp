#include "ts_vpp.h"
#include "ts_struct.h"

namespace ptir_close
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 6;

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

    mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
    tsSession::Load(m_session, ptir, 1);

    m_pPar->vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;

    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);
    AllocSurfaces();

    Init(m_session, m_pPar);

    mfxStatus sts = MFX_ERR_NONE;
    while (1)
    {
        sts = RunFrameVPPAsyncEx();

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

    //free surfaces locked by test
    for(std::map<mfxSyncPoint,mfxFrameSurface1*>::iterator it = m_surf_out.begin(); it != m_surf_out.end(); ++it) //iterate through output surfaces map
    {
        (*it).second->Data.Locked --; //access surface and decrease locked counter
    }
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    if (tc.mode == MFXCLOSE)
    {
        MFXClose();
        m_initialized = false; //calling VPP close in test destructor on closed session causes seg.fault
    }
    else
        Close();

    for (mfxU32 i = 0; i < m_pSurfPoolIn->PoolSize(); i++)
    {
        EXPECT_EQ(0, m_pSurfPoolIn->GetSurface(i)->Data.Locked);
    }
    for (mfxU32 i = 0; i < m_pSurfPoolOut->PoolSize(); i++)
    {
        EXPECT_EQ(0, m_pSurfPoolOut->GetSurface(i)->Data.Locked);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_close);

}

