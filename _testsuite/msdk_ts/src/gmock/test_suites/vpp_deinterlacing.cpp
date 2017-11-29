/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//     Copyright (c) 2016-2017 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_deinterlacing
{

class TestSuite : protected tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    enum
    {
        INIT,
        RESET,
    };

private:

    enum
    {
        MFX_PAR = 1,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        mfxU32 nFramesToProcess;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_BOB}}},

    {/*01*/ MFX_ERR_NONE, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED}}},

    {/*02*/ MFX_ERR_NONE, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED_NOREF}}},

    {/*03*/ MFX_ERR_NONE, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED_SCD}}},

    // PTIR modes are unsupported by regular VPP
    {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_DOUBLE}}},
    {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_SINGLE}}},
    {/*06*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FULL_FR_OUT}}},
    {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_HALF_FR_OUT}}},
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_24FPS_OUT}}},
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN}}},
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_30FPS_OUT}}},
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE}}},
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 0,{
        {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE},
        {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, 1},
        {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.TelecineLocation, 1}}
    },

    // customer case: reset after processing 1 frame
    {/*13*/ MFX_ERR_NONE, RESET, 1,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth,  5} }},
    {/*14*/ MFX_ERR_NONE, RESET, 3,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth,  5} }},

    {/*15*/ MFX_ERR_NONE, RESET, 1,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth,  5},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1} }},
    {/*16*/ MFX_ERR_NONE, RESET, 4,{{MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth,  5},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
                                    {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1} }},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxStatus sts = tc.sts;

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_par.AsyncDepth = 1;

    SETPARS(&m_par, MFX_PAR);

    mfxExtVPPDeinterlacing* extVPPDi = (mfxExtVPPDeinterlacing*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DEINTERLACING);
    if (MFX_DEINTERLACING_ADVANCED_SCD == extVPPDi->Mode)
    {
        if (MFX_OS_FAMILY_WINDOWS == g_tsOSFamily)
            sts = MFX_ERR_UNSUPPORTED; // only linux supports SCD mode

        if (MFX_OS_FAMILY_LINUX == g_tsOSFamily && MFX_HW_APL == g_tsHWtype)
            sts = MFX_ERR_UNSUPPORTED; // only Linux for BDW & SKL supports SCD mode for now

        if (!(m_par.vpp.In.FourCC == MFX_FOURCC_NV12 || m_par.vpp.In.FourCC == MFX_FOURCC_YV12))
            sts = MFX_ERR_UNSUPPORTED; // SCD kernels support only planar 8-bit formats
    }

    if ((   (MFX_FOURCC_UYVY == m_par.vpp.In.FourCC || MFX_FOURCC_UYVY == m_par.vpp.Out.FourCC)
         || (MFX_FOURCC_AYUV == m_par.vpp.In.FourCC || MFX_FOURCC_AYUV == m_par.vpp.Out.FourCC)
         || (MFX_FOURCC_P010 == m_par.vpp.In.FourCC || MFX_FOURCC_P010 == m_par.vpp.Out.FourCC)
         || (MFX_FOURCC_Y210 == m_par.vpp.In.FourCC || MFX_FOURCC_Y210 == m_par.vpp.Out.FourCC)
         || (MFX_FOURCC_Y410 == m_par.vpp.In.FourCC || MFX_FOURCC_Y410 == m_par.vpp.Out.FourCC))
        && g_tsHWtype < MFX_HW_ICL)
        sts = MFX_ERR_INVALID_VIDEO_PARAM; // Y210 deinterlacing works since ICL

    SetHandle();

    {
        auto parOut = m_par;
        m_pParOut = &parOut;

        g_tsStatus.expect((sts == MFX_ERR_INVALID_VIDEO_PARAM) ? MFX_ERR_UNSUPPORTED : sts);
        Query();

        m_pParOut = m_pPar;
    }

    g_tsStatus.expect((sts == MFX_ERR_UNSUPPORTED) ? MFX_ERR_INVALID_VIDEO_PARAM : sts);
    Init(m_session, m_pPar);

    if (RESET == tc.mode)
    {
        if (!m_initialized)
            throw tsSKIP;

        ProcessFrames(tc.nFramesToProcess);
        Reset();
        // library should free input surfaces used as reference
        mfxStatus locked_sts = m_pSurfPoolIn->CheckLockedCounter();
        if (locked_sts != MFX_ERR_NONE)
        {
            g_tsLog << "ERROR: there is Locked IN surface after Reset()\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    }

    Close(true); // Close and check surface pools locked cnt

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_deinterlacing);

}

namespace vpp_8b_420_yv12_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC             = MFX_FOURCC_YV12;
            m_par.vpp.In.BitDepthLuma       = 8;
            m_par.vpp.In.BitDepthChroma     = 8;
            m_par.vpp.In.Shift              = 0;
            m_par.vpp.In.ChromaFormat       = MFX_CHROMAFORMAT_YUV420;

            m_par.vpp.Out.FourCC            = MFX_FOURCC_NV12;
            m_par.vpp.Out.BitDepthLuma      = 8;
            m_par.vpp.Out.BitDepthChroma    = 8;
            m_par.vpp.Out.Shift             = 0;
            m_par.vpp.Out.ChromaFormat      = MFX_CHROMAFORMAT_YUV420;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_420_yv12_deinterlacing);
}

