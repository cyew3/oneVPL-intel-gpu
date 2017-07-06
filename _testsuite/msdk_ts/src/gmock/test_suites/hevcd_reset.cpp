/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2017 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_reset
{

class TestSuite : public tsVideoDecoder
{
    static const mfxU32 max_num_ctrl     = 3;
    static const mfxU32 max_num_ctrl_par = 4;

protected:

    struct tc_struct
    {
        mfxStatus sts;
        const std::string stream[2];
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };

public:

    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC), m_session(0), m_repeat(1){}
    ~TestSuite() { }

    int RunTest(tc_struct const&);

protected:

    static const unsigned int n_cases;
    static const tc_struct test_case[];

    mfxSession m_session;
    mfxU32 m_repeat;

    enum CTRL_TYPE
    {
          STAGE   = 0xFF000000
        , INIT    = 0x01000000
        , RESET   = 0x00000000
        , SESSION = 1
        , MFXVPAR
        , CLOSE_DEC
        , REPEAT
        , ALLOCATOR
        , EXT_BUF
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
            case REPEAT    : base = (void**)&m_repeat;       break;
            case ALLOCATOR :
            {
                if (m_pVAHandle && (c.par[0] != frame_allocator::SOFTWARE))
                    SetAllocator(m_pVAHandle, true);
                else
                    SetAllocator(
                                 new frame_allocator(
                                 (frame_allocator::AllocatorType)    c.par[0],
                                 (frame_allocator::AllocMode)        c.par[1],
                                 (frame_allocator::LockMode)         c.par[2],
                                 (frame_allocator::OpaqueAllocMode)  c.par[3]
                                 ),
                                 false
                             );
                m_use_memid = true;
                break;
            }
            case CLOSE_DEC : Close(); break;
            case EXT_BUF   : m_par.AddExtBuffer(c.par[0], c.par[1]); break;
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

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, {"", ""}},
    {/* 1*/ MFX_ERR_NONE, {"", ""}, {REPEAT, 0, {50}}},
    {/* 2*/ MFX_ERR_NONE, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {50}}}
    },
    {/* 3*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/* 4*/ MFX_ERR_INVALID_HANDLE, {"", ""}, {RESET|SESSION}},
    {/* 5*/ MFX_ERR_NOT_INITIALIZED, {"", ""}, {RESET|CLOSE_DEC}},
    {/* 6*/ MFX_ERR_INVALID_VIDEO_PARAM, { "", "" }, { RESET | MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV411} } },
    {/* 7*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, {720 + 32}}},
    {/* 8*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {480 + 32}}},
    {/* 9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {10}}},
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}}},
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {1}}},
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {2}}},
    {/*13*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*14*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""},
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*15*/ MFX_ERR_NONE, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*16*/ MFX_ERR_NONE, {"", ""},
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  {720 - 8}}},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {480 - 8}}},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  {8192 + 10}}},
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {4096 + 10}}},
    {/*21*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, {10}}},
    {/*22*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, {MFX_PICSTRUCT_FIELD_SINGLE}}},
    {/*23*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, {10}}},
    {/*24*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, {720 + 10}}},
    {/*25*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, {480 + 10}}},
    {/*26*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, {2}}},
    {/*27*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, {2}}},
    {/*28*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, {275}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YV12}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOptionSPSPPS)}}},
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVideoSignalInfo)}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOption)}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOption2)}}},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVPPDenoise)}}},
    {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVppAuxData)}}},
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtAVCRefListCtrl)}}},
    {/*37*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtAvcTemporalLayers)}}},
    {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtPictureTimingSEI)}}},
    {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtEncoderCapability)}}},
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtEncoderResetOption)}}},
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {0, 0}}},
};

unsigned int const TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(tc_struct const& tc)
{
    TS_START;

    const char* stream0 = g_tsStreamPool.Get(tc.stream[0]);
    const char* stream1 = g_tsStreamPool.Get(tc.stream[1] == "" ? tc.stream[0] : tc.stream[1]);
    g_tsStreamPool.Reg();


    MFXInit();
    m_session = tsSession::m_session;

    apply_par(tc, INIT);

    if(m_uid)
    {
        Load();
    }

    if(tc.stream[0] != "")
    {
        if (m_bs_processor)
            delete m_bs_processor;
        m_bs_processor = new tsBitstreamReader(stream0, 100000);
    } else
    {
        m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
        m_par_set = true;
    }

    if (g_tsIsSSW && (m_par.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        g_tsLog<<"WARNING: Case Skipped!\n";
    }
    else
    {
        Init();
        GetVideoParam();

        if(tc.stream[0] != "")
        {
            DecodeFrames(3);
        }

        if(tc.stream[1] != "")
        {
            if (m_bs_processor)
                delete m_bs_processor;
            m_bs_processor = new tsBitstreamReader(stream1, 100000);
            m_bitstream.DataLength = 0;
            DecodeHeader();
        }

        apply_par(tc, RESET);
        if (g_tsIsSSW && (m_par.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            g_tsLog<<"WARNING: Reset Skipped!\n";
        }
        else
        {
            for(mfxU32 i = 0; i < m_repeat; i ++)
            {
                g_tsStatus.expect(tc.sts);
                Reset(m_session, m_pPar);
            }

            if((tc.stream[0] != "" || tc.stream[1] != "") && (tc.sts >= MFX_ERR_NONE))
            {
                DecodeFrames(3);
            }
        }
    }

    TS_END;
    return 0;
}

template <unsigned fourcc>
struct TestSuiteExt
    : public TestSuite
{
    static
    int RunTest(unsigned int id);

    static tc_struct const test_cases[];
    static unsigned int const n_cases;
};

/* 8b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_NV12>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE, {"conformance/hevc/itu/RPS_B_qualcomm_5.bit", ""},
    {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
    {REPEAT, 0, {2}}}
    },

    {/* 1*/ MFX_ERR_NONE, {"conformance/hevc/itu/RPS_B_qualcomm_5.bit", ""},
        {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"conformance/hevc/itu/RPS_B_qualcomm_5.bit", "conformance/hevc/itu/PICSIZE_B_Bossen_1.bin"}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_NV12>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_NV12>::test_cases) / sizeof(TestSuite::tc_struct);

