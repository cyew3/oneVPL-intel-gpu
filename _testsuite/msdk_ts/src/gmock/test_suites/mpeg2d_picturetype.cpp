#include "ts_decoder.h"
#include "ts_struct.h"

namespace mpeg2d_picturetype
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2) {}
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < PoolSize(); i++)
        {
            mfxFrameSurface1* p = GetSurface(i);
            if (0 != p->Data.NumExtParam)
            {
                delete p->Data.ExtParam[0];
                delete [] p->Data.ExtParam;
            }
        }
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_frames = 20;

    enum
    {
        DISPORDER = 1,
        RAND,
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
    {/*01*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_I.mpg"  , 0},
    {/*02*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_IP.mpg" , 1},
    {/*03*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 2},
    {/*04*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 2},
    {/*05*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 3},
    {/*06*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_fld_IPB.mpg", 4},
    {/*07*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 3},
    {/*08*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_fld_IPB.mpg", 4},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

//Table of stream's structure aligned with 3 frames
// for interlace cases aligned with 2 frames (except first frame)
const TestSuite::stream_struct TestSuite::pictype_case[] =
{
    {0, 5,  { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF,                                   }},

    {1, 15, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF }},

    {2, 15, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B                  , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B                  , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B                  , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B                  , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B                  , MFX_FRAMETYPE_B                    }},

    {3, 13, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xI|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xI|MFX_FRAMETYPE_xREF}},

    {4, 13, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB                                     , MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF}}

};

class Verifier : public tsSurfaceProcessor
{
public:
    std::list <mfxU16> expected_pictype;
    mfxU16 frame;
    Verifier(const mfxU16* type, const mfxU16 N)
        : expected_pictype(type, type + N), frame(0) {}
    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (s.Data.NumExtParam != 1)
        {
            expected_pictype.pop_front();
            frame++;
            return MFX_ERR_NONE;
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
            g_tsLog << "ERROR: Frame#" << frame << " real picture type = " << pictype  << " is not equal expected = " << expected_pictype.front() << "\n";
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        expected_pictype.pop_front();
        frame++;

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

    Init();
    AllocSurfaces();
    mfxU32 step =  (tc.mode & RAND) ? 2 : 1;
    for (mfxU32 i = 0; i < PoolSize(); i+=step)
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

TS_REG_TEST_SUITE_CLASS(mpeg2d_picturetype);
}
