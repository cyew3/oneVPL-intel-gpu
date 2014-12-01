#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_alloc.h"
#include "ts_decoder.h"

namespace ptir_time_stamps
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

class tCamFrameReader : public tsSurfaceProcessor
{
private:
    //tsRawReader*  m_raw_reader;
    std::string   m_fullname;
    std::string   m_filename;
    mfxFrameInfo  m_fi;
    mfxU32        m_n_frames;
    mfxU32        m_frame;
public:
    tCamFrameReader(const char* fullname, const char* filename, mfxFrameInfo fi, mfxU32 n_frames = 0xFFFFFFFF)
        : m_fullname(fullname)
        , m_filename(filename)
        , m_fi(fi)
        , m_n_frames(n_frames)
        , m_frame(0)
        //, m_raw_reader(0)
    {
    }
    ~tCamFrameReader()
    {
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        char fname[300];
#if defined(_WIN32) || defined(_WIN64)
#define ts_sprintf sprintf_s
#define ts_strcpy  strcpy_s
#define ts_strcat  strcat_s
#else
#define ts_sprintf sprintf
#define ts_strcpy  strcpy
#define ts_strcat  strcat
#endif
        ts_strcpy(fname,m_fullname.c_str());
        ts_strcat(fname,"/");
        ts_strcat(fname,m_filename.c_str());
        tsRawReader raw_reader(fname, m_fi, 1);
        return raw_reader.ProcessSurface(s);
    }
};

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP() {}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
private:
    static const char path[];
    static const mfxU32 max_num_ctrl     = 11;
    static const mfxU32 max_num_ctrl_par = 4;
    mfxSession m_session;
    mfxU32 m_repeat;

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32    stream_id;
        mfxU32    frames_to_process;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };

    struct stream_prop
    {
        std::string name;
        mfxU32    w;
        mfxU32    h;
        mfxU16    num_frames;
        mfxU32    format;
        mfxU16    frame_rate_n;
        mfxU16    frame_rate_d;
    };

    static const tc_struct test_case[];
    static const stream_prop stream[];

    // TODO: need to pick streams
    enum STREAM_ID
    {
        //   TRASH                      = 8
        iceage_720x576_982             = 0
    };

    enum CTRL_TYPE
    {
          STAGE     = 0xF0000000
        , MFXVPAR   = 1 <<  1
        , EXT_BUF   = 1 <<  2
    };

};

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::stream_prop TestSuite::stream[] =
{
    {/*iceage_720x480_982i  */ "YUV/iceage_720x576_982.yuv",  720, 576, 982, MFX_FOURCC_NV12, 50, 1},
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, iceage_720x576_982, 5,
       {{MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.CropH, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.PicStruct, {MFX_PICSTRUCT_FIELD_TFF}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.PicStruct, {MFX_PICSTRUCT_PROGRESSIVE}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, {30}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, {30}}}
    },
    {/*01*/ MFX_ERR_NONE, iceage_720x576_982, 5,
       {{MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.PicStruct, {MFX_PICSTRUCT_FIELD_TFF}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.PicStruct, {MFX_PICSTRUCT_PROGRESSIVE}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, {30}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, {60}}}
    },
    {/*02*/ MFX_ERR_NONE, iceage_720x576_982, 5,
       {{MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.CropH, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.PicStruct, {MFX_PICSTRUCT_FIELD_BFF}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.PicStruct, {MFX_PICSTRUCT_PROGRESSIVE}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, {30}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, {30}}}
    },
    {/*03*/ MFX_ERR_NONE, iceage_720x576_982, 5,
       {{MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, {720}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {576}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.PicStruct, {MFX_PICSTRUCT_FIELD_BFF}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.PicStruct, {MFX_PICSTRUCT_PROGRESSIVE}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, {30}},
        {MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, {60}}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto tc = test_case[id];

    const char* fullname_init = 0;
    const char* filename_init = 0;

    fullname_init = g_tsStreamPool.Get(stream[tc.stream_id].name);
    filename_init = stream[tc.stream_id].name.c_str();
    g_tsStreamPool.Reg();

    //MFXInit();
    MFXInit(MFX_IMPL_AUTO_ANY, m_pVersion, m_pSession);
    mfxIMPL impl;
    ::MFXQueryIMPL(*m_pSession, &impl);

    // always load plug-in
    mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
    m_session = tsSession::m_session;
    tsSession::Load(m_session, ptir, 1);

    for(mfxU32 i = 0; i < max_num_ctrl; i++)
    {
        if (tc.ctrl[i].type & MFXVPAR)
        {
            if(tc.ctrl[i].field)
            {
                tsStruct::set(&m_par, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
            }
        }
    }

    mfxHDL hdl;
    mfxHandleType type;
    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY));
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);
    m_spool_in.GetAllocator()->get_hdl(type, hdl);

    if (!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY))
    {
        SetHandle(m_session, type, hdl);
    }

    g_tsStatus.expect(tc.sts);
    Query();
    Init(m_session, m_pPar);
    mfxStatus sts = MFX_ERR_NONE;

    AllocSurfaces();
    m_pSurfIn  = m_pSurfPoolIn->GetSurface();
    m_pSurfOut = m_pSurfPoolOut->GetSurface();

    g_tsStreamPool.Get(filename_init);
    g_tsStreamPool.Reg();

    tsRawReader reader(filename_init, m_par.vpp.In, stream[tc.stream_id].num_frames);
    m_surf_in_processor = &reader;

    if(MFX_ERR_NONE == tc.sts)
    {
        ProcessFrames(tc.frames_to_process);
    }
    else
    {
        g_tsStatus.expect(tc.sts);
        RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);
        g_tsStatus.check();

        return 0;
    }

    g_tsStatus.expect(tc.sts);

    /////////
    Close();
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_time_stamps);
}