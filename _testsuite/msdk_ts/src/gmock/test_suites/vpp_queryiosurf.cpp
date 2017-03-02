/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

File Name: vpp_queryiosurf.cpp
\* ****************************************************************************** */

/*!
\file vpp_queryiosurf.cpp
\brief Gmock test vpp_queryiosurf

Description:
This suite tests  VPP\n

Algorithm:
- Initialize MSDK
- Set test suite params
- Set test case params
- Call QueryIOSurf
- Check return status

*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace vpp_queryiosurf
{

    enum
    {
        MFX_PAR = 0,
    };

    //! \brief Enum with test modes
    enum MODE
    {
        STANDART,
        SESSION_INVALID,
        PARAM_ASYNCDEPTH,
        PARAM_ZERO,
        REQUEST_ZERO
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! \brief test mode
        MODE mode;
        //! \brief Array for DoNotUse algs (if required)
        mfxU32 dnu[4];
        //! \brief QueryIOSurf expected return status
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

        {/*00*/ STANDART, { MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 }
        }
        },

        {/*01*/ STANDART, { MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 }
        }
        },

        {/*02*/ STANDART,{ MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE },

        {/*03*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE },

        {/*04*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP } , MFX_ERR_NONE },

        {/*05*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE },

        {/*06*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF }
        }
        },

        {/*07*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 }
        }
        },

        {/*08*/ SESSION_INVALID,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_INVALID_HANDLE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 }
        }
        },

        {/*09*/ PARAM_ZERO,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_NULL_PTR ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 }
        }
        },

        {/*10*/ REQUEST_ZERO,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_NULL_PTR ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 }
        }
        },

        {/*11*/ REQUEST_ZERO,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_NULL_PTR ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0 }
        }
        },

        {/*12*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_INVALID_VIDEO_PARAM ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0 }
        }
        },

        {/*13*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_INVALID_VIDEO_PARAM ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 730 }
        }
        },

        {/*14*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_INVALID_VIDEO_PARAM ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 730 }
        }
        },

        {/*15*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_INVALID_VIDEO_PARAM ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 586 }
        }
        },

        {/*16*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_INVALID_VIDEO_PARAM ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 586 }
        }
        },

        {/*17*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_INVALID_VIDEO_PARAM ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 586 }
        }
        },

        {/*18*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }, MFX_ERR_INVALID_VIDEO_PARAM ,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 586 }
        }
        },

        {/*19*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 }
        }
        },

        {/*20*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 }
        }
        },

        {/*21*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE },

        {/*22*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE },

        {/*23*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP } , MFX_ERR_NONE },

        {/*24*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE },

        {/*25*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF }
        }
        },

        {/*26*/ PARAM_ASYNCDEPTH,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 }
        }
        },

        {/*27*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_INVALID_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
        }
        },

        {/*28*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_INVALID_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
        }
        },

        {/*29*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_INVALID_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY }
        }
        },

        {/*30*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_INVALID_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY }
        }
        },

        {/*31*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY }
        }
        },

        {/*32*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY }
        }
        },

        {/*33*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY }
        }
        },

        {/*35*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY }
        }
        },


        {/*35*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_INVALID_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY }
        }
        },

        {/*36*/ STANDART,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_SCENE_ANALYSIS, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL } , MFX_ERR_INVALID_VIDEO_PARAM,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 576 },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY }
        }
        }

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        MFXInit();
        auto& tc = test_case[id];

        mfxU16 fr_min_1 = -1;
        mfxU16 fr_min_2 = -1;
        mfxU16 fr_sug_1 = -1;
        mfxU16 fr_sug_2 = -1;

        m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE, sizeof(mfxExtVPPDoNotUse));
        mfxExtVPPDoNotUse* buf = (mfxExtVPPDoNotUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);
        for(mfxU32 al : tc.dnu) {
            if(al != 0)
                buf->NumAlg++;
        }
        buf->AlgList = new mfxU32[buf->NumAlg];
        memcpy(buf->AlgList, tc.dnu, sizeof(tc.dnu));

        SETPARS(&m_par, MFX_PAR);

        mfxSession work_ses = m_session;
        switch(tc.mode) {
            case MODE::SESSION_INVALID:
                work_ses = 0;
                break;
            case MODE::PARAM_ZERO:
                m_pPar = nullptr;
                break;
            case MODE::PARAM_ASYNCDEPTH:
                QueryIOSurf(work_ses, m_pPar, m_pRequest);
                fr_min_1 = m_request[0].NumFrameMin;
                fr_min_2 = m_request[1].NumFrameMin;
                fr_sug_1 = m_request[0].NumFrameSuggested;
                fr_sug_2 = m_request[1].NumFrameSuggested;
                m_par.AsyncDepth = 1;
                break;
            case MODE::REQUEST_ZERO:
                m_pRequest = nullptr;
                break;
            default:
                break;
        }


        g_tsStatus.expect(tc.expected);
        QueryIOSurf(work_ses, m_pPar, m_pRequest);

        if(tc.mode == MODE::PARAM_ASYNCDEPTH && 
           fr_min_1 == m_request[0].NumFrameMin &&
           fr_min_2 == m_request[1].NumFrameMin&&
           fr_sug_1 == m_request[0].NumFrameSuggested&&
           fr_sug_1 == m_request[1].NumFrameSuggested) {
            g_tsLog << "ERROR, QueryIOSurf didn't change NumFrameMin and NumFrameSuggested when AsyncDepth was specified\n";
        }

        TS_END;
        return 0;
    }

    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_queryiosurf);
}