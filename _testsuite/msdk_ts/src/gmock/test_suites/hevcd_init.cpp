/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: hevcd_init.cpp

\* ****************************************************************************** */
#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_init
{

class TestSuite : public tsVideoDecoder
{
    static const mfxU32 n_par = 5;

public:

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 alloc_mode;
        bool set_alloc;
        mfxU32 zero_ptr;
        mfxU32 num_call;
        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

public:

    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC) {}
    ~TestSuite() {}
    int RunTest(tc_struct const&);
    static const unsigned int n_cases;

protected:

    enum
    {
          NONE = 0
        , MFXVP
        , SESSION
        , UID
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_INVALID_HANDLE, frame_allocator::ALLOC_MAX, false, SESSION, 1},
    {/* 1*/ MFX_ERR_NULL_PTR, frame_allocator::ALLOC_MAX, false, MFXVP, 1},
    {/* 2*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, UID, 1},
    {/* 3*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1},
    {/* 4*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, true, NONE, 1},
    {/* 5*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/* 6*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, true, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/* 7*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, true, NONE, 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE}},
    {/* 8*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}},
    {/* 9*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}}},
    {/* 10*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, true, NONE, 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}},
    {/* 11*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, true, NONE, 1, {
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}}},
    {/* 12*/ MFX_ERR_MEMORY_ALLOC, frame_allocator::ALLOC_MIN_MINUS_1, true, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*13*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*14*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, true, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MIN_MINUS_1, false, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MIN_MINUS_1, true, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*17*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.AsyncDepth, 1}},
    {/*18*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.AsyncDepth, 10}},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  0}}},
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  4106}}},
    {/*21*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 4},
    {/*22*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 5},
    {/*23*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 6},
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1,{ &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 1 } },
    {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1,{ &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 1 } },
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1,{ &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 42 } },
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1,{ &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 42 } },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(tc_struct const& tc)
{
    TS_START;

    m_par_set = true; //don't use DecodeHeader
    m_par.AsyncDepth = 5;

    if(tc.zero_ptr == UID)
    {
        m_uid = 0;
    }

    if(tc.zero_ptr != SESSION)
    {
        MFXInit();

        if(m_uid)
        {
            Load();
        }

        for (mfxU32 i = 0; i < n_par; i++)
        {
            if (tc.set_par[i].f)
            {
                tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            }
        }


        if(tc.set_alloc)
        {
            frame_allocator::AllocatorType alloc_type;
            if (g_tsImpl == MFX_IMPL_SOFTWARE) {
                alloc_type = frame_allocator::SOFTWARE;
            } else if (g_tsImpl & MFX_IMPL_VIA_D3D11) {
                alloc_type = frame_allocator::HARDWARE_DX11;
            } else alloc_type = frame_allocator::HARDWARE;

            SetAllocator(
                new frame_allocator(alloc_type,
                                (frame_allocator::AllocMode)        tc.alloc_mode,
                                (frame_allocator::LockMode)         frame_allocator::ENABLE_ALL,
                                (frame_allocator::OpaqueAllocMode)  frame_allocator::ALLOC_ERROR
                               ),
                               false);
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            if ((g_tsImpl != MFX_IMPL_SOFTWARE) && (!m_pVAHandle))
            {
                mfxHDL hdl;
                mfxHandleType type;
                m_pVAHandle = m_pFrameAllocator;
                m_pVAHandle->get_hdl(type, hdl);
                SetHandle(m_session, type, hdl);
                m_is_handle_set = (g_tsStatus.get() >= 0);
            }
        }

        if(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        {
            mfxExtOpaqueSurfaceAlloc& osa = m_par;

            QueryIOSurf();

            if(frame_allocator::ALLOC_MIN_MINUS_1 == tc.alloc_mode)
            {
                m_request.NumFrameSuggested = m_request.NumFrameMin - 1;
            }

            AllocOpaque(m_request, osa);
        }
    }

    if(tc.zero_ptr == MFXVP)
    {
        m_pPar = 0;
    }

    for(mfxU32 i = 0; i < tc.num_call; i ++)
    {
        g_tsStatus.expect(tc.sts);
        Init(m_session, m_pPar);

        if((g_tsStatus.get() < 0) && (tc.sts != MFX_ERR_MEMORY_ALLOC) && (tc.set_alloc))
        {
            EXPECT_EQ(0u, GetAllocator()->cnt_surf());
        }

        if(m_initialized)
        {
            Close();
        }
    }

    TS_END;
    return 0;
}

struct bits;
struct chromas;
template <unsigned v>
struct int_ : std::integral_constant<unsigned, v> {};

static const tsStruct::Field* const FourCC        (&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC);
static const tsStruct::Field* const ChromaFormat  (&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat);
static const tsStruct::Field* const BitDepthLuma  (&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma);

template <unsigned profile, unsigned fourcc, typename, typename>
struct TestSuiteExt;

//should be cpp11's variadics ...
template <
    unsigned profile,
    unsigned fourcc,
    typename b1, typename b2, typename b3,
    typename c1, typename c2, typename c3
>
struct TestSuiteExt<profile, fourcc, bits(b1, b2, b3), chromas(c1, c2, c3)>
    : public TestSuite
{
    static
    int RunTest(unsigned int id);
    static
    unsigned int GetCount();

    static tc_struct const test_cases[];
};

template <
    unsigned profile,
    unsigned fourcc,
    typename b1, typename b2, typename b3, //default correct wrong
    typename c1, typename c2, typename c3  //correct wrong...
>
TestSuite::tc_struct const TestSuiteExt<profile, fourcc, bits(b1, b2, b3), chromas(c1, c2, c3)>::test_cases[] =
{
    {/*24*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {{FourCC, fourcc}, {BitDepthLuma, b1::value}}},
    {/*25*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {{FourCC, fourcc}, {BitDepthLuma, b2::value}}},
    {/*26*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {{FourCC, fourcc}, {ChromaFormat, c1::value}}},

    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {{FourCC, fourcc}, {BitDepthLuma, b3::value}}},

    {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {{FourCC, fourcc}, {ChromaFormat, c2::value}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {{FourCC, fourcc}, {ChromaFormat, c3::value}}},
};

template <
    unsigned profile,
    unsigned fourcc,
    typename b1, typename b2, typename b3,
    typename c1, typename c2, typename c3
>
int TestSuiteExt<profile, fourcc, bits(b1, b2, b3), chromas(c1, c2, c3)>::RunTest(unsigned int id)
{
    auto tc =
        id >= TestSuite::n_cases ?
        test_cases[id - TestSuite::n_cases] : TestSuite::test_case[id];

    TestSuite suite;
    suite.m_par.mfx.CodecProfile = profile;

    return suite.RunTest(tc);
}

template <
    unsigned profile,
    unsigned fourcc,
    typename b1, typename b2, typename b3,
    typename c1, typename c2, typename c3
>
unsigned int TestSuiteExt<profile, fourcc, bits(b1, b2, b3), chromas(c1, c2, c3)>::GetCount()
{ return TestSuite::n_cases + 6; }

#define TS_SEQ(type, a, b, c)  type(int_<a>, int_<b>, int_<c>)
#define TS_BITS(b1, b2, b3)    TS_SEQ(bits,    b1, b2, b3)
#define TS_CHROMAS(c1, c2, c3) TS_SEQ(chromas, c1, c2, c3)

#define TS_REG_TEST_SUITE_TMPL(name, profile, fourcc, bits, chromas) \
    TS_REG_TEST_SUITE(name,                                          \
        (TestSuiteExt<profile, fourcc, bits, chromas>::RunTest),     \
        (TestSuiteExt<profile, fourcc, bits, chromas>::GetCount())   \
    )

TS_REG_TEST_SUITE_TMPL(
    hevcd_init,
    MFX_PROFILE_HEVC_MAIN,
    MFX_FOURCC_NV12,
    TS_BITS(0, 8, 10), TS_CHROMAS(MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV422)
);

TS_REG_TEST_SUITE_TMPL(
    hevcd_422_init,
    MFX_PROFILE_HEVC_REXT,
    MFX_FOURCC_YUY2,
    TS_BITS(0, 8, 10), TS_CHROMAS(MFX_CHROMAFORMAT_YUV422, MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV420)
);

TS_REG_TEST_SUITE_TMPL(
    hevcd_444_init,
    MFX_PROFILE_HEVC_REXT,
    MFX_FOURCC_AYUV,
    TS_BITS(0, 8, 10), TS_CHROMAS(MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV422)
);

TS_REG_TEST_SUITE_TMPL(
    hevc10d_init,
    MFX_PROFILE_HEVC_MAIN10,
    MFX_FOURCC_P010,
    TS_BITS(0, 10, 8), TS_CHROMAS(MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV422)
);

TS_REG_TEST_SUITE_TMPL(
    hevc10d_422_init,
    MFX_PROFILE_HEVC_REXT,
    MFX_FOURCC_Y210,
    TS_BITS(0, 10, 8), TS_CHROMAS(MFX_CHROMAFORMAT_YUV422, MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV420)
);

TS_REG_TEST_SUITE_TMPL(
    hevc10d_444_init,
    MFX_PROFILE_HEVC_REXT,
    MFX_FOURCC_Y410,
    TS_BITS(0, 10, 8), TS_CHROMAS(MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV422, MFX_CHROMAFORMAT_YUV420)
);

#undef SEQ
#undef BITS
#undef CHROMAS

#undef TS_REG_TEST_SUITE_TMPL
}
