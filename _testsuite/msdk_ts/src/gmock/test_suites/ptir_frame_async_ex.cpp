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
    enum
    {
        MFX_PAR = 1,
        EXT_DI,
        FRAME_SURFACE,
    };

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
        SET_CROP_OUT,
        USE_EXT_BUF
    };

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
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE},

    // IOPattern cases
    {/*01*/ MFX_ERR_NONE, 0, {MFX_PAR,&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*02*/ MFX_ERR_NONE, 0, {MFX_PAR,&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},

    // unaligned W/H right before RunFrameAsync
    {/*03*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, UNALIGNED_H},
    {/*04*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, UNALIGNED_W},

    // crops are mandatory for processing, but for PTIR it is not supported
    {/*05*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, SET_CROP_IN,  {{FRAME_SURFACE,&tsStruct::mfxFrameSurface1.Info.CropW, 320},
                                                             {FRAME_SURFACE,&tsStruct::mfxFrameSurface1.Info.CropH, 240}}},
    {/*06*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, SET_CROP_OUT, {{FRAME_SURFACE,&tsStruct::mfxFrameSurface1.Info.CropW, 320},
                                                             {FRAME_SURFACE,&tsStruct::mfxFrameSurface1.Info.CropH, 240}}},

    // W/H are mandatory
    {/*07*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_IN_W},
    {/*08*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_IN_H},
    {/*09*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_OUT_W},
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_OUT_H},

    {/*11*/ MFX_ERR_NULL_PTR, NULL_SYNCP},
    {/*12*/ MFX_ERR_NULL_PTR, NULL_SURF_WORK},

    // MFX_DEINTERLACING_30FPS_OUT
    {/*13*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.In.Width, 704},
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.Out.Width, 704},
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.In.CropW, 704},
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
        {EXT_DI,&tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_30FPS_OUT}}},

    // MFX_DEINTERLACING_DETECT_INTERLACE
    {/*14*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.In.Width, 704},
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.Out.Width, 704},
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.In.CropW, 704},
        {MFX_PAR,&tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxU32 expected_ps[10] = {MFX_PICSTRUCT_PROGRESSIVE, MFX_PICSTRUCT_FIELD_TFF, MFX_PICSTRUCT_FIELD_TFF, MFX_PICSTRUCT_FIELD_TFF, MFX_PICSTRUCT_FIELD_TFF,
        MFX_PICSTRUCT_PROGRESSIVE, MFX_PICSTRUCT_FIELD_TFF, MFX_PICSTRUCT_FIELD_TFF, MFX_PICSTRUCT_FIELD_TFF, MFX_PICSTRUCT_FIELD_TFF};

    std::vector<mfxExtBuffer*> buffs;

    MFXInit();

    mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
    tsSession::Load(m_session, ptir, 1);

    if (tc.mode == USE_EXT_BUF)
    {
        m_par.vpp.In.PicStruct = 0;
        m_par.vpp.In.FrameRateExtN = 0;
        m_par.vpp.In.FrameRateExtD = 0;
        m_par.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.vpp.Out.FrameRateExtN = 0;
        m_par.vpp.Out.FrameRateExtD = 0;

        mfxExtVPPDeinterlacing di = {0};
        di.Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
        di.Header.BufferSz = sizeof(mfxExtVPPDeinterlacing);
        SETPARS(&di, EXT_DI);
        buffs.push_back((mfxExtBuffer*)&di);
    }

    // set up parameters for case
    SETPARS(m_pPar, MFX_PAR);

    // adding buffers
    if (buffs.size())
    {
        m_par.NumExtParam = (mfxU16)buffs.size();
        m_par.ExtParam = &buffs[0];
    }

    tsRawReader stream(g_tsStreamPool.Get("PTIR/ktsf_704x480_300_30.yuv"), m_pPar->vpp.In);
    g_tsStreamPool.Reg();
    if (tc.mode == USE_EXT_BUF)
        m_surf_in_processor = &stream;

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
            SETPARS(m_pSurfIn, FRAME_SURFACE);

            if(m_surf_in_processor)
            {
                m_pSurfIn = m_surf_in_processor->ProcessSurface(m_pSurfIn, m_pFrameAllocator);
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

            if (tc.mode == USE_EXT_BUF)
            {
                if (m_pSurfIn->Info.PicStruct != expected_ps[processed])
                {
                    g_tsLog << "ERROR: Frame#" << processed << " has incorrect PicStruct = " << m_pSurfIn->Info.PicStruct
                            << " (expected = " << expected_ps[processed] << ")\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }
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

