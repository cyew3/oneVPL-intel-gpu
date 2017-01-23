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

#include <climits>

namespace hevcd_stream_change
{

#define EXPECT_NE_THROW(expected, actual, message)  \
do {                                                \
    if(expected == actual)                          \
    {                                               \
        EXPECT_NE(expected, actual) << message;     \
        throw tsFAIL;                               \
    }                                               \
} while (0,0)

#define EXPECT_EQ_THROW(expected, actual, message)  \
do {                                                \
    if(expected != actual)                          \
    {                                               \
        EXPECT_EQ(expected, actual) << message;     \
        throw tsFAIL;                               \
    }                                               \
} while (0,0)

class TestSuite : public tsVideoDecoder
{

protected:

    static const mfxU32 max_num_ctrl     = 3;
    struct tc_struct
    {
        const char* stream;
        mfxU32 n_frames;
        mfxStatus sts;
    };

public:

    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }

    int RunTest(tc_struct const* first, tc_struct const* last);
};

struct RefVerifier : tsSurfaceProcessor
{
    mfxU32            frame;
    mfxFrameSurface1* work_surf;

    RefVerifier()
        : frame(0)
        , work_surf(nullptr)
    {}

    static const int luma           = 0;
    static const int chroma         = 1;
    static const int bits_per_pixel = 2;
    static const int chroma_height  = 3;

    std::tuple<mfxU8 const* /*Luma*/, mfxU8 const* /*Croma*/, size_t /*Bits per Pixel*/, int /*Chroma height*/>
    plane(mfxFrameSurface1 const* s)
    {
        switch (s->Info.FourCC)
        {
            case MFX_FOURCC_NV12:
                return std::make_tuple(s->Data.Y + s->Info.CropX + s->Info.CropY * s->Data.Pitch, s->Data.U + s->Info.CropX + s->Info.CropY / 2 * s->Data.Pitch, sizeof(mfxU8) * CHAR_BIT, s->Info.CropH / 2);
            case MFX_FOURCC_P010:
                return std::make_tuple(s->Data.Y + s->Info.CropX + s->Info.CropY * s->Data.Pitch, s->Data.U + s->Info.CropX + s->Info.CropY / 2 * s->Data.Pitch, sizeof(mfxU16) * CHAR_BIT, s->Info.CropH / 2);

            case MFX_FOURCC_NV16:
                return std::make_tuple(s->Data.Y + s->Info.CropX + s->Info.CropY * s->Data.Pitch, s->Data.U + s->Info.CropX + s->Info.CropY / 2 * s->Data.Pitch, sizeof(mfxU8) * CHAR_BIT, s->Info.CropH);
            case MFX_FOURCC_P210:
                return std::make_tuple(s->Data.Y + s->Info.CropX + s->Info.CropY * s->Data.Pitch, s->Data.Y + s->Info.CropX + s->Info.CropY * s->Data.Pitch, sizeof(mfxU16) * CHAR_BIT, s->Info.CropH);

            case MFX_FOURCC_AYUV:
                return std::make_tuple(s->Data.V + s->Info.CropX + s->Info.CropY * s->Data.Pitch, nullptr, sizeof(mfxU8) * CHAR_BIT * 4, 0);
            case MFX_FOURCC_YUY2:
                return std::make_tuple(s->Data.Y + s->Info.CropX + s->Info.CropY * s->Data.Pitch, nullptr, sizeof(mfxU8) * CHAR_BIT * 2, 0);
            case MFX_FOURCC_Y210:
                return std::make_tuple(s->Data.Y + s->Info.CropX + s->Info.CropY * s->Data.Pitch, nullptr, sizeof(mfxU16) * CHAR_BIT * 2, 0);
            case MFX_FOURCC_Y410:
                return std::make_tuple(reinterpret_cast<mfxU8 const*>(s->Data.Y410 + s->Info.CropX + s->Info.CropY * s->Data.Pitch), nullptr, sizeof(mfxU8) * CHAR_BIT * 4, 0);

            default:
                assert(!"Unsupported format");
                return std::make_tuple(nullptr, nullptr, 0, 0);
        }
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& ref_surf)
    {
        assert(work_surf && "ERROR: Working surface is not set");

        //compare frames
        EXPECT_EQ(ref_surf.Info.CropH,        work_surf->Info.CropH);
        EXPECT_EQ(ref_surf.Info.CropW,        work_surf->Info.CropW);
        EXPECT_EQ(ref_surf.Info.AspectRatioH, work_surf->Info.AspectRatioH);
        EXPECT_EQ(ref_surf.Info.AspectRatioW, work_surf->Info.AspectRatioW);
        EXPECT_EQ(ref_surf.Info.FourCC,       work_surf->Info.FourCC);

        /* https://jira01.devtools.intel.com/browse/MQ-507 */

        auto w = plane(work_surf);
        auto r = plane(&ref_surf);

        //Luma
        {
            assert(std::get<luma>(w) && "ERROR: Unexpected null pointer for working surface");
            assert(std::get<luma>(r) && "ERROR: Unexpected null pointer for reference surface");

            EXPECT_TRUE(
                check(std::get<luma>(w), std::get<luma>(r), std::get<bits_per_pixel>(w), work_surf->Info.CropW, work_surf->Info.CropH, work_surf->Data.Pitch, ref_surf.Data.Pitch)
            ) << "ERROR: Surface #"<< frame <<" decoded by resolution-changed session is not b2b with surface decoded by verification session!!!\n";
        }

        //Chroma
        if (std::get<chroma>(w))
        {
            assert(std::get<chroma>(w) && "ERROR: Unexpected null pointer for working surface");
            assert(std::get<chroma>(r) && "ERROR: Unexpected null pointer for reference surface");

            EXPECT_TRUE(
                check(std::get<chroma>(w), std::get<chroma>(r), std::get<bits_per_pixel>(w), work_surf->Info.CropW, std::get<chroma_height>(w), work_surf->Data.Pitch, ref_surf.Data.Pitch)
            ) << "ERROR: Surface #"<< frame <<" decoded by resolution-changed session is not b2b with surface decoded by verification session!!!\n";
        }

        ++frame;
        return MFX_ERR_NONE;
    }

private:

