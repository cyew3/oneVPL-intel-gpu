/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2018 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_surface_provider.h"

#define TEST_NAME vp9d_res_change

namespace TEST_NAME
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9){}
    ~TestSuite() { }
    static const size_t n_cases_nv12;
    static const size_t n_cases_p010;
    static const size_t n_cases_ayuv;
    static const size_t n_cases_y410;
    static const size_t n_cases_p016;
    static const size_t n_cases_y416;

    template<decltype(MFX_FOURCC_YV12) fourcc>
    int RunTest_fourcc(const size_t id);

private:
    static const mfxU32 max_num_ctrl     = 3;

    struct tc_struct
    {
        const char* stream;
        mfxU32 n_frames;
        mfxStatus sts;
    };
    int RunTest(const tc_struct (&tcs)[max_num_ctrl]);

    static const tc_struct test_case_nv12[][max_num_ctrl];
    static const tc_struct test_case_p010[][max_num_ctrl];
    static const tc_struct test_case_ayuv[][max_num_ctrl];
    static const tc_struct test_case_y410[][max_num_ctrl];
    static const tc_struct test_case_p016[][max_num_ctrl];
    static const tc_struct test_case_y416[][max_num_ctrl];
};

#define MAKE_TEST_TABLE(FOURCC, L, L_N, M, M_N, S, S_N)                                                                    \
const TestSuite::tc_struct TestSuite::test_case_##FOURCC[][max_num_ctrl] =                                                 \
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
const size_t TestSuite::n_cases_##FOURCC = sizeof(TestSuite::test_case_##FOURCC)/sizeof(TestSuite::tc_struct[max_num_ctrl]);

const int frameCount = 10;

MAKE_TEST_TABLE(nv12, "forBehaviorTest/dif_resol/vp9/8b420/bbc_704x576_8b420_10.ivf", frameCount,
                      "forBehaviorTest/dif_resol/vp9/8b420/bbc_640x480_8b420_10.ivf", frameCount,
                      "forBehaviorTest/dif_resol/vp9/8b420/bbc_352x288_8b420_10.ivf", frameCount)

MAKE_TEST_TABLE(p010, "forBehaviorTest/dif_resol/vp9/10b420/Mobile_ProRes_1280x720_50f_10b420_10.ivf", frameCount,
                      "forBehaviorTest/dif_resol/vp9/10b420/Mobile_ProRes_640x360_50f_10b420_10.ivf", frameCount,
                      "forBehaviorTest/dif_resol/vp9/10b420/Mobile_ProRes_352x288_50f_10b420_10.ivf", frameCount)

MAKE_TEST_TABLE(ayuv, "forBehaviorTest/dif_resol/vp9/8b444/matrix_titles_444_1280x532_15.ivf", frameCount,
                      "forBehaviorTest/dif_resol/vp9/8b444/matrix_titles_444_640x288_15.ivf", frameCount,
                      "forBehaviorTest/dif_resol/vp9/8b444/matrix_titles_444_320x144_15.ivf", frameCount)

MAKE_TEST_TABLE(y410, "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_1920x1080_001_intra_basic_1.4.vp9", frameCount,
                      "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_001_intra_basic_1.4.vp9", frameCount,
                      "conformance/vp9/google/vp93-2-20-10bit-yuv444.ivf", frameCount)

MAKE_TEST_TABLE(p016, "conformance/vp9/SBE/12bit/Syntax_VP9_FC2p2b12_1920x1080_001_intra_basic_1.3.vp9", frameCount,
                      "conformance/vp9/SBE/12bit/Syntax_VP9_FC2p2b12_432x240_001_intra_basic_1.3.vp9", frameCount,
                      "conformance/vp9/google/vp92-2-20-12bit-yuv420.ivf", frameCount)

MAKE_TEST_TABLE(y416, "conformance/vp9/google/vpxenc_704x576_20_444_12bit_tiles.ivf", frameCount,
                      "conformance/vp9/google/vpxenc_352x288_100_444_12bit.ivf", frameCount,
                      "conformance/vp9/google/vp93-2-20-12bit-yuv444.ivf", frameCount)

#undef MAKE_TEST_TABLE

template<decltype(MFX_FOURCC_YV12) fourcc>
int TestSuite::RunTest_fourcc(const size_t id)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12: return RunTest(test_case_nv12[id]);
        case MFX_FOURCC_AYUV: return RunTest(test_case_ayuv[id]);
        case MFX_FOURCC_P010: return RunTest(test_case_p010[id]);
        case MFX_FOURCC_Y410: return RunTest(test_case_y410[id]);
        case MFX_FOURCC_P016: return RunTest(test_case_p016[id]);
        case MFX_FOURCC_Y416: return RunTest(test_case_y416[id]);
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

    int old_width = 1;
    int old_height = 1;

    for (auto& tc : tcs) 
    {
        if(0 == tc.stream)
            break;

        tsBitstreamReaderIVF r(g_tsStreamPool.Get(tc.stream), 100000);
        m_bs_processor = &r;
        
        m_bitstream.DataLength = 0;
        DecodeHeader();

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.CropH = 0;
        m_par.mfx.FrameInfo.AspectRatioH = m_par.mfx.FrameInfo.AspectRatioW = 0;

        bool sameResolution = (old_width == m_par.mfx.FrameInfo.Width) &&
            (old_height == m_par.mfx.FrameInfo.Height);
        EXPECT_FALSE(sameResolution) << "Two consequential streams have the same resolution";
        old_width = m_par.mfx.FrameInfo.Width;
        old_height = m_par.mfx.FrameInfo.Height;

        if(m_initialized)
        {
            mfxU32 nSurfPreReset = GetAllocator()->cnt_surf();

            g_tsStatus.expect(tc.sts);
            Reset();

            EXPECT_EQ(nSurfPreReset, GetAllocator()->cnt_surf()) << "ERROR: expected no surface allocation at Reset!\n";

            if(g_tsStatus.get() < 0)
                break;
        }
        else
        {
            SetPar4_DecodeFrameAsync();
        }

        std::unique_ptr<tsSurfaceProvider> ref_decoder = tsSurfaceProvider::CreateIvfVP9Decoder(g_tsStreamPool.Get(tc.stream));
        tsSurfaceComparator v(std::move(ref_decoder));
        m_surf_processor = &v;

        DecodeFrames(tc.n_frames);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_nv12_res_change,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases_nv12);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_ayuv_res_change,  RunTest_fourcc<MFX_FOURCC_AYUV>, n_cases_ayuv);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_p010_res_change, RunTest_fourcc<MFX_FOURCC_P010>, n_cases_p010);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_y410_res_change, RunTest_fourcc<MFX_FOURCC_Y410>, n_cases_y410);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_420_p016_res_change, RunTest_fourcc<MFX_FOURCC_P016>, n_cases_p016);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_444_y416_res_change, RunTest_fourcc<MFX_FOURCC_Y416>, n_cases_y416);

#undef TEST_NAME

}
