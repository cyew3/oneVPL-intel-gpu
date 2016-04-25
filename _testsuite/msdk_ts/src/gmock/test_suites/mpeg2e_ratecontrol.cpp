/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

File Name: mpeg2e_ratecontrol.cpp

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace mpeg2e_ratecontrol
{
    enum
    {
        MFX_PAR = 1
    };

    struct tc_struct
    {
        mfxStatus   queryStatus;
        mfxStatus   initStatus;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_MPEG2) { }
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        //       queryStatus                        initStatus
        {/*00*/ MFX_ERR_NONE,                      MFX_ERR_NONE,                        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}},
        {/*01*/ MFX_ERR_NONE,                      MFX_ERR_NONE,                        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR}},
        {/*02*/ MFX_ERR_NONE,                      MFX_ERR_NONE,                        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP}},
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1}},
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2}},
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3}},
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4}},
        {/*07*/ MFX_ERR_UNSUPPORTED,               MFX_ERR_INVALID_VIDEO_PARAM     ,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA}},
        {/*08*/ MFX_ERR_UNSUPPORTED,               MFX_ERR_INVALID_VIDEO_PARAM     ,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ}},
        {/*09*/ MFX_ERR_UNSUPPORTED,               MFX_ERR_INVALID_VIDEO_PARAM     ,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM}},
        {/*10*/ MFX_ERR_UNSUPPORTED,               MFX_ERR_INVALID_VIDEO_PARAM     ,    {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        m_pPar->mfx.CodecProfile = 0x40;
        m_pPar->mfx.CodecLevel = 0x8;

        SETPARS(&m_par, MFX_PAR);
        mfxU16 rateControl = m_par.mfx.RateControlMethod;

        set_brc_params(&m_par);

        m_pPar->mfx.CodecId = MFX_CODEC_MPEG2;
        m_pParOut->mfx.CodecId = MFX_CODEC_MPEG2;

        MFXInit();

        g_tsStatus.expect(tc.queryStatus);
        Query();

        if(MFX_ERR_UNSUPPORTED == tc.queryStatus)
        {
            EXPECT_EQ(0, m_pParOut->mfx.RateControlMethod) << "ERROR: ratecontrol method was not zeroed) \n";
        }
        if(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == tc.queryStatus)
        {
            EXPECT_NE(rateControl, m_pParOut->mfx.RateControlMethod) << "ERROR: ratecontrol method was not changed) \n";
        }

        SETPARS(&m_par, MFX_PAR);
        switch (m_par.mfx.RateControlMethod)
        {
        case MFX_RATECONTROL_RESERVED1:
        case MFX_RATECONTROL_RESERVED2:
        case MFX_RATECONTROL_RESERVED3:
        case MFX_RATECONTROL_RESERVED4:
            m_par.mfx.TargetKbps = 4000;
            break;
        default:
            break;
        }

        g_tsStatus.expect(tc.initStatus);
        Init();

        if(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == tc.initStatus)
        {
            tsExtBufType<mfxVideoParam> par_out = m_par;
            GetVideoParam();
            GetVideoParam(m_session, &par_out);
            if(m_par.mfx.RateControlMethod == MFX_RATECONTROL_LA || m_par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
            {
                mfxExtCodingOption2& co2_old = m_par;
                mfxExtCodingOption2& co2_new = par_out;
                EXPECT_NE(co2_old.LookAheadDepth, co2_new.LookAheadDepth) << "LookAheadDepth was not changed) \n";
            }
            EXPECT_NE(rateControl, m_pParOut->mfx.RateControlMethod) << "ERROR: RateControlMethod was not changed) \n";
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(mpeg2e_ratecontrol);
}
