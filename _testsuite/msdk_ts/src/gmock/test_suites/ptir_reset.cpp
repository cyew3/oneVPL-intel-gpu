/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_alloc.h"
#include "ts_decoder.h"

namespace ptir_reset
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(), m_session(0), m_repeat(1) {}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 12;
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
        , RESET     = 0x10000000
        , SESSION   = 1 << 1
        , MFXVPAR   = 1 << 2
        , CLOSE_VPP = 1 << 3
        , REPEAT    = 1 << 4
        , ALLOCATOR = 1 << 5
        , NULLPTR   = 1 << 6
        , WARNING   = 1 << 7
        , IGNORE   =  1 << 9
    };

    void apply_par(const tc_struct& p, mfxU32 stage)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;

            if(stage != (c.type & STAGE))
                continue;

            switch(c.type & ~STAGE & ~WARNING & ~IGNORE)
            {
            case SESSION   : base = (void**)&m_session;      break;
            case MFXVPAR   : base = (void**)&m_pPar;         break;
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
            case NULLPTR   : m_pPar = 0; break;
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

        if (m_par.vpp.In.Width > m_par.vpp.In.CropW)
            m_par.vpp.In.CropW = m_par.vpp.In.Width;
        if (m_par.vpp.In.Height > m_par.vpp.In.CropH)
            m_par.vpp.In.CropH = m_par.vpp.In.Height;

        if (m_par.vpp.Out.Width > m_par.vpp.Out.CropW)
            m_par.vpp.Out.CropW = m_par.vpp.Out.Width;
        if (m_par.vpp.Out.Height > m_par.vpp.Out.CropH)
            m_par.vpp.Out.CropH = m_par.vpp.Out.Height;
    }
};

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, },
    {/*01*/ MFX_ERR_NONE, 0, {RESET|REPEAT, 0, {50}}},
    {/*02*/ MFX_ERR_NONE, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|REPEAT, 0, {50}}}
    },
    {/*03*/ MFX_ERR_NONE, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|REPEAT, 0, {2}}}
    },
    {/*04*/ MFX_ERR_NONE, 0,
       {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {RESET|REPEAT, 0, {2}}}
    },
    {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/*06*/ MFX_ERR_INVALID_HANDLE,           0, {RESET|SESSION}},
    {/*07*/ MFX_ERR_NOT_INITIALIZED,          0, {RESET|CLOSE_VPP}},
    {/*08*/ MFX_ERR_NULL_PTR,                 0, {RESET|NULLPTR}},
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, {MFX_CHROMAFORMAT_MONOCHROME}}},

    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0,
       {{WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,  {720 + 32}},
        {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Width, {720 + 32}}}
    },
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0,
       {{WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {480 + 32}},
        {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {480 + 32}}}
    },
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth,    {10}}},
    {/*13*/ MFX_ERR_NONE,                     0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth,    {1}}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected,     {1}}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected,     {2}}},
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,     // can't attach Opaque buffer on Reset
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,     // can't attach Opaque buffer on Reset
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },

    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,  {720 - 8}}},
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height, {480 - 8}}},

    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {{IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropX,  {10}},
        {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, {10}}}
    },
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {{IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropY,  {10}},
        {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, {10}}}
    },
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {{IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropW,  {720 + 10}},
        {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, {720 + 10}}}
    },
    {/*25*/ MFX_ERR_NONE, 0, {
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, {MFX_PICSTRUCT_FIELD_TFF}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, {30}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, {1}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, {24}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, {1}} }
    },
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, {24}}},
    {/*27*/ MFX_ERR_NONE, 0, {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.AspectRatioW, {2}}},
    {/*28*/ MFX_ERR_NONE, 0, {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.AspectRatioH, {2}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, {MFX_FOURCC_YV12}}},

    {/*30*/ MFX_ERR_NONE, 0,
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {3840}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {3840}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {3840}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {3840}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Width,  {2048}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropW,  {2048}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {2048}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, {2048}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {2048}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropW,   {2048}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {2048}},
           {IGNORE|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropH,  {2048}}  }
    },
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,     // unaligned resolution on reset
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {3840}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {3840}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {3840}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {3840}},
           {WARNING|RESET|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, {2048 + 5}},
           {WARNING|RESET|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height,{2048 + 5}},
           {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {2048 + 5}},
           {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {2048 + 5}}  }
    },
    {/*32*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0,    // reset with higher resolution is not supported
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {2048}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {2048}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {2048}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2048}},
           {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Width,  {3840}},
           {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {3840}},
           {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {3840}},
           {WARNING|RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {3840}}  }
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto tc = test_case[id];

    MFXInit();
    // always load plug-in
    mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
    m_session = tsSession::m_session;
    tsSession::Load(m_session, ptir, 1);

    CreateAllocators();

    SetFrameAllocator();
    SetHandle();

    AllocSurfaces();

    m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
    apply_par(tc, INIT);
    Init();

    if(tc.stream_id)
        ProcessFrames(2);

    apply_par(tc, RESET);

    for(mfxU32 i = 0; i < m_repeat; i ++)
    {
        g_tsStatus.expect(tc.sts);
        Reset(m_session, m_pPar);

        if(tc.stream_id)
            ProcessFrames(2);
    }

    //Check that changed parameters took action
    bool param_check_req = false;
    for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        if(tc.ctrl[i].type & MFXVPAR) { param_check_req = true; break;}

    if(m_initialized && (tc.sts >= MFX_ERR_NONE))
    {
        if(param_check_req)
        {
            tsExtBufType<mfxVideoParam> parOut;
            GetVideoParam(m_session, &parOut);
            for(mfxU32 i = 0; i < max_num_ctrl; i ++)
            {
                if((tc.ctrl[i].type & RESET) && (tc.ctrl[i].type & MFXVPAR) && !( (tc.ctrl[i].type & WARNING) || (tc.ctrl[i].type & IGNORE) ))
                    tsStruct::check_eq(&parOut, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
                if((tc.ctrl[i].type & RESET) && (tc.ctrl[i].type & MFXVPAR) && ( (tc.ctrl[i].type & WARNING) || (tc.ctrl[i].type & IGNORE) ))
                    tsStruct::check_ne(&parOut, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
            }
        }

        Close();
    }
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_reset);
}