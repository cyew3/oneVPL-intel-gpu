#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define TEST_NAME avce_vui_flag

namespace TEST_NAME
{

#define ON   MFX_CODINGOPTION_ON
#define OFF  MFX_CODINGOPTION_OFF

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC ){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 3;

    struct tc_struct
    {
        struct
        {
            mfxU16    DisableVUI;                     /* tri-state option */
            mfxU16    FixedFrameRate;                 /* tri-state option */

            mfxU16    VuiNalHrdParameters;
            mfxU16    VuiVclHrdParameters;
            mfxU16    PicTimingSEI       ;

            mfxU16    AspectRatioInfoPresent;         /* tri-state option */
            mfxU16    OverscanInfoPresent;            /* tri-state option */
            mfxU16    OverscanAppropriate;            /* tri-state option */
            mfxU16    TimingInfoPresent;              /* tri-state option */
            mfxU16    BitstreamRestriction;           /* tri-state option */
            mfxU16    LowDelayHrd;                    /* tri-state option */
            mfxU16    MotionVectorsOverPicBoundaries; /* tri-state option */
        } In, Expected;
        mfxStatus query_sts;
        mfxU16    ratecontrol;
    };
    void SetParams(tsExtBufType<mfxVideoParam>& par, const tc_struct& tc);
    void CheckParams(tsExtBufType<mfxVideoParam>& par, const tc_struct& tc);

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ {OFF,0 }, {OFF,0, },  },
    {/*01*/ {ON,0  }, {ON, 0, }, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    {/*02*/ {ON,0  }, {ON,OFF, OFF,OFF,OFF, OFF,OFF,0,OFF,OFF,OFF }, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    {/*03*/ {ON,0,  OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,}, {ON, 0, OFF,OFF,OFF, OFF,OFF,0,OFF,OFF,OFF,OFF,} },
    {/*04*/ {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,}, {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,} },
    {/*05*/ {OFF,0, OFF,OFF,OFF, ON,OFF,OFF,OFF,OFF,0,OFF,},    {OFF,0, OFF,OFF,OFF, ON, OFF,OFF,OFF,OFF,OFF,OFF,} },  //AspectRatioInfoPresent
    {/*06*/ {OFF,0, OFF,OFF,OFF, OFF,ON,OFF,OFF,OFF,0,OFF,},    {OFF,0, OFF,OFF,OFF, OFF,ON,OFF,OFF,OFF,OFF,OFF,} }, //OverscanInfoPresent
    {/*07*/ {OFF,0, OFF,OFF,OFF, OFF,OFF,ON,OFF,OFF,0,OFF,},    {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,},
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM }, //OverscanAppropriate, OverscanInfoPresent is required for this flag
    {/*08*/ {OFF,0, OFF,OFF,OFF, OFF,ON,ON,OFF,OFF,0,OFF,},     {OFF,0, OFF,OFF,OFF, OFF,ON,ON,OFF,OFF,OFF,OFF,} },  //OverscanAppropriate
    {/*09*/ {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,ON,OFF,0,OFF,},    {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,ON,OFF,OFF,OFF,} }, //TimingInfoPresent
    {/*10*/ {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,ON,0,OFF,},    {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,ON,OFF,OFF,} }, //BitstreamRestriction
    {/*11*/ {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,ON,OFF,},  {OFF,0, 0,0,OFF,     OFF,OFF,OFF,OFF,OFF,OFF,OFF,},
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM}, //LowDelayHrd without HrdParameters
    {/*12*/ {OFF,0, ON,ON,OFF,   OFF,OFF,OFF,OFF,OFF,ON,OFF,},  {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,},
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM }, //LowDelayHrd with HRD but on CQP ratecontrol
    {/*13*/ {OFF,0, ON,ON,OFF,   OFF,OFF,OFF,OFF,OFF,ON,OFF,},  {OFF,0, ON,ON,OFF, OFF,OFF,OFF,OFF,OFF,ON,OFF,},
        MFX_ERR_NONE, MFX_RATECONTROL_VBR }, //LowDelayHrd with HRD
    {/*14*/ {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,ON,},  {OFF,0, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,}, 
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM }, //MotionVectorsOverPicBoundaries, BitstreamRestriction is required for this flag
    {/*15*/ {OFF,ON, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,},{OFF,OFF, OFF,OFF,OFF, OFF,OFF,OFF,OFF,OFF,OFF,OFF,},
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM }, //FixedFrameRate, TimingInfoPresent is required for this flag
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


class Verifier : public tsBitstreamProcessor, private tsParserH264AU
{
public:
    Verifier(tsExtBufType<mfxVideoParam>& par)
        :cod1( *((mfxExtCodingOption*)  par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION))  ),
         cod2( *((mfxExtCodingOption2*) par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2)) ), 
         cod3( *((mfxExtCodingOption3*) par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3)) )
    {
        tsParserH264AU::set_trace_level(0);

        exp_vui_parameters_present_flag = IsOn(cod2.DisableVUI) ? 0 : 1;
        if(exp_vui_parameters_present_flag)
        {
            exp_vui_parameters_present_flag =
                IsOn(cod3.AspectRatioInfoPresent) ||
                IsOn(cod3.OverscanInfoPresent   ) ||
                IsOn(cod3.OverscanAppropriate   ) ||
                IsOn(cod3.TimingInfoPresent     ) ||
                IsOn(cod1.VuiNalHrdParameters   ) ||
                IsOn(cod1.VuiVclHrdParameters   ) ||
                IsOn(cod3.BitstreamRestriction  ) ||
                IsOn(cod3.LowDelayHrd           );
        }

        SetExpected(cod3.AspectRatioInfoPresent        , exp_aspect_ratio_info_present_flag         );
        SetExpected(cod3.OverscanInfoPresent           , exp_overscan_info_present_flag             );

        mfxU16 OverscanAppropriate = IsOn(cod3.OverscanInfoPresent) ? cod3.OverscanAppropriate : MFX_CODINGOPTION_OFF;
        SetExpected(OverscanAppropriate           , exp_overscan_appropriate_flag              , 0);
        SetExpected(cod3.TimingInfoPresent             , exp_timing_info_present_flag               );
        SetExpected(cod3.BitstreamRestriction          , exp_bitstream_restriction_flag             );
        
        mfxU16 LowDelayHrd = (IsOn(cod1.VuiNalHrdParameters) || IsOn(cod1.VuiVclHrdParameters)) ? cod3.LowDelayHrd : MFX_CODINGOPTION_OFF;
        SetExpected(LowDelayHrd                   , exp_low_delay_hrd_flag                     , 
            (cod1.VuiNalHrdParameters || cod1.VuiVclHrdParameters) ? 1 : 0);

        //When the motion_vectors_over_pic_boundaries_flag syntax element is not present, motion_vectors_over_pic_boundaries_flag value shall be inferred to be equal to 1.
        mfxU16 MotionVectorsOverPicBoundaries = IsOn(cod3.BitstreamRestriction) ? cod3.MotionVectorsOverPicBoundaries : MFX_CODINGOPTION_OFF;
        SetExpected(MotionVectorsOverPicBoundaries, exp_motion_vectors_over_pic_boundaries_flag, 1);
    }
    ~Verifier() {}

    bool IsOn(const mfxU16& option)
    {
        return MFX_CODINGOPTION_ON == option;
    }

    void SetExpected(const mfxU16& option, Bs8u& expected, Bs8u def = 1)
    {
        if(MFX_CODINGOPTION_ON == option)
            expected = 1;
        else if(MFX_CODINGOPTION_OFF == option)
            expected = 0;
        else
            expected = def;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        tsParserH264AU::SetBuffer(bs);
        mfxU32 checked = 0;

        while (checked++ < nFrames)
        {
            UnitType& hdr = ParseOrDie();
            for (UnitType::nalu_param* nalu = hdr.NALU; nalu < hdr.NALU + hdr.NumUnits; nalu ++)
            {
                if(0x07 == nalu->nal_unit_type) //SPS
                {
                    EXPECT_EQ( exp_vui_parameters_present_flag, nalu->SPS->vui_parameters_present_flag);

                    if(nalu->SPS->vui_parameters_present_flag && MFX_CODINGOPTION_ON != cod2.DisableVUI)
                        EXPECT_NE(nullptr, nalu->SPS->vui) << "ERROR: VUI is missing in encoded stream!\n";

                    if(nalu->SPS->vui_parameters_present_flag && nalu->SPS->vui && MFX_CODINGOPTION_ON != cod2.DisableVUI)
                    {
                        const vui_params *vui = nalu->SPS->vui;

                        EXPECT_EQ(exp_aspect_ratio_info_present_flag          , vui->aspect_ratio_info_present_flag         );
                        EXPECT_EQ(exp_overscan_info_present_flag              , vui->overscan_info_present_flag             );
                        EXPECT_EQ(exp_overscan_appropriate_flag               , vui->overscan_appropriate_flag              );
                        EXPECT_EQ(exp_timing_info_present_flag                , vui->timing_info_present_flag               );
                        EXPECT_EQ(exp_bitstream_restriction_flag              , vui->bitstream_restriction_flag             );
                        EXPECT_EQ(exp_low_delay_hrd_flag                      , vui->low_delay_hrd_flag                     );
                        EXPECT_EQ(exp_motion_vectors_over_pic_boundaries_flag , vui->motion_vectors_over_pic_boundaries_flag);
                    }
                }
            }
        }

        return MFX_ERR_NONE;
    }

