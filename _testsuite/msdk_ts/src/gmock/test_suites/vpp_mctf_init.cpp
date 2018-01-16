/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright (c) 2018 Intel Corporation. All Rights Reserved.

File Name: vpp_mctf_init.cpp

\* ****************************************************************************** */

/*!
\file vpp_mctf_init.cpp
\brief Gmock test vpp_init

Description:
This suite tests VPP - MCTF initialization\n
<<<<<<< HEAD
1 test case for check unsupported on Windows
=======
50 test cases with the following differences:
- Valid, invalid or already inited session\n
- Filters\n
- Video or system IOPattern\n
- Output status

Algorithm:
- Initialize Media SDK lib\n
- Set frame allocator\n
- Initialize the VPP operation\n
>>>>>>> [msdk_gmock] To update tests for MCTF
*/

#include "ts_vpp.h"
#include "ts_struct.h"

//!Namespace of VPP Init test
namespace vpp_mctf_init
{

    //!\brief Main test class
    class TestSuite : protected tsVideoVPP, public tsSurfaceProcessor
    {
    public:
        //! \brief A constructor (VPP)
        TestSuite() : tsVideoVPP(true)
        {
            m_surf_in_processor = this;
            m_par.vpp.In.FourCC = MFX_FOURCC_YUY2;
            m_par.vpp.Out.FourCC = MFX_FOURCC_NV12;
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        virtual int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
    protected:

        enum
        {
            MFX_PAR
        };

        typedef enum
        {
            STANDARD,
            NULL_PTR_SESSION,
            NULL_PTR_PARAM,
            DOUBLE_VPP_INIT,
        } TestMode;

        struct tc_struct
        {
            mfxStatus sts;
            TestMode  mode;

            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU64 v;
            } set_par[MAX_NPARS];
        };

        //! \brief Set of test cases
        static const tc_struct test_case[];

        //! \brief Stream generator
        tsNoiseFiller m_noise;

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxFrameAllocator* pfa = (mfxFrameAllocator*)m_spool_in.GetAllocator();
            m_noise.tsSurfaceProcessor::ProcessSurface(&s, pfa);

            return MFX_ERR_NONE;
        }

