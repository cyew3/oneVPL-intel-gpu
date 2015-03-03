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
          NOTHING = 0
        , MFXVP
        , SESSION
        , UID
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 alloc_mode_plus1;
        mfxU32 zero_ptr;
        mfxU32 num_call_minus1;
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
    {/* 0*/ MFX_ERR_INVALID_HANDLE, 0, SESSION},
    {/* 1*/ MFX_ERR_NULL_PTR, 0, MFXVP},
    {/* 2*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, UID},
    {/* 3*/ MFX_ERR_NONE, },
    {/* 4*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/* 5*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0, 0, {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}},
    {/* 6*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0, 0, {
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}}},
    {/* 7*/ MFX_ERR_NONE, 1 + frame_allocator::ALLOC_MAX, 0, 0},
    {/* 8*/ MFX_ERR_NONE, 1 + frame_allocator::ALLOC_MAX, 0, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/* 9*/ MFX_ERR_MEMORY_ALLOC, 1 + frame_allocator::ALLOC_MIN_MINUS_1, 0, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*10*/ MFX_ERR_NONE, 0, 0, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*11*/ MFX_ERR_NONE, 1 + frame_allocator::ALLOC_MAX, 0, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, 1 + frame_allocator::ALLOC_MIN_MINUS_1, 0, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*13*/ MFX_ERR_NONE, 0, 0, 0, {&tsStruct::mfxVideoParam.AsyncDepth, 1}},
    {/*14*/ MFX_ERR_NONE, 0, 0, 0, {&tsStruct::mfxVideoParam.AsyncDepth, 10}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0, 0, {
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  0},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  0}}},
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0, 0, {
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  4106},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  4106}}},
    {/*17*/ MFX_ERR_NONE, 0, 0, 4},
    {/*18*/ MFX_ERR_NONE, 0, 0, 5},
    {/*19*/ MFX_ERR_NONE, 0, 0, 6},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    bool set_allocator = !!tc.alloc_mode_plus1;
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

        if(set_allocator)
        {
            UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
            GetAllocator()->reset(
                (frame_allocator::AllocMode)(tc.alloc_mode_plus1-1), 
                frame_allocator::LockMode::ENABLE_ALL, 
                frame_allocator::OpaqueAllocMode::ALLOC_ERROR);
            SetFrameAllocator(m_session, GetAllocator());
        }
        // set default param
        m_pPar->mfx.CodecProfile = 1;
        for(mfxU32 i = 0; i < n_par; i ++)
        {
            if(tc.set_par[i].f)
            {
                tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            }
        }

        if(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        {
            mfxExtOpaqueSurfaceAlloc& osa = m_par;

            QueryIOSurf();

            if(1 + frame_allocator::ALLOC_MIN_MINUS_1 == tc.alloc_mode_plus1)
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

    for(mfxU32 i = 0; i <= tc.num_call_minus1; i ++)
    {
        g_tsStatus.expect(tc.sts);
        Init(m_session, m_pPar);

        if(g_tsStatus.get() < 0 && GetAllocator())
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