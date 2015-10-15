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
        MFXPAR = 1
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
