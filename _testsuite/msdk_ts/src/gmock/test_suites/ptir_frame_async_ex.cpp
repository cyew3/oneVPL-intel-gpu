#include "ts_vpp.h"
#include "ts_struct.h"

namespace ptir_frame_async_ex
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('P','T','I','R')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 6;

    enum
    {
        ALLOC_OPAQUE = 1,
        UNALIGNED_W,
        UNALIGNED_H,
        ZERO_IN_W,
        ZERO_IN_H,
        ZERO_OUT_W,
        ZERO_OUT_H,
        NULL_SYNCP,
        NULL_SURF_WORK,
        SET_CROP_IN,
        SET_CROP_OUT
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE},

    // IOPattern cases
    {/*01*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*02*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    //{/*3*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    //{/*4*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    //{/*5*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    //{/*6*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    //{/*7*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    //{/*8*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    //{/*9*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},

    // unaligned W/H right before RunFrameAsync
    {/*03*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, UNALIGNED_H},
    {/*04*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, UNALIGNED_W},

    // crops are mandatory for processing, but for PTIR it is not supported
    {/*05*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, SET_CROP_IN,  {{&tsStruct::mfxFrameSurface1.Info.CropW, 320},
                                                             {&tsStruct::mfxFrameSurface1.Info.CropH, 240}}},
    {/*06*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, SET_CROP_OUT, {{&tsStruct::mfxFrameSurface1.Info.CropW, 320},
                                                             {&tsStruct::mfxFrameSurface1.Info.CropH, 240}}},

    // W/H are mandatory
    {/*07*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_IN_W},
    {/*08*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_IN_H},
    {/*09*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_OUT_W},
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_OUT_H},

    {/*11*/ MFX_ERR_NULL_PTR, NULL_SYNCP},
    {/*12*/ MFX_ERR_NULL_PTR, NULL_SURF_WORK},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
    tsSession::Load(m_session, ptir, 1);

    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f && SET_CROP_IN != tc.mode && SET_CROP_OUT != tc.mode)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    bool isSW = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
    m_spool_in.UseDefaultAllocator(isSW);
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);

    if (!m_is_handle_set)
    {
        mfxHDL hdl;
        mfxHandleType type;
        if (isSW)
        {
            if (!m_pVAHandle)
            {
                m_pVAHandle = new frame_allocator(
                        TS_HW_ALLOCATOR_TYPE,
                        frame_allocator::ALLOC_MAX,
                        frame_allocator::ENABLE_ALL,
                        frame_allocator::ALLOC_EMPTY);
            }
            m_pVAHandle->get_hdl(type, hdl);
        }
        else
        {
            m_spool_in.GetAllocator()->get_hdl(type, hdl);
        }
        SetHandle(m_session, type, hdl);
    }

    if (tc.mode == ALLOC_OPAQUE)
    {
        mfxExtOpaqueSurfaceAlloc& osa = m_par;
        QueryIOSurf();
        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_spool_in.AllocOpaque(m_request[0], osa);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_spool_out.AllocOpaque(m_request[1], osa);
    }

    AllocSurfaces();

    Init(m_session, m_pPar);

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    if (tc.mode > 1)
    {
        mfxU32 n = 10;
        mfxU32 processed = 0;
        mfxU32 submitted = 0;
        mfxStatus res = MFX_ERR_NONE;
        mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
        async = TS_MIN(n, async - 1);

        while(processed < n)
        {
            m_pSurfIn  = m_pSurfPoolIn->GetSurface();
            m_pSurfWork = m_pSurfPoolOut->GetSurface();
            if (tc.mode == UNALIGNED_H)
            {
                m_pSurfIn->Info.Height = 1080;
            }
            if (tc.mode == UNALIGNED_W)
            {
                m_pSurfIn->Info.Width = 1080;
            }

            if (tc.mode == ZERO_IN_H)
            {
                m_pSurfIn->Info.Height = 0;
            }
            if (tc.mode == ZERO_IN_W)
            {
                m_pSurfIn->Info.Width = 0;
            }
            if (tc.mode == ZERO_OUT_H)
            {
                m_pSurfWork->Info.Height = 0;
                //m_pSurfOut->Info.Height = 0;
            }
            if (tc.mode == ZERO_OUT_W)
            {
                m_pSurfWork->Info.Width = 0;
                //m_pSurfOut->Info.Width = 0;
            }
            if (tc.mode == NULL_SYNCP)
            {
                m_pSyncPoint = 0;
            }
            if (tc.mode == NULL_SURF_WORK)
            {
                m_pSurfWork = 0;
            }
            if (tc.mode == SET_CROP_IN)
            {
                for(mfxU32 i = 0; i < n_par; i++)
                {
                    if(tc.set_par[i].f)
                    {
                        tsStruct::set(m_pSurfIn, *tc.set_par[i].f, tc.set_par[i].v);
                    }
                }
            }
            if (tc.mode == SET_CROP_OUT)
            {
                for(mfxU32 i = 0; i < n_par; i++)
                {
                    if(tc.set_par[i].f)
                    {
                        tsStruct::set(m_pSurfIn, *tc.set_par[i].f, tc.set_par[i].v);
                    }
                }
            }

            res = RunFrameVPPAsyncEx(m_session, m_pSurfIn, m_pSurfWork, &m_pSurfOut, m_pSyncPoint);

            if(MFX_ERR_MORE_DATA == res)
            {
                if(!m_pSurfIn)
                {
                    if(submitted)
                    {
                        processed += submitted;

                        while(m_surf_out.size()) SyncOperation();
                    }
                    break;
                }
                continue;
            }

            if(MFX_ERR_MORE_SURFACE == res || res > 0)
            {
                continue;
            }

            if(res < 0)
            {
                g_tsStatus.check();
                break;
            }

            if(++submitted >= async)
            {
                while(m_surf_out.size()) SyncOperation();
                processed += submitted;
                submitted = 0;
                async = TS_MIN(async, (n - processed));
            }
        }
        g_tsLog << processed << " FRAMES PROCESSED\n";
    }
    else
    {
        ProcessFramesEx(10);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_frame_async_ex);

}

