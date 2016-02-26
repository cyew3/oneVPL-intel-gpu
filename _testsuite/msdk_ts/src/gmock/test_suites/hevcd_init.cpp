#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_init
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 5;

    enum
    {
          NONE = 0
        , MFXVP
        , SESSION
        , UID
    };

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
    {/* 7*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}},
    {/* 8*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}}},
    {/* 9*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, true, NONE, 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}},
    {/* 10*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, true, NONE, 1, {
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}}},
    {/* 11*/ MFX_ERR_MEMORY_ALLOC, frame_allocator::ALLOC_MIN_MINUS_1, true, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*12*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*13*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, true, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MIN_MINUS_1, false, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MIN_MINUS_1, true, NONE, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*16*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.AsyncDepth, 1}},
    {/*17*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 1, {&tsStruct::mfxVideoParam.AsyncDepth, 10}},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  0}}},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, frame_allocator::ALLOC_MAX, false, NONE, 1, {
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  4106}}},
    {/*20*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 4},
    {/*21*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 5},
    {/*22*/ MFX_ERR_NONE, frame_allocator::ALLOC_MAX, false, NONE, 6},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
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

        // set default param
        m_pPar->mfx.CodecProfile = 1;
        for (mfxU32 i = 0; i < n_par; i++)
        {
            if (tc.set_par[i].f)
            {
                tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            }
        }


        if(tc.set_alloc)
        {
            SetAllocator(
            new frame_allocator((frame_allocator::AllocatorType)    (g_tsImpl == MFX_IMPL_SOFTWARE) ? frame_allocator::SOFTWARE : frame_allocator::HARDWARE,
                                (frame_allocator::AllocMode)        tc.alloc_mode,
                                (frame_allocator::LockMode)         frame_allocator::ENABLE_ALL,
                                (frame_allocator::OpaqueAllocMode)  frame_allocator::ALLOC_ERROR
                               ),
                               false);
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            if (g_tsImpl != MFX_IMPL_SOFTWARE)
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
            EXPECT_EQ(0, GetAllocator()->cnt_surf());
        }

        if(m_initialized)
        {
            Close();
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevcd_init);

}
