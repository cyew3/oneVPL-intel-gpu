#include "ts_encoder.h"
#include "ts_struct.h"

namespace avce_cqp_hrd
{

class TestSuite : public tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}

    int RunTest(unsigned int id);

    enum
    {
        MFXPAR = 1,
        EXT_COD
    };

    enum
    {
        FEATURE_ON = 1,
        FEATURE_OFF
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
        } set_par[5];
    };
    static const tc_struct test_case[];
    static const unsigned int n_cases;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, FEATURE_ON, {}},
    {/*01*/ MFX_ERR_NONE, FEATURE_ON, {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 800}},
    {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FEATURE_OFF, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR}},
    {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FEATURE_OFF, {EXT_COD, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF}},
    {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FEATURE_OFF, {EXT_COD, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_ON}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    MFXInit();

    // remove default values
    m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 0;

    // set required parameters to turn on CQP HRD
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.FrameInfo.FrameRateExtN = 30;
    m_par.mfx.FrameInfo.FrameRateExtD = 1;
    m_par.mfx.TargetKbps = 600;
    mfxU32 fr = mfxU32(m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD);
    // buffer = 0.5 sec
    mfxU32 br = m_par.mfx.MaxKbps ? m_par.mfx.MaxKbps : m_par.mfx.TargetKbps;
    m_par.mfx.BufferSizeInKB = mfxU16(br / fr * mfxU16(fr / 2));
    m_par.mfx.InitialDelayInKB = mfxU16(m_par.mfx.BufferSizeInKB / 2);

    mfxExtCodingOption& cod = m_par;
    cod.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
    cod.NalHrdConformance = MFX_CODINGOPTION_OFF;

    // set test case parameters
    SETPARS(m_pPar, MFXPAR);
    SETPARS(&cod, EXT_COD);

    g_tsStatus.expect(tc.sts);
    Init();

    // Check CQP and VuiNalHrdParameters
    GetVideoParam();
    g_tsLog << "Check value of RateControlMethod...\n";
    if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        g_tsLog << "\nERROR: RateControlMethod != MFX_RATECONTROL_CQP\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    mfxExtCodingOption* co = (mfxExtCodingOption*)m_par.ExtParam[0];
    g_tsLog << "Check value of VuiNalHrdParameters...\n";
    if (tc.mode == FEATURE_ON)
    {
        if (co->VuiNalHrdParameters != MFX_CODINGOPTION_ON)
        {
            g_tsLog << "\nERROR: VuiNalHrdParameters != MFX_CODINGOPTION_ON\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    } else if (tc.mode == FEATURE_OFF)
    {
        if (co->VuiNalHrdParameters != MFX_CODINGOPTION_OFF)
        {
            g_tsLog << "\nERROR: VuiNalHrdParameters != MFX_CODINGOPTION_OFF\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    }

    // EncodeFrames(3);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_cqp_hrd);
};