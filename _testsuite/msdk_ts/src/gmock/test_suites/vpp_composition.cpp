/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: vpp_composition.cpp
\* ****************************************************************************** */

/*!
\file vpp_composition.cpp
\brief Gmock test vpp_composition

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
namespace vpp_composition
{

    enum
    {
        MFX_PAR  = 1,
        COMP_PAR = 2,
    };

    //! The number of max algs in mfxExtVPPDoUse structure
    const mfxU32 MAX_ALG_NUM = 3;

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
        //! Array with algs for mfxExtVPPDoUse structure
        mfxU32 algs[MAX_ALG_NUM];

    };

    //!\brief Main test class
    class TestSuite:tsVideoVPP
    {
    public:
        //! \brief A constructor
        TestSuite(): tsVideoVPP() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! \brief Sets required External buffers, return ID of extbuff allocated
        //! \param tc - test case params structure
        mfxU32 setExtBuff(tc_struct tc);
        //! \brief Deallocate external buffer 
        //! \param type - external buffer ID
        void cleanExtBuff(mfxU32 type);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    mfxU32 TestSuite::setExtBuff(tc_struct tc)
    {
        mfxExtVPPDoUse* ext_douse = (mfxExtVPPDoUse*) m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        if(ext_douse != nullptr) {
            ext_douse->AlgList = new mfxU32[ext_douse->NumAlg];
            memcpy(ext_douse->AlgList, tc.algs, sizeof(mfxU32)*ext_douse->NumAlg);
            return MFX_EXTBUFF_VPP_DOUSE;
        }

        mfxExtVPPComposite* ext_composite = (mfxExtVPPComposite*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_COMPOSITE);
        if(ext_composite != nullptr) {
            ext_composite->InputStream = new mfxVPPCompInputStream[ext_composite->NumInputStream];
            for(int i = 0; i < ext_composite->NumInputStream; i++) {
                mfxVPPCompInputStream* str = ext_composite->InputStream;
                SETPARS(str, COMP_PAR);
                str = nullptr;
                return MFX_EXTBUFF_VPP_COMPOSITE;
            }
        }
        return 0;
    }

    void TestSuite::cleanExtBuff(mfxU32 type)
    {
        switch(type) {
            case MFX_EXTBUFF_VPP_DOUSE:
            {
                mfxExtVPPDoUse* ext_douse = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
                delete[] ext_douse->AlgList;
                break;
            }
            case MFX_EXTBUFF_VPP_COMPOSITE:
            {
                mfxExtVPPComposite* ext_composite = (mfxExtVPPComposite*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_COMPOSITE);
                delete[] ext_composite->InputStream;
                break;
            }
        }
    }
  
  const tc_struct TestSuite::test_case[] =
    {
        {/*00*/MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM,
            {{MFX_PAR, &tsStruct::mfxExtVPPDoUse.NumAlg, 1}},
            { MFX_EXTBUFF_VPP_COMPOSITE }
        },

        {/*01*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        { { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50},
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 721 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 481 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*02*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        { { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 2 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 2 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 719 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 479 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE,
        { { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE,
        { { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_AYUV },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_AYUV },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE,
        { { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_Y210 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_Y210 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE,
        { { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_Y410 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_Y410 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        MFXInit();

        SETPARS(&m_par, MFX_PAR);
        mfxU32 MFX_PAR_type = setExtBuff(tc);

        mfxStatus sts_query = tc.q_sts,
                  sts_init  = tc.i_sts;

        if (g_tsHWtype < MFX_HW_CNL
            && (m_par.vpp.In.FourCC == MFX_FOURCC_AYUV || m_par.vpp.Out.FourCC == MFX_FOURCC_AYUV
            ||  m_par.vpp.In.FourCC == MFX_FOURCC_Y210 || m_par.vpp.Out.FourCC == MFX_FOURCC_Y210
            ||  m_par.vpp.In.FourCC == MFX_FOURCC_Y410 || m_par.vpp.Out.FourCC == MFX_FOURCC_Y410))
        {
            sts_query = MFX_ERR_UNSUPPORTED;
            sts_init  = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        g_tsStatus.expect(sts_query);
        Query();

        g_tsStatus.expect(sts_init);
        Init();

        cleanExtBuff(MFX_PAR_type);
        Close();

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_composition);
}