/* 8b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_YUY2>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE, {"conformance/hevc/422format/inter_422_8.bin", ""},
    {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
    {REPEAT, 0, {2}}}
    },

    {/* 1*/ MFX_ERR_NONE, {"conformance/hevc/422format/inter_422_8.bin", ""},
        {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"conformance/hevc/422format/inter_422_8.bin", "conformance/hevc/itu/RPS_B_qualcomm_5.bit"}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_YUY2>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_YUY2>::test_cases) / sizeof(TestSuite::tc_struct);

/* 8b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_AYUV>::test_cases[] =
{
    { MFX_ERR_NONE, { "conformance/hevc/10bit/GENERAL_8b_444_RExt_Sony_1.bit"} }
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_AYUV>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_AYUV>::test_cases) / sizeof(TestSuite::tc_struct);

/* 10b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_P010>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE, {"conformance/hevc/10bit/DBLK_A_MAIN10_VIXS_3.bit", ""},
    {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
    {REPEAT, 0, {2}}}
    },

    {/* 1*/ MFX_ERR_NONE, {"conformance/hevc/10bit/DBLK_A_MAIN10_VIXS_3.bit", ""},
        {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"conformance/hevc/10bit/DBLK_A_MAIN10_VIXS_3.bit", "conformance/hevc/itu/RPS_B_qualcomm_5.bit"}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_P010>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_P010>::test_cases) / sizeof(TestSuite::tc_struct);

/* 10b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y210>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE, {"conformance/hevc/10bit/inter_422_10.bin", ""},
    {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
    {REPEAT, 0, {2}}}
    },

    {/* 1*/ MFX_ERR_NONE, {"conformance/hevc/10bit/inter_422_10.bin", ""},
        {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"conformance/hevc/10bit/inter_422_10.bin", "conformance/hevc/itu/RPS_B_qualcomm_5.bit"}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y210>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_Y210>::test_cases) / sizeof(TestSuite::tc_struct);

/* 10b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y410>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE, {"conformance/hevc/StressBitstreamEncode/rext444_10b/Stress_HEVC_Rext444_10bHT62_432x240_30fps_302_inter_stress_2.2.hevc", ""},
    {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
    {REPEAT, 0, {2}}}
    },

    {/* 1*/ MFX_ERR_NONE, {"conformance/hevc/StressBitstreamEncode/rext444_10b/Stress_HEVC_Rext444_10bHT62_432x240_30fps_302_inter_stress_2.2.hevc", ""},
        {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"conformance/hevc/StressBitstreamEncode/rext444_10b/Stress_HEVC_Rext444_10bHT62_432x240_30fps_302_inter_stress_2.2.hevc", "conformance/hevc/itu/RPS_B_qualcomm_5.bit"}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y410>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_Y410>::test_cases) / sizeof(TestSuite::tc_struct);

