/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_intra_refresh.cpp
\* ****************************************************************************** */

/*!
\file avce_intra_refresh.cpp
\brief Gmock test avce_intra_refresh

Description:
This suite tests AVC encoder\n

Algorithm:
- Initializing MSDK lib
- Set suite params (AVC encoder params)
- Set case params
- Set expected Query() status
- Call Query() and check IntRef values if necessary (invalid values)
- Set case params
- Set expected Init() status
- Call Init()
- If returned status is MFX_ERR_NONE call GetVideoParam() and check IntRef values were chenged
- Reinitialize MSDK lib
- Call Init()
- Set case params
- Set expected Reset() status
- Call Reset()
- Check returned status

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_intra_refresh{

    enum{
        MFX_PAR = 1,
        CDO2_PAR = 2,
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct{
        //! Expected Query() status
        mfxStatus q_sts;
        //! Expected Init() status
        mfxStatus i_sts;
        //! Expected Reset() status
        mfxStatus r_sts;
        /*! \brief Structure contains params for some fields of encoder */

        struct f_pair{

            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxI32 v;

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
        par.mfx.CodecLevel              = MFX_LEVEL_AVC_41;
        par.mfx.CodecProfile            = MFX_PROFILE_AVC_HIGH;
        par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.Width         = 320;
        par.mfx.FrameInfo.Height        = 240;
        par.mfx.FrameInfo.CropW         = 320;
        par.mfx.FrameInfo.CropH         = 240;
        par.mfx.TargetKbps              = 5000;
        par.mfx.MaxKbps                 = par.mfx.TargetKbps;
        par.mfx.GopRefDist              = 1;
        par.mfx.GopPicSize              = 4;
        par.mfx.TargetUsage             = MFX_TARGETUSAGE_1;
        par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        mfxExtCodingOption2* co2 = (mfxExtCodingOption2* )par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        co2->IntRefType      = 1;
        co2->IntRefCycleSize = 2;
        co2->IntRefQPDelta   = 0;
        return par;
    }

    const tc_struct TestSuite::test_case[] =
    {
        //VALID VARTICAL PARAMS
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 1},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, 2},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta, 4}}},
        //VALID HORIZONTAL PARMAS (UNSUPPORTED FOR snb, ivb, vlv, hsw, bdw)
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 2},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, 2},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta, 4}}},
        //VALID WITH NEGATIVE IntRefQPDelta
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 1},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, 2},
        {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta, (mfxI16)-24}}},
        //INVALID IntRefType = 3
        {/*03*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 3},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, 2},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta, 4}}},
        //INVALID IntRefQPDelta = 53
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 1},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, 2},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta, 53}}},
        //INVALID IntRefQPDelta = -53
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 1},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, 2},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta,(mfxI16) -53}}},
        //VALID ALL ZEROES
        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 0},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, 0},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta, 0}}},
        //INVALID ALL -1
        {/*07*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM,
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefType,(mfxI16) -1},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefCycleSize, (mfxI16)-1},
         {CDO2_PAR, &tsStruct::mfxExtCodingOption2.IntRefQPDelta,(mfxI16) -1}}},
        //VALID
        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {{CDO2_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0}}},
        //INVALID
        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {{CDO2_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}}},
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];

        mfxStatus q_sts = tc.q_sts;
        mfxStatus i_sts = tc.i_sts;
        mfxStatus r_sts = tc.r_sts;


       if (id == 2)//Horizontal intra refresh is unsupported 
            if (g_tsHWtype <= MFX_HW_BDW)
            {
                q_sts = MFX_ERR_UNSUPPORTED;
                i_sts = MFX_ERR_INVALID_VIDEO_PARAM;
                r_sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }

        MFXInit();

        m_par = initParams();
        SETPARS(&m_par, CDO2_PAR);

        mfxExtCodingOption2* m_co2 = (mfxExtCodingOption2*) m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        mfxI32 bef_IntRefType      = -100;
        mfxI32 bef_IntRefCycleSize = -100;
        mfxI32 bef_IntRefQPDelta   = -100;

        mfxI32* check_value;
        if (q_sts != MFX_ERR_NONE){
            if (m_co2->IntRefType < 0 || m_co2->IntRefType > 2)
                bef_IntRefType = m_co2->IntRefType;
            if (m_co2->IntRefCycleSize < 0)
                bef_IntRefCycleSize = (m_co2->IntRefCycleSize);
            if (m_co2->IntRefQPDelta < -51 || m_co2->IntRefQPDelta > 51)
                bef_IntRefQPDelta = m_co2->IntRefQPDelta;
        }

        g_tsStatus.expect(q_sts);
        Query();

        if (q_sts != MFX_ERR_NONE){
            if (bef_IntRefType != -100)
                EXPECT_NE(bef_IntRefType, m_co2->IntRefType) << "IntRefType wasn't changed";
            if (bef_IntRefCycleSize != -100)
                EXPECT_NE(bef_IntRefCycleSize, m_co2->IntRefCycleSize) << "IntRefCycleSize wasn't changed";
            if (bef_IntRefQPDelta != -100)
                EXPECT_NE(bef_IntRefQPDelta, m_co2->IntRefQPDelta) << "IntRefQPDelta wasn't changed";
        }

        SETPARS(&m_par, CDO2_PAR);

        g_tsStatus.expect(i_sts);
        Init();


        if (tc.i_sts >= MFX_ERR_NONE){
            GetVideoParam();
            if (bef_IntRefType != -100)
                EXPECT_NE(bef_IntRefType, m_co2->IntRefType) << "IntRefType wasn't changed";
            if (bef_IntRefCycleSize != -100)
                EXPECT_NE(bef_IntRefCycleSize, m_co2->IntRefCycleSize) << "IntRefCycleSize wasn't changed";
            if (bef_IntRefQPDelta != -100)
                EXPECT_NE(bef_IntRefQPDelta, m_co2->IntRefQPDelta) << "IntRefQPDelta wasn't changed";
        }

        MFXClose();

        MFXInit();
        m_par = initParams();
        Init();

        SETPARS(&m_par, CDO2_PAR);
        g_tsStatus.expect(r_sts);
        Reset();

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_intra_refresh);
}



