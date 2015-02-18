#include "ts_decoder.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace screen_capture_init
{
typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size);
}

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
        ALLOC_OPAQUE,
        ALLOC_OPAQUE_MORE,
        ALLOC_OPAQUE_LESS
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
        struct
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*01*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},

    // No crop, but should be ignored on Init()
    {/*02*/ MFX_ERR_NONE, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 16},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 16},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 144},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 176}}},

    // W/H are required
    {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0}}},

    // FourCC cases (only NV12/RGB32 supported)
    {/*04*/ MFX_ERR_NONE, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}},
    {/*06*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P8}},
    {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}},
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P8_TEXTURE}},

    // ChromaFormat cases (only 420 for NV12 and 444 for RGB32 supported)
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME}},
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}}},
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV400}},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV411}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422H},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422V},
                                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}}},

    {/*16*/ MFX_ERR_NONE, ALLOC_OPAQUE, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*17*/ MFX_ERR_NONE, ALLOC_OPAQUE_LESS, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*18*/ MFX_ERR_NONE, ALLOC_OPAQUE_MORE, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},

    // ext buffers
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtDecodedFrameInfo  )}},
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtDecVideoProcessing)}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    Load();

    SETPARS(m_pPar, MFXPAR);

    if (tc.mode & ALLOC_OPAQUE)
    {
        mfxExtOpaqueSurfaceAlloc osa = {0};
        osa.Header.BufferId = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION;
        osa.Header.BufferSz = sizeof(mfxExtOpaqueSurfaceAlloc);
        QueryIOSurf();
        if (tc.mode == ALLOC_OPAQUE_LESS)
        {
            m_request.NumFrameSuggested = m_request.NumFrameMin = m_request.NumFrameMin - 1;
        }
        else if (tc.mode == ALLOC_OPAQUE_MORE)
        {
            m_request.NumFrameSuggested = m_request.NumFrameMin = m_request.NumFrameMin + 1;
        }

        AllocOpaque(m_request, osa);
    }

    if (tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(m_par, tc.set_buf.buf, tc.set_buf.size);
    }

    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(screen_capture_init);

}