#include "ts_decoder.h"
#include "ts_struct.h"

namespace avcd_picturetype
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC) {}
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < PoolSize(); i++)
        {
            mfxFrameSurface1* p = GetSurface(i);
            p->Data.NumExtParam = 0;
            delete [] p->Data.ExtParam;
        }
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_frames = 20;

    enum
    {
        DISPORDER = 1,
        DECORDER,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        std::string stream;
        mfxU16 struct_id;
    };
    struct stream_struct
    {
        mfxU16 id;
        mfxU16 n_frames;
        mfxU16 pictype[max_frames];
    };

    static const tc_struct test_case[];
    static const stream_struct pictype_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/*00*/ MFX_ERR_NONE, DECORDER,  "forBehaviorTest/picturetype/salesman_176x144_f15p_refdist6_gop12.h264"           , 0},
    {/*00*/ MFX_ERR_NONE, DISPORDER, "forBehaviorTest/picturetype/salesman_176x144_f5p_refdist1_gop4.h264"             , 1},
    {/*00*/ MFX_ERR_NONE, DISPORDER, "forBehaviorTest/picturetype/salesman_176x144_f5p_refdist1_gop1_idrinterval4.h264", 2},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

//Table of stream's structure aligned with 3 frames
const TestSuite::stream_struct TestSuite::pictype_case[] =
{
    {0, 15, { MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_B|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_B                                    , MFX_FRAMETYPE_B|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_B                                    , MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_B|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_B                                    , MFX_FRAMETYPE_B|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_B                  ,}},

    {1, 5,  { MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF,                                   }},

    {2, 5,  { MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF                  , MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF                  ,                                   }}
};

class Verifier : public tsSurfaceProcessor
{
public:
    std::list <mfxU16> expected_pictype;
    Verifier(const mfxU16* type, const mfxU16 N)
        : expected_pictype(type, type + N) {}
    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (s.Data.NumExtParam != 1)
        {
            g_tsLog << "ERROR: expected number of external buffers is not equal 1\n";
            return MFX_ERR_ABORTED;
        }
        if (s.Data.ExtParam[0] == NULL)
        {
            g_tsLog << "ERROR: null pointer ExtParam\n";
            return MFX_ERR_NULL_PTR;
        }
        if (s.Data.ExtParam[0]->BufferId != MFX_EXTBUFF_DECODED_FRAME_INFO || 
            s.Data.ExtParam[0]->BufferSz == 0)
        {
            g_tsLog << "ERROR: Wrong external buffer or zero size buffer\n";
            return MFX_ERR_NOT_INITIALIZED;
        }
        mfxU16 pictype = ((mfxExtDecodedFrameInfo*)s.Data.ExtParam[0])->FrameType;
        if (expected_pictype.empty())
            return MFX_ERR_NONE;
        if (pictype != expected_pictype.front())
        {
            g_tsLog << "ERROR: Real picture type = " << pictype  << " is not equal expected = " << expected_pictype.front() << "\n";
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        expected_pictype.pop_front();

        return MFX_ERR_NONE;
    }

};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    const stream_struct& pt = pictype_case[tc.struct_id];
    MFXInit();

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    if (tc.mode & DECORDER)
        m_par.mfx.DecodedOrder = 1;
    Init();
    AllocSurfaces();
    for (mfxU32 i = 0; i < PoolSize(); i++)
    {
        mfxFrameSurface1* p = GetSurface(i);
        p->Data.ExtParam = new mfxExtBuffer*[1];
        p->Data.ExtParam[0] = (mfxExtBuffer*)new mfxExtDecodedFrameInfo;
        memset(p->Data.ExtParam[0], 0, sizeof(mfxExtDecodedFrameInfo));
        p->Data.ExtParam[0]->BufferId = MFX_EXTBUFF_DECODED_FRAME_INFO;
        p->Data.ExtParam[0]->BufferSz = sizeof(mfxExtDecodedFrameInfo);
        p->Data.NumExtParam = 1;
    }
    Verifier v(pt.pictype, pt.n_frames);
    m_surf_processor = &v;

    DecodeFrames(pt.n_frames);

    g_tsStatus.expect(tc.sts);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_picturetype);
}
