/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

File Name: vpp_crop_query_init.cpp
\* ****************************************************************************** */

/*!
\file vpp_crop_query_init.cpp
\brief Gmock test vpp_crop

Description:
This suite tests crop VPP\n

*/
#include "ts_vpp.h"
#include "ts_struct.h"

/*! \brief Main test name space */
namespace vpp_crop_query
{
    enum
    {
        MFX_PAR
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        mfxStatus q_sts;

        /*! \brief Structure contains params for some fields of VPP */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const tsStruct::Field *f;
            //! Field value
            mfxU32 v;

        } set_par[MAX_NPARS];
    };
    //!\brief Main test class
    class TestSuite : protected tsVideoVPP
    {
    public:
        //! \brief A constructor (VPP)
        TestSuite() : tsVideoVPP(true) {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        int RunTest(const tc_struct &tc);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
        {
            // SYSTEM_MEM->SYSTEM_MEM
            // Normal crop with save res.
            {/*00*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*01*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Normal crop FHD->HD (16:9).
            {/*02*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1088},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1080},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1366},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 768},
                }
            },
            // Normal crop QHD->FHD (16:9).
            {/*03*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Normal crop 4K->FHD (16:9).
            {/*04*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 3840},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 2160},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 3840},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 2160},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Normal crop 8K->FHD (16:9).
            {/*05*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 7680},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 4320},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 7680},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 4320},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Normal crop to lower res(4:3).
            {/*06*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1600},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1200},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1600},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1200},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1024},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 768},
                }
            },
            // Normal crop (4:3).
            {/*07*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1440},
                }
            },
            // Normal crop (4:3).
            {/*08*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1440},
                }
            },
            // Normal crop (16:9)->(4:3).
            {/*09*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1440},
                }
            },
            // Normal crop (4:3)->(16:9).
            {/*10*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*13*/ MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: res must div by 16.
            {/*14*/ MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1088},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1080},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 1080},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },

            // VIDEO_MEM->VIDEO_MEM
            // Normal crop with save res.
            {/*15*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*16*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*17*/ MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },

            // SYSTEM_MEM->VIDEO_MEM
            // Normal crop with save res.
            {/*18*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*19*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*20*/ MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },

            // VIDEO_MEM->SYSTEM_MEM
            // Normal crop with save res.
            {/*21*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*22*/ MFX_ERR_NONE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*23*/ MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        return RunTest(test_case[id]);
    }

    int TestSuite::RunTest(const tc_struct& tc)
    {
        TS_START;

        MFXInit();

        SETPARS(m_pPar, MFX_PAR);

        tsExtBufType<mfxVideoParam> par_out;
        par_out = m_par;

        g_tsStatus.expect(tc.q_sts);
        Query(m_session, m_pPar, &par_out);


        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_crop_query);
} // namespace vpp_crop_query

/*! \brief Main test init name space */
namespace vpp_crop_init
{
    enum
    {
        MFX_PAR
    };

    enum Mode
    {
        STANDART,
        DOUBLE_VPP_INIT,
        DOUBLE_VPP_INIT_CLOSE
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        mfxStatus i_sts;
        Mode mode;

