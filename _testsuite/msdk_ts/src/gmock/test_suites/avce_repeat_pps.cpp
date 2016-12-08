/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_repeat_pps.cpp
\* ****************************************************************************** */


/*!
\file avce_repeat_pps.cpp
\brief Gmock test avce_repeat_pps

Description:
This suite tests AVC encoder\n
4 test cases with different RepeatPPS parameter (for HW and SW implementation different expected sts)\n


Algorithm:
- Initializing Media SDK lib
- Initializing suite and case params
- Initializing Frame Allocator
- Remove ext buffer if RepeatPPS = MFX_CODINGOPTION_UNKNOWN
- Run Query
- Check query status
- Check zeroed RepeatPPS param if Query returns ERR
- Init encoder
- Check init status
- If Init returns WRN sts then call GetVideoParam and check changed RepeatPPS

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

//! \brief Test suite namespace
namespace avce_repeat_pps
{

    enum {
        MFX_PAR=1,
    };

    /*! \brief Structure contains statuses should be returned in Query() and Init() methods*/
    struct sts{
            //!* \brief Expected status returned by Query()
            mfxStatus QueryStatus;
            //!* \brief Expected status returned by Init()
            mfxStatus InitStatus;
            //! \brief A constructor
            //! \param q - expected Query() sts
            //! \param i - expected Init() sts
            sts (mfxStatus q, mfxStatus i): QueryStatus(q), InitStatus(i)
            {}
    };

    /*! \brief Structure contains test cases params */
    struct tc_struct
    {
        /*! \brief Statuses should be returned if HW implemented*/
        sts hw_sts;
        /*! \brief Statuses should be returned if SW implemented*/
        sts sw_sts;
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {
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
        //! \brief Initializes FrameAllocator
        /*! \details This method is required because of some troubles on Windows platform
        before Init() calling frameAllocator should be defined*/
        void allocatorInit();

        //! Set of test cases
        static const tc_struct test_case[];

        mfxExtCodingOption2 m_co2;
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
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Width         = 352;
        m_par.mfx.FrameInfo.Height        = 288;
        m_par.mfx.FrameInfo.CropW         = 352;
        m_par.mfx.FrameInfo.CropH         = 288;
        m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }


    void TestSuite::allocatorInit(){
        if(     !m_pFrameAllocator
            && (   (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            if(!GetAllocator())
            {
                if (m_pVAHandle)
                    SetAllocator(m_pVAHandle, true);
                else
                    UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ sts(MFX_ERR_NONE, MFX_ERR_NONE),
                sts(MFX_ERR_NONE, MFX_ERR_NONE),
                {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.RepeatPPS, MFX_CODINGOPTION_UNKNOWN}}},
        {/*01*/ sts(MFX_ERR_NONE, MFX_ERR_NONE),
                sts(MFX_ERR_NONE, MFX_ERR_NONE),
                {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.RepeatPPS, MFX_CODINGOPTION_OFF}}},
        {/*02*/ sts(MFX_ERR_NONE, MFX_ERR_NONE),
                sts(MFX_ERR_UNSUPPORTED, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
                {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.RepeatPPS, MFX_CODINGOPTION_ON}}},
        {/*03*/ sts(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
                sts(MFX_ERR_UNSUPPORTED, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
                {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.RepeatPPS, MFX_CODINGOPTION_ADAPTIVE}}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];
        initParams();
        MFXInit();

        SETPARS(&m_par, MFX_PAR);
        mfxExtCodingOption2 *m_co2 = reinterpret_cast <mfxExtCodingOption2*> (m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));

        if (m_co2->RepeatPPS == MFX_CODINGOPTION_UNKNOWN)
            m_par.RemoveExtBuffer(MFX_EXTBUFF_CODING_OPTION2);


        allocatorInit();
        TS_CHECK_MFX;

        mfxStatus q_sts;
        mfxStatus i_sts;

        if (g_tsImpl == MFX_IMPL_HARDWARE){
            q_sts = tc.hw_sts.QueryStatus;
            i_sts = tc.hw_sts.InitStatus;
        }
        else
        {
            q_sts = tc.sw_sts.QueryStatus;
            i_sts = tc.sw_sts.InitStatus;
        }


        g_tsStatus.expect(q_sts);
        Query();

        if (m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2) && (i_sts<MFX_ERR_NONE)){
            EXPECT_EQ(0, m_co2->RepeatPPS) << "RepeatPPS value wasn't zeroed";
        }

        SETPARS(&m_par, MFX_PAR);
        g_tsStatus.expect(i_sts);
        Init();

        if (i_sts>MFX_ERR_NONE){
            mfxU32 before = m_co2->RepeatPPS;
            GetVideoParam();
            EXPECT_NE(m_co2->RepeatPPS, before) << "RepeatPPS value wasn't changed";
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_repeat_pps);
};