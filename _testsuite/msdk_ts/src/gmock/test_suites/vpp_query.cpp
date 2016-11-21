/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: vpp_query.cpp
\* ****************************************************************************** */

/*!
\file vpp_query.cpp
\brief Gmock test vpp_query

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
namespace vpp_query
{
    enum
    {
        MFX_PAR
    };
    enum MODE
    {
        STANDART,
        IN_PARAM_ZERO,
        OUT_PARAM_ZERO,
        SESSION_NULL
    };
    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        MODE mode;
        mfxStatus expected_sts;

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
        //! \brief A constructor (AVC VPP)
        TestSuite(): tsVideoVPP(true)
        {
            m_par.vpp.In.Height = m_par.vpp.In.CropH = 576;
            m_par.vpp.In.AspectRatioW = 1;
            m_par.vpp.In.AspectRatioH = 1;
            m_par.vpp.Out = m_par.vpp.In;
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
        {/*00*/ STANDART, MFX_ERR_NONE,
        {

            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 }
        }
        },

        {/*01*/ STANDART, MFX_ERR_NONE,
        {

            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 }
        }
        },

        {/*02*/ STANDART, MFX_ERR_NONE,
        {

            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 }

        }
        },

        {/*03*/ STANDART, MFX_ERR_NONE,
        {

            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 }

        }
        },

        {/*04*/ STANDART, MFX_ERR_NONE,
        {

            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YUY2 }

        }
        },

        {/*05*/ STANDART, MFX_ERR_NONE,
        {

            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YUY2 }

        }
        },

        {/*06*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YUY2 }
        }
        },


        {/*07*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YUY2 }
        }
        },

        {/*08*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR,  &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY }
        }
        },

        {/*09*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR,   &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY }
        }
        },

        {/*10*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY }
        }
        },

        {/*11*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY }
        }
        },

        {/*12*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0 }
        }
        },

        {/*13*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP }
        }
        },

        {/*14*/ IN_PARAM_ZERO, MFX_ERR_NONE},

        {/*15*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, 0x1111111 },
        }
        },

        {/*16*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB3 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 }
        }
        },

        {/*17*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB3 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB3 }
        }
        },

        {/*18*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB3 }
        }
        },

        {/*19*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB3 }
        }
        },

        {/*20*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB3 }
        }
        },

        {/*21*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB3 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YV12 }
        }
        },

        {/*22*/ STANDART, MFX_WRN_PARTIAL_ACCELERATION,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YV12 }
        }
        },

        {/*23*/ STANDART, MFX_WRN_PARTIAL_ACCELERATION,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YV12 }
        }
        },

        {/*24*/ STANDART, MFX_WRN_PARTIAL_ACCELERATION,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YV12 }
        }
        },

        {/*25*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB3 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YUY2 }
        }
        },

        {/*26*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 666 }
        }
        },

        {/*27*/ SESSION_NULL, MFX_ERR_INVALID_HANDLE },
        {/*28*/ OUT_PARAM_ZERO, MFX_ERR_NULL_PTR },
        {/*29*/ OUT_PARAM_ZERO, MFX_ERR_NULL_PTR,
        {
            {MFX_PAR , &tsStruct::mfxVideoParam.AsyncDepth, 10 }
        }
        }

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        MFXInit();
        auto& tc = test_case[id];
        mfxSession work_ses = m_session;
        mfxVideoParam par_out = {0};

        SETPARS(&m_par, MFX_PAR);
        if(tc.mode == MODE::SESSION_NULL) {
            work_ses = 0;
        }

        if(tc.mode == MODE::IN_PARAM_ZERO) {
            m_pPar = nullptr;
        }

        if(tc.mode == MODE::OUT_PARAM_ZERO) {
            m_pParOut = nullptr;
        }

        g_tsStatus.expect(tc.expected_sts);
        Query(work_ses, m_pPar, &par_out);
        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_query);
}


