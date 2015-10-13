#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace vpp_frc_interp
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

    enum
    {
        INIT = 1,
        QUERY = 1 << 2,
        RESET = 1 << 3,
        QUERY_IOSURF = 1 << 4
    };

    enum
    {
        NULL_PAR = 1 << 5,
        DEFAULT,
        MFX_PAR,
    };

    enum
    {
        HW = 0,
        SW = 1
    };


    struct tc_struct
    {
        mfxStatus sts[2]; // field sts is array of 2 elements,
                          // where first one - status for HW, second one - status for SW
        mfxU32 mode;
        struct
        {
            mfxU32 ext_type;
            callback func;
            mfxU32 buf;
            mfxU32 size;
            mfxU32 val;
        } set_buf[MAX_NPARS];
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

    void AttachBuffers(const tc_struct& tc, const mfxU32 mode,
                       tsExtBufType<mfxVideoParam> &par_in, tsExtBufType<mfxVideoParam> &par_out)
    {
        // remove old buffers
        if (par_in.NumExtParam)
        {
            for (mfxU32 i = 0; i < par_in.NumExtParam; i++)
                par_in.RemoveExtBuffer(par_in.ExtParam[i]->BufferId);
        }
        if (par_out.NumExtParam)
        {
            for (mfxU32 i = 0; i < par_out.NumExtParam; i++)
                par_out.RemoveExtBuffer(par_out.ExtParam[i]->BufferId);
        }

        for (mfxU32 i = 0; i < MAX_NPARS; i++)
        {
            // set ExtBuffer
            if ((tc.mode & mode) && (tc.set_buf[i].ext_type == mode) || (tc.set_buf[i].ext_type == DEFAULT))
            {
                if (tc.set_buf[i].func == NULL)
                    break;
                (*tc.set_buf[i].func)(par_in, tc.set_buf[i].buf, tc.set_buf[i].size);
                (*tc.set_buf[i].func)(par_out, tc.set_buf[i].buf, tc.set_buf[i].size);

                if (par_in.ExtParam[par_in.NumExtParam - 1]->BufferId == MFX_EXTBUFF_VPP_DOUSE)
                {
                    mfxExtVPPDoUse *in = (mfxExtVPPDoUse*)par_in.ExtParam[par_in.NumExtParam - 1];
                    mfxExtVPPDoUse *out = (mfxExtVPPDoUse*)par_out.ExtParam[par_out.NumExtParam - 1];
                    // delete all filters from DoUse if exists
                    if (in->AlgList != NULL)
                    {
                        in->NumAlg = 0;
                        delete in->AlgList;
                    }
                    in->NumAlg = 1;
                    in->AlgList = new mfxU32 [in->NumAlg];
                    in->AlgList[0] = tc.set_buf[i].val;
                    if (out->AlgList != NULL)
                    {
                        out->NumAlg = 0;
                        delete in->AlgList;
                    }
                    out->NumAlg = 1;
                    out->AlgList = new mfxU32 [out->NumAlg];
                    out->AlgList[0] = 0;
                }
                else if (par_in.ExtParam[par_in.NumExtParam - 1]->BufferId == MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION)
                {
                        mfxExtVPPFrameRateConversion *in = (mfxExtVPPFrameRateConversion*)par_in.ExtParam[par_in.NumExtParam - 1];
                        in->Algorithm = tc.set_buf[i].val;
                }
            }
        }
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // Init, Reset, and Query with filter enabled by FRC buffer with mode = 4
    // SW doesn't support interpolated FRC (Algorithm = 4)
    { /*00*/ {MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM}, INIT,
                            {INIT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4}},

    { /*01*/ {MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM}, RESET,
                            {RESET, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4}},

    { /*02*/ {MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM}, QUERY|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4}},

    { /*03*/ {MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM}, QUERY,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4}},

    { /*04*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY_IOSURF|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4}},

    // Init, Reset, and Query with filter enabled by VPPDoUse buffer
    // status for SW equals status for HW with filter enabled by VPPDoUse buffer
    { /*05*/ {MFX_ERR_NONE, MFX_ERR_NONE}, INIT,
                            {INIT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}},

    { /*06*/ {MFX_ERR_NONE, MFX_ERR_NONE}, RESET,
                            {RESET, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}},

    { /*07*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}},

    { /*08*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}},

    { /*09*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY_IOSURF|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}},

    // RGB4 on input for HW, RGB4 on input and output for SW. MFX_FRCALGM_FRAME_INTERPOLATION is currently not supported.
    // Library switches to MFX_FRCALGM_PRESERVE_TIMESTAMP and returnes MFX_ERR_NONE
    { /*10*/ {MFX_ERR_NONE, MFX_WRN_FILTER_SKIPPED}, INIT,
                            {INIT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*11*/ {MFX_ERR_NONE, MFX_WRN_FILTER_SKIPPED}, RESET,
                            {RESET, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*12*/ {MFX_ERR_NONE, MFX_WRN_FILTER_SKIPPED}, QUERY|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*13*/ {MFX_ERR_NONE, MFX_WRN_FILTER_SKIPPED}, QUERY,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*14*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY_IOSURF|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    // RGB4 on input for HW, RGB4 on input and output for SW. Filter by VPPDoUse buffer
    // possibly need to correct expected status for SW: RGB4->RGB4 - resize only, library returns MFX_FILTER_SKIPPED
    { /*15*/ {MFX_ERR_NONE, MFX_ERR_NONE}, INIT,
                            {INIT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*16*/ {MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM}, RESET,
                            {RESET, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*17*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*18*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    { /*19*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY_IOSURF|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION},
                            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},

    //Init, Reset, and Query with filter enabled by FRC buffer with bad mode
    { /*20*/ {MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM}, INIT,
                            {INIT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 66}},

    { /*21*/ {MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM}, RESET,
                            {RESET, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 66}},

    { /*22*/ {MFX_ERR_UNSUPPORTED, MFX_ERR_UNSUPPORTED}, QUERY|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 66}},

    { /*23*/ {MFX_ERR_UNSUPPORTED, MFX_ERR_UNSUPPORTED}, QUERY,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 66}},

    { /*24*/ {MFX_ERR_NONE, MFX_ERR_NONE}, QUERY_IOSURF|NULL_PAR,
                            {DEFAULT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 66}},

    // Init by VPPDoUse and Reset by FRC buffer and vice versa
    // SW return ERR_NONE if filter by VPPDoUse buffer
    // and WRN_INCOMPATIBLE_VIDEO_PARAM - filter by FRC buffer
    { /*25*/ {MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM}, INIT|RESET, {
                            {INIT, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4},
                            {RESET, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}}
    },
    { /*26*/ {MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM}, INIT|RESET, {
                            {INIT, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION},
                            {RESET, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion), 4}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    // set required parameters
    m_par.vpp.In.FourCC = MFX_FOURCC_NV12;
    m_par.vpp.Out.FourCC = MFX_FOURCC_NV12;
    m_par.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_par.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_par.vpp.In.FrameRateExtN = 30;
    m_par.vpp.In.FrameRateExtD = 1;
    m_par.vpp.Out.FrameRateExtN = 60;
    m_par.vpp.Out.FrameRateExtD = 1;
    m_par.vpp.In.Width = 720;
    m_par.vpp.In.Height = 480;
    m_par.vpp.Out.Width = 720;
    m_par.vpp.Out.Height = 480;
    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();
    AllocSurfaces();

    tsExtBufType<mfxVideoParam> par_out;
    memcpy(&par_out, m_pPar, sizeof(mfxVideoParam));
    if (tc.mode & NULL_PAR)
        // check that Query fills output structure
        memset(&par_out, 0, sizeof(mfxVideoParam));

    if (tc.mode & QUERY)
    {
        AttachBuffers(tc, DEFAULT, m_par, par_out);
        SETPARS(m_pPar, MFX_PAR);
        // set status for HW or SW implementation
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            m_par.vpp.Out.FourCC = m_par.vpp.In.FourCC;
            g_tsStatus.expect(tc.sts[SW]);
        }
        else
            g_tsStatus.expect(tc.sts[HW]);

        Query(m_session, m_pPar, &par_out);
    } else if (tc.mode & QUERY_IOSURF)
    {
        AttachBuffers(tc, DEFAULT, m_par, par_out);
        SETPARS(m_pPar, MFX_PAR);
         // set status for HW or SW implementation
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            m_par.vpp.Out.FourCC = m_par.vpp.In.FourCC;
            g_tsStatus.expect(tc.sts[SW]);
        }
        else
            g_tsStatus.expect(tc.sts[HW]);

        QueryIOSurf();
    }  else if (tc.mode == (INIT|RESET)) {
        Query(m_session, m_pPar, &par_out);

        AttachBuffers(tc, INIT, m_par, par_out);
        SETPARS(m_pPar, MFX_PAR);
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            // check last ext buffer
            if (m_par.ExtParam[m_par.NumExtParam - 1]->BufferId == MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION)
                g_tsStatus.expect(tc.sts[SW]);
            else
                g_tsStatus.expect(MFX_ERR_NONE);
        }
        else
            g_tsStatus.expect(tc.sts[HW]);

        Init();

        AttachBuffers(tc, RESET, m_par, par_out);
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            // check last ext buffer
            if (m_par.ExtParam[m_par.NumExtParam - 1]->BufferId == MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION)
                g_tsStatus.expect(tc.sts[SW]);
            else
                g_tsStatus.expect(MFX_ERR_NONE);
        }
        else
            g_tsStatus.expect(tc.sts[HW]);

        Reset();
    } else if (tc.mode & INIT)
    {
        Query(m_session, m_pPar, &par_out);

        AttachBuffers(tc, INIT, m_par, par_out);
        SETPARS(m_pPar, MFX_PAR);
        // set status for HW or SW implementation
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            m_par.vpp.Out.FourCC = m_par.vpp.In.FourCC;
            g_tsStatus.expect(tc.sts[SW]);
        }
        else
            g_tsStatus.expect(tc.sts[HW]);

        Init();
    } else if (tc.mode & RESET)
    {
        Query(m_session, m_pPar, &par_out);

        SETPARS(&par_out, MFX_PAR);
        m_par.vpp.In.FourCC  = par_out.vpp.In.FourCC;
        m_par.vpp.Out.FourCC = par_out.vpp.Out.FourCC;
        Init();

        AttachBuffers(tc, RESET, m_par, par_out);
        SETPARS(m_pPar, MFX_PAR);
        // set status for HW or SW implementation
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            m_par.vpp.Out.FourCC = m_par.vpp.In.FourCC;
            g_tsStatus.expect(tc.sts[SW]);
        }
        else
            g_tsStatus.expect(tc.sts[HW]);

        Reset();
    } else
    {
        g_tsLog << "ERROR: No mode for case #" << id << "\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_frc_interp);

}

