/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avcd_hw_slice_groups_present.cpp
\* ****************************************************************************** */

/*!
\file avcd_hw_slice_groups_present.cpp
\brief Gmock test avcd_hw_slice_groups_present

Description:
This suite tests AVC decoder\n

Algorithm:
- Initialize MSDK
- Set test suite params
- Set test case params (scenario and SliceGroupsPresent par)
- Call required functions
- Check return statuses with expected
*/
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avcd_hw_slice_groups_present{

    enum{
        MFX_PAR = 1,
    };
    /*!Enum to identify the scenario of test case*/
    enum{
        INIT = 1,
        RESET = 2,
        QUERY = 3,
    };

    /*!\brief Structure of test suite parameters*/

    struct tc_struct{
        /*! Name of function to test (test case scenario)*/
        mfxU8 function_name;
        /*! \brief Structure contains params for some fields of decoder */

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
    class TestSuite:tsVideoDecoder{
    public:
        //! \brief A constructor (AVC decoder)
        TestSuite() : tsVideoDecoder(MFX_CODEC_AVC) { }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common for whole test suite
        tsExtBufType<mfxVideoParam> initParams();
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType<mfxVideoParam> par;
        par.mfx.CodecId                 = MFX_CODEC_AVC;
        par.mfx.CodecProfile            = MFX_PROFILE_AVC_MAIN;
        par.mfx.CodecLevel              = MFX_LEVEL_AVC_4;
        par.mfx.BufferSizeInKB          = 500;
        par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.Width         = 352;
        par.mfx.FrameInfo.Height        = 288;
        par.mfx.FrameInfo.CropW         = 352;
        par.mfx.FrameInfo.CropH         = 288;
        par.mfx.FrameInfo.FrameRateExtN = 600000;
        par.mfx.FrameInfo.FrameRateExtD = 20000;
        par.mfx.FrameInfo.AspectRatioW  = 1;
        par.mfx.FrameInfo.AspectRatioH  = 1;
        par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        par.AsyncDepth                  = 1;
        par.IOPattern                   = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

        return par;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ INIT,  {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.SliceGroupsPresent, 1}}},
        {/*01*/ RESET, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.SliceGroupsPresent, 1}}},
        {/*02*/ QUERY, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.SliceGroupsPresent, 1}}},
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];
        MFXInit();

        m_par = initParams();
 
        if (tc.function_name == QUERY){
            SETPARS(&m_par, MFX_PAR);
            g_tsStatus.expect(MFX_WRN_PARTIAL_ACCELERATION);
        }
        Query(m_session, m_pPar, m_pParOut);

        if (tc.function_name == INIT || tc.function_name == RESET){
            if (tc.function_name == INIT){
                SETPARS(&m_par, MFX_PAR);
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            }
            Init();
        }

        if (tc.function_name == RESET){
            SETPARS(&m_par, MFX_PAR);
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            Reset();
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avcd_hw_slice_groups_present);
}



