/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <memory>
#include "ts_decoder.h"
#include "ts_struct.h"

#define TEST_NAME vp9d_query_io_surf
namespace TEST_NAME
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9)  { }
    ~TestSuite() {}

    static const unsigned int n_cases;


    template<const mfxU32 fourcc>
    int RunTest_fourcc(const unsigned int id);

private:

    enum
    {
        MFX_PAR = 1,
        NULL_PAR,
        NULL_REQ,
        NULL_SESSION,
        PAR_ACCEL,
        ASYNC,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU64 v;
            mfxU32 stage;
        } set_par[MAX_NPARS];
        const char* stream;
    };

    static const tc_struct test_case[];
    int RunTest(const tc_struct& tc);

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0,
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NULL_PTR, NULL_PAR},
    {/*03*/ MFX_ERR_NULL_PTR, NULL_REQ},
    {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,
        {&tsStruct::mfxVideoParam.IOPattern, 0},
    },
    {/*05*/ MFX_ERR_NONE, 0,
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*06*/ MFX_ERR_NONE, 0,
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*07*/ MFX_ERR_NONE, 0,
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
    },
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
    },
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY},
    },
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*12*/ MFX_ERR_NONE, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*13*/ MFX_ERR_NONE, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*14*/ MFX_ERR_NONE, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*15*/ MFX_ERR_NONE, 0, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*16*/ MFX_ERR_NONE, ASYNC, 
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

struct streamDesc
{
    mfxU16 w;
    mfxU16 h;
    const char *name;
};

const streamDesc streams[] = {
    {352,288,"forBehaviorTest/foreman_cif.ivf"                                                      },
    {432,240,"conformance/vp9/SBE/8bit_444/Stress_VP9_FC2p1ss444_432x240_250_extra_stress_2.2.0.vp9"},
    {432,240,"conformance/vp9/SBE/10bit/Stress_VP9_FC2p2b10_432x240_050_intra_stress_1.5.vp9"       },
    {432,240,"conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_101_inter_basic_2.0.0.vp9"},
};

const streamDesc& getStreamDesc(const mfxU32& fourcc)
{
    switch(fourcc)
    {
    case MFX_FOURCC_NV12: return streams[0];
    case MFX_FOURCC_AYUV: return streams[1];
    case MFX_FOURCC_P010: return streams[2];
    case MFX_FOURCC_Y410: return streams[3];
    default: assert(0); return streams[0];
    }
}

template<const mfxU32 fourcc>
int TestSuite::RunTest_fourcc(const unsigned int id)
{
    const streamDesc& bsDesc = getStreamDesc(fourcc);

    m_par.mfx.FrameInfo.FourCC = fourcc;
    set_chromaformat_mfx(&m_par);
    m_par.mfx.FrameInfo.Width = bsDesc.w;
    m_par.mfx.FrameInfo.Height = bsDesc.h;

    if(MFX_FOURCC_P010 == fourcc || MFX_FOURCC_Y410 == fourcc)
        m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 10;
    m_par_set = true; //we are not testing DecodeHeader here

    const tc_struct& tc = test_case[id];
    return RunTest(tc);
}

int TestSuite::RunTest(const tc_struct& tc)
{
    TS_START;

    MFXInit();

    if(m_uid)
        Load();

    mfxFrameAllocRequest request_tmp = {0};
    tsStruct::SetPars(m_par, tc);

    if (tc.mode ==ASYNC)
    {
        m_par.AsyncDepth = 3;
        QueryIOSurf(m_session, m_pPar, &request_tmp);
        m_par.AsyncDepth = 1;
    }

    //test section
    {
        mfxSession ses     = (NULL_SESSION  == tc.mode) ? nullptr : m_session;
        mfxVideoParam* par = (NULL_PAR      == tc.mode) ? nullptr : &m_par;
        mfxFrameAllocRequest* req = (NULL_REQ      == tc.mode) ? nullptr : &m_request;

        g_tsStatus.expect(tc.sts);
        QueryIOSurf(ses, par, req);

        if (g_tsStatus.get() > MFX_ERR_NONE)
        {
            EXPECT_EQ(0, memcmp(&(m_par.mfx.FrameInfo), &(m_request.Info), sizeof(mfxFrameInfo)))
                << "ERROR: QueryIOSurf didn't fill up frame info in returned mfxFrameAllocRequest structure \n";
            if (m_pPar->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            {
                EXPECT_EQ(MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_DECODE, m_pRequest->Type)
                    << "ERROR: QueryIOSurf set wrong request type \n";
            }
            else if (m_pPar->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            {
                EXPECT_EQ(MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_FROM_DECODE, m_pRequest->Type)
                    << "ERROR: QueryIOSurf set wrong request type \n";
            }
        }

        if (tc.mode == ASYNC)
        {
            EXPECT_NE(request_tmp.NumFrameMin, m_request.NumFrameSuggested)
                << "ERROR: QueryIOSurf didn't change NumFrameMin and NumFrameSuggested when AsyncDepth was specified \n";
            EXPECT_NE(request_tmp.NumFrameSuggested, m_request.NumFrameSuggested)
                << "ERROR: QueryIOSurf didn't change NumFrameMin and NumFrameSuggested when AsyncDepth was specified \n";
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_query_io_surf,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_query_io_surf, RunTest_fourcc<MFX_FOURCC_P010>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_query_io_surf,  RunTest_fourcc<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_query_io_surf, RunTest_fourcc<MFX_FOURCC_Y410>, n_cases);

}

