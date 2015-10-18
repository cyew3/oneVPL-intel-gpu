#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_alloc.h"
#include "ts_decoder.h"

namespace camera_get_video_param
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('C','A','M','R')), m_session(0), m_repeat(1){}
    ~TestSuite() { }
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
        , NOCAMCTRL = 1 << 8
        , FAILED    = 1 << 9
        , WARNING   = 1 << 10
    };

    enum STREAM
    {
        PROCESS_TRASH = 0x1
    };

    void apply_par(const tc_struct& p, mfxU32 stage, tsExtBufType<mfxVideoParam>*& pPar)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;

            if(stage != (c.type & STAGE))
                continue;

            switch(c.type & ~STAGE & ~FAILED & ~WARNING & ~NOCAMCTRL)
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
                {
                    tsStruct::set(*base, *c.field, c.par[0]);
                    std::cout << "  Set field " << c.field->name << " to " << c.par[0] << "\n";
                }
                else
                    *base = (void*)c.par[0];
            }
        }
    }
};

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/* 0*/ MFX_ERR_NONE, },
    {/* 1*/ MFX_ERR_NONE, 0, {GETVP|REPEAT, 0, {50}}},
    {/* 2*/ MFX_ERR_NONE, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {GETVP|REPEAT, 0, {50}}}
    },
    {/* 3*/ MFX_ERR_NONE, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {GETVP|REPEAT, 0, {2}}}
    },
    {/* 4*/ MFX_ERR_NOT_INITIALIZED, 0,
       {{FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {GETVP|REPEAT, 0, {2}}}
    },
    {/* 5*/ MFX_ERR_INVALID_HANDLE,           0, {GETVP|SESSION}},
    {/* 6*/ MFX_ERR_NOT_INITIALIZED,          0, {GETVP|CLOSE_VPP}},
    {/* 7*/ MFX_ERR_NULL_PTR,                 0, {GETVP|NULLPTR}},
    {/* 8*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, {MFX_CHROMAFORMAT_YUV411}}},
    {/* 9*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width, {720 + 31}}},
    {/*10*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height, {480 + 31}}},
    {/*11*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {500}}},
    {/*12*/ MFX_ERR_NONE,                     0, {INIT|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}}},
    {/*13*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {1}}},
    {/*14*/ MFX_ERR_NOT_INITIALIZED,          0, {FAILED|INIT|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {2}}},

    {/*15*/ MFX_ERR_NONE, 0, {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance         )}}},
    {/*16*/ MFX_ERR_NONE, 0, {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}}},
    {/*17*/ MFX_ERR_NONE, 0, {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}},
    {/*18*/ MFX_ERR_NONE, 0, {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamVignetteCorrection   )}}},
    {/*19*/ MFX_ERR_NONE, 0, {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBayerDenoise         )}}},
    {/*20*/ MFX_ERR_NONE, 0, {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}}},

    {/*21*/ MFX_ERR_NONE, 0, 
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance      )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval   )}}  }
    },
    {/*22*/ MFX_ERR_NONE, 0, {{INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}}},
    {/*23*/ MFX_ERR_NONE, 0, 
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}  }
    },
    {/*24*/ MFX_ERR_NONE, 0, 
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}}
    },
    {/*25*/ MFX_ERR_NONE, 0, 
        {  {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamGammaCorrection   )}},
           {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance      )}},
           {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval   )}}  }
    },
    {/*26*/ MFX_ERR_NONE, 0, 
        {  {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}}
    },
    {/*27*/ MFX_ERR_NONE, 0, 
        {  {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}},
           {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}  }
    },
    {/*28*/ MFX_ERR_NONE, 0, 
        {  {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
           {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
           {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}}
    },
    {/*29*/ MFX_ERR_NONE, 0, 
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance          )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval       )}},
           {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}}
    },
    {/*30*/ MFX_ERR_NONE, 0, 
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {4096}}  }
    },
    {/*31*/ MFX_ERR_NONE, 0, 
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {4096}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval       )}},
           {GETVP|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
           {GETVP|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {2048}},
           {GETVP|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {2048}}  }
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto tc = test_case[id];

    MFXInit();
    m_session = tsSession::m_session;

    if(m_uid)
    {
        Load();
    }
    
    tsExtBufType<mfxVideoParam>* pParInit = &m_par;
    apply_par(tc, INIT, pParInit);
    if(FAILED & tc.ctrl[0].type)
        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
    if(WARNING & tc.ctrl[0].type)
        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    Init(m_session, pParInit);

    //remove all buffers after Init and return required filter
    //Component should not relay on provided in Init buffer
    mfxU32 tmpNumExtPar = m_par.NumExtParam;
    for(mfxU32 i = 0; i < tmpNumExtPar; ++i)
        m_par.RemoveExtBuffer(m_par.ExtParam[0]->BufferId);
    m_par.NumExtParam = 0;
    m_par.RefreshBuffers();
    if( !((tc.ctrl[0].type & GETVP) && (tc.ctrl[0].type & NOCAMCTRL)) )
        mfxExtCamPipeControl& cam_ctrl = m_par;

    if(tc.stream_id)
    {
        {   //Workaround for buggy QueryIOSurf
            QueryIOSurf();
            if(m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY )    m_request[0].Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
            if(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)    m_request[1].Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
            if(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY )     m_request[0].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
            if(m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)     m_request[1].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        }
        ProcessFrames(2);
    }

    tsExtBufType<mfxVideoParam> par_out;
    mfxVideoParam par_out_copy;
    tsExtBufType<mfxVideoParam>* pParOut = &par_out;
    apply_par(tc, GETVP, pParOut);
    par_out_copy = par_out;
    
    mfxExtBuffer* pExtParamCopy[20] = {0,}; //20 should be enough
    if(par_out.ExtParam)
        memcpy(pExtParamCopy,par_out.ExtParam,sizeof(mfxExtBuffer*) * par_out.NumExtParam);

    for(mfxU32 i = 0; i < m_repeat; i ++)
    {
        g_tsStatus.expect(tc.sts);
        GetVideoParam(m_session, pParOut);
    }

    if(m_initialized)
    {
        //check that buffer array was not changed
        if(par_out_copy.NumExtParam)
        {
            EXPECT_EQ(par_out_copy.NumExtParam, par_out.NumExtParam) << "Fail.: GetVideoParam should not change number of attached ext buffers\n";
            EXPECT_EQ(par_out_copy.ExtParam, par_out.ExtParam) << "Fail.: GetVideoParam should not change ptr to extBuffers\n";
            for(mfxU32 i = 0; i < TS_MIN(par_out_copy.NumExtParam, par_out.NumExtParam); ++i)
            {
                EXPECT_EQ(pExtParamCopy[i], par_out.ExtParam[i]) << "Fail.: GetVideoParam should not change ptr to extBuffers\n";
            }
            par_out.NumExtParam = par_out_copy.NumExtParam; //restore data to avoid heap corruption in destructors
            par_out.ExtParam = par_out_copy.ExtParam;
        }

        //Check that parameters are took place
        bool param_check_req = false;
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
            if(tc.ctrl[i].type & INIT) { param_check_req = true; break;}

        bool filters_check_required = false;
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
            if(tc.ctrl[i].type & EXT_BUF) { filters_check_required = true; break;}

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

        if(filters_check_required)
        {
            tsExtBufType<mfxVideoParam> parOut;
            mfxExtVPPDoUse& do_use = parOut;
            do_use.NumAlg = max_num_ctrl+1;
            mfxU32 filter_from_getvideoparam[max_num_ctrl+1] = {0,};
            do_use.AlgList = filter_from_getvideoparam;
            GetVideoParam(m_session, &parOut);
            EXPECT_EQ(&do_use, (mfxExtVPPDoUse*) *parOut.ExtParam) << "Fail.: GetVideoParam should not change ptr to extBuffers\n";

            mfxU32 expected_filters_list[max_num_ctrl + 1] = {0,};
            expected_filters_list[0] = MFX_EXTBUF_CAM_PIPECONTROL;
            bool   filter_found[max_num_ctrl + 1]          = {false,};

            mfxU32 actual_f_n = 1;

            for(mfxU32 i = 0; i < max_num_ctrl; i ++)
            {
                if((tc.ctrl[i].type & INIT) && (tc.ctrl[i].type & EXT_BUF))
                {
                    expected_filters_list[actual_f_n] = tc.ctrl[i].par[0];
                    actual_f_n++;
                }
            }
            for(mfxU32 i = 0; i < actual_f_n; ++i)
            {
                for(mfxU32 j = 0; j < actual_f_n; ++j)
                {
                    if(expected_filters_list[i] == filter_from_getvideoparam[j])
                    {
                        EXPECT_EQ(false,filter_found[i]) << "Fail.: Filter was reported more than once\n";
                        filter_found[i] = true;
                    }
                }
            }
            for(mfxU32 i = 0; i < actual_f_n; ++i)
            {
                EXPECT_EQ(true,filter_found[i]) << "Fail. Init had returned ERR_NONE, but required filter was not reported by GetVideoParam\n";
            }
        }
    }

    if(m_initialized)
        Close();
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(camera_get_video_param);
}