/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_mbext_brc.cpp
\* ****************************************************************************** */

/*!
\file avce_mbext_brc.cpp
\brief Gmock test avce_mbext_brc

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK lib
- Set test suite params
- Set test case params
- Set expected sts (depends on test case params and lib implimentation)
- Call function which is chosen in test case
- Check returned status

*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_mbext_brc{

    enum{
        MFX_PAR = 1,
        CDO2_PAR = 2,
    };

    enum {
        INIT = 1,
        QUERY = 2,
        RESET = 3,
    };

    /*!\brief Structure of test suite parameters */

    struct tc_struct{
        //! Function code (1 - Init(), 2 - Query(), 3 - Reset())
        mfxU32 function;
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
        tsExtBufType <mfxVideoParam> par;

        par.mfx.CodecId                 = MFX_CODEC_AVC;
        par.mfx.CodecProfile            = MFX_PROFILE_AVC_HIGH;
        par.mfx.CodecLevel              = MFX_LEVEL_AVC_41;
        par.mfx.TargetKbps              = 5000;
        par.mfx.MaxKbps                 = par.mfx.TargetKbps;
        par.mfx.RateControlMethod       = MFX_RATECONTROL_CBR;
        par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.Width         = 352;
        par.mfx.FrameInfo.Height        = 288;
        par.mfx.FrameInfo.CropW         = 352;
        par.mfx.FrameInfo.CropH         = 288;
        par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;;
        return par;
    }

/*!\note Some test cases are commented because of ExtBRC option was deprecated*/
    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ INIT, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON}}},
        {/*01*/ INIT, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF}}},
        {/*02*/ INIT, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, 100}}}, //Invalid value
        {/*03*/ INIT, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_UNKNOWN}}},
        {/*04*/ INIT, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ADAPTIVE}}},

        {/*05*/ QUERY, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON}}},
        {/*06*/ QUERY, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF}}},
        {/*07*/ QUERY, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, 100}}}, //Invalid value
        {/*08*/ QUERY, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_UNKNOWN}}},
        {/*09*/ QUERY, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ADAPTIVE}}},

        {/*10*/ RESET, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON}}},
        {/*11*/ RESET, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF}}},
        {/*12*/ RESET, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, 100}}}, //Invalid value
        {/*13*/ RESET, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_UNKNOWN}}},
        {/*14*/ RESET, {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ADAPTIVE}}},

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];
        MFXInit();
        tsExtBufType <mfxVideoParam> par = initParams();
        m_par = par;
        SETPARS (&par, CDO2_PAR);

        mfxExtCodingOption2* m_co2 = (mfxExtCodingOption2*) par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);

        if (tc.function != RESET){
            SETPARS (&m_par, CDO2_PAR);
            m_co2 = (mfxExtCodingOption2*) m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        }

        mfxStatus exp = MFX_ERR_NONE;
        mfxStatus expected = MFX_ERR_NONE;

        if (g_tsHWtype < MFX_HW_HSW && m_co2->MBBRC == MFX_CODINGOPTION_ON)
            exp = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (m_co2->MBBRC == MFX_CODINGOPTION_ADAPTIVE)
            exp = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (tc.function == QUERY){
            if (m_co2->MBBRC == 100){
                expected = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            } else {
                expected = exp;
            }
            g_tsStatus.expect(expected);
            Query();
        }

        if (tc.function == INIT || tc.function == RESET){
            if (m_co2->MBBRC == 100 && tc.function != RESET){
                expected = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            } else if (tc.function == RESET){
                     expected=MFX_ERR_NONE;
            } else {
                expected = exp;
            }

            g_tsStatus.expect(expected);
            Init();
        }

        if (tc.function == RESET){
            SETPARS(&m_par, CDO2_PAR);
            if(m_co2->MBBRC == 100) {
                expected = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            } else {
                expected = exp;
            }
            g_tsStatus.expect(expected);
            Reset();
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_mbext_brc);
}



