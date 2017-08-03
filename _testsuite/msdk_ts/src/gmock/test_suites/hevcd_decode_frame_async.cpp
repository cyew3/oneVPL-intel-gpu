/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_decode_frame_async
{

class DecodeSuite : public tsVideoDecoder
{
public:

    DecodeSuite() : tsVideoDecoder(MFX_CODEC_HEVC){}
    ~DecodeSuite() { }

    static const unsigned int n_cases;

    int run(unsigned int id, const char* sname);

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
                                    (frame_allocator::AllocatorType)    ((m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)) ? frame_allocator::SOFTWARE : frame_allocator::HARDWARE,
                                    (frame_allocator::AllocMode)        c.par[0],
                                    (frame_allocator::LockMode)         c.par[1],
                                    (frame_allocator::OpaqueAllocMode)  c.par[2]
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
                    //no way to have persistent pointers here, the only valid value is NULL
                    *base = NULL;
            }
        }
    }
};

const DecodeSuite::tc_struct DecodeSuite::test_case[] =
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
    {/*11*/ MFX_ERR_UNDEFINED_BEHAVIOR,
        {
            {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
            {INIT|ALLOCATOR, 0, {frame_allocator::ALLOC_MAX, frame_allocator::LARGE_PITCH_LOCK}},
            {INIT|MEMID, 0, { 1 }}
        }
    },
    {/*12*/ MFX_ERR_UNDEFINED_BEHAVIOR,
        {
            {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
            {INIT|ALLOCATOR, 0, {frame_allocator::ALLOC_MAX, frame_allocator::LARGE_PITCH_LOCK}},
        }
    },
    {/*13*/ MFX_ERR_UNDEFINED_BEHAVIOR, {BITSTREAM, &tsStruct::mfxBitstream.DataOffset, {100001}}},
    {/*14*/ MFX_ERR_MORE_SURFACE, {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.Locked, {1}}},
    {/*15*/ MFX_ERR_UNDEFINED_BEHAVIOR,
        {
            {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
            {INIT|ALLOCATOR, 0, {frame_allocator::ALLOC_MAX, frame_allocator::ZERO_LUMA_LOCK}},
            {INIT|MEMID, 0, {1}}
        }
    },
    {/*16*/ MFX_ERR_UNDEFINED_BEHAVIOR,
        {
            {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
            {INIT|ALLOCATOR, 0, {frame_allocator::ALLOC_MAX, frame_allocator::ZERO_LUMA_LOCK}},
        }
    },
    {/*17*/ MFX_ERR_NONE, {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/*18*/ MFX_ERR_NONE, {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}},
    {/*19*/ MFX_ERR_UNDEFINED_BEHAVIOR,
        {
            {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
            {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.PitchHigh, {0x8000}}
        }
    },
    {/*20*/ MFX_ERR_UNDEFINED_BEHAVIOR,
        {
            {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
            {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.Y, {0}}
        }
    },
    {/*21*/ MFX_ERR_NONE, {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}},
    {/*22*/ MFX_ERR_NONE, {INIT|MEMID, 0, {1}}},
    {/*23*/ MFX_ERR_UNDEFINED_BEHAVIOR,
       {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {SURF_WORK, &tsStruct::mfxFrameSurface1.Data.Y, {1}}}
    },

};

const unsigned int DecodeSuite::n_cases = sizeof(DecodeSuite::test_case)/sizeof(DecodeSuite::tc_struct);
const unsigned int MAX_FRAMES = 30;

int DecodeSuite::run(unsigned int id, const char* sname)
{
    TS_START;

    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;
    auto tc = test_case[id];
    mfxStatus expected = tc.sts;

    if (tc.sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
        DecodeHeader();

    if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCD_SW.Data, sizeof(MFX_PLUGINID_HEVCE_SW.Data)))
    {
        m_par.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
    else
    {
        m_par.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }

    apply_par(tc, INIT);

    Init();

    SetPar4_DecodeFrameAsync();
    m_session = tsSession::m_session;
    m_ppSurfOut = &m_pSurfOut;

    apply_par(tc, RUN);

    if(expected == MFX_ERR_NONE)
    {
        DecodeFrames(MAX_FRAMES);
    }
    else
    {
        g_tsStatus.expect(expected);
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

template <unsigned fourcc>
char const* query_stream(unsigned int, std::integral_constant<unsigned, fourcc>);

/* 8 bit */
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
{ return "conformance/hevc/itu/RPS_B_qualcomm_5.bit"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_YUY2>)
{ return "conformance/hevc/422format/inter_422_8.bin"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>)
{ return "conformance/hevc/10bit/GENERAL_8b_444_RExt_Sony_1.bit"; }

/* 10 bit */
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
{ return "conformance/hevc/10bit/WP_A_MAIN10_Toshiba_3.bit"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y210>)
{ return "conformance/hevc/10bit/inter_422_10.bin"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>)
{ return "conformance/hevc/StressBitstreamEncode/rext444_10b/Stress_HEVC_Rext444_10bHT62_432x240_30fps_302_inter_stress_2.2.hevc"; }

/* 12 bit */
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P016>)
{ return "conformance/hevc/12bit/420format/GENERAL_12b_420_RExt_Sony_1.bit"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y216>)
{ return "conformance/hevc/12bit/422format/GENERAL_12b_422_RExt_Sony_1.bit"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y416>)
{ return "conformance/hevc/12bit/444format/GENERAL_12b_444_RExt_Sony_1.bit"; }

template <unsigned fourcc, unsigned profile>
char const* query_stream(unsigned int id, std::integral_constant<unsigned, fourcc>, std::integral_constant<unsigned, profile>)
{ return query_stream(id, std::integral_constant<unsigned, fourcc>{}); }

/* SCC */
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_NV12>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
{ return "conformance/hevc/scc/scc-main/020_main_palette_all_lf.hevc"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
{ return "conformance/hevc/scc/scc-main10/020_main10_palette_all_lf.hevc"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
{ return "conformance/hevc/scc/scc-main444/020_main444_palette_all_lf.hevc"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
{ return "conformance/hevc/scc/scc-main444_10/020_main444_10_palette_all_lf.hevc"; }

template <unsigned fourcc, unsigned profile = MFX_PROFILE_UNKNOWN>
struct TestSuite
    : public DecodeSuite
{
    static
    int RunTest(unsigned int id)
    {
        const char* sname =
            g_tsStreamPool.Get(query_stream(id, std::integral_constant<unsigned, fourcc>(), std::integral_constant<unsigned, profile>{}));
        g_tsStreamPool.Reg();

        DecodeSuite suite;
        return suite.run(id, sname);
    }
};

TS_REG_TEST_SUITE(hevcd_decode_frame_async,     TestSuite<MFX_FOURCC_NV12>::RunTest, TestSuite<MFX_FOURCC_NV12>::n_cases);
TS_REG_TEST_SUITE(hevcd_422_decode_frame_async, TestSuite<MFX_FOURCC_YUY2>::RunTest, TestSuite<MFX_FOURCC_YUY2>::n_cases);
TS_REG_TEST_SUITE(hevcd_444_decode_frame_async, TestSuite<MFX_FOURCC_AYUV>::RunTest, TestSuite<MFX_FOURCC_AYUV>::n_cases);

TS_REG_TEST_SUITE(hevc10d_decode_frame_async,     TestSuite<MFX_FOURCC_P010>::RunTest, TestSuite<MFX_FOURCC_P010>::n_cases);
TS_REG_TEST_SUITE(hevc10d_422_decode_frame_async, TestSuite<MFX_FOURCC_Y210>::RunTest, TestSuite<MFX_FOURCC_Y210>::n_cases);
TS_REG_TEST_SUITE(hevc10d_444_decode_frame_async, TestSuite<MFX_FOURCC_Y410>::RunTest, TestSuite<MFX_FOURCC_Y410>::n_cases);

TS_REG_TEST_SUITE(hevc12d_420_p016_decode_frame_async, TestSuite<MFX_FOURCC_P016>::RunTest, TestSuite<MFX_FOURCC_P016>::n_cases);
TS_REG_TEST_SUITE(hevc12d_422_y216_decode_frame_async, TestSuite<MFX_FOURCC_Y216>::RunTest, TestSuite<MFX_FOURCC_Y216>::n_cases);
TS_REG_TEST_SUITE(hevc12d_444_y416_decode_frame_async, TestSuite<MFX_FOURCC_Y416>::RunTest, TestSuite<MFX_FOURCC_Y416>::n_cases);

TS_REG_TEST_SUITE(hevcd_420_nv12_scc_decode_frame_async,   (TestSuite<MFX_FOURCC_NV12, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuite<MFX_FOURCC_NV12, MFX_PROFILE_HEVC_SCC>::n_cases));
TS_REG_TEST_SUITE(hevcd_444_ayuv_scc_decode_frame_async,   (TestSuite<MFX_FOURCC_AYUV, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuite<MFX_FOURCC_AYUV, MFX_PROFILE_HEVC_SCC>::n_cases));
TS_REG_TEST_SUITE(hevc10d_420_p010_scc_decode_frame_async, (TestSuite<MFX_FOURCC_P010, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuite<MFX_FOURCC_P010, MFX_PROFILE_HEVC_SCC>::n_cases));
TS_REG_TEST_SUITE(hevc10d_444_y410_scc_decode_frame_async, (TestSuite<MFX_FOURCC_Y410, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuite<MFX_FOURCC_Y410, MFX_PROFILE_HEVC_SCC>::n_cases));

}
