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

namespace camera_run_frame_async
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
        char fname2[50];
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
        ts_sprintf(fname2, "__%8.8d.gr16", m_frame);
        ts_strcpy(fname,m_fullname.c_str());
        ts_strcat(fname,"/");
        ts_strcat(fname,m_filename.c_str());
        ts_strcat(fname,fname2);

        tsRawReader raw_reader(fname, m_fi, 1);

        return raw_reader.ProcessSurface(s);

    }

};


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
    static const char path[];
    static const mfxU32 max_num_ctrl     = 20;
    static const mfxU32 max_num_ctrl_par = 4;
    mfxSession m_session;
    mfxU32 m_repeat;

    enum STREAM_STAGE
    {
          AFTER_INIT  = 0
        , AFTER_RESET = 1
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32    stream_id[2];
        mfxU32    frames_to_process[2];
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
        mfxU32    bayer_format;
        mfxU16    frame_rate_n;
        mfxU16    frame_rate_d;
    };

    static const tc_struct test_case[];
    static const stream_prop stream[];

    enum CTRL_TYPE
    {
          STAGE     = 0xF0000000
        , INIT      = 0x80000000
        , RESET     = 0x40000000
        , RUN       = 0x20000000
      //, ADD_STAGE = 0x10000000
        , SESSION   = 1 <<  1
        , MFXVPAR   = 1 <<  2
        , CLOSE_VPP = 1 <<  3
        , REPEAT    = 1 <<  4
        , ALLOCATOR = 1 <<  5
        , EXT_BUF   = 1 <<  6
        , NULLPTR   = 1 <<  7
        , NOCAMCTRL = 1 <<  7
        , SURF_IN   = 1 <<  8
        , SURF_OUT  = 1 <<  9
        , SYNCP     = 1 << 10
        , MEMID     = 1 << 11
        , ASYNC_EX  = 1 << 12
    };

    enum STREAM_ID
    {
          TRASH                      = 8
        , S_004_03_A01               = 0
        , S_4K1K_RAW__SR_B__5000p    = 1
        , S_4KDCI_HRAW__SR_H__5000p  = 2
        , S_4KDCI_HRAW__SR_O__5000p  = 3
        , S_4KDCI_HRAW__SR_O__5994p  = 4
        , S_4KDCI_RAW__SR_V__5994p   = 5
        , S_QFHD_HRAW__SR_B__5000p   = 6
    };

    void apply_par(const tc_struct& p, mfxU32 stage)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;

            if(stage != (c.type & STAGE))
                continue;

            switch(c.type & ~STAGE)
            {
            case SESSION   : base = (void**)&m_session;      break;
            case MFXVPAR   : base = (void**)&m_pPar;         break;
            case SURF_IN   : base = (void**)&m_pSurfIn;      break;
            case SURF_OUT  : base = (void**)&m_pSurfOut;     break;
            case SYNCP     : base = (void**)&m_pSyncPoint;   break;
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
            case NULLPTR   : 
                {
                switch(c.type & ~STAGE & ~NULLPTR)
                {
                    case MFXVPAR   : m_pPar     = 0; break;
                    case SESSION   : m_session  = 0; break;
                    case SURF_IN   : m_pSurfIn  = 0; break;
                    case SURF_OUT  : m_pSurfOut = 0; break;
                    default: break;
                }
                break;
                }
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
                    *base = (void*)c.par;
            }
        }
    }
};

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::stream_prop TestSuite::stream[] = 
{
    {/*S_004_03_A01  */            "004-03_A01",              4096, 2160, 50, MFX_CAM_BAYER_RGGB, 50, 1},
    {/*S_4K1K_RAW__SR_B__5000p  */ "4K1K_RAW__SR_B__5000p",   4096, 1080, 50, MFX_CAM_BAYER_BGGR, 50, 1},
    {/*S_4K1K_RAW__SR_B__5000p  */ "4K1K_RAW__SR_B__5000p",   4096, 1080, 50, MFX_CAM_BAYER_BGGR, 50, 1},
    {/*S_4KDCI_HRAW__SR_H__5000p*/ "4KDCI_HRAW__SR_H__5000p", 4096, 1080, 50, MFX_CAM_BAYER_GRBG, 50, 1},
    {/*S_4KDCI_HRAW__SR_O__5000p*/ "4KDCI_HRAW__SR_O__5000p", 4096, 1080, 50, MFX_CAM_BAYER_RGGB, 50, 1},
    {/*S_4KDCI_HRAW__SR_O__5994p*/ "4KDCI_HRAW__SR_O__5994p", 4096, 1080, 60, MFX_CAM_BAYER_RGGB, 5994, 100},
    {/*S_4KDCI_RAW__SR_V__5994p */ "4KDCI_RAW__SR_V__5994p",  4096, 2160, 60, MFX_CAM_BAYER_GBRG, 5994, 100},
    {/*S_QFHD_HRAW__SR_B__5000p */ "QFHD_HRAW__SR_B__5000p",  3840, 1080, 50, MFX_CAM_BAYER_BGGR, 50, 1},
    {/*TRASH */                     "",                       720,   480, 50, MFX_CAM_BAYER_BGGR, 0, 0},
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/* 0*/ MFX_ERR_NONE, {TRASH,TRASH}, {5,5} },
    {/* 1*/ MFX_ERR_NOT_INITIALIZED, {0,0}, {0,0}, {RUN|CLOSE_VPP }  },
    {/* 2*/ MFX_ERR_INVALID_HANDLE,  {0,0}, {0,0}, {RUN|SESSION /*|NULLPTR*/ } },
    {/* 3*/ MFX_ERR_MORE_DATA,       {0,0}, {0,0}, {RUN|SURF_IN /*|NULLPTR*/ } },
    {/* 4*/ MFX_ERR_NULL_PTR,        {0,0}, {0,0}, {RUN|SURF_OUT/*|NULLPTR*/ } },
    {/* 5*/ MFX_ERR_NULL_PTR,        {0,0}, {0,0}, {RUN|SYNCP   /*|NULLPTR*/ } },
    {/* 6*/ MFX_ERR_NULL_PTR,        {0,0}, {0,0}, 
           {{RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Data.MemId,  {0} },
            {RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Data.Y16,    {0} } }
    },
    {/* 7*/ MFX_ERR_NULL_PTR, {0,0}, {0,0}, 
           {{RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Data.MemId,  {0} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Data.B,    {0} } }
    },
    {/* 8*/ MFX_ERR_UNDEFINED_BEHAVIOR, {0,0}, {0,0}, 
           {{RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.Width,     {0} },
            {RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.Height,    {0} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.Width,    {0} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.Height,   {0} } }
    },
    {/* 9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {0,0}, {0,0}, 
           {{RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.Width,     {320} },
            {RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.Height,    {240} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.Width,    {320} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.Height,   {240} } }
    },
    {/*10*/ MFX_ERR_NONE, {0,0}, {0,0}, 
           {{RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.Width,     {4096} },
            {RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.Height,    {4096} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.Width,    {4096} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.Height,   {4096} } }
    },
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {0,0}, {0,0}, 
           {{RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Info.CropW,  {0} },
            {RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Info.CropH,  {0} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.CropW,  {0} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.CropH,  {0} } }
    },
    {/*12*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {0,0}, {0,0}, 
           {{RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Info.CropW,  {4096} },
            {RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Info.CropH,  {4096} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.CropW,  {4096} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.CropH,  {4096} } }
    },
    {/*13*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {0,0}, {0,0}, 
           {{RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Info.CropX,  {4096} },
            {RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Info.CropY,  {4096} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.CropW,  {4096} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.CropY,  {4096} } }
    },
    {/*14*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {0,0}, {0,0}, 
           {{RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.FourCC,         {MFX_FOURCC_R16} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.ChromaFormat,   {MFX_CHROMAFORMAT_MONOCHROME} } }
    },
    {/*15*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {0,0}, {0,0}, 
           {{RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.FourCC,          {MFX_FOURCC_RGB4} },
            {RUN|SURF_IN,  &tsStruct::mfxFrameSurface1.Info.ChromaFormat,    {MFX_CHROMAFORMAT_YUV444} } }
    },
    {/*16*/ MFX_ERR_NONE, {0,0}, {0,0}, 
           {{RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Info.PicStruct,    {MFX_PICSTRUCT_FIELD_TFF} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Info.PicStruct,    {MFX_PICSTRUCT_FRAME_TRIPLING} } }
    },
    {/*17*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {0,0}, {0,0}, 
           {{RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Data.PitchLow,  {0xFFFF} },
            {RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Data.PitchHigh, {0xFFFF} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Data.PitchLow,  {0xFFFF} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Data.PitchHigh, {0xFFFF} } }
    },
    {/*18*/ MFX_ERR_NONE, {0,0}, {0,0},
           {{RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Data.PitchLow,  {0} },
            {RUN|SURF_IN,   &tsStruct::mfxFrameSurface1.Data.PitchHigh, {0} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Data.PitchLow,  {0} },
            {RUN|SURF_OUT,  &tsStruct::mfxFrameSurface1.Data.PitchHigh, {0} } }
    },

