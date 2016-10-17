/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: vpp_sw_wrn_filter_skipped.cpp
\* ****************************************************************************** */

/*!
\file vpp_sw_wrn_filter_skipped.cpp
\brief Gmock test vpp_sw_wrn_filter_skipped

Description:
This suite tests AVC VPP\n

Algorithm:
- Initialize MSDK
- Set test suite params
- Set test case params
- Attach buffers
- Call Query, Init, Reset functions
- Check return statuses

*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace vpp_sw_wrn_filter_skipped
{

    enum
    {
        MFX_PAR = 1,
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //!Expected Query() return status
        mfxStatus q_exp;
        //!Expected Init() return status
        mfxStatus i_exp;
        //!Expected Reset() return status
        mfxStatus r_exp;

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
        //! The max number of algs used in mfxExtVPPDoUse structure
        static const mfxU32 MAX_ALG_NUM = 3;
        //! Array with algs for DoUse filter (MAX_ALG_NUM - is max value in test suite now)
        mfxU32 alg_duse[MAX_ALG_NUM];
    };

    //!\brief Main test class
    class TestSuite:tsVideoVPP
    {
    public:
        //! \brief A constructor (AVC VPP)
        TestSuite(): tsVideoVPP(true)
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_RGB4;
        }
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

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/MFX_WRN_FILTER_SKIPPED, MFX_WRN_FILTER_SKIPPED,MFX_WRN_FILTER_SKIPPED,
        { { MFX_PAR, &tsStruct::mfxExtVPPDoUse.NumAlg, 1 } },
        { MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION }
        },

        {/*01*/MFX_WRN_FILTER_SKIPPED, MFX_WRN_FILTER_SKIPPED,MFX_WRN_FILTER_SKIPPED,
        { { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 75 },
        { MFX_PAR, &tsStruct::mfxExtVPPDoUse.NumAlg, 1 } },
        { MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION }

        },

        {/*02*/MFX_WRN_FILTER_SKIPPED, MFX_WRN_FILTER_SKIPPED,MFX_WRN_FILTER_SKIPPED,
        { { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 75 },
        { MFX_PAR, &tsStruct::mfxExtVPPDoUse.NumAlg, 1 } },
        { MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        if(g_tsImpl != MFX_IMPL_SOFTWARE) {
            g_tsLog << "[ SKIPPED ] This test is supported only for SW impl\n";
            return 0;
        }
        MFXInit();
        auto& tc = test_case[id];

        SETPARS(&m_par, MFX_PAR);
        mfxExtVPPDoUse* ext = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        if(ext) {
            if(!ext->AlgList) {
                ext->AlgList = new mfxU32[ext->NumAlg];
            }
            memcpy(ext->AlgList, tc.alg_duse, sizeof(mfxU32)*ext->NumAlg);
        }
        g_tsStatus.expect(tc.q_exp);
        Query();

        SETPARS(&m_par, MFX_PAR);
        ext = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        if(ext && ext->AlgList) {
            memcpy(ext->AlgList, tc.alg_duse, sizeof(mfxU32)*ext->NumAlg);
        }

        g_tsStatus.expect(tc.i_exp);
        Init();

        g_tsStatus.expect(tc.r_exp);
        Reset();

        if(ext && ext->AlgList) {
            delete[] ext->AlgList;
            ext->AlgList = nullptr;
        }
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_sw_wrn_filter_skipped);
}
