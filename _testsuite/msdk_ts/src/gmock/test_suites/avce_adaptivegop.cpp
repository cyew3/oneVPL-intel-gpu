/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_adaptivegop.cpp
\* ****************************************************************************** */

/*!
\file avce_adaptivegop.cpp
\brief Gmock test avce_adaptivegop

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK lib
- Set test suite params
- Set test case params
- Call Query() function 
- Check returned status
- If returned status is not MFX_ERR_NONE check AdaptiveI and Adaptive B values are zeroed
- Call Init() function 
- Check returned status

*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"


/*! \brief Main test name space */
namespace avce_adaptivegop{

    enum{
        MFX_PAR = 1
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! Expected Query() return status
        mfxStatus q_sts;
        //! Expected Init() return status
        mfxStatus i_sts;

        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;
        } set_par[MAX_NPARS];
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
        void initParams();
        //! Set of test cases
        static const tc_struct test_case[];


    };


    void TestSuite::initParams(){

        m_par.mfx.CodecId                 = MFX_CODEC_AVC;
        m_par.mfx.CodecProfile            = MFX_PROFILE_AVC_HIGH;
        m_par.mfx.CodecLevel              = MFX_LEVEL_AVC_41;
        m_par.mfx.TargetKbps              = 3000;
        m_par.mfx.MaxKbps                 = m_par.mfx.TargetKbps;
        m_par.mfx.RateControlMethod       = MFX_RATECONTROL_CBR;
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.Width         = 352;
        m_par.mfx.FrameInfo.Height        = 288;
        m_par.mfx.FrameInfo.CropW         = 352;
        m_par.mfx.FrameInfo.CropH         = 288;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));

    }

    
    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, 
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_UNKNOWN},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_UNKNOWN}}},
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_OFF},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_OFF}}},
        {/*02*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_OFF}}},
        {/*03*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_OFF},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_ON}}},
        {/*04*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_ON}}},
        {/*05*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_ADAPTIVE},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_ADAPTIVE}}},
        {/*06*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_ADAPTIVE},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_OFF}}},
        {/*07*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveI, MFX_CODINGOPTION_OFF},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.AdaptiveB, MFX_CODINGOPTION_ADAPTIVE}}}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];

        MFXInit();
        initParams();
        SETPARS(&m_par, MFX_PAR);

        g_tsStatus.expect(tc.q_sts);
        Query();

        mfxExtCodingOption2 *m_co2 = reinterpret_cast <mfxExtCodingOption2*>(m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));

        if (tc.q_sts != MFX_ERR_NONE){
            EXPECT_EQ(0, m_co2->AdaptiveI) << "AdaptiveI was not zeroed";
            EXPECT_EQ(0, m_co2->AdaptiveB) << "AdaptiveB was not zeroed";
            SETPARS(&m_par, MFX_PAR);
        }

        g_tsStatus.expect(tc.i_sts);
        Init();
        TS_END;
        return 0;
    }
     //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_adaptivegop);
}