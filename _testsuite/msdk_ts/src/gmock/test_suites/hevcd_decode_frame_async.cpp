#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_decode_frame_async
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 3;
    static const mfxU32 max_num_ctrl_par = 4;
    mfxSession m_session;
    mfxFrameSurface1** m_ppSurfOut;


    struct tc_struct
    {
        mfxStatus sts;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };

    static const tc_struct test_case[];

    enum CTRL_TYPE
    {
          STAGE = 0xFF000000
        , INIT  = 0x01000000
        , RUN   = 0x00000000
        , SESSION = 1
        , BITSTREAM
        , SURF_WORK
        , SURF_OUT
        , SYNCP
        , MFXVPAR
        , CLOSE_DEC
        , ALLOCATOR
        , MEMID
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
            case BITSTREAM : base = (void**)&m_pBitstream;   break;
            case SURF_WORK : base = (void**)&m_pSurf;        break;
            case SURF_OUT  : base = (void**)&m_ppSurfOut;    break;
            case SYNCP     : base = (void**)&m_pSyncPoint;   break;
            case MFXVPAR   : base = (void**)&m_pPar;         break;
            case MEMID     : m_use_memid = !!c.par[0];    break;
            case ALLOCATOR : SetAllocator(
                                 new frame_allocator(
                                    (frame_allocator::AllocatorType)    c.par[0],
                                    (frame_allocator::AllocMode)        c.par[1],
                                    (frame_allocator::LockMode)         c.par[2],
                                    (frame_allocator::OpaqueAllocMode)  c.par[3]
                                 ),
                                 false
                             ); break;
            case CLOSE_DEC : Close(); break;
            default: break;
            }

            if(base)
            {
                if(c.field)
                    tsStruct::set(*base, *c.field, c.par[0]);
                else
                    *base = (void*)c.par[0];
            }
        }
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, },
    {/* 1*/ MFX_ERR_INVALID_HANDLE, {SESSION}},
    {/* 2*/ MFX_ERR_MORE_DATA, {BITSTREAM}},
    {/* 3*/ MFX_ERR_NULL_PTR, {SURF_WORK}},
    {/* 4*/ MFX_ERR_NULL_PTR, {SURF_OUT}},
    {/* 5*/ MFX_ERR_NULL_PTR, {SYNCP}},
    {/* 6*/ MFX_ERR_MORE_DATA, {BITSTREAM, &tsStruct::mfxBitstream.DataLength, {10}}},
    {/* 7*/ MFX_ERR_NOT_INITIALIZED, {CLOSE_DEC}},
    {/* 8*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
       {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, {320}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, {320}}}
    },
    {/* 9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
       {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {208}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, {208}}}
    },
    {/*10*/ MFX_ERR_NONE, {BITSTREAM, &tsStruct::mfxBitstream.DataFlag, {MFX_BITSTREAM_COMPLETE_FRAME}}},
    {/*11*/ MFX_ERR_ABORTED,
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX, frame_allocator::LARGE_PITCH_LOCK}},
        {INIT|MEMID, 0, {1}}}
    },
    {/*12*/ MFX_ERR_UNDEFINED_BEHAVIOR, {BITSTREAM, &tsStruct::mfxBitstream.DataOffset, {100001}}},
    {/*13*/ MFX_ERR_MORE_SURFACE, {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.Locked, {1}}},
    {/*14*/ MFX_ERR_ABORTED,
        {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX, frame_allocator::ZERO_LUMA_LOCK}},
        {INIT|MEMID, 0, {1}}}
    },
    {/*15*/ MFX_ERR_NONE, {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/*16*/ MFX_ERR_UNDEFINED_BEHAVIOR, {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.PitchHigh, {0x8000}}},
    {/*17*/ MFX_ERR_UNDEFINED_BEHAVIOR, {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.Y, {0}}},
    {/*18*/ MFX_ERR_NONE, {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}},
    {/*19*/ MFX_ERR_UNDEFINED_BEHAVIOR,
       {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.Y, {1}}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const char* sname = g_tsStreamPool.Get("conformance/hevc/itu/RPS_B_qualcomm_5.bit");
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;
    auto tc = test_case[id];

    if (tc.sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
        DecodeHeader();

    apply_par(tc, INIT);
    Init();

    SetPar4_DecodeFrameAsync();
    m_session = tsSession::m_session;
    m_ppSurfOut = &m_pSurfOut;

    apply_par(tc, RUN);

    if(tc.sts == MFX_ERR_NONE)
    {
        DecodeFrames(30);
    }
    else
    {
        g_tsStatus.expect(tc.sts);
        TRACE_FUNC5(MFXVideoDECODE_DecodeFrameAsync, m_session, m_pBitstream, m_pSurf, m_ppSurfOut, m_pSyncPoint);
        mfxStatus mfxRes = MFXVideoDECODE_DecodeFrameAsync(m_session, m_pBitstream, m_pSurf, m_ppSurfOut, m_pSyncPoint);
        TS_TRACE(mfxRes);
        TS_TRACE(m_pBitstream);
        TS_TRACE(m_ppSurfOut);
        TS_TRACE(m_pSyncPoint);
        if (mfxRes == MFX_ERR_NONE)
        {
            SyncOperation(*m_pSyncPoint);
        }
        else
        {
            g_tsStatus.m_status = mfxRes;
            g_tsStatus.check();
        }

    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevcd_decode_frame_async);
}