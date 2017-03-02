/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

File Name: vpp_query_extbuf.cpp
\* ****************************************************************************** */

/*!
\file vpp_query_extbuf.cpp
\brief Gmock test vpp_query_extbuf

Description:
This suite tests  VPP\n

Algorithm:
- Initialize MSDK
- Set test suite params
- Attach extbuffer
- Call Query
- Check return status

*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"


/*! \brief Main test name space */
namespace vpp_query_extbuf
{

    enum
    {
        MFX_PAR = 0,
    };

    //! \brief Enum with test modes
    enum MODE
    {
        STANDART,
        PROCAMP,
        SCLY,
        MULTIBUF,
        NULL_BUF
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! \brief Test mode
        MODE mode;
        //! \brief Query expected reurn status
        mfxStatus expected;

        /*! \brief Structure contains params for some fields of VPP */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        }set_par[MAX_NPARS];
        //! \brief Array for DoNotUse algs (if required)
        mfxU32 dnu[4];
        //! \brief Array with values for mfxExtVPPProcamp (if required)
        mfxF64 procamp_val[4];
    };

    //!\brief Main test class
    class TestSuite:tsVideoVPP
    {
    public:
        //! \brief A constructor 
        TestSuite(): tsVideoVPP(true) {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };


    const        tc_struct TestSuite::test_case[] =
    {
        {/*00*/ STANDART, MFX_ERR_UNSUPPORTED, { {MFX_PAR, &tsStruct::mfxExtCodingOption.Header, MFX_EXTBUFF_CODING_OPTION} }, {},{} },
        {/*01*/ STANDART, MFX_ERR_UNSUPPORTED, { { MFX_PAR, &tsStruct::mfxExtCodingOptionSPSPPS.Header, MFX_EXTBUFF_CODING_OPTION_SPSPPS } }, {},{} },
        {/*02*/ STANDART, MFX_ERR_UNDEFINED_BEHAVIOR,{ { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 0 } } ,{},{} },
        {/*03*/ STANDART, MFX_ERR_UNSUPPORTED, { { MFX_PAR, &tsStruct::mfxExtVppAuxData.Header, MFX_EXTBUFF_VPP_AUXDATA } },{},{} },
        {/*04*/ STANDART, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxExtVPPDenoise.Header, MFX_EXTBUFF_VPP_DENOISE } },{},{} },

        {/*05*/ SCLY, MFX_ERR_UNSUPPORTED, {},{} },

        {/*06*/ STANDART, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 1 } }, { MFX_EXTBUFF_VPP_DENOISE },{} },
        {/*07*/ STANDART, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 1 } },{ MFX_EXTBUFF_VPP_SCENE_ANALYSIS },{} },

        {/*08*/ STANDART, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50}}, {},{} },
        {/*09*/ STANDART, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,{ { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 101 } },{},{} },

        {/*10*/ STANDART, MFX_ERR_NONE,{ { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 } },{},{} },
        {/*11*/ STANDART, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,{ { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 101 } },{},{} },

        {/*12*/ STANDART, MFX_ERR_NONE,{ { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 2 } },{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS },{} },
        {/*13*/ STANDART, MFX_ERR_UNSUPPORTED,{ { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 2 } },{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS },{} },

        {/*14*/ PROCAMP, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{}, {50.1, 5.11, 50.1, 5.11}},

        {/*15*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ -100.1 , 5.11, 50.1, 5.11 } },

        {/*16*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ 50.1, -0.1, 50.1, 5.11 } },

        {/*17*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ 50.1, 5.11, -180.1, 5.11 } },

        {/*18*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ 50.1, 5.11, 50.1, -0.1 } },

        {/*19*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ 100.1, 5.11, 50.1, 5.11 } },

        {/*20*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ 50.1, 10.1, 50.1, 5.11 } },

        {/*21*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ 50.1, 5.11, 180.1, 5.11 } },

        {/*22*/ PROCAMP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Header, MFX_EXTBUFF_VPP_PROCAMP },
        },{},{ 50.1, 5.11, 50.1, 10.1 } },


        {/*23*/ MULTIBUF, MFX_ERR_NONE, 
        {
            { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 1 },
            { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
            { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
        },
        {MFX_EXTBUFF_VPP_SCENE_ANALYSIS}, { 50.1, 5.11, 50.1, 5.1 }
        },

        {/*24*/ NULL_BUF, MFX_ERR_NULL_PTR, {}, {}},

        {/*25*/ STANDART, MFX_ERR_UNDEFINED_BEHAVIOR, 
        {
            { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 1 },
            { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 }
        }, {MFX_EXTBUFF_VPP_DENOISE}, {}
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        MFXInit();
        auto& tc = test_case[id];
        SETPARS(&m_par, MFX_PAR);
        mfxExtVPPDoNotUse* buf;
        mfxExtVPPProcAmp* buf1;

        switch(tc.mode) {
            case MODE::MULTIBUF:
                m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE, sizeof(mfxExtVPPDoNotUse));
                buf = (mfxExtVPPDoNotUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);

                if(buf != nullptr) {
                    buf->NumAlg = 1;
                    buf->AlgList = new mfxU32[buf->NumAlg];
                    mfxU32 al[1] = { MFX_EXTBUFF_VPP_SCENE_ANALYSIS };
                    memcpy(buf->AlgList, al, sizeof(al));
                }
                break;

            case MODE::NULL_BUF:
                m_par.AddExtBuffer(MFX_MAKEFOURCC('N', 'U', 'L', 'L'), 0);
                break;

            case MODE::SCLY:
                m_par.AddExtBuffer(MFX_EXTBUFF_VPP_SCENE_ANALYSIS, sizeof(mfxExtBuffer));
                break;

            case MODE::PROCAMP:
                buf1 = (mfxExtVPPProcAmp*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_PROCAMP);
                buf1->Brightness = tc.procamp_val[0];
                buf1->Contrast = tc.procamp_val[1];
                buf1->Hue = tc.procamp_val[2];
                buf1->Saturation = tc.procamp_val[3];
                break;


            case MODE::STANDART:
                buf = (mfxExtVPPDoNotUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);

                if(buf != nullptr) {
                    buf->AlgList = new mfxU32[buf->NumAlg];
                    memcpy(buf->AlgList, tc.dnu, sizeof(mfxU32)*buf->NumAlg);
                }

                break;

            default:
                break;
        }
        g_tsStatus.expect(tc.expected);
        Query();

        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_query_extbuf);
}