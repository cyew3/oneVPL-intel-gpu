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

namespace camera_reset
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('C','A','M','R')), m_session(0), m_repeat(1){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus Init()
    {
        return Init(m_session, m_pPar);
    }
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

    mfxStatus Reset(mfxSession session, mfxVideoParam *par)
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
        return tsVideoVPP::Reset(session, par);
    }
    mfxStatus Reset()
    {
        return Reset(m_session, m_pPar);
    }

    mfxStatus Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
    {
        if(session && in)
        {
            if((MFX_FOURCC_A2RGB10 == in->vpp.In.FourCC) ||
               (MFX_FOURCC_ARGB16  == in->vpp.In.FourCC) ||
               (MFX_FOURCC_R16     == in->vpp.In.FourCC))
               in->vpp.In.BitDepthLuma = 10;
            if((MFX_FOURCC_A2RGB10 == in->vpp.Out.FourCC) ||
               (MFX_FOURCC_ARGB16  == in->vpp.Out.FourCC) ||
               (MFX_FOURCC_R16     == in->vpp.Out.FourCC))
               in->vpp.Out.BitDepthLuma = 10;
        }
        return tsVideoVPP::Query(session, in, out);
    }

private:
    static const mfxU32 max_num_ctrl     = 10;
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
        , EXT_BUF   = 1 << 6
        , NULLPTR   = 1 << 7
        , NOCAMCTRL = 1 << 8
    };

    enum STREAM
    {
        PROCESS_TRASH = 0x1
    };

    void apply_par(const tc_struct& p, mfxU32 stage)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;

            if(stage != (c.type & STAGE))
                continue;

            switch(c.type & ~STAGE & ~NOCAMCTRL)
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
            case EXT_BUF   : m_par.AddExtBuffer(c.par[0], c.par[1]); break;
            case NULLPTR   : m_pPar = 0; break;
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
                    *((mfxU32*)base) = c.par[0];
            }
        }
    }
};

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/* 0*/ MFX_ERR_NONE, },
    {/* 1*/ MFX_ERR_NONE, 0, {RESET|REPEAT, 0, {50}}},
    {/* 2*/ MFX_ERR_NONE, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {50}}}
    },
    {/* 3*/ MFX_ERR_NONE, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {2}}}
    },
    {/* 4*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/* 5*/ MFX_ERR_INVALID_HANDLE,           0, {RESET|SESSION}},
    {/* 6*/ MFX_ERR_NONE,                     0, {RESET}},
    {/* 7*/ MFX_ERR_NULL_PTR,                 0, {RESET|NULLPTR}},
    {/* 8*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, {MFX_CHROMAFORMAT_YUV411}}},
    {/* 9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,  {720 + 32}}},
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height, {480 + 32}}},
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth,    {10}}},
    {/*12*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth,    {5}}},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected,     {1}}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM,      0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected,     {2}}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
        {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,  {720 - 8}}},
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height, {480 - 8}}},
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropX, {10}}},
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropY, {10}}},
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropW, {720 + 10}}},
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.CropH, {480 + 10}}},
    {/*25*/ MFX_ERR_NONE, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.AspectRatioW, {2}}},
    {/*26*/ MFX_ERR_NONE, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, {2}}},
    {/*27*/ MFX_ERR_NONE, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, {275}}},
    {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, {MFX_FOURCC_YV12}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOptionSPSPPS)}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVideoSignalInfo)}}},
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOption)}}},
    {/*32*/ MFX_ERR_UNDEFINED_BEHAVIOR, 0, {RESET|EXT_BUF, 0, {0, 0}}},

    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamGammaCorrection      )}}},
    {/*34*/ MFX_ERR_NONE               , 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance         )}}},
    {/*35*/ MFX_ERR_NONE               , 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamCscYuvRgb            )}}},
    {/*36*/ MFX_ERR_NONE               , 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamTotalColorControl    )}}},
    {/*37*/ MFX_ERR_NONE               , 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}}},
    {/*38*/ MFX_ERR_NONE               , 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}},
    {/*39*/ MFX_ERR_NONE               , 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}}},
    {/*40*/ MFX_ERR_NONE               , 0, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamPadding              )}}},

    {/*41*/ MFX_ERR_NONE, 0,
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance      )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval   )}}  }
    },
    {/*42*/ MFX_ERR_NONE, 0,
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
//           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamVignetteCorrection   )}},
//           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBayerDenoise         )}}
    }
    },
    {/*43*/ MFX_ERR_NONE, 0,
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}  }
    },
    {/*44*/ MFX_ERR_NONE, 0,
        {  {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
//           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamVignetteCorrection   )}},
//           {INIT|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBayerDenoise         )}}  
    }
    },
    {/*45*/ MFX_ERR_NONE, 0,
        {  {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance      )}},
           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval   )}}  }
    },
    {/*46*/ MFX_ERR_NONE, 0,
        {  {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamTotalColorControl )}},
           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamCscYuvRgb         )}}}
    },
    {/*47*/ MFX_ERR_NONE, 0,
        {  {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
//           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamVignetteCorrection   )}},
//           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBayerDenoise         )}}  
    }
    },
    {/*48*/ MFX_ERR_NONE, 0,
        {  {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}},
           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}}  }
    },
    {/*49*/ MFX_ERR_NONE, 0,
        {  {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
//           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamVignetteCorrection   )}},
//           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBayerDenoise         )}}  
    }
    },
    {/*50*/ MFX_ERR_NONE, 0,
        {  {INIT|EXT_BUF,  0, {EXT_BUF_PAR(mfxExtCamWhiteBalance          )}},
           {INIT|EXT_BUF,  0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval       )}},
           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
//           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamVignetteCorrection   )}},
//           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBayerDenoise         )}}  
    }
    },
    {/*51*/ MFX_ERR_NONE, 0,
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {4096}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Width, {2048}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height,{2048}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {2048}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {2048}}  }
    },
    {/*52*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {4096}},
           {RESET|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, {2048 + 5}},
           {RESET|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height,{2048 + 5}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {2048 + 5}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {2048 + 5}}  }
    },
    {/*53*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0,
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {2048}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {2048}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {2048}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2048}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {4096}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {4096}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {4096}}  }
    },
    {/*54*/ MFX_ERR_NONE, 0,
        {  {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,   {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height,  {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,   {4096}},
           {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height,  {4096}},
           {INIT|EXT_BUF,  0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval       )}},
           {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Width, {2048}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height,{2048}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Width,   {2048}},
           {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.In.Height,  {2048}}  }
    },
    {/*55*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {RESET|NOCAMCTRL, 0,  0} }, //mfxExtCamPipeControl buffer is mandatory
    {/*56*/ MFX_ERR_NONE, PROCESS_TRASH}, //more cases with actual file processing are TBD
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

    apply_par(tc, INIT);
    Init();
    GetVideoParam();

    //remove all buffers after Init and return required filter
    //Component should not relay on provided buffer
    mfxU32 tmpNumExtPar = m_par.NumExtParam;
    for(mfxU32 i = 0; i < tmpNumExtPar; ++i)
        m_par.RemoveExtBuffer(m_par.ExtParam[0]->BufferId);
    m_par.NumExtParam = 0;
    m_par.RefreshBuffers();
    if( !((tc.ctrl[0].type & RESET) && (tc.ctrl[0].type & NOCAMCTRL)) )
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
    if(tc.sts>MFX_ERR_NONE)
    for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        if(tc.ctrl[i].type & MFXVPAR) { param_check_req = true; break;}
    if(param_check_req)
    {
        tsExtBufType<mfxVideoParam> parOut;
        GetVideoParam(m_session, &parOut);
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            if((tc.ctrl[i].type & RESET) && (tc.ctrl[i].type & MFXVPAR))
            {
                tsStruct::check_eq(&parOut, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
            }
        }
    }

    //Check actual filter list
    bool filters_check_required = false;
    if(tc.sts>MFX_ERR_NONE)
    for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        if(tc.ctrl[i].type & EXT_BUF) { filters_check_required = true; break;}
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
            if((tc.ctrl[i].type & RESET) && (tc.ctrl[i].type & EXT_BUF))
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
                    EXPECT_FALSE(filter_found[i]) << "Fail.: Filter was reported more than once\n";
                    filter_found[i] = true;
                }
            }
        }
        for(mfxU32 i = 0; i < actual_f_n; ++i)
        {
            EXPECT_TRUE(filter_found[i]) << "Fail. Reset had returned ERR_NONE, but required filter was not reported by GetVideoParam\n";
        }
        //GetVideoParam should still work with explicitly filters attached
        GetVideoParam();
    }

    Close();
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(camera_reset);
}