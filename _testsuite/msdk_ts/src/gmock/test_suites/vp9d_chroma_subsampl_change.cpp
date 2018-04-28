/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_surface_provider.h"

#define TEST_NAME vp9d_chroma_subsampl_change

namespace TEST_NAME
{


class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9){}
    ~TestSuite() { }
    static const size_t n_cases_8b;
    static const size_t n_cases_10b;

    template<mfxU16 bit_depth>
    int RunTest_BitDepth(const size_t id);

private:
    static const mfxU32 max_num_ctrl     = 3;

    struct tc_struct
    {
        const char* stream;
        mfxU32 n_frames;
        mfxStatus sts;
    };
    int RunTest(const tc_struct (&tcs)[max_num_ctrl]);

    static const tc_struct test_case_8b[][max_num_ctrl];
    static const tc_struct test_case_10b[][max_num_ctrl];
};

#define MAKE_TEST_TABLE(CHROMA, L, L_N, M, M_N, S, S_N)                                                                    \
const TestSuite::tc_struct TestSuite::test_case_##CHROMA[][max_num_ctrl] =                                                 \
{                                                                                                                          \
    {/* 0*/ {M, 0  },  {S, 0  },                                 {}                                       },               \
    {/* 1*/ {S, 0  },  {M, 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}, {}                                       },               \
    {/* 2*/ {M, M_N},  {S, S_N},                                 {}                                       },               \
    {/* 3*/ {M, 0  },  {S, 0  },                                 {L, 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM} },               \
    {/* 4*/ {L, 0  },  {M, 0  },                                 {S, 0  }                                 },               \
    {/* 5*/ {L, L_N},  {M, M_N},                                 {S, S_N}                                 },               \
    {/* 6*/ {L, 0  },  {S, 0  },                                 {M, 0  }                                 },               \
    {/* 7*/ {L, L_N},  {S, S_N},                                 {M, M_N}                                 },               \
    {/* 8*/ {L, L_N},  {M, M_N},                                 {L, L_N}                                 },               \
};                                                                                                                         \
const size_t TestSuite::n_cases_##CHROMA = sizeof(TestSuite::test_case_##CHROMA)/sizeof(TestSuite::tc_struct[max_num_ctrl]);

MAKE_TEST_TABLE(8b, "conformance/vp9/SBE/8bit/Syntax_VP9_432x240_001_intra_basic_1.3.vp9", 10,
                    "conformance/vp9/SBE/8bit_444/Syntax_VP9_FC2p1ss444_432x240_001_intra_basic_1.4.vp9", 10,
                    "conformance/vp9/SBE/8bit/Syntax_VP9_432x240_001_intra_basic_1.3.vp9", 10)

MAKE_TEST_TABLE(10b, "conformance/vp9/SBE/10bit/Syntax_VP9_FC2p2b10_432x240_001_intra_basic_1.3.vp9", 10,
                     "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_001_intra_basic_1.4.vp9", 10,
                     "conformance/vp9/SBE/10bit/Syntax_VP9_FC2p2b10_432x240_001_intra_basic_1.3.vp9", 10)

#undef MAKE_TEST_TABLE

template<mfxU16 bit_depth>
int TestSuite::RunTest_BitDepth(const size_t id)
{
    switch (bit_depth)
    {
        case 8:  return RunTest(test_case_8b[id]);
        case 10: return RunTest(test_case_10b[id]);
        default: assert(!"Wrong/unsupported format requested!"); break;
    }
    throw tsFAIL;
}

int TestSuite::RunTest(const tc_struct (&tcs)[max_num_ctrl])
{
    TS_START;
    for (auto& tc : tcs)
        g_tsStreamPool.Get( tc.stream ? tc.stream : std::string(), "");

    g_tsStreamPool.Reg();
    m_use_memid = true;

    mfxU32 old_fourcc = 0;

    for (auto& tc : tcs) 
    {
        if(0 == tc.stream)
            break;

        g_tsLog << "New stream: " << tc.stream << "\n";

        tsBitstreamReaderIVF r(g_tsStreamPool.Get(tc.stream), 100000);
        m_bs_processor = &r;
        
        m_bitstream.DataLength = 0;
        DecodeHeader();

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.CropH = 0;
        m_par.mfx.FrameInfo.AspectRatioH = m_par.mfx.FrameInfo.AspectRatioW = 0;

        bool same_fourcc = (old_fourcc == m_par.mfx.FrameInfo.FourCC);
        EXPECT_FALSE(same_fourcc) << "ERROR: two consequential streams have the same FourCC.";
        old_fourcc = m_par.mfx.FrameInfo.FourCC;

        if(m_initialized)
        {
            mfxU32 nSurfPreReset = GetAllocator()->cnt_surf();

            g_tsStatus.disable_next_check();
            Reset();
            if (MFX_ERR_NONE == g_tsStatus.get())
            {
                EXPECT_EQ(nSurfPreReset, GetAllocator()->cnt_surf()) << "ERROR: expected no surface allocation at Reset!\n";
            }
            else if (MFX_ERR_INVALID_VIDEO_PARAM == g_tsStatus.get() ||
                     MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == g_tsStatus.get())
            {
                g_tsLog << "Reset is unavailable, performing decoder Close()-Init() (without session close)\n";

                Close();
                SetPar4_DecodeFrameAsync();
            }

            if(g_tsStatus.get() < 0)
                break;
        }
        else
        {
            SetPar4_DecodeFrameAsync();
        }

        if (tc.n_frames)
        {
            std::unique_ptr<tsSurfaceProvider> ref_decoder = tsSurfaceProvider::CreateIvfVP9Decoder(g_tsStreamPool.Get(tc.stream));
            tsSurfaceComparator v(std::move(ref_decoder));
            m_surf_processor = &v;

            DecodeFrames(tc.n_frames);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_444_chroma_subsampl_change, RunTest_BitDepth<8>, n_cases_8b);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_444_chroma_subsampl_change, RunTest_BitDepth<10>, n_cases_10b);

#undef TEST_NAME

}
