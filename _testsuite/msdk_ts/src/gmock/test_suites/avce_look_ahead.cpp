/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

File Name: avce_look_ahead.cpp
\* ****************************************************************************** */

/*!
\file avce_look_ahead.cpp
\brief Gmock test avce_look_ahead

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK lib
- Set test suite params
- Set tes case params
- Set expected Query status
- Call Query() function
- Check Query() return status
- If returned status isn't MFX_ERR_NONE check LookAheadDepth and LookAheadDS chenges
- Set expected Init status
- Call Init() function
- Check Init() return status
- if returned status is MFX_WRN_INCOMATIBLE_VIDEO_PARAM call GetVideoParam and Check it chages
- if returned status is MFX_ERR_NONE encode frames and check encoding status

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_look_ahead{

    enum{
        MFX_PAR = 1,
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct{
        //! Expected Query() return status
        mfxStatus q_sts;
        //! Expected Init() return status
        mfxStatus i_sts;
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair{

            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        }set_par[MAX_NPARS];

    };

    //!\brief Main test class
    class TestSuite:tsVideoEncoder{
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) { }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common mfor whole test suite
        tsExtBufType<mfxVideoParam> initParams();
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType<mfxVideoParam> par;
        par.mfx.CodecId = MFX_CODEC_AVC;
        par.mfx.TargetKbps = 3000;
        par.mfx.MaxKbps = par.mfx.TargetKbps;
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.ChromaFormat = 1;
        par.mfx.FrameInfo.PicStruct = 1;
        par.mfx.FrameInfo.Width = 352;
        par.mfx.FrameInfo.Height = 288;
        par.mfx.FrameInfo.CropW = 352;
        par.mfx.FrameInfo.CropH = 288;
        par.mfx.GopRefDist = 1;
        par.IOPattern = 2;
        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof (mfxExtCodingOption2));
        return par;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 9},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_OFF}}},

        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_OFF}}},

        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 100},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_OFF}}},

        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_OFF}}},

        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 101},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_UNKNOWN}}},

        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 9},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_UNKNOWN}}},

        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_UNKNOWN}}},

        {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_UNKNOWN}}},

        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 141},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x}}},

        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 9},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x}}},

        {/*10*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x}}},

        {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_2x}}},

        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 201},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, 4}}},

        {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 9},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_4x}}},

        {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_4x}}},

        {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 100},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_4x}}},

        {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, MFX_LOOKAHEAD_DS_4x}}},

        {/*17*/ MFX_ERR_NONE, MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, 10},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption2.LookAheadDS, 4}}}

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];

        MFXInit();

        m_par = initParams();
        SETPARS(&m_par, MFX_PAR);
        tsExtBufType<mfxVideoParam> par = m_par;
        

        mfxExtCodingOption2 *m_co2 = reinterpret_cast <mfxExtCodingOption2*> (m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));
        mfxExtCodingOption2 *co2 = reinterpret_cast <mfxExtCodingOption2*> (par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));

        mfxStatus exp = tc.q_sts;
        if (g_tsHWtype < MFX_HW_HSW)
            exp = MFX_ERR_UNSUPPORTED;
        if (g_tsImpl == MFX_IMPL_SOFTWARE && !(m_co2->LookAheadDS == MFX_LOOKAHEAD_DS_UNKNOWN
                                          || m_co2->LookAheadDS == MFX_LOOKAHEAD_DS_OFF )
                                          && m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA)
            exp = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        g_tsStatus.expect(exp);
        Query();

        if (exp ==  MFX_ERR_UNSUPPORTED && par.mfx.RateControlMethod == MFX_RATECONTROL_LA)
            EXPECT_EQ(0, m_par.mfx.RateControlMethod) << "RateControlMethod was not zeroed";

        if (exp ==  MFX_ERR_UNSUPPORTED && par.mfx.RateControlMethod != MFX_RATECONTROL_LA && g_tsImpl == MFX_IMPL_SOFTWARE)
            EXPECT_NE(m_co2->LookAheadDS, co2->LookAheadDS) << "LookAheadDS was not changed";

        if (exp == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && co2->LookAheadDS!=MFX_LOOKAHEAD_DS_4x)
            EXPECT_NE(m_co2->LookAheadDepth, co2->LookAheadDepth) << "LookAheadDepth was not changed";

        m_par = initParams();
        SETPARS(&m_par, MFX_PAR);
        par = m_par;
        m_co2 = reinterpret_cast <mfxExtCodingOption2*> (m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));
        co2 = reinterpret_cast <mfxExtCodingOption2*> (par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));
        
        exp = tc.i_sts;
        if (g_tsHWtype < MFX_HW_HSW)
            exp = MFX_ERR_UNSUPPORTED;
        if (g_tsImpl == MFX_IMPL_SOFTWARE && !(m_co2->LookAheadDS == MFX_LOOKAHEAD_DS_UNKNOWN
                                          || m_co2->LookAheadDS == MFX_LOOKAHEAD_DS_OFF )
                                          && m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA)
            exp = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        g_tsStatus.expect(exp);
        Init();

        if (exp == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM){
            GetVideoParam();
            if (co2->LookAheadDS != MFX_LOOKAHEAD_DS_4x)
                EXPECT_NE(m_co2->LookAheadDepth, co2->LookAheadDepth) << "LookAheadDepth was not changed";
            if (g_tsImpl == MFX_IMPL_SOFTWARE && !(m_co2->LookAheadDS == MFX_LOOKAHEAD_DS_UNKNOWN
                                          || m_co2->LookAheadDS == MFX_LOOKAHEAD_DS_OFF ))
                EXPECT_NE(m_co2->LookAheadDS, co2->LookAheadDS) << "LookAheadDS was not changed";
        }

        if (exp == MFX_ERR_NONE){
            for (mfxU16 submitted = 0; submitted <= (m_co2->LookAheadDepth+1); submitted++){
                mfxStatus sts = EncodeFrameAsync();
                EXPECT_EQ(sts, (submitted == m_co2->LookAheadDepth+1 ? MFX_ERR_NONE:MFX_ERR_MORE_DATA));
            }
        }
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_look_ahead);
}

