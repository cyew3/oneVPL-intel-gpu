#include "ts_decoder.h"
#include "ts_struct.h"

namespace screen_capture_query
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_CAPTURE, true, MFX_MAKEFOURCC('C','A','P','T')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

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
    {/*00*/ MFX_ERR_NONE, 1, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*01*/ MFX_ERR_NONE, 1, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},

    {/*02*/ MFX_ERR_UNSUPPORTED, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0}}},

    {/*03*/ MFX_ERR_NONE, 1, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*04*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}},
    {/*05*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P8}},
    {/*06*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}},
    {/*07*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P8_TEXTURE}},

    {/*08*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME}},
    {/*09*/ MFX_ERR_UNSUPPORTED, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}}},
    {/*10*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV400}},
    {/*11*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV411}},
    {/*12*/ MFX_ERR_UNSUPPORTED, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422H},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}}},
    {/*13*/ MFX_ERR_UNSUPPORTED, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422V},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}}},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    Load();

    SETPARS(m_pPar, MFXPAR);

    g_tsStatus.expect(tc.sts);

    Query(m_session, m_pPar, m_pParOut);

    if (tc.mode==0)
    {
        for (mfxU32 i = 0; i < MAX_NPARS; i++)
        {
            mfxU64 val = 0;
            if (tc.set_par[i].f && tc.set_par[i].ext_type == MFXPAR)
            {
                val = tsStruct::get(m_pParOut, *tc.set_par[i].f);
            }

            if (val != 0)
            {
                g_tsLog << "ERROR: Field " << tc.set_par[i].f->name << " should be zero-ed (real is " << val << ")\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(screen_capture_query);

}