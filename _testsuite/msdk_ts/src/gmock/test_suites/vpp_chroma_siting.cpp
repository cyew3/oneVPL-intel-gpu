/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.

File Name: vpp_chroma_siting.cpp
\* ****************************************************************************** */

/*!
\file vpp_chroma_siting.cpp
\brief Gmock test vpp_chroma_siting

Description:
This suite tests VPP\n

Algorithm:
- Initialize session
- Allocate ext buffers
- Call Query()
- Check returned status
- Call Init()
- Check returned status
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace vpp_chroma_siting
{
    enum
    {
        MFX_EXTBUFF_COLOR_CONVERSION = 1,
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! Expected Query() return status
        mfxStatus q_sts;
        //! Expected Init() return status
        mfxStatus i_sts;

        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;
        }set_par[MAX_NPARS];
    };

    //!\brief Main test class
    class TestSuite : protected tsVideoVPP
    {
    public:
        //! \brief A constructor
        TestSuite(): tsVideoVPP() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! \brief Initialize params common mfor whole test suite
        void initParams();
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    void TestSuite::initParams() {
        m_par.vpp.In.FourCC = MFX_FOURCC_RGB4;
        m_par.vpp.In.BitDepthLuma = 8;
        m_par.vpp.In.BitDepthChroma = 8;
        m_par.vpp.In.Shift = 0;
        m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV444;

        m_par.vpp.Out.FourCC = MFX_FOURCC_NV12;
        m_par.vpp.Out.BitDepthLuma = 8;
        m_par.vpp.Out.BitDepthChroma = 8;
        m_par.vpp.Out.Shift = 0;
        m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

  const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_HORIZONTAL_LEFT | MFX_CHROMA_SITING_VERTICAL_TOP },
        },
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_HORIZONTAL_LEFT | MFX_CHROMA_SITING_VERTICAL_CENTER },
        },
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_HORIZONTAL_LEFT | MFX_CHROMA_SITING_VERTICAL_BOTTOM },
        },
        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_HORIZONTAL_CENTER | MFX_CHROMA_SITING_VERTICAL_CENTER },
        },
        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_HORIZONTAL_CENTER | MFX_CHROMA_SITING_VERTICAL_TOP },
        },
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_HORIZONTAL_CENTER | MFX_CHROMA_SITING_VERTICAL_BOTTOM },
        },
        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_UNKNOWN },
        },
        {/*07*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_VERTICAL_TOP | MFX_CHROMA_SITING_VERTICAL_BOTTOM },
        },
        {/*08*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_VERTICAL_CENTER | MFX_CHROMA_SITING_VERTICAL_BOTTOM },
        },
        {/*09*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_VERTICAL_BOTTOM | MFX_CHROMA_SITING_VERTICAL_BOTTOM },
        },
        {/*10*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
          { MFX_EXTBUFF_COLOR_CONVERSION,  &tsStruct::mfxExtColorConversion.ChromaSiting, MFX_CHROMA_SITING_VERTICAL_BOTTOM | MFX_CHROMA_SITING_VERTICAL_TOP },
        },

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        initParams();
        MFXInit();

        SETPARS(&m_par, MFX_EXTBUFF_COLOR_CONVERSION);

        g_tsStatus.expect(tc.q_sts);
        Query();

        g_tsStatus.expect(tc.i_sts);
        Init();

        Close();

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_chroma_siting);
}