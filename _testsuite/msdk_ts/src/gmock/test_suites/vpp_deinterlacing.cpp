/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//     Copyright (c) 2016-2017 Intel Corporation. All Rights Reserved.
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
    {/*00*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_BOB}}},

    {/*01*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_BOB},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,  MFX_FOURCC_Y210},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_Y210}}},

    {/*02*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED}}},

    {/*03*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,  MFX_FOURCC_Y210},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_Y210}}},

    {/*04*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED_NOREF}}},

    {/*05*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED_NOREF},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,  MFX_FOURCC_Y210},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_Y210}}},

    {/*06*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED_SCD}}},

    {/*07*/ MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED_SCD},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,  MFX_FOURCC_Y210},
                              {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_Y210}}},

    // PTIR modes are unsupported by regular VPP
    {/*08*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_DOUBLE}}},
    {/*09*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_SINGLE}}},
    {/*10*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FULL_FR_OUT}}},
    {/*11*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_HALF_FR_OUT}}},
    {/*12*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_24FPS_OUT}}},
    {/*13*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN}}},
    {/*14*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_30FPS_OUT}}},
    {/*15*/ MFX_ERR_UNSUPPORTED, 0, {{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE}}},
    {/*16*/ MFX_ERR_UNSUPPORTED, 0, {
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

    if ((MFX_FOURCC_Y210 == m_par.vpp.In.FourCC || MFX_FOURCC_Y210 == m_par.vpp.Out.FourCC) && g_tsHWtype < MFX_HW_CNL)
        sts = MFX_ERR_INVALID_VIDEO_PARAM; // y210 deinterlacing works since CNL

    mfxExtVPPDeinterlacing* extVPPDi = (mfxExtVPPDeinterlacing*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DEINTERLACING);
    if (MFX_DEINTERLACING_ADVANCED_SCD == extVPPDi->Mode)
    {
        if (MFX_OS_FAMILY_WINDOWS == g_tsOSFamily)
            sts = MFX_ERR_UNSUPPORTED; // only linux supports SCD mode

        if (MFX_OS_FAMILY_LINUX == g_tsOSFamily && MFX_HW_APL == g_tsHWtype)
            sts = MFX_ERR_UNSUPPORTED; // only Linux for BDW & SKL supports SCD mode for now
    }

    SetHandle();

    g_tsStatus.expect(sts);
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_deinterlacing);

}