private:
    const mfxExtCodingOption  cod1;
    const mfxExtCodingOption2 cod2;
    const mfxExtCodingOption3 cod3;

    Bs8u exp_vui_parameters_present_flag;
    Bs8u exp_aspect_ratio_info_present_flag;
    Bs8u exp_overscan_info_present_flag;
    Bs8u exp_overscan_appropriate_flag;
    Bs8u exp_timing_info_present_flag;
    Bs8u exp_bitstream_restriction_flag;
    Bs8u exp_low_delay_hrd_flag;
    Bs8u exp_motion_vectors_over_pic_boundaries_flag;
};

void TestSuite::SetParams(tsExtBufType<mfxVideoParam>& par, const tc_struct& tc)
{
    mfxExtCodingOption&  cod1 = par;
    mfxExtCodingOption2& cod2 = par;
    mfxExtCodingOption3& cod3 = par;

    cod1.VuiNalHrdParameters = tc.In.VuiNalHrdParameters;
    cod1.VuiVclHrdParameters = tc.In.VuiVclHrdParameters;
    cod1.PicTimingSEI        = tc.In.PicTimingSEI       ;

    cod2.DisableVUI     = tc.In.DisableVUI    ;
    cod2.FixedFrameRate = tc.In.FixedFrameRate;

    cod3.AspectRatioInfoPresent         = tc.In.AspectRatioInfoPresent        ;
    cod3.OverscanInfoPresent            = tc.In.OverscanInfoPresent           ;
    cod3.OverscanAppropriate            = tc.In.OverscanAppropriate           ;
    cod3.TimingInfoPresent              = tc.In.TimingInfoPresent             ;
    cod3.BitstreamRestriction           = tc.In.BitstreamRestriction          ;
    cod3.LowDelayHrd                    = tc.In.LowDelayHrd                   ;
    cod3.MotionVectorsOverPicBoundaries = tc.In.MotionVectorsOverPicBoundaries;

    if(tc.ratecontrol)
        par.mfx.RateControlMethod = tc.ratecontrol;
    if(tc.ratecontrol)
        set_brc_params(&par);
}

