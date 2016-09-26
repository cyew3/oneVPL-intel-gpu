/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 - 2016 Intel Corporation.All Rights Reserved.

File Name : vpp_deinterlac_inv_telec.cpp
\* ****************************************************************************** */

/*!
\file vpp_deinterlac_inv_telec.cpp
\brief Gmock test vpp_deinterlac_inv_telec

Description:
This suite tests VPP\n

Algorithm:
- Initialize session
- Set VPP params (Picstruct is a random param)
- Set test case params
- Call QueryIOSurf()
- Check return status
- Call Query()
- Check return status
- Call Init()
- Check return status
- Call Reset()
- Check return status

*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <cmath>

/*! \brief Main test name space */
namespace vpp_deinterlac_inv_telec
{

    enum
    {
        MFX_PAR = 1,
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! Expected QueryIOSurf() status
        mfxStatus qios_sts;
        //! Expected Query() status
        mfxStatus q_sts;
        //! Expected Init() status
        mfxStatus i_sts;
        //! Expected Reset() status
        mfxStatus r_sts;
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

    };

    //!\brief Main test class
    class TestSuite:tsVideoVPP
    {
    public:
        //! \brief A constructor
        TestSuite(): tsVideoVPP() {
            m_par.vpp.In.FourCC     = MFX_FOURCC_YUY2;
            m_par.vpp.In.PicStruct  = pow(2, (rand() % 3));
            m_par.vpp.Out.PicStruct = pow(2, (rand() % 3));
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
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        }
        },

        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },


        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001},
        } },

        {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },


        {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*15*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*16*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*17*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*18*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*19*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*20*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1001 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*21*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*22*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*23*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*24*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*25*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*26*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1001 },
        } },

        {/*27*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*28*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 15 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 },
        } },

        {/*29*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 888 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 27 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 7777 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 196 },
        } },

        {/*30*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 98497 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 5860 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 8562 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 150 },
        } },

        {/*31*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 888 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 27 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 7777 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 196 },
        } },

        {/*32*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 98497 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 5860 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 8562 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 150 },
        } },

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        MFXInit();
        SETPARS(&m_par, MFX_PAR);
        g_tsStatus.expect(tc.qios_sts);
        QueryIOSurf();
        g_tsStatus.expect(tc.q_sts);
        Query();
        g_tsStatus.expect(tc.i_sts);
        Init();
        g_tsStatus.expect(tc.r_sts);
        Reset();
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_deinterlac_inv_telec);
}



