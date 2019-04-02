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
namespace vpp_crop_query_init
{
    enum
    {
        MFX_PAR
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        mfxStatus i_sts;
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
            {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
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

            // VIDEO_MEM->VIDEO_MEM
            {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
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

            // SYSTEM_MEM->VIDEO_MEM
            {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNSUPPORTED,
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
            {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE,
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
            {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNSUPPORTED,
                {
                    {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
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
        mfxStatus query_sts = tc.q_sts;

        CreateAllocators();
        SetFrameAllocator();
        SetHandle();

        tsExtBufType<mfxVideoParam> par_out;
        par_out = m_par;

        try
        {
            g_tsStatus.expect(query_sts);
            Query(m_session, m_pPar, &par_out);
        } catch(tsRes r){ }

        try
        {
            g_tsStatus.expect(init_sts);
            Init(m_session, m_pPar);
        } catch(tsRes r){ }

        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_crop_query_init);
} // namespace vpp_crop_query_init

namespace vpp_rext_crop
{
    using namespace tsVPPInfo;

    template<eFmtId FmtID>
    class TestSuite : public vpp_crop_query_init::TestSuite
    {
    public:
        TestSuite()
            : vpp_crop_query_init::TestSuite()
        {}

        int RunTest(unsigned int id)
        {

            m_par.vpp.In.FourCC         = m_par.vpp.Out.FourCC         = Formats[FmtID].FourCC;
            m_par.vpp.In.BitDepthLuma   = m_par.vpp.Out.BitDepthLuma   = Formats[FmtID].BdY;
            m_par.vpp.In.BitDepthChroma = m_par.vpp.Out.BitDepthChroma = Formats[FmtID].BdC;
            m_par.vpp.In.ChromaFormat   = m_par.vpp.Out.ChromaFormat   = Formats[FmtID].ChromaFormat;
            m_par.vpp.In.Shift          = m_par.vpp.Out.Shift          = Formats[FmtID].Shift;

            vpp_crop_query_init::tc_struct tc = test_case[id];
            if (MFX_ERR_UNSUPPORTED == CCSupport()[FmtID][FmtID])
            {
                tc.i_sts = MFX_ERR_INVALID_VIDEO_PARAM;
                tc.q_sts = MFX_ERR_UNSUPPORTED;
            }
            else if (!tc.i_sts && MFX_WRN_PARTIAL_ACCELERATION == CCSupport()[FmtID][FmtID])
            {
                tc.i_sts = MFX_WRN_PARTIAL_ACCELERATION;
                tc.q_sts = MFX_WRN_PARTIAL_ACCELERATION;
            }

            return  vpp_crop_query_init::TestSuite::RunTest(tc);
        }
    };

#define REG_TEST(NAME, FMT_ID)                                     \
    namespace NAME                                                 \
    {                                                              \
        typedef vpp_rext_crop::TestSuite<FMT_ID> TestSuite; \
        TS_REG_TEST_SUITE_CLASS(NAME);                             \
    }

    REG_TEST(vpp_8b_420_yv12_crop_query_init, FMT_ID_8B_420_YV12);
    REG_TEST(vpp_8b_422_uyvy_crop_query_init, FMT_ID_8B_422_UYVY);
    REG_TEST(vpp_8b_422_yuy2_crop_query_init, FMT_ID_8B_422_YUY2);
    REG_TEST(vpp_8b_444_ayuv_crop_query_init, FMT_ID_8B_444_AYUV);
    REG_TEST(vpp_8b_444_rgb4_crop_query_init, FMT_ID_8B_444_RGB4);
    REG_TEST(vpp_10b_420_p010_crop_query_init, FMT_ID_10B_420_P010);
    REG_TEST(vpp_10b_422_y210_crop_query_init, FMT_ID_10B_422_Y210);
    REG_TEST(vpp_10b_444_y410_crop_query_init, FMT_ID_10B_444_Y410);
    REG_TEST(vpp_12b_420_p016_crop_query_init, FMT_ID_12B_420_P016);
    REG_TEST(vpp_12b_422_y216_crop_query_init, FMT_ID_12B_422_Y216);
    REG_TEST(vpp_12b_444_y416_crop_query_init, FMT_ID_12B_444_Y416);
#undef REG_TEST
}