void TestSuite::CheckParams(tsExtBufType<mfxVideoParam>& par, const tc_struct& tc)
{
    mfxExtCodingOption&  cod1 = par;
    mfxExtCodingOption2& cod2 = par;
    mfxExtCodingOption3& cod3 = par;

    if(tc.Expected.VuiNalHrdParameters           ) EXPECT_EQ(tc.Expected.VuiNalHrdParameters            , cod1.VuiNalHrdParameters            );
    if(tc.Expected.VuiVclHrdParameters           ) EXPECT_EQ(tc.Expected.VuiVclHrdParameters            , cod1.VuiVclHrdParameters            );
    if(tc.Expected.PicTimingSEI                  ) EXPECT_EQ(tc.Expected.PicTimingSEI                   , cod1.PicTimingSEI                   );
    if(tc.Expected.DisableVUI                    ) EXPECT_EQ(tc.Expected.DisableVUI                     , cod2.DisableVUI                     );
    if(tc.Expected.FixedFrameRate                ) EXPECT_EQ(tc.Expected.FixedFrameRate                 , cod2.FixedFrameRate                 );
    if(tc.Expected.AspectRatioInfoPresent        ) EXPECT_EQ(tc.Expected.AspectRatioInfoPresent         , cod3.AspectRatioInfoPresent         );
    if(tc.Expected.OverscanInfoPresent           ) EXPECT_EQ(tc.Expected.OverscanInfoPresent            , cod3.OverscanInfoPresent            );
    if(tc.Expected.OverscanAppropriate           ) EXPECT_EQ(tc.Expected.OverscanAppropriate            , cod3.OverscanAppropriate            );
    if(tc.Expected.TimingInfoPresent             ) EXPECT_EQ(tc.Expected.TimingInfoPresent              , cod3.TimingInfoPresent              );
    if(tc.Expected.BitstreamRestriction          ) EXPECT_EQ(tc.Expected.BitstreamRestriction           , cod3.BitstreamRestriction           );
    if(tc.Expected.LowDelayHrd                   ) EXPECT_EQ(tc.Expected.LowDelayHrd                    , cod3.LowDelayHrd                    );
    if(tc.Expected.MotionVectorsOverPicBoundaries) EXPECT_EQ(tc.Expected.MotionVectorsOverPicBoundaries , cod3.MotionVectorsOverPicBoundaries );
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    mfxExtCodingOption&  cod1 = m_par;
    mfxExtCodingOption2& cod2 = m_par;
    mfxExtCodingOption3& cod3 = m_par;

    SetParams(m_par, tc);
    g_tsStatus.expect(tc.query_sts);
    Query();

    SetParams(m_par, tc);
    g_tsStatus.expect(tc.query_sts);
    Init();

    GetVideoParam();
    CheckParams(m_par, tc);

    if(m_initialized)
    {
        Verifier verif(m_par);
        m_bs_processor = &verif;
        EncodeFrames(5);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}