//     {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, {S_004_03_A01,S_004_03_A01}, {5,5},
//            {{INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
//             {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2160}},
//             {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW,  {4096}},
//             {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH,  {2160}}}
//     }, //disable currently - test is wrong.
//     {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, {S_004_03_A01,S_4K1K_RAW__SR_B__5000p}, {5,5},      //Reset to lower resolution
//            {{INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}}, //with processing
//             {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2160}}, //and with filter change
//             {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW,  {4096}},
//             {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH,  {2160}},
//             {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {1080}},
//             {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropH,  {1080}},
//             {INIT|EXT_BUF,  0, {EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
//             //{INIT|EXT_BUF,  0, {EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
//             //{INIT|EXT_BUF,  0, {EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
//             //{RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCamVignetteCorrection   )}}}
//     }, //disable currently - test is wrong.

    {/*21*/ MFX_ERR_NONE, {TRASH,TRASH}, {5,5}, {RESET|REPEAT, 0, {10}}},
    
//     {/*22*/ MFX_ERR_NONE, {S_004_03_A01,S_4K1K_RAW__SR_B__5000p}, {5,5},
//        {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MIN}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2160}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW,  {4096}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH,  {2160}},
//         {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {1080}},
//         {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropH,  {1080}},
//         {REPEAT, 0, {7}}}
//     }, //disable currently - test is wrong.
//     {/*23*/ MFX_ERR_NONE, {S_004_03_A01,S_4K1K_RAW__SR_B__5000p}, {5,5},
//        {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MIN}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2160}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW,  {4096}},
//         {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH,  {2160}},
//         {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {1080}},
//         {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropH,  {1080}},
//         {INIT|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {0}},
//         {REPEAT, 0, {7}}}
//     },//disable currently - test is wrong.
    //{/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, {S_4KDCI_RAW__SR_V__5994p,S_QFHD_HRAW__SR_B__5000p}, {5,5},
    //   {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MIN}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2160}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW,  {4096}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH,  {2160}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Width,  {4096}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.Height, {2160}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.CropW,  {4096}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.In.CropH,  {2160}},
    //    {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {3840}},
    //    {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropH,  {1080}},
    //    {INIT|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
    //    {REPEAT, 0, {7}}}
    //}, ////disable currently - test is wrong.
    //{/*25*/ MFX_ERR_NONE, {S_4KDCI_RAW__SR_V__5994p,S_QFHD_HRAW__SR_B__5000p}, {5,5},
    //   {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MIN}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Width,  {4096}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, {2160}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW,  {4096}},
    //    {INIT|MFXVPAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH,  {2160}},
    //    {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.Height, {3840}},
    //    {RESET|MFXVPAR, &tsStruct::mfxVideoParam.vpp.Out.CropH,  {1080}},
    //    {INIT|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {5}},
    //    {REPEAT, 0, {7}}}
    //},//disable currently - test is wrong.

    {/*26*/ MFX_ERR_NONE, {TRASH,TRASH}, {5,5},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {2}}}
    },

    //{/*27*/ MFX_ERR_NONE, {TRASH,TRASH}, {5,5},
    //    {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}} }
    //},
    //{/*28*/ MFX_ERR_NULL_PTR, {TRASH,TRASH}, {5,5},
    //    {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}} }
    //},
    //{/*29*/ MFX_ERR_NONE, {TRASH,TRASH}, {5,5},
    //    {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}} }
    //}, //disable currently - test is wrong.
    //{/*30*/ MFX_ERR_LOCK_MEMORY, {TRASH,TRASH}, {5,5},
    //    {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    //     {INIT|ALLOCATOR, 0, {frame_allocator::HARDWARE, frame_allocator::ALLOC_MIN, frame_allocator::DISABLE_LOCK}} }
    //},
    //{/*31*/ MFX_ERR_LOCK_MEMORY, {TRASH,TRASH}, {5,5},
    //    {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    //     {INIT|ALLOCATOR, 0, {frame_allocator::HARDWARE, frame_allocator::ALLOC_MIN, frame_allocator::DISABLE_LOCK}} }
    //},
};