namespace vpp_8b_422_uyvy_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC             = MFX_FOURCC_UYVY;
            m_par.vpp.In.BitDepthLuma       = 8;
            m_par.vpp.In.BitDepthChroma     = 8;
            m_par.vpp.In.Shift              = 0;
            m_par.vpp.In.ChromaFormat       = MFX_CHROMAFORMAT_YUV422;

            m_par.vpp.Out.FourCC            = MFX_FOURCC_UYVY;
            m_par.vpp.Out.BitDepthLuma      = 8;
            m_par.vpp.Out.BitDepthChroma    = 8;
            m_par.vpp.Out.Shift             = 0;
            m_par.vpp.Out.ChromaFormat      = MFX_CHROMAFORMAT_YUV422;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_422_uyvy_deinterlacing);
}

namespace vpp_8b_422_yuy2_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC             = MFX_FOURCC_YUY2;
            m_par.vpp.In.BitDepthLuma       = 8;
            m_par.vpp.In.BitDepthChroma     = 8;
            m_par.vpp.In.Shift              = 0;
            m_par.vpp.In.ChromaFormat       = MFX_CHROMAFORMAT_YUV422;

            m_par.vpp.Out.FourCC            = MFX_FOURCC_YUY2;
            m_par.vpp.Out.BitDepthLuma      = 8;
            m_par.vpp.Out.BitDepthChroma    = 8;
            m_par.vpp.Out.Shift             = 0;
            m_par.vpp.Out.ChromaFormat      = MFX_CHROMAFORMAT_YUV422;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_422_yuy2_deinterlacing);
}

namespace vpp_8b_444_ayuv_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC             = MFX_FOURCC_AYUV;
            m_par.vpp.In.BitDepthLuma       = 8;
            m_par.vpp.In.BitDepthChroma     = 8;
            m_par.vpp.In.Shift              = 0;
            m_par.vpp.In.ChromaFormat       = MFX_CHROMAFORMAT_YUV444;

            m_par.vpp.Out.FourCC            = MFX_FOURCC_AYUV;
            m_par.vpp.Out.BitDepthLuma      = 8;
            m_par.vpp.Out.BitDepthChroma    = 8;
            m_par.vpp.Out.Shift             = 0;
            m_par.vpp.Out.ChromaFormat      = MFX_CHROMAFORMAT_YUV444;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_444_ayuv_deinterlacing);
}

namespace vpp_8b_444_rgb4_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC             = MFX_FOURCC_RGB4;
            m_par.vpp.In.BitDepthLuma       = 8;
            m_par.vpp.In.BitDepthChroma     = 8;
            m_par.vpp.In.Shift              = 0;
            m_par.vpp.In.ChromaFormat       = MFX_CHROMAFORMAT_YUV444;

            m_par.vpp.Out.FourCC            = MFX_FOURCC_RGB4;
            m_par.vpp.Out.BitDepthLuma      = 8;
            m_par.vpp.Out.BitDepthChroma    = 8;
            m_par.vpp.Out.Shift             = 0;
            m_par.vpp.Out.ChromaFormat      = MFX_CHROMAFORMAT_YUV444;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_444_rgb4_deinterlacing);
}

namespace vpp_10b_420_p010_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_P010;
            m_par.vpp.In.BitDepthLuma = 10;
            m_par.vpp.In.BitDepthChroma = 10;
            m_par.vpp.In.Shift = 1;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

            m_par.vpp.Out.FourCC = MFX_FOURCC_P010;
            m_par.vpp.Out.BitDepthLuma = 10;
            m_par.vpp.Out.BitDepthChroma = 10;
            m_par.vpp.Out.Shift = 1;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_10b_420_p010_deinterlacing);
}

namespace vpp_10b_422_y210_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_Y210;
            m_par.vpp.In.BitDepthLuma = 10;
            m_par.vpp.In.BitDepthChroma = 10;
            m_par.vpp.In.Shift = 1;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV422;

            m_par.vpp.Out.FourCC = MFX_FOURCC_Y210;
            m_par.vpp.Out.BitDepthLuma = 10;
            m_par.vpp.Out.BitDepthChroma = 10;
            m_par.vpp.Out.Shift = 1;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_10b_422_y210_deinterlacing);
}

namespace vpp_10b_444_y410_deinterlacing
{
    class TestSuite : public vpp_deinterlacing::TestSuite
    {
    public:
        TestSuite() : vpp_deinterlacing::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_Y410;
            m_par.vpp.In.BitDepthLuma = 10;
            m_par.vpp.In.BitDepthChroma = 10;
            m_par.vpp.In.Shift = 0;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV444;

            m_par.vpp.Out.FourCC = MFX_FOURCC_Y410;
            m_par.vpp.Out.BitDepthLuma = 10;
            m_par.vpp.Out.BitDepthChroma = 10;
            m_par.vpp.Out.Shift = 0;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_10b_444_y410_deinterlacing);
}