    bool check(mfxU8 const* w, mfxU8 const* r, size_t bits_per_pixel, size_t width, size_t height, size_t w_pitch, size_t r_pitch)
    {
        auto end = w + height * w_pitch;
        for (; w != end; w += w_pitch, r += r_pitch)
        {
            auto l_size = width * bits_per_pixel / CHAR_BIT;
            if (memcmp(w, r, l_size))
                break;
        }

        return w == end;
    }
};

class Verifier
    : public tsSurfaceProcessor
{
    tsVideoDecoder    decoder;
    tsBitstreamReader reader;

    RefVerifier       rv;

public:

    Verifier(const char* fname, mfxU32 codec)
        : tsSurfaceProcessor()
        , decoder(codec)
        , reader(fname, 100000)
    {
        decoder.m_surf_processor = &rv;
        decoder.m_bs_processor = &reader;
        //we need to have output surface from second decoder instance for every incoming working surface
        decoder.m_par.AsyncDepth = 1;
    }

    ~Verifier()
    {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        rv.work_surf = &s;
        decoder.DecodeFrames(1);

        return MFX_ERR_NONE;
    }
};

const char* stream_path = "forBehaviorTest/dif_resol/";

int TestSuite::RunTest(tc_struct const* first, tc_struct const* last)
{
    TS_START;

    std::for_each(
        first, last,
        [](tc_struct const& tc) { if (tc.stream) g_tsStreamPool.Get(tc.stream, stream_path); }
    );

    g_tsStreamPool.Reg();
    m_use_memid = true;

    for (; first != last; ++first)
    {
        auto& tc = *first;
        if(0 == tc.stream)
            break;

        g_tsStreamPool.Get(tc.stream, stream_path);

        tsBitstreamReader r(g_tsStreamPool.Get(tc.stream), 100000);
        m_bs_processor = &r;
        
        m_bitstream.DataLength = 0;
        DecodeHeader();

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.CropH = 0;
        m_par.mfx.FrameInfo.AspectRatioH = m_par.mfx.FrameInfo.AspectRatioW = 0;

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

        Verifier v(g_tsStreamPool.Get(tc.stream), m_par.mfx.CodecId);
        m_surf_processor = &v;

        DecodeFrames(tc.n_frames);
    }

    TS_END;
    return 0;
}

template <unsigned fourcc, typename>
struct TestSuiteExt
    : public TestSuite
{
    static
    int RunTest(unsigned int id);

    static tc_struct const test_cases[][max_num_ctrl];
    static unsigned int const n_cases;
};

#define TS_REG_INCLUDE_HEVCD_STREAM_CHANGE
#include "hevcd_stream_change_res.cxx"
#include "hevcd_stream_change_bitdepth.cxx"
#include "hevcd_stream_change_chroma.cxx"
#undef TS_REG_INCLUDE_HEVCD_STREAM_CHANGE

template <unsigned fourcc, typename tag>
int TestSuiteExt<fourcc, tag>::RunTest(unsigned int id)
{
    TestSuite suite;
    tc_struct const* first = test_cases[id];
    return suite.RunTest(first, first + max_num_ctrl);
}

}