        /*! \brief Structure contains params for some fields of VPP */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const tsStruct::Field *f;
            //! Field value
            mfxU32 v;

        } set_par[MAX_NPARS];
    };
    //!\brief Main test class
    class TestSuite : protected tsVideoVPP
    {
    public:
        //! \brief A constructor (VPP)
        TestSuite() : tsVideoVPP(true) {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        int RunTest(const tc_struct &tc);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
        {
            // SYSTEM_MEM->SYSTEM_MEM
            // Normal crop with save res.
            {/*00*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*01*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Normal crop FHD->HD (16:9).
            {/*02*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1088},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1080},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1366},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 768},
                }
            },
            // Normal crop QHD->FHD (16:9).
            {/*03*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Normal crop 4K->FHD (16:9).
            {/*04*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 3840},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 2160},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 3840},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 2160},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Normal crop 8K->FHD (16:9).
            {/*05*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 7680},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 4320},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 7680},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 4320},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Normal crop to lower res(4:3).
            {/*06*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1600},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1200},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1600},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1200},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1024},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 768},
                }
            },
            // Normal crop (4:3).
            {/*07*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1440},
                }
            },
            // Normal crop (4:3).
            {/*08*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1440},
                }
            },
            // Normal crop (16:9)->(4:3).
            {/*09*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2560},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1440},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1440},
                }
            },
            // Normal crop (4:3)->(16:9).
            {/*10*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 2048},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1536},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // Normal crop with init->close->init.
            {/*11*/ MFX_ERR_NONE, DOUBLE_VPP_INIT_CLOSE,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Crop with init->init.
            {/*12*/ MFX_ERR_NONE, DOUBLE_VPP_INIT,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: res must div by 16.
            {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1088},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 1080},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 1080},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 1920},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 1080},
                }
            },
            // VIDEO_MEM->VIDEO_MEM
            // Normal crop with save res.
            {/*15*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*16*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // SYSTEM_MEM->VIDEO_MEM
            // Normal crop with save res.
            {/*18*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*19*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // VIDEO_MEM->SYSTEM_MEM
            // Normal crop with save res.
            {/*21*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 576},
                }
            },
            // Normal crop to lower res.
            {/*22*/ MFX_ERR_NONE, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
            // Negative test. Unsupported crop: cX > W.
            {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDART,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 576},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 704},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 705},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 577},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 352},
                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 288},
                }
            },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        return RunTest(test_case[id]);
    }

    int TestSuite::RunTest(const tc_struct& tc)
    {
        TS_START;

        MFXInit();

        SETPARS(m_pPar, MFX_PAR);

        mfxStatus init_sts = tc.i_sts;
        Mode mode = tc.mode;

        CreateAllocators();
        SetFrameAllocator();
        SetHandle();

        tsExtBufType<mfxVideoParam> par_out;
        par_out = m_par;

        switch(mode)
        {
            case DOUBLE_VPP_INIT:
                g_tsStatus.expect(init_sts);
                Init(m_session, m_pPar);
                // FourCC may unsupported
                if (init_sts != MFX_ERR_INVALID_VIDEO_PARAM)
                    init_sts = MFX_ERR_UNDEFINED_BEHAVIOR;
                break;

            case DOUBLE_VPP_INIT_CLOSE:
                //double init with close does not lead to an error
                g_tsStatus.expect(init_sts);
                Init(m_session, m_pPar);
                Close();
                break;

            default:
            break;
        }

        g_tsStatus.expect(init_sts);
        Init(m_session, m_pPar);

        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_crop_init);
} // namespace vpp_crop_init

namespace vpp_rext_crop_query
{
    using namespace tsVPPInfo;

    template<eFmtId FmtID>
    class TestSuite : public vpp_crop_query::TestSuite
    {
    public:
        TestSuite()
            : vpp_crop_query::TestSuite()
        {}

        int RunTest(unsigned int id)
        {

            m_par.vpp.In.FourCC         = m_par.vpp.Out.FourCC         = Formats[FmtID].FourCC;
            m_par.vpp.In.BitDepthLuma   = m_par.vpp.Out.BitDepthLuma   = Formats[FmtID].BdY;
            m_par.vpp.In.BitDepthChroma = m_par.vpp.Out.BitDepthChroma = Formats[FmtID].BdC;
            m_par.vpp.In.ChromaFormat   = m_par.vpp.Out.ChromaFormat   = Formats[FmtID].ChromaFormat;
            m_par.vpp.In.Shift          = m_par.vpp.Out.Shift          = Formats[FmtID].Shift;

            vpp_crop_query::tc_struct tc = test_case[id];
            if (MFX_ERR_UNSUPPORTED == CCSupport()[FmtID][FmtID])
            {
                tc.q_sts = MFX_ERR_UNSUPPORTED;
            }

            return  vpp_crop_query::TestSuite::RunTest(tc);
        }
    };

#define REG_TEST(NAME, FMT_ID)                                     \
    namespace NAME                                                 \
    {                                                              \
        typedef vpp_rext_crop_query::TestSuite<FMT_ID> TestSuite;  \
        TS_REG_TEST_SUITE_CLASS(NAME);                             \
    }

