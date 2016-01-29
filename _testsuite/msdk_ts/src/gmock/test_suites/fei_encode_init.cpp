#include "ts_encoder.h"
#include "ts_struct.h"

namespace fei_encode_init
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;

    enum
    {
        NULL_SESSION = 1,
        NULL_PARAMS,
    };

    enum
    {
        MFXPAR = 1,
        MFXFEIPAR,
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
    {/*00*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*01*/ MFX_ERR_NULL_PTR, NULL_PARAMS},

    // IOPattern cases
    {/*02*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},
    {/*03*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},

    // RateControlMethods (only CQP supported)
    {/*04*/ MFX_ERR_NONE, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPI, 21},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 23},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 24}}},
    {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0}}},
    {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 600}}},

    {/*07*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4}},
    {/*08*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 4}},

    /* invalid values for each field for Init() function */
    {/*09*/ MFX_ERR_MEMORY_ALLOC, 0, {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 0xffff}},
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.Protected, 0xffff}},
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, 0x50}},
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, 0x40}},
    {/*13*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 0xffffffff}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecId, 0xff}},
    {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, 0x40}},
    {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, 0xffff}},
    {/*17*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumThread, 0xffff}},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 8}},
    {/*19*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0xffff}},
    {/*20*/ MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0xffff}},
    {/*21*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 0xf0}},
    {/*22*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 0xffff}},
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16}},
    {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 0xff}},
    {/*25*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 0xffff}},
    {/*26*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 0xffff}},
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 0x80}},
    {/*28*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxDecFrameBuffering, 0xffff}},
    {/*29*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.ExtendedPicStruct, 0x80}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TimeStampCalc, 4}},
    {/*31*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.SliceGroupsPresent, 0xffff}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.JPEGChromaFormat, 0x40}},
    {/*33*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.InterleavedDec, 0x40}},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.Interleaved, 0xffff}},
    {/*35*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.Quality, 0xffff}},
    {/*36*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RestartInterval, 0xffff}},
    {/*37*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0xffff}},
    {/*38*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0xffff}},
    {/*39*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0xffff}},
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, 0xffff}},
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0xf1}},
    {/*42*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0xf1}},
    {/*43*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0xffff}},
    {/*44*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0xffff}},
    {/*45*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0xffff}},
    {/*46*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0xffff}},
    {/*47*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0xffffffff}},
    {/*48*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0xffffffff}},
    {/*49*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 0xffff}},
    {/*50*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 0xffff}},
    {/*51*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x80}},
    {/*52*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 0xff}},
    {/*53*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXFEIPAR, &tsStruct::mfxExtFeiParam.Func, MFX_FEI_FUNCTION_PAK}},
    {/*54*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXFEIPAR, &tsStruct::mfxExtFeiParam.SingleFieldProcessing, 0x40}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if (!(tc.mode & NULL_SESSION))
    {
        MFXInit();
    }

    m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    // set up parameters for case
    SETPARS(m_pPar, MFXPAR);
    mfxExtFeiParam& feiParam = m_par;
    SETPARS(&feiParam, MFXFEIPAR);

    bool hw_surf = m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;
    if (hw_surf)
        SetFrameAllocator(m_session, m_pVAHandle);

    if (tc.mode == NULL_PARAMS)
    {
        m_pPar = 0;
    }
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_init);
};