const char TestSuite::path[] = "camera/canon_gen/";

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto tc = test_case[id];

    const char* fullname_init = 0;
    const char* filename_init = 0;
    const char* fullname_reset = 0;
    const char* filename_reset = 0;
    if((tc.stream_id[AFTER_INIT] ^ TRASH) && (tc.stream_id[AFTER_INIT] != 0))
    {
        fullname_init = g_tsStreamPool.Get(stream[tc.stream_id[AFTER_INIT]].name, path);
        filename_init = stream[tc.stream_id[AFTER_INIT]].name.c_str();
    }
    else
    {
        fullname_init = g_tsStreamPool.Get(stream[0].name);
        filename_init = stream[0].name.c_str();
    }

    if((tc.stream_id[AFTER_RESET] ^ TRASH) && (tc.stream_id[AFTER_RESET] != 0))
    {
        g_tsStreamPool.Get(stream[tc.stream_id[AFTER_RESET]].name, path);
        fullname_reset = g_tsStreamPool.Get(stream[tc.stream_id[AFTER_RESET]].name);
        filename_reset = stream[tc.stream_id[AFTER_RESET]].name.c_str();
    }
    else
    {
        fullname_reset = g_tsStreamPool.Get(stream[0].name);
        filename_reset = stream[0].name.c_str();
    }
    //if(((tc.stream_id[AFTER_INIT] ^ TRASH) && (tc.stream_id[AFTER_INIT] != 0)) || ((tc.stream_id[AFTER_RESET] ^ TRASH) && (tc.stream_id[AFTER_RESET] != 0)))
        g_tsStreamPool.Reg();

    MFXInit();
    m_session = tsSession::m_session;

    if(m_uid)
    {
        Load();
    }

    if((tc.stream_id[AFTER_INIT] ^ TRASH) && (tc.stream_id[AFTER_INIT] != 0))
    {
        m_par.vpp.In.Width = m_par.vpp.In.CropW = stream[tc.stream_id[AFTER_INIT]].w;
        m_par.vpp.In.Height = m_par.vpp.In.CropH = stream[tc.stream_id[AFTER_INIT]].h;
        m_par.vpp.In.FrameRateExtN = stream[tc.stream_id[AFTER_INIT]].frame_rate_n;
        m_par.vpp.In.FrameRateExtD = stream[tc.stream_id[AFTER_INIT]].frame_rate_d;
        mfxExtCamPipeControl& cam_ctrl = m_par;
        cam_ctrl.RawFormat = stream[tc.stream_id[AFTER_INIT]].bayer_format;
        tsReader stream(g_tsStreamPool.Get( TestSuite::stream[tc.stream_id[AFTER_INIT] ].name ));
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

    tCamFrameReader first_reader(fullname_init, filename_init, m_par.vpp.In, stream[tc.stream_id[AFTER_INIT]].num_frames);
    if((tc.stream_id[AFTER_INIT] ^ TRASH) && (tc.stream_id[AFTER_INIT] != 0))
        m_surf_in_processor = &first_reader;

    //Uncomment for debug
    //tsSurfaceWriter writer("output.rgb32");
    //m_surf_out_processor = &writer;


    {   //Workaround for buggy QueryIOSurf
        QueryIOSurf();
        if(m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY )    m_request[0].Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
        if(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)    m_request[1].Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
        if(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY )     m_request[0].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        if(m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)     m_request[1].Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    }
    AllocSurfaces();
    m_pSurfIn  = m_pSurfPoolIn->GetSurface();
    m_pSurfOut = m_pSurfPoolOut->GetSurface();
    apply_par(tc, RUN);
    if(MFX_ERR_NONE == tc.sts)
    {
        ProcessFrames(tc.frames_to_process[AFTER_INIT]);
    }
    else
    {
        g_tsStatus.expect(tc.sts);
        RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);
        g_tsStatus.check();

        return 0;
    }

    if((tc.stream_id[AFTER_RESET] ^ TRASH) && (tc.stream_id[AFTER_RESET] != 0))
    {
        m_par.vpp.In.Width = m_par.vpp.In.CropW = stream[tc.stream_id[AFTER_RESET]].w;
        m_par.vpp.In.Height = m_par.vpp.In.CropH = stream[tc.stream_id[AFTER_RESET]].h;
        m_par.vpp.In.FrameRateExtN = stream[tc.stream_id[AFTER_RESET]].frame_rate_n;
        m_par.vpp.In.FrameRateExtD = stream[tc.stream_id[AFTER_RESET]].frame_rate_d;
        mfxExtCamPipeControl& cam_ctrl = m_par;
        cam_ctrl.RawFormat = stream[tc.stream_id[AFTER_RESET]].bayer_format;
    }
    apply_par(tc, RESET);

    mfxU32 return_verbosity = 0; //avoid huge logs
    if(g_tsTrace && (m_repeat > 3))
        return_verbosity = g_tsTrace;


    tCamFrameReader second_reader(fullname_reset, filename_reset, m_par.vpp.In, stream[tc.stream_id[AFTER_RESET]].num_frames);
    if((tc.stream_id[AFTER_RESET] ^ TRASH) && (tc.stream_id[AFTER_RESET] != 0))
        m_surf_in_processor = &second_reader;

    for(mfxU32 i = 0; i < m_repeat; i ++)
    {
        g_tsStatus.expect(tc.sts);
        Reset(m_session, m_pPar);

        if(tc.stream_id[AFTER_RESET])
            ProcessFrames(tc.frames_to_process[AFTER_RESET]);
        if(return_verbosity)
            g_tsTrace = 0;
    }
    if(return_verbosity)
        g_tsTrace = return_verbosity;

    Close();
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(camera_run_frame_async);
}