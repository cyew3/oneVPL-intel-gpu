/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_b_pyr_control.cpp
\* ****************************************************************************** */

/*!
\file avce_b_pyr_control.cpp
\brief Gmock test avce_b_pyr_control

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK lib
- Set test suite params
- Set test case params
- Set expected Query() sts (depends on lib implementation)
- Call Query() function
- Check returned status
- If returned status isn't MFX_ERR_NONE check BRefType value is zeroed and set it again to defined in test case value
- Set expected Init() sts (depends on lib implementation)
- Call Init() function
- Check returned status
- If returned status isn't MFX_ERR_NONE check BRefType value is zeroed

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_b_pyr_control{

    enum {
        MFX_PAR = 1,
    };
    /*! \brief Structure with Quiery() and Init() expected statuses*/
    struct sts{
        //! Query() expected status
        mfxStatus q_sts;
        //! Init() expected status
        mfxStatus i_sts;
        //! \brief A constructor
        //! \param q - Query() expected status
        //! \param i - Init() expected status
        sts(mfxStatus i, mfxStatus q): q_sts(q), i_sts(i){}
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! Expected statuses of Query() and Init() functions if Software lib version
        sts sw_sts;
        //! Expected statuses of Query() and Init() functions if Hardware lib version
        sts hw_sts;
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

    void TestSuite::initParams() {

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
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Width         = 352;
        m_par.mfx.FrameInfo.Height        = 288;
        m_par.mfx.FrameInfo.CropW         = 352;
        m_par.mfx.FrameInfo.CropH         = 288;
        m_par.mfx.GopRefDist              = 7;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof (mfxExtCodingOption2));
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ sts(MFX_ERR_NONE, MFX_ERR_NONE), 
                sts(MFX_ERR_NONE, MFX_ERR_NONE),
                {{MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_UNKNOWN}}},
        {/*01*/ sts(MFX_ERR_NONE, MFX_ERR_NONE), 
                sts(MFX_ERR_NONE, MFX_ERR_NONE),
                {{MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_OFF}}},
        {/*00*/ sts(MFX_ERR_UNSUPPORTED, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
                sts(MFX_ERR_NONE, MFX_ERR_NONE), 
                {{MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_PYRAMID}}}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];

        MFXInit();
        initParams();
        SETPARS(&m_par, MFX_PAR);

        mfxStatus exp = MFX_ERR_NONE;
        if (g_tsImpl!=MFX_IMPL_SOFTWARE)
            exp = tc.hw_sts.q_sts;
        else
            exp = tc.sw_sts.q_sts;

        g_tsStatus.expect(exp);
        Query();

        mfxExtCodingOption2 *m_co2 =  reinterpret_cast <mfxExtCodingOption2*> (m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));

        if (exp != MFX_ERR_NONE){
            EXPECT_EQ (0, m_co2->BRefType) << "BRefType was not zeroed";
            SETPARS(&m_par, MFX_PAR);
        }

        if (g_tsImpl!=MFX_IMPL_SOFTWARE)
            exp = tc.hw_sts.i_sts;
        else
            exp = tc.sw_sts.i_sts;

        g_tsStatus.expect(exp);
        Init();

        if (exp != MFX_ERR_NONE){
            EXPECT_EQ (0, m_co2->BRefType) << "BRefType was not zeroed";
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_b_pyr_control);
}
