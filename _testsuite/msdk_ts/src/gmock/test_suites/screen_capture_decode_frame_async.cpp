#include "ts_decoder.h"
#include "ts_struct.h"

namespace screen_capture_decode_frame_async
{

class TestSuite : tsVideoDecoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_CAPTURE, true, MFX_MAKEFOURCC('C','A','P','T'))
    {
        m_surf_processor = this;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (s.Data.TimeStamp != -1)
        {
            g_tsLog << "ERROR: s.Data.TimeStamp == " << s.Data.TimeStamp << ". Should be -1\n";
            return MFX_ERR_ABORTED;
        }
        return MFX_ERR_NONE;
    }

private:
    enum
    {
        MFXPAR = 1,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*01*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    Load();

    SETPARS(m_pPar, MFXPAR);
    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(g_tsImpl))
    {
        mfxHandleType type;
        mfxHDL hdl;
        UseDefaultAllocator( !!(m_pPar->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) );
        if(GetAllocator() && GetAllocator()->get_hdl(type, hdl))
            SetHandle(m_session, type, hdl);
    }
    Init(m_session, m_pPar);

    g_tsStatus.expect(tc.sts);
    DecodeFrames(3);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(screen_capture_decode_frame_async);

}