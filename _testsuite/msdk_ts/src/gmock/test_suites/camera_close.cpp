/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_vpp.h"
#include "ts_struct.h"

namespace camera_close
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('C','A','M','R')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus Init(mfxSession session, mfxVideoParam *par)
    {
        if(session && par)
        {
            if((MFX_FOURCC_A2RGB10 == par->vpp.In.FourCC) ||
               (MFX_FOURCC_ARGB16  == par->vpp.In.FourCC) ||
               (MFX_FOURCC_R16     == par->vpp.In.FourCC))
               par->vpp.In.BitDepthLuma = 10;
            if((MFX_FOURCC_A2RGB10 == par->vpp.Out.FourCC) ||
               (MFX_FOURCC_ARGB16  == par->vpp.Out.FourCC) ||
               (MFX_FOURCC_R16     == par->vpp.Out.FourCC))
               par->vpp.Out.BitDepthLuma = 10;
        }
        return tsVideoVPP::Init(session, par);
    }

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
        mfxU32 impl;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, 0, MFX_IMPL_VIA_D3D11},
    {/* 1*/ MFX_ERR_NONE, MFXCLOSE, MFX_IMPL_VIA_D3D11},
    {/* 2*/ MFX_ERR_NONE, 0, MFX_IMPL_VIA_D3D9},
    {/* 3*/ MFX_ERR_NONE, MFXCLOSE, MFX_IMPL_VIA_D3D9}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    m_impl = tc.impl;
    MFXInit();

    if(m_uid)
    {
        Load();
    }

    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);
    {   //Workaround for buggy QueryIOSurf
        QueryIOSurf();
        if(m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY )    m_request[0].Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
        if(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)    m_request[1].Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
        if(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY )     m_request[0].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        if(m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)     m_request[1].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    }

    AllocSurfaces();

    Init(m_session, m_pPar);

    mfxStatus sts = MFX_ERR_NONE;
    while (1)
    {
        sts = RunFrameVPPAsync();

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

TS_REG_TEST_SUITE_CLASS(camera_close);

}

