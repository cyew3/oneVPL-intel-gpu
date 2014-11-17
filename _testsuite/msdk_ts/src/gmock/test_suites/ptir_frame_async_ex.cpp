#include "ts_vpp.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace ptir_frame_async_ex
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}

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
        struct 
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/* 0*/ MFX_ERR_NONE},

    // IOPattern cases
    {/*1*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*2*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*3*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*4*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*5*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*6*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*7*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*8*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*9*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},

    // unaligned W/H right before RunFrameAsync
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, UNALIGNED_H},
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, UNALIGNED_W},

    // crops are mandatory for processing, but for PTIR it is not supported
    {/*12*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, SET_CROP_IN,  {{&tsStruct::mfxFrameSurface1.Info.CropW, 320},
                                                             {&tsStruct::mfxFrameSurface1.Info.CropH, 240}}},
    {/*13*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, SET_CROP_OUT, {{&tsStruct::mfxFrameSurface1.Info.CropW, 320},
                                                             {&tsStruct::mfxFrameSurface1.Info.CropH, 240}}},
    
    // W/H are mandatory
    {/*14*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_IN_W},
    {/*15*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_IN_H},
    {/*16*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_OUT_W},
    {/*17*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ZERO_OUT_H},

    {/*18*/ MFX_ERR_NULL_PTR, NULL_SYNCP},
    {/*19*/ MFX_ERR_NULL_PTR, NULL_SURF_WORK},
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
    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f && SET_CROP_IN != tc.mode && SET_CROP_OUT != tc.mode)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    if(tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(this->m_par, tc.set_buf.buf, tc.set_buf.size);
    }

    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);

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

