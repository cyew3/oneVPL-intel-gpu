/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_vpp.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace ptir_get_video_param
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('P','T','I','R')), m_session(0), m_repeat(1) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 6;
    static const mfxU32 max_num_ctrl_par = 4;
    mfxSession m_session;
    mfxU32 m_repeat;

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32    stream_id;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };

    static const tc_struct test_case[];

    enum CTRL_TYPE
    {
          STAGE     = 0xFF000000
        , INIT      = 0x01000000
        , GETVP     = 0x10000000
        , SESSION   = 1 << 1
        , MFXVPAR   = 1 << 2
        , CLOSE_VPP = 1 << 3
        , REPEAT    = 1 << 4
        , ALLOCATOR = 1 << 5
        , EXT_BUF   = 1 << 6
        , NULLPTR   = 1 << 7
        , FAILED    = 1 << 8
        , WARNING   = 1 << 9
    };

    void apply_par(const tc_struct& p, mfxU32 stage, tsExtBufType<mfxVideoParam>*& pPar)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;

            if(stage != (c.type & STAGE))
                continue;

            switch(c.type & ~STAGE & ~FAILED & ~WARNING)
            {
            case SESSION   : base = (void**)&m_session;      break;
            case MFXVPAR   : base = (void**)&pPar;         break;
            case REPEAT    : base = (void**)&m_repeat;       break;
            case ALLOCATOR : m_spool_out.SetAllocator(
                                 new frame_allocator(
                                 (frame_allocator::AllocatorType)    c.par[0],
                                 (frame_allocator::AllocMode)        c.par[1],
                                 (frame_allocator::LockMode)         c.par[2],
                                 (frame_allocator::OpaqueAllocMode)  c.par[3]
                                 ),
                                 false
                             ); m_use_memid = true; break;
            case CLOSE_VPP : Close(); break;
            case EXT_BUF   : pPar->AddExtBuffer(c.par[0], c.par[1]); break;
            case NULLPTR   : pPar = 0; break;
            default: break;
            }

            if(base)
            {
                if(c.field)
                    tsStruct::set(*base, *c.field, c.par[0]);
                else
                    *base = (void*)c.par;
            }
        }
    }

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, {GETVP|REPEAT, 0, {50}}},
    {/*01*/ MFX_ERR_NONE, 0,
      {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
       {GETVP|REPEAT, 0, {50}}}
    },
    {/*02*/ MFX_ERR_NONE, 0,
      {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
       {GETVP|REPEAT, 0, {2}}}
    },
    {/*03*/ MFX_ERR_NONE, 0,
      {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
       {GETVP|REPEAT, 0, {2}}}
    },
    {/*04*/ MFX_ERR_INVALID_HANDLE, 0, {GETVP|SESSION}},
    {/*05*/ MFX_ERR_NOT_INITIALIZED,          0, {GETVP|CLOSE_VPP}},
    {/*06*/ MFX_ERR_NULL_PTR,                 0, {GETVP|NULLPTR}},
    {/*07*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, {MFX_CHROMAFORMAT_MONOCHROME}}},
    {/*08*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width, {720 + 31}}},
    {/*09*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height, {480 + 31}}},
    {/*10*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {100}}},
    {/*11*/ MFX_ERR_NONE,                     0, {INIT|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}}},
    {/*12*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {1}}},
    {/*13*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {2}}},
    {/*14*/ MFX_ERR_NONE, 0,
       {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {3840}},
          {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {3840}},
          {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {3840}},
          {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {3840}}  }
    },
    {/*15*/ MFX_ERR_NONE, 0,
       {  {GETVP|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {3840}},
          {GETVP|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {3840}},
          {GETVP|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Width,   {2048}},
          {GETVP|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height,  {2048}}  }
    },
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    // always load plug-in
    mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
    m_session = tsSession::m_session;
    tsSession::Load(m_session, ptir, 1);

    tsExtBufType<mfxVideoParam>* pParInit = &m_par;
    //m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
    apply_par(tc, INIT, pParInit);
    if(FAILED & tc.ctrl[0].type)
        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
    if(WARNING & tc.ctrl[0].type)
        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    Init(m_session, pParInit);

    if(tc.stream_id)
    {
        ProcessFrames(2);
    }

    tsExtBufType<mfxVideoParam> par_out;
    mfxVideoParam par_out_copy;
    tsExtBufType<mfxVideoParam>* pParOut = &par_out;
    apply_par(tc, GETVP, pParOut);
    par_out_copy = par_out;


    for(mfxU32 i = 0; i < m_repeat; i ++)
    {
        g_tsStatus.expect(tc.sts);
        GetVideoParam(m_session, pParOut);
    }

    if(m_initialized)
    {
        //Check that parameters are took place
        bool param_check_req = false;
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
            if(tc.ctrl[i].type & INIT) { param_check_req = true; break;}

        if(param_check_req)
        {
            tsExtBufType<mfxVideoParam> parOut;
            GetVideoParam(m_session, &parOut);
            for(mfxU32 i = 0; i < max_num_ctrl; i ++)
            {
                if((tc.ctrl[i].type & INIT) && (tc.ctrl[i].type & MFXVPAR) && !(tc.ctrl[i].type & WARNING))
                    tsStruct::check_eq(&parOut, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
                if((tc.ctrl[i].type & INIT) && (tc.ctrl[i].type & MFXVPAR) && (tc.ctrl[i].type & WARNING))
                    tsStruct::check_ne(&parOut, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
            }
        }
        Close();
    }

    ///////////////////////////////////////////////////////////////////////////
    //g_tsStatus.expect(tc.sts);
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_get_video_param);

}