    REG_TEST(vpp_8b_420_yv12_crop_query, FMT_ID_8B_420_YV12);
    REG_TEST(vpp_8b_422_uyvy_crop_query, FMT_ID_8B_422_UYVY);
    REG_TEST(vpp_8b_422_yuy2_crop_query, FMT_ID_8B_422_YUY2);
    REG_TEST(vpp_8b_444_ayuv_crop_query, FMT_ID_8B_444_AYUV);
    REG_TEST(vpp_8b_444_rgb4_crop_query, FMT_ID_8B_444_RGB4);
    REG_TEST(vpp_10b_420_p010_crop_query, FMT_ID_10B_420_P010);
    REG_TEST(vpp_10b_422_y210_crop_query, FMT_ID_10B_422_Y210);
    REG_TEST(vpp_10b_444_y410_crop_query, FMT_ID_10B_444_Y410);
    REG_TEST(vpp_12b_420_p016_crop_query, FMT_ID_12B_420_P016);
    REG_TEST(vpp_12b_422_y216_crop_query, FMT_ID_12B_422_Y216);
    REG_TEST(vpp_12b_444_y416_crop_query, FMT_ID_12B_444_Y416);
#undef REG_TEST
}

namespace vpp_rext_crop_init
{
    using namespace tsVPPInfo;

    template<eFmtId FmtID>
    class TestSuite : public vpp_crop_init::TestSuite
    {
    public:
        TestSuite()
            : vpp_crop_init::TestSuite()
        {}

        int RunTest(unsigned int id)
        {

            m_par.vpp.In.FourCC         = m_par.vpp.Out.FourCC         = Formats[FmtID].FourCC;
            m_par.vpp.In.BitDepthLuma   = m_par.vpp.Out.BitDepthLuma   = Formats[FmtID].BdY;
            m_par.vpp.In.BitDepthChroma = m_par.vpp.Out.BitDepthChroma = Formats[FmtID].BdC;
            m_par.vpp.In.ChromaFormat   = m_par.vpp.Out.ChromaFormat   = Formats[FmtID].ChromaFormat;
            m_par.vpp.In.Shift          = m_par.vpp.Out.Shift          = Formats[FmtID].Shift;

            vpp_crop_init::tc_struct tc = test_case[id];
            if (MFX_ERR_UNSUPPORTED == CCSupport()[FmtID][FmtID])
            {
                tc.i_sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }
            else if (!tc.i_sts && MFX_WRN_PARTIAL_ACCELERATION == CCSupport()[FmtID][FmtID])
            {
                tc.i_sts = MFX_WRN_PARTIAL_ACCELERATION;
            }

            return  vpp_crop_init::TestSuite::RunTest(tc);
        }
    };

#define REG_TEST(NAME, FMT_ID)                                     \
    namespace NAME                                                 \
    {                                                              \
        typedef vpp_rext_crop_init::TestSuite<FMT_ID> TestSuite;   \
        TS_REG_TEST_SUITE_CLASS(NAME);                             \
    }

    REG_TEST(vpp_8b_420_yv12_crop_init, FMT_ID_8B_420_YV12);
    REG_TEST(vpp_8b_422_uyvy_crop_init, FMT_ID_8B_422_UYVY);
    REG_TEST(vpp_8b_422_yuy2_crop_init, FMT_ID_8B_422_YUY2);
    REG_TEST(vpp_8b_444_ayuv_crop_init, FMT_ID_8B_444_AYUV);
    REG_TEST(vpp_8b_444_rgb4_crop_init, FMT_ID_8B_444_RGB4);
    REG_TEST(vpp_10b_420_p010_crop_init, FMT_ID_10B_420_P010);
    REG_TEST(vpp_10b_422_y210_crop_init, FMT_ID_10B_422_Y210);
    REG_TEST(vpp_10b_444_y410_crop_init, FMT_ID_10B_444_Y410);
    REG_TEST(vpp_12b_420_p016_crop_init, FMT_ID_12B_420_P016);
    REG_TEST(vpp_12b_422_y216_crop_init, FMT_ID_12B_422_Y216);
    REG_TEST(vpp_12b_444_y416_crop_init, FMT_ID_12B_444_Y416);
#undef REG_TEST
}