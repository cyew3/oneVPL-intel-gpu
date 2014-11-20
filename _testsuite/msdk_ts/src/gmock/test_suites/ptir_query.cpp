#include "ts_vpp.h"
#include "ts_struct.h"

namespace ptir_query
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
        MFX_PAR = 1,
        EXT_DI,
    };

    enum
    {
        USE_EXT_BUF = 3,

        ALLOC_OPAQUE = 4,
        ALLOC_OPAQUE_LESS = 4+1,
        ALLOC_OPAQUE_MORE = 4+2
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        mfxU32 di_mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // unsupported DI modes
    {/*00*/ MFX_ERR_UNSUPPORTED, USE_EXT_BUF, 0, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB}}},
    {/*01*/ MFX_ERR_UNSUPPORTED, USE_EXT_BUF, 0, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED}}},

    // MFX_DEINTERLACING_AUTO_DOUBLE
    {/*02*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_AUTO_DOUBLE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    {/*03*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_AUTO_DOUBLE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},    // 0
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 60}}}, // 1
    {/*04*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_AUTO_DOUBLE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},   // 60
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 60}}},// 1

    // MFX_DEINTERLACING_AUTO_SINGLE
    {/*05*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_AUTO_SINGLE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    {/*06*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_AUTO_SINGLE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 30}}},
    {/*07*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_AUTO_SINGLE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 30}}},

    // MFX_DEINTERLACING_FULL_FR_OUT
    {/*08*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_FULL_FR_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN}}},
    {/*09*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_FULL_FR_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1}}},
    {/*10*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_FULL_FR_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1}}},

    // MFX_DEINTERLACING_HALF_FR_OUT
    {/*11*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_HALF_FR_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN}}},
    {/*12*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_HALF_FR_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1}}},
    {/*13*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_HALF_FR_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1}}},

    // MFX_DEINTERLACING_24FPS_OUT
    {/*14*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_24FPS_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN}}},
    {/*15*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_24FPS_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1}}},
    {/*16*/ MFX_ERR_UNSUPPORTED, 0, MFX_DEINTERLACING_24FPS_OUT, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

#define SETPARS(p, type)\
for(mfxU32 i = 0; i < n_par; i++) \
{ \
    if(tc.set_par[i].f && tc.set_par[i].ext_type == type) \
    { \
        tsStruct::set(p, *tc.set_par[i].f, tc.set_par[i].v); \
    } \
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    // always load plug-in
    mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
    tsSession::Load(m_session, ptir, 1);

    mfxExtVPPDeinterlacing di = {0};
    di.Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
    di.Header.BufferSz = sizeof(mfxExtVPPDeinterlacing);
    mfxExtBuffer* buf[1];
    buf[0] = (mfxExtBuffer*)&di;

    if (tc.di_mode)
    {
        m_par.vpp.In.PicStruct = 0;
        m_par.vpp.In.FrameRateExtN = 0;
        m_par.vpp.In.FrameRateExtD = 0;
        m_par.vpp.Out.FrameRateExtN = 0;
        m_par.vpp.Out.FrameRateExtD = 0;

        switch (tc.di_mode)
        {
        case MFX_DEINTERLACING_AUTO_DOUBLE:
            m_par.vpp.In.PicStruct = MFX_PICSTRUCT_UNKNOWN;
            m_par.vpp.In.FrameRateExtN = 0;
            m_par.vpp.In.FrameRateExtD = 1;
            m_par.vpp.Out.FrameRateExtN = 30;
            m_par.vpp.Out.FrameRateExtD = 1;
            break;

        case MFX_DEINTERLACING_AUTO_SINGLE:
            m_par.vpp.In.PicStruct = MFX_PICSTRUCT_UNKNOWN;
            m_par.vpp.In.FrameRateExtN = 0;
            m_par.vpp.In.FrameRateExtD = 1;
            m_par.vpp.Out.FrameRateExtN = 60;
            m_par.vpp.Out.FrameRateExtD = 1;
            break;

        case MFX_DEINTERLACING_FULL_FR_OUT:
            m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            m_par.vpp.In.FrameRateExtN = 30;
            m_par.vpp.In.FrameRateExtD = 1;
            m_par.vpp.Out.FrameRateExtN = 60;
            m_par.vpp.Out.FrameRateExtD = 1;
            break;

        case MFX_DEINTERLACING_HALF_FR_OUT:
            m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            m_par.vpp.In.FrameRateExtN = 30;
            m_par.vpp.In.FrameRateExtD = 1;
            m_par.vpp.Out.FrameRateExtN = 30;
            m_par.vpp.Out.FrameRateExtD = 1;
            break;

        case MFX_DEINTERLACING_24FPS_OUT:
            m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            m_par.vpp.In.FrameRateExtN = 30;
            m_par.vpp.In.FrameRateExtD = 1;
            m_par.vpp.Out.FrameRateExtN = 24;
            m_par.vpp.Out.FrameRateExtD = 1;
            break;

        case MFX_DEINTERLACING_FIXED_TELECINE_PATTERN:
            di.Mode = MFX_DEINTERLACING_FIXED_TELECINE_PATTERN;
            di.TelecinePattern = MFX_TELECINE_PATTERN_2332;
            m_par.ExtParam = buf;
            m_par.NumExtParam = 1;
            SETPARS(&di, EXT_DI);
            break;

        case MFX_DEINTERLACING_30FPS_OUT:
            di.Mode = MFX_DEINTERLACING_30FPS_OUT;
            m_par.ExtParam = buf;
            m_par.NumExtParam = 1;
            SETPARS(&di, EXT_DI);
            break;

        case MFX_DEINTERLACING_DETECT_INTERLACE:
            di.Mode = MFX_DEINTERLACING_DETECT_INTERLACE;
            m_par.ExtParam = buf;
            m_par.NumExtParam = 1;
            SETPARS(&di, EXT_DI);
            break;

        default:
            break;
        }
    }

    if (tc.mode & USE_EXT_BUF)
    {
        m_par.vpp.In.PicStruct = 0;
        m_par.vpp.In.FrameRateExtN = 0;
        m_par.vpp.In.FrameRateExtD = 0;
        m_par.vpp.Out.PicStruct = 0;
        m_par.vpp.Out.FrameRateExtN = 0;
        m_par.vpp.Out.FrameRateExtD = 0;

        m_par.ExtParam = buf;
        m_par.NumExtParam = 1;
        SETPARS(&di, EXT_DI);
    }

    // set up parameters for case
    SETPARS(m_pPar, MFX_PAR);

    bool isSW = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
    m_spool_in.UseDefaultAllocator(isSW);
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);

    mfxHDL hdl;
    mfxHandleType type;
    if (isSW)
    {
        if (!m_pVAHandle)
        {
            m_pVAHandle = new frame_allocator(
                    frame_allocator::HARDWARE,
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

    if (tc.mode & ALLOC_OPAQUE)
    {
        mfxExtOpaqueSurfaceAlloc& osa = m_par;
        QueryIOSurf();
        if (tc.mode & ALLOC_OPAQUE_LESS)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin - 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin - 1;
        }
        else if (tc.mode & ALLOC_OPAQUE_MORE)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin + 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin + 1;
        }
        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_spool_in.AllocOpaque(m_request[0], osa);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_spool_out.AllocOpaque(m_request[1], osa);
    }

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    Query(m_session, m_pPar, m_pPar);

    // check m_par
    for (mfxU32 i = 0; i < n_par; i++)
    {
        mfxU64 val = 0;
        if (tc.set_par[i].f && tc.set_par[i].ext_type == MFX_PAR)
        {
            val = tsStruct::get(m_pPar, *tc.set_par[i].f);
        }
        if ((tc.mode & USE_EXT_BUF) &&
            tc.set_par[i].f && tc.set_par[i].ext_type == EXT_DI)
        {
            val = tsStruct::get((mfxExtVPPDeinterlacing*)m_pPar->ExtParam[0], *tc.set_par[i].f);
        }
        if (val != 0)
        {
            g_tsLog << "ERROR: Field " << tc.set_par[i].f->name << " should be zero-ed (real is " << val << ")\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_query);

}