/* 12b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_P016>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE,{ "conformance/hevc/12bit/420format/GENERAL_12b_420_RExt_Sony_1.bit", "" },
    { { INIT | ALLOCATOR, 0,{ frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX } },
    { REPEAT, 0,{ 2 } } }
    },

    {/* 1*/ MFX_ERR_NONE,{ "conformance/hevc/12bit/420format/GENERAL_12b_420_RExt_Sony_1.bit", "" },
    { { INIT | MFXVPAR, &tsStruct::mfxVideoParam.IOPattern,{ MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    { REPEAT, 0,{ 2 } } }
    },

    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,{ "conformance/hevc/12bit/420format/GENERAL_12b_420_RExt_Sony_1.bit", "conformance/hevc/itu/RPS_B_qualcomm_5.bit" } },
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_P016>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_P016>::test_cases) / sizeof(TestSuite::tc_struct);

/* 12b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y216>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE,{ "conformance/hevc/12bit/422format/GENERAL_12b_422_RExt_Sony_1.bit", "" },
    { { INIT | ALLOCATOR, 0,{ frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX } },
    { REPEAT, 0,{ 2 } } }
    },

    {/* 1*/ MFX_ERR_NONE,{ "conformance/hevc/12bit/422format/GENERAL_12b_422_RExt_Sony_1.bit", "" },
    { { INIT | MFXVPAR, &tsStruct::mfxVideoParam.IOPattern,{ MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    { REPEAT, 0,{ 2 } } }
    },
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,{ "conformance/hevc/12bit/422format/GENERAL_12b_422_RExt_Sony_1.bit", "conformance/hevc/itu/RPS_B_qualcomm_5.bit" } },
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y216>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_Y216>::test_cases) / sizeof(TestSuite::tc_struct);

/* 12b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y416>::test_cases[] =
{
    {/* 0*/ MFX_ERR_NONE,{ "conformance/hevc/12bit/444format/GENERAL_12b_444_RExt_Sony_1.bit", "" },
    { { INIT | ALLOCATOR, 0,{ frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX } },
    { REPEAT, 0,{ 2 } } }
    },

    {/* 1*/ MFX_ERR_NONE,{ "conformance/hevc/12bit/444format/GENERAL_12b_444_RExt_Sony_1.bit", "" },
    { { INIT | MFXVPAR, &tsStruct::mfxVideoParam.IOPattern,{ MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    { REPEAT, 0,{ 2 } } }
    },
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,{ "conformance/hevc/12bit/444format/GENERAL_12b_444_RExt_Sony_1.bit", "conformance/hevc/itu/RPS_B_qualcomm_5.bit" } },
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y416>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_Y416>::test_cases) / sizeof(TestSuite::tc_struct);

template <unsigned fourcc>
int TestSuiteExt<fourcc>::RunTest(unsigned int id)
{
    auto tc =
        id >= TestSuite::n_cases ?
        test_cases[id - TestSuite::n_cases] : TestSuite::test_case[id];

    TestSuite suite;
    return suite.RunTest(tc);
}

TS_REG_TEST_SUITE(hevcd_reset,     TestSuiteExt<MFX_FOURCC_NV12>::RunTest, TestSuiteExt<MFX_FOURCC_NV12>::n_cases);
TS_REG_TEST_SUITE(hevcd_422_reset, TestSuiteExt<MFX_FOURCC_YUY2>::RunTest, TestSuiteExt<MFX_FOURCC_YUY2>::n_cases);
TS_REG_TEST_SUITE(hevcd_444_reset, TestSuiteExt<MFX_FOURCC_AYUV>::RunTest, TestSuiteExt<MFX_FOURCC_AYUV>::n_cases);

TS_REG_TEST_SUITE(hevc10d_reset,     TestSuiteExt<MFX_FOURCC_P010>::RunTest, TestSuiteExt<MFX_FOURCC_P010>::n_cases);
TS_REG_TEST_SUITE(hevc10d_422_reset, TestSuiteExt<MFX_FOURCC_Y210>::RunTest, TestSuiteExt<MFX_FOURCC_Y210>::n_cases);
TS_REG_TEST_SUITE(hevc10d_444_reset, TestSuiteExt<MFX_FOURCC_Y410>::RunTest, TestSuiteExt<MFX_FOURCC_Y410>::n_cases);

TS_REG_TEST_SUITE(hevc12d_reset,     TestSuiteExt<MFX_FOURCC_P016>::RunTest, TestSuiteExt<MFX_FOURCC_P016>::n_cases);
TS_REG_TEST_SUITE(hevc12d_422_reset, TestSuiteExt<MFX_FOURCC_Y216>::RunTest, TestSuiteExt<MFX_FOURCC_Y216>::n_cases);
TS_REG_TEST_SUITE(hevc12d_444_reset, TestSuiteExt<MFX_FOURCC_Y416>::RunTest, TestSuiteExt<MFX_FOURCC_Y416>::n_cases);

}
