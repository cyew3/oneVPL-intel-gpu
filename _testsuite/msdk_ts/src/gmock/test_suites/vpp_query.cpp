/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

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
    class TestSuite : protected tsVideoVPP
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
        int RunTest(unsigned int id) { return RunTest(test_case[id]); }
        int RunTest(const tc_struct& tc);
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
#if defined WIN32 || WIN64
        {/*13*/ STANDART, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP }
        }
        },
#else
        {/*13*/ STANDART, MFX_ERR_UNSUPPORTED,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP }
        }
        },
#endif
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

    int TestSuite::RunTest(const tc_struct& tc)
    {
        TS_START;
        MFXInit();
        mfxSession work_ses = m_session;

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
        Query(work_ses, m_pPar, m_pParOut);
        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_query);
}

namespace vpp_rext_query
{
    const struct
    {
        mfxU32 FourCC;
        mfxU16 ChromaFormat;
        mfxU16 BdY;
        mfxU16 BdC;
        mfxU16 Shift;
    } Formats[] =
    {
        /*00*/{ MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420, 8, 8, 0 },
        /*01*/{ MFX_FOURCC_YV12, MFX_CHROMAFORMAT_YUV420, 8, 8, 0 },
        /*02*/{ MFX_FOURCC_UYVY, MFX_CHROMAFORMAT_YUV422, 8, 8, 0 },
        /*03*/{ MFX_FOURCC_YUY2, MFX_CHROMAFORMAT_YUV422, 8, 8, 0 },
        /*04*/{ MFX_FOURCC_AYUV, MFX_CHROMAFORMAT_YUV444, 8, 8, 0 },
        /*05*/{ MFX_FOURCC_RGB4, MFX_CHROMAFORMAT_YUV444, 8, 8, 0 },
        /*06*/{ MFX_FOURCC_P010, MFX_CHROMAFORMAT_YUV420, 10, 10, 1 },
        /*07*/{ MFX_FOURCC_Y210, MFX_CHROMAFORMAT_YUV422, 10, 10, 1 },
        /*08*/{ MFX_FOURCC_Y410, MFX_CHROMAFORMAT_YUV444, 10, 10, 0 },
        /*09*/{ MFX_FOURCC_A2RGB10, MFX_CHROMAFORMAT_YUV444, 10, 10, 0 },
    };
    const unsigned int nFormats = sizeof(vpp_rext_query::Formats) / sizeof(vpp_rext_query::Formats[0]);
    const mfxStatus HW = MFX_ERR_NONE;
    const mfxStatus SW = MFX_WRN_PARTIAL_ACCELERATION;
    const mfxStatus NO = MFX_ERR_UNSUPPORTED;

    const mfxStatus CCSupport[][nFormats][nFormats] =
    {
        {//gen9
        //   in\out NV12  YV12  UYVY  YUY2  AYUV  RGB4  P010  Y210  Y410  A2RGB10
        /*   NV12*/{  HW,   SW,   NO,   HW,   NO,   HW,   SW,   NO,   NO,      NO },
        /*   YV12*/{  HW,   SW,   NO,   HW,   NO,   HW,   SW,   NO,   NO,      NO },
        /*   UYVY*/{  SW,   SW,   NO,   SW,   NO,   SW,   SW,   NO,   NO,      NO },
        /*   YUY2*/{  HW,   SW,   NO,   HW,   NO,   HW,   SW,   NO,   NO,      NO },
        /*   AYUV*/{  NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,      NO },
        /*   RGB4*/{  HW,   SW,   NO,   HW,   NO,   HW,   SW,   NO,   NO,      NO },
        /*   P010*/{  SW,   NO,   NO,   NO,   NO,   HW,   SW,   NO,   NO,      NO },
        /*   Y210*/{  NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,      NO },
        /*   Y410*/{  NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,      NO },
        /*A2RGB10*/{  NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,   NO,      NO },
        },
        {//gen11
        //   in\out NV12  YV12  UYVY  YUY2  AYUV  RGB4  P010  Y210  Y410  A2RGB10
        /*   NV12*/{  HW,   SW,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   YV12*/{  HW,   SW,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   UYVY*/{  HW,   SW,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   YUY2*/{  HW,   SW,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   AYUV*/{  HW,   NO,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   RGB4*/{  HW,   SW,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   P010*/{  HW,   NO,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   Y210*/{  HW,   NO,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*   Y410*/{  HW,   NO,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        /*A2RGB10*/{  HW,   NO,   NO,   HW,   HW,   HW,   HW,   HW,   HW,      HW },
        },
    };

    template<mfxU16 FmtID>
    class TestSuite : public vpp_query::TestSuite
    {
    public:
        TestSuite()
            : vpp_query::TestSuite()
        {}

        static const unsigned int n_cases = nFormats;

        int RunTest(unsigned int id)
        {
            m_par.vpp.Out.FourCC            = vpp_rext_query::Formats[FmtID].FourCC;
            m_par.vpp.Out.ChromaFormat      = vpp_rext_query::Formats[FmtID].BdY;
            m_par.vpp.Out.BitDepthLuma      = vpp_rext_query::Formats[FmtID].BdC;
            m_par.vpp.Out.BitDepthChroma    = vpp_rext_query::Formats[FmtID].ChromaFormat;
            m_par.vpp.Out.Shift             = vpp_rext_query::Formats[FmtID].Shift;

            m_par.vpp.In.FourCC             = vpp_rext_query::Formats[id].FourCC;
            m_par.vpp.In.ChromaFormat       = vpp_rext_query::Formats[id].BdY;
            m_par.vpp.In.BitDepthLuma       = vpp_rext_query::Formats[id].BdC;
            m_par.vpp.In.BitDepthChroma     = vpp_rext_query::Formats[id].ChromaFormat;
            m_par.vpp.In.Shift              = vpp_rext_query::Formats[id].Shift;

            vpp_query::tc_struct tc = { vpp_query::STANDART };
            tc.expected_sts = vpp_rext_query::CCSupport[g_tsHWtype >= MFX_HW_ICL][id][FmtID];

            return  vpp_query::TestSuite::RunTest(tc);
        }
    };

#define REG_VPP_REXT_QUERY_TEST(NAME, FMT_ID)                \
    namespace NAME                                           \
    {                                                        \
        typedef vpp_rext_query::TestSuite<FMT_ID> TestSuite; \
        TS_REG_TEST_SUITE_CLASS(NAME);                       \
    }

    REG_VPP_REXT_QUERY_TEST(vpp_8b_420_yv12_query, 1);
    REG_VPP_REXT_QUERY_TEST(vpp_8b_422_uyvy_query, 2);
    REG_VPP_REXT_QUERY_TEST(vpp_8b_422_yuy2_query, 3);
    REG_VPP_REXT_QUERY_TEST(vpp_8b_444_ayuv_query, 4);
    REG_VPP_REXT_QUERY_TEST(vpp_8b_444_rgb4_query, 5);
    REG_VPP_REXT_QUERY_TEST(vpp_10b_420_p010_query, 6);
    REG_VPP_REXT_QUERY_TEST(vpp_10b_422_y210_query, 7);
    REG_VPP_REXT_QUERY_TEST(vpp_10b_444_y410_query, 8);
#undef REG_VPP_REXT_QUERY_TEST
}


