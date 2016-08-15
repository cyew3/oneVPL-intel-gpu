/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace vp9d_query_io_surf
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9)  { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

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
        std::string stream;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxF32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION, "forBehaviorTest/foreman_cif.ivf"},
    {/*02*/ MFX_ERR_NULL_PTR, NULL_PAR, "forBehaviorTest/foreman_cif.ivf"},
    {/*03*/ MFX_ERR_NULL_PTR, NULL_REQ, "forBehaviorTest/foreman_cif.ivf"},
    {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0},
    },
    {/*05*/ MFX_ERR_UNSUPPORTED, PAR_ACCEL, "conformance/vp9/SBE/10bit/Syntax_VP9_FC2p2b10_16384x240_201_extra_tiles_2.0.0.vp9",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*06*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*07*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
    },
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
    },
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY},
    },
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*12*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*13*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*14*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*15*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*16*/ MFX_ERR_NONE, ASYNC, "forBehaviorTest/foreman_cif.ivf",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];

    MFXInit();

    if(m_uid)
        Load();

    mfxFrameAllocRequest request_tmp = {0};

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 11264); 

    m_bs_processor = &reader;

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

    // Provide offset to uncompressed header for ivf format (offset = 8 DWORDS of IVF stream header + 3 DWORDS of IVF frame header)
    m_pBitstream->DataOffset = 44;
    m_pBitstream->DataLength = m_pBitstream->DataLength - m_pBitstream->DataOffset;

    DecodeHeader();

    m_pPar->IOPattern = tc.set_par[0].v; 

    mfxSession m_session_tmp = m_session;

    if (tc.mode == NULL_PAR)
        m_pPar = 0;
    else if (tc.mode == NULL_SESSION)
        m_session = 0;
    else if (tc.mode == NULL_REQ)
        m_pRequest = 0;
    else if (tc.mode ==ASYNC)
    {
        m_par.AsyncDepth = 3;
        QueryIOSurf(m_session, m_pPar, &request_tmp);
        m_par.AsyncDepth = 1;
    }

    g_tsStatus.expect(tc.sts);

    if(tc.mode == PAR_ACCEL && g_tsImpl == MFX_IMPL_SOFTWARE)
        g_tsStatus.expect(MFX_ERR_NONE);

    QueryIOSurf(m_session, m_pPar, m_pRequest);

    if (g_tsStatus.get() > MFX_ERR_NONE)
    {
        if (tc.mode != NULL_SESSION && tc.mode != NULL_PAR && tc.mode != NULL_REQ)
            EXPECT_EQ(0, memcmp(&(m_par.mfx.FrameInfo), &(m_request.Info), sizeof(mfxFrameInfo)))
                << "ERROR: QueryIOSurf didn't fill up frame info in returned mfxFrameAllocRequest structure \n";
        else if (m_pPar->IOPattern&MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
        {
            EXPECT_EQ(MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_DECODE, m_pRequest->Type)
                << "ERROR: QueryIOSurf set wrong request type \n";
        }
        else if (m_pPar->IOPattern&MFX_IOPATTERN_OUT_VIDEO_MEMORY)
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
    else if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vp9d_query_io_surf);

}

