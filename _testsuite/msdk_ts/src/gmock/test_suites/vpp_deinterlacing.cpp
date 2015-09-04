#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_deinterlacing
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    {
        memset(&m_vpp_di, 0, sizeof(mfxExtVPPDeinterlacing));
        m_vpp_di.Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
        m_vpp_di.Header.BufferSz = sizeof(mfxExtVPPDeinterlacing);
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    tsNoiseFiller m_noise;
    mfxExtVPPDeinterlacing m_vpp_di;

    enum
    {
        MFX_PAR = 1,
        EXT_VPP
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
    {/*00*/ MFX_ERR_NONE, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB}}},
    {/*01*/ MFX_ERR_NONE, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED}}},
    {/*02*/ MFX_ERR_NONE, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED_NOREF}}},
    
    // PTIR modes are unsupported by regular VPP
    {/*03*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_DOUBLE}}},
    {/*04*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_SINGLE}}},
    {/*05*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FULL_FR_OUT}}},
    {/*06*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_HALF_FR_OUT}}},
    {/*07*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_24FPS_OUT}}},
    {/*08*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN}}},
    {/*09*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_30FPS_OUT}}},
    {/*10*/ MFX_ERR_UNSUPPORTED, 0, {{EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE}}},
    {/*11*/ MFX_ERR_UNSUPPORTED, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE},
        {EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, 1},
        {EXT_VPP, &tsStruct::mfxExtVPPDeinterlacing.TelecineLocation, 1}}
    }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_par.AsyncDepth = 1;

    SETPARS(&m_vpp_di, EXT_VPP);
    SETPARS(m_pPar, MFX_PAR);

    mfxStatus expect = tc.sts;
    if (m_vpp_di.Mode == MFX_DEINTERLACING_ADVANCED_NOREF && g_tsOSFamily == MFX_OS_FAMILY_WINDOWS) {
        expect = MFX_ERR_UNSUPPORTED;
    }

    m_par.ExtParam = new mfxExtBuffer*[1];
    m_par.ExtParam[0] = (mfxExtBuffer*)&m_vpp_di;
    m_par.NumExtParam = 1;

    SetHandle();

    g_tsStatus.expect(expect);
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_deinterlacing);

}
