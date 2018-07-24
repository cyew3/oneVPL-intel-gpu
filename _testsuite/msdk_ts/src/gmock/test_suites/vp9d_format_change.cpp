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

#define TEST_NAME vp9d_format_change

namespace TEST_NAME
{

class TestSuite : tsVideoDecoder
{
public:
    static const unsigned int n_cases = 6;
    static const mfxU32 fourCC_list[n_cases];

    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9){}
    ~TestSuite() { }

    template<mfxU32 target_fourCC>
    int RunTest_Chroma(const unsigned int id);
private:
};

const mfxU32 TestSuite::fourCC_list[TestSuite::n_cases] = {
    MFX_FOURCC_NV12,
    MFX_FOURCC_AYUV,
    MFX_FOURCC_P010,
    MFX_FOURCC_Y410,
    MFX_FOURCC_P016,
    MFX_FOURCC_Y416
};

static std::string GetStreamByFourCC(mfxU32 fourCC) {
    switch (fourCC) {
    case MFX_FOURCC_NV12:
        return "conformance/vp9/SBE/8bit/Syntax_VP9_432x240_001_intra_basic_1.3.vp9";
    case MFX_FOURCC_AYUV:
        return "conformance/vp9/SBE/8bit_444/Syntax_VP9_FC2p1ss444_432x240_001_intra_basic_1.4.vp9";
    case MFX_FOURCC_P010:
        return "conformance/vp9/SBE/10bit/Syntax_VP9_FC2p2b10_432x240_001_intra_basic_1.3.vp9";
    case MFX_FOURCC_Y410:
        return "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_001_intra_basic_1.4.vp9";
    case MFX_FOURCC_P016:
        return "conformance/vp9/google/vp92-2-20-12bit-yuv420.ivf";
    case MFX_FOURCC_Y416:
        return "conformance/vp9/google/vp93-2-20-12bit-yuv444.ivf";
    default :
        return "";
    }
}

template<mfxU32 target_fourCC>
int TestSuite::RunTest_Chroma(const unsigned int id)
{
    mfxU32 n_frames = 2;
    mfxU32 test_fourCC = fourCC_list[id];
    if (target_fourCC == test_fourCC)
    {
        return 0;
    }
    mfxU32 tested_fourCC[2] = { target_fourCC, test_fourCC };

    TS_START;

    if (g_tsHWtype <= MFX_HW_ICL && (test_fourCC == MFX_FOURCC_P016 || test_fourCC == MFX_FOURCC_Y416)) {
        g_tsLog << "WARNING: This case is for ICL+ platforms\n";
        throw tsSKIP;
    }

    for (auto& fourCC : tested_fourCC)
    {
        g_tsStreamPool.Get(GetStreamByFourCC(fourCC));
    }

    g_tsStreamPool.Reg();
    m_use_memid = true;

    mfxU32 old_fourcc = 0;

    for (auto& fourCC : tested_fourCC)
    {
        std::string stream = GetStreamByFourCC(fourCC);
        g_tsLog << "target FourCC " << target_fourCC << " switch to " << fourCC <<"\n";

        g_tsLog << "New stream: " << stream << "\n";

        tsBitstreamReaderIVF r(g_tsStreamPool.Get(stream), 100000);
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
                // Don't use the surfaces on next decode cycle
                // they maybe wrong type, shape, taste
                FreeSurfaces();
                // On next decode cycle call QueryIOSurf again
                // don't use previous cached request
                m_request.NumFrameMin = 0;
                SetPar4_DecodeFrameAsync();
            }

            if(g_tsStatus.get() < 0)
                break;
        }
        else
        {
            SetPar4_DecodeFrameAsync();
        }

        if (n_frames)
        {
            std::unique_ptr<tsSurfaceProvider> ref_decoder = tsSurfaceProvider::CreateIvfVP9Decoder(g_tsStreamPool.Get(stream));
            tsSurfaceComparator v(std::move(ref_decoder));
            m_surf_processor = &v;

            DecodeFrames(n_frames);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_nv12_format_change,  RunTest_Chroma<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_ayuv_format_change,  RunTest_Chroma<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_p010_format_change, RunTest_Chroma<MFX_FOURCC_P010>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_y410_format_change, RunTest_Chroma<MFX_FOURCC_Y410>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_420_p016_format_change, RunTest_Chroma<MFX_FOURCC_P016>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_444_y416_format_change, RunTest_Chroma<MFX_FOURCC_Y416>, n_cases);

#undef TEST_NAME

}
