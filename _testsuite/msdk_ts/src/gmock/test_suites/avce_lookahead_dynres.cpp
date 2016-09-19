/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_encoder.h"
#include "ts_struct.h"

namespace avce_lookahead_dynres
{
class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}

    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    struct tc_par;

    enum
    {
        MFX_PAR = 1,
        LA,
        RESET,
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
    {/*0*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_OFF},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 496},
       },
    },
    {/*1*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 496},
       },
    },
    {/*2*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_4x},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 496},
       },
    },
    {/*3*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000000},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 20},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 576},
        {RESET, &tsStruct::mfxVideoParam.mfx.TargetKbps, 5000000},
       },
    },
    {/*4*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000000},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 20},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
        {RESET, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1000000},
       },
    },
    {/*5*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000000},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 20},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 176},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 144},
        {RESET, &tsStruct::mfxVideoParam.mfx.TargetKbps, 750000},
       },
    },
    {/*6*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000000},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 40},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 576},
        {RESET, &tsStruct::mfxVideoParam.mfx.TargetKbps, 5000000},
       },
    },
    {/*7*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000000},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 40},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
        {RESET, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1000000},
       },
    },
    {/*8*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000000},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x},
        {LA, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 40},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 176},
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 144},
        {RESET, &tsStruct::mfxVideoParam.mfx.TargetKbps, 750000},
       },
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
    mfxExtCodingOption2* cod2 = (mfxExtCodingOption2*)m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);

    SETPARS(m_pPar, MFX_PAR);
    SETPARS(cod2, LA);

    set_brc_params(&m_par);

    SetFrameAllocator();
    AllocSurfaces();

    if (m_par.mfx.LowPower == MFX_CODINGOPTION_ON)
    {
        g_tsLog << "WARNING: CASE WAS SKIPPED\n";
        //behavior LowPower with LA_RATE_CONTROL checks in avce_constrains_vbr case 55.
    }
    else
    {

        if (MFX_LOOKAHEAD_DS_4x == cod2->LookAheadDS)
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        else
            g_tsStatus.expect(MFX_ERR_NONE);
        Init(m_session, &m_par);

        SetFrameAllocator();
        AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 1920 * 1920 * 3);
        EncodeFrames(3);

        SETPARS(m_pPar, RESET);

        g_tsStatus.expect(tc.sts);
        Reset(m_session, m_pPar);
    }
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_lookahead_dynres);
};

