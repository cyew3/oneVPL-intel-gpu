/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016 Intel Corporation. All Rights Reserved.
//
//
*/

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
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR = 1,
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
    {/*00*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB}}},
    {/*01*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED}}},
    {/*02*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED_NOREF}}},
    {/*03*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED_SCD}}},

    // PTIR modes are unsupported by regular VPP
    {/*04*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_DOUBLE}}},
    {/*05*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_SINGLE}}},
    {/*06*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FULL_FR_OUT}}},
    {/*07*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_HALF_FR_OUT}}},
    {/*08*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_24FPS_OUT}}},
    {/*09*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN}}},
    {/*10*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_30FPS_OUT}}},
    {/*11*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE}}},
    {/*12*/ MFX_ERR_UNSUPPORTED, 0, {
        {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE},
        {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, 1},
        {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.TelecineLocation, 1}}
    },

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxStatus sts = tc.sts;

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_par.AsyncDepth = 1;

    SETPARS(&m_par, MFX_PAR);

    mfxExtVPPDeinterlacing* extVPPDi = (mfxExtVPPDeinterlacing*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DEINTERLACING);
    if (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)
    {
                if (extVPPDi->Mode == MFX_DEINTERLACING_ADVANCED_SCD)
            sts = MFX_ERR_UNSUPPORTED; // only linux supports SCD mode
    }
    /* one more limitation for VPP SCD */
    if ((MFX_OS_FAMILY_LINUX == g_tsOSFamily) &&
        (MFX_HW_BXT == g_tsHWtype) &&
        (extVPPDi->Mode == MFX_DEINTERLACING_ADVANCED_SCD))
        sts = MFX_ERR_UNSUPPORTED; // only Linux for BDW & SKL supports SCD mode for now

    SetHandle();

    g_tsStatus.expect(sts);
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_deinterlacing);

}
