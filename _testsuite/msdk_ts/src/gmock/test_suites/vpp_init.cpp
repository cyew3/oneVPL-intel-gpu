/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

File Name: vpp_init.cpp

\* ****************************************************************************** */

/*!
\file vpp_init.cpp
\brief Gmock test vpp_init

Description:
This suite tests VPP initialization\n
35 test cases with the following differences:
- Valid, invalid or already inited session\n
- Filters\n
- Video or system IOPattern\n
- Output status

Algorithm:
- Initialize Media SDK lib\n
- Set frame allocator\n
- Initialize the VPP operation\n
*/

#include "ts_vpp.h"
#include "ts_struct.h"

//!Namespace of VPP Init test
namespace vpp_init
{

//!\brief Main test class
class TestSuite: tsVideoVPP, public tsSurfaceProcessor
{
public:
    //! \brief A constructor (VPP)
    TestSuite(): tsVideoVPP(true)
    {
        m_surf_in_processor  = this;
        m_par.vpp.In.FourCC  = MFX_FOURCC_YUY2;
        m_par.vpp.Out.FourCC = MFX_FOURCC_NV12;
        m_par.IOPattern      = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
    //! \brief A destructor
    ~TestSuite() { }
    //! \brief Main method. Runs test case
    //! \param id - test case number
    int RunTest(unsigned int id);
    //! The number of test cases
    static const unsigned int n_cases;
private:

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
};

mfxU64 binary_64f64u(const mfxF64 in)
{
    mfxU64 out;
    memcpy(&out, &in, sizeof(mfxU64));
    return out;
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_INVALID_HANDLE, NULL_PTR_SESSION,
        {
        },
    },
    {/*01*/ MFX_ERR_NULL_PTR, NULL_PTR_PARAM,
        {
        },
    },
    {/*02*/ MFX_ERR_NONE, DOUBLE_VPP_INIT,
        {
        },
    },
    {/*03*/ MFX_ERR_NONE, STANDARD,
        {
        },
    },
    {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YUY2},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width,        0},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height,       0},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_RGB4},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width,       0},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height,      0},
        },
    },
    {/*05*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern,           MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        },
    },
    {/*06*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width,        730},
        },
    },
    {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width,       730},
        },
    },
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height,       490},
        },
    },
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height,      490},
        },
    },
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height,       490},
        },
    },
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height,      490},
        },
    },
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.Protected,           MFX_PROTECTION_PAVP},
        },
    },
    {/*13*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.Protected,           MFX_PROTECTION_PAVP},
            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern,           MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        },
    },
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.Protected,           0xF},
            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern,           MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        },
    },
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB3},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_NV12},
        },
    },
    {/*16*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB4},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_NV12},
        },
    },
    {/*17*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_NV12},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_NV12},
        },
    },
    {/*18*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YV12},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_NV12},
        },
    },
    {/*19*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YUY2},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_NV12},
        },
    },
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB3},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_RGB3},
        },
    },
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB4},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_RGB3},
        },
    },
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB3},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YUY2},
        },
    },
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB3},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YV12},
        },
    },
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB4},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YV12},
        },
    },
    {/*25*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB4},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YUY2},
        },
    },
    {/*26*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_NV12},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YUY2},
        },
    },
    {/*27*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YV12},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YUY2},
        },
    },
    {/*28*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YV12},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YUY2},
        },
    },
    {/*29*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YUY2},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_YUY2},
        },
    },
    {/*30*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_UNKNOWN},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_UNKNOWN},
        },
    },
    {/*31*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_UNKNOWN},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_PROGRESSIVE},
        },
    },
    {/*32*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_PROGRESSIVE},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_UNKNOWN},
        },
    },
    {/*33*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_PROGRESSIVE},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_PROGRESSIVE},
        },
    },
    {/*34*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_PROGRESSIVE},
        },
    },
    {/*35*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_FIELD_TFF},
        },
    },
    {/*36*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_FIELD_BFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_PROGRESSIVE},
        },
    },
    {/*37*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,    MFX_PICSTRUCT_FIELD_BFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct,   MFX_PICSTRUCT_FIELD_BFF},
        },
    },
    {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtCodingOption.ViewOutput,     MFX_CODINGOPTION_OFF},
        },
    },
    {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVppAuxData.PicStruct,        MFX_PICSTRUCT_UNKNOWN},
        },
    },
    {/*40*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor,      50},
        },
    },
    {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor,      101},
        },
    },
    {/*42*/ MFX_ERR_NONE, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness,       binary_64f64u(50)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast,         binary_64f64u(5)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue,              binary_64f64u(-100)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation,       binary_64f64u(6)},
        },
    },
    {/*43*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness,       binary_64f64u(150)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast,         binary_64f64u(5)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue,              binary_64f64u(-100)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation,       binary_64f64u(6)},
        },
    },
    {/*44*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness,       binary_64f64u(50)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast,         binary_64f64u(15)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue,              binary_64f64u(-100)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation,       binary_64f64u(6)},
        },
    },
    {/*45*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness,       binary_64f64u(50)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast,         binary_64f64u(5)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue,              binary_64f64u(-200)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation,       binary_64f64u(6)},
        },
    },
    {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, STANDARD,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness,       binary_64f64u(50)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast,         binary_64f64u(5)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue,              binary_64f64u(-100)},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation,       binary_64f64u(16)},
        },
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];

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
        ProcessFrames(10);
    }

    TS_END;
    return 0;
}

//!\brief Reg the test suite into test system
TS_REG_TEST_SUITE_CLASS(vpp_init);
}