        int RunTest(const tc_struct& tc);
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // mctf related tests
        //// this is sort of negative test (needs to be removed or on/off depending on a platform?)
        //{/*00*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        //    {
        //        { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
        //    },
        //}
        //,
        {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength,        21 },
            },
        },

        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength,        std::numeric_limits<mfxU16>::max() },
            },
        },

        {/*02*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength,        0 },
            },
        },

        {/*03*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength,        10 },
            },
        },

        {/*04*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength,        20 },
            },
        },
        // check output fourCC

        {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_YV12 },
            },
        },
        {/*06*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_NV16 },
            },
        },

        {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_YUY2 },
            },
        },
        {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_RGB3 },
            },
        },
        {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_RGB4 },
            },
        },
        {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_P010 },
            },
        },
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_P016 },
            },
        },
        {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_P210 },
            },
        },
        {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_BGR4 },
            },
        },
        {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_A2RGB10 },
            },
        },
        {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_ARGB16 },
            },
        },
        {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_ABGR16 },
            },
        },
        {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_R16 },
            },
        },
        {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_AYUV },
            },
        },
        {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_AYUV_RGB4 },
            },
        },
        {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_UYVY },
            },
        },
        {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_Y210 },
            },
        },
        {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_Y216 },
            },
        },
        {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_Y410 },
            },
        },
        {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_Y416 },
            },
        },

        // mctf does not work with interlace
        {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_UNKNOWN },
            },
        },
        // mctf does not work with interlace
        {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_FIELD_TFF },
            },
        },

        // mctf does not work with interlace
        {/*27*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
            },
        },

        {/*28*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,   MFX_FOURCC_NV12 },
            },
        },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        // mctf, overlap param
        {/*29*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap,        MFX_CODINGOPTION_ON },
            },
        },

        {/*30*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap,        MFX_CODINGOPTION_OFF },
            },
        },

        {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap,        MFX_CODINGOPTION_ADAPTIVE },
            },
        },

        {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap,        MFX_CODINGOPTION_ADAPTIVE + 1 },
            },
        },

        {/*33*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap,        MFX_CODINGOPTION_UNKNOWN },
            },
        },

        // mctf, deblock param
        {/*34*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking,        MFX_CODINGOPTION_ON },
            },
        },

        {/*35*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking,        MFX_CODINGOPTION_OFF },
            },
        },

        {/*36*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking,        MFX_CODINGOPTION_ADAPTIVE },
            },
        },

        {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking,        MFX_CODINGOPTION_ADAPTIVE + 1 },
            },
        },

        {/*38*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking,        MFX_CODINGOPTION_UNKNOWN },
            },
        },

        // mctf, TemporalMode param
        {/*39*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,        MFX_MCTF_TEMPORAL_MODE_SPATIAL },
            },
        },

        {/*40*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,        MFX_MCTF_TEMPORAL_MODE_1REF },
            },
        },

        {/*41*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,        MFX_MCTF_TEMPORAL_MODE_2REF },
            },
        },

        {/*42*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,        MFX_MCTF_TEMPORAL_MODE_4REF },
            },
        },

        {/*43*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,        MFX_MCTF_TEMPORAL_MODE_UNKNOWN },
            },
        },

        {/*44*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking,        MFX_MCTF_TEMPORAL_MODE_4REF + 1 },
            },
        },

        // mctf, MVPrecision param
        {/*45*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision,        MFX_MVPRECISION_INTEGER },
            },
        },

        {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision,        MFX_MVPRECISION_HALFPEL },
            },
        },

        {/*47*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision,        MFX_MVPRECISION_QUARTERPEL },
            },
        },

        {/*48*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision,        MFX_MVPRECISION_UNKNOWN },
            },
        },

        {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision,        3 },
            },
        },

        {/*50*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision,        MFX_MVPRECISION_QUARTERPEL + 1 },
            },
        },

        // any FRC-like algorithm is bad for MCTF with delays
        {/*51*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_4REF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN,   30 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD,    1 },

                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN,   60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD,    1 },
            },
        },

        // any FRC-like algorithm is bad for MCTF with delays
        {/*52*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_2REF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN,   30 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD,    1 },

                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN,   60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD,    1 },
            },
        },

        {/*53*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_1REF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN,   30 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD,    1 },

                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN,   60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD,    1 },
            },
        },

        {/*54*/ MFX_ERR_NONE, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_SPATIAL },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN,   30 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD,    1 },

                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN,   60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD,    1 },
            },
        },

        // any FRC-like algorithm is bad for MCTF with delays
        {/*55*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_4REF },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Header,  MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION },
            },
        },

        // any FRC-like algorithm is bad for MCTF with delays
        {/*56*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_2REF },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Header,  MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION },
            },
        },

        // any FRC-like algorithm is bad for MCTF with delays
        {/*57*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_1REF },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Header,  MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION },
            },
        },

        // any FRC-like algorithm is bad for MCTF with delays
        {/*58*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_MCTF_TEMPORAL_MODE_SPATIAL },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Header,  MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION },
            },
        },
#endif

        // by default, 2-ref case is used; its incompatible with FRC
        {/*59*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN,   30 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD,    1 },

                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN,   60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD,    1 },
            },
        },
        // any FRC-like algorithm is bad for MCTF with delays
        {/*60*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode,  MFX_EXTBUFF_VPP_MCTF },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Header,  MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION },
            },
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        return  RunTest(test_case[id]);
    }

    int TestSuite::RunTest(const TestSuite::tc_struct& tc)
    {
        TS_START;
        mfxStatus sts = tc.sts;
        SETPARS(&m_par, MFX_PAR);

        if (MFX_FOURCC_YV12 == m_par.vpp.In.FourCC && MFX_ERR_NONE == tc.sts
            && MFX_OS_FAMILY_WINDOWS == g_tsOSFamily && g_tsImpl & MFX_IMPL_VIA_D3D11)
            sts = MFX_WRN_PARTIAL_ACCELERATION;

        if (NULL_PTR_SESSION != tc.mode)
        {
            MFXInit();
        }
        if (NULL_PTR_PARAM == tc.mode)
        {
            m_pPar = nullptr;
        }
        if (DOUBLE_VPP_INIT == tc.mode)
        {
            g_tsStatus.expect(tc.sts);
            Init(m_session, m_pPar);
            Close();
        }

        CreateAllocators();

        SetFrameAllocator();
        SetHandle();

        g_tsStatus.expect(sts);
        Init(m_session, m_pPar);

        if (sts == MFX_ERR_NONE)
        {
            AllocSurfaces();
            ProcessFrames(g_tsConfig.sim ? 3 : 10);
        }

        TS_END;
        return 0;
    }

    //!\brief Reg the test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_mctf_init);
}