/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace avcd_reset
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC)  { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    private:

    enum
    {
        MFX_PAR = 1,
        RESET,
        NULL_PAR,
        NULL_SESSION,
        SESSION_NOT_INITIALIZED,
        ASYNC,
        DIFF_FRAME_INFO,
        COMPARE_SURF,
        ALLOC_OPAQUE,
        ALLOC_OPAQUE_SYSTEM = ALLOC_OPAQUE+1,
        ALLOC_OPAQUE_D3D = ALLOC_OPAQUE+2,
        ALLOC_OPAQUE_LESS_SYSTEM = ALLOC_OPAQUE+3,
        ALLOC_OPAQUE_LESS_D3D = ALLOC_OPAQUE+4,
        ALLOC_OPAQUE_MORE_SYSTEM = ALLOC_OPAQUE+5,
        ALLOC_OPAQUE_MORE_D3D = ALLOC_OPAQUE+6,
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
    {/*00*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.h264"},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION, "forBehaviorTest/foreman_cif.h264"},
    {/*02*/ MFX_ERR_NOT_INITIALIZED, SESSION_NOT_INITIALIZED, "forBehaviorTest/foreman_cif.h264"},
    {/*03*/ MFX_ERR_NULL_PTR, NULL_PAR, "forBehaviorTest/foreman_cif.h264"},
    {/*04*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME},
    },
    {/*05*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, DIFF_FRAME_INFO, "forBehaviorTest/foreman_cif.h264"},
    {/*06*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, ASYNC, "forBehaviorTest/foreman_cif.h264",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {RESET, &tsStruct::mfxVideoParam.AsyncDepth, 10},
        }
    },
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, ASYNC, "forBehaviorTest/foreman_cif.h264",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 10},
            {RESET, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        }
    },
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP},
    },
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP},
    },
    {/*11*/ MFX_ERR_NONE, ALLOC_OPAQUE_SYSTEM, "forBehaviorTest/foreman_cif.h264",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*12*/ MFX_ERR_NONE, ALLOC_OPAQUE_D3D, "forBehaviorTest/foreman_cif.h264",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*13*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ALLOC_OPAQUE_LESS_SYSTEM, "forBehaviorTest/foreman_cif.h264",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*14*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ALLOC_OPAQUE_LESS_D3D, "forBehaviorTest/foreman_cif.h264",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*15*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ALLOC_OPAQUE_MORE_SYSTEM, "forBehaviorTest/foreman_cif.h264",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*16*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ALLOC_OPAQUE_MORE_D3D, "forBehaviorTest/foreman_cif.h264",
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.CodecProfile, 5},
    },
    {/*18*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.CodecProfile, 122},
    },
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.CodecProfile, 244},
    },
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422},
    },
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444},
    },
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV411},
    },
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, "forBehaviorTest/foreman_cif.h264",
        {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422V},
    },
    {/*24*/ MFX_ERR_NONE, COMPARE_SURF, "forBehaviorTest/foreman_cif.h264"},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];

    MFXInit();
    if(m_uid)
        Load();

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1024);
    m_bs_processor = &reader;

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

    DecodeHeader();

    SETPARS(m_pPar, MFX_PAR);

    if (tc.mode == ASYNC)
    {
        tsExtBufType<mfxVideoParam> tmp_par = m_par;
        tmp_par.AsyncDepth = 100;

        g_tsStatus.expect(MFX_ERR_NONE);
        MFXVideoDECODE_Query(m_session, 0, &tmp_par);
    }

    if (tc.mode & ALLOC_OPAQUE)
    {
        AllocSurfaces();

        m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
        mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        MFXVideoDECODE_QueryIOSurf(m_session, m_pPar, &m_request);

        if (tc.mode == ALLOC_OPAQUE_SYSTEM || tc.mode == ALLOC_OPAQUE_LESS_SYSTEM || tc.mode == ALLOC_OPAQUE_MORE_SYSTEM)
            m_request.Type = MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_DECODE|MFX_MEMTYPE_OPAQUE_FRAME;

        AllocOpaque(m_request, *osa);
    }

    g_tsStatus.expect(MFX_ERR_NONE);
    Init(m_session, m_pPar);

    mfxSession m_session_tmp = m_session;

    tsExtBufType<mfxVideoParam> par_reset = m_par;
    mfxVideoParam* pPar_reset = &par_reset;
    SETPARS(pPar_reset, RESET);

    if(tc.mode == DIFF_FRAME_INFO)
    {
        pPar_reset->mfx.FrameInfo.Width *= 2;
        pPar_reset->mfx.FrameInfo.Height *= 2;
        pPar_reset->mfx.FrameInfo.CropW = (pPar_reset->mfx.FrameInfo.Width + 15) & ~0xF;
        pPar_reset->mfx.FrameInfo.CropH = (pPar_reset->mfx.FrameInfo.Height + 15) & ~0xF;
    }

    if (!(tc.mode & ALLOC_OPAQUE))
    {
        AllocSurfaces();
        if(!m_pFrameAllocator && (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
    }
    else
    {
        mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)par_reset.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (tc.mode == ALLOC_OPAQUE_LESS_SYSTEM || tc.mode == ALLOC_OPAQUE_LESS_D3D)
        {
            m_request.NumFrameSuggested = m_request.NumFrameMin = m_request.NumFrameMin - 1;
        }
        else if (tc.mode == ALLOC_OPAQUE_MORE_SYSTEM || tc.mode == ALLOC_OPAQUE_MORE_D3D)
        {
            m_request.NumFrameSuggested = m_request.NumFrameMin = m_request.NumFrameMin + 1;
        }

        AllocOpaque(m_request, *osa);
    }

    if (tc.mode == NULL_PAR)
        pPar_reset = 0;
    else if (tc.mode == NULL_SESSION)
        m_session = 0;
    else if (tc.mode == SESSION_NOT_INITIALIZED)
        Close();
    else if (tc.mode == COMPARE_SURF)
        DecodeFrames(1);

    if (tc.mode != ASYNC)
    {
        g_tsStatus.expect(tc.sts);
        Reset(m_session, pPar_reset);
    }
    else
    {
        TRACE_FUNC2(MFXVideoDECODE_Reset, m_session, pPar_reset);
        mfxStatus sts = MFXVideoDECODE_Reset(m_session, pPar_reset);
        if (sts != MFX_ERR_INCOMPATIBLE_VIDEO_PARAM && sts != MFX_ERR_INVALID_VIDEO_PARAM)
            TS_FAIL_TEST("Component specified that it supports AsyncDepth in Query but when Reset was called with different AsyncDepth it din't return a correct error or warning", MFX_ERR_ABORTED);
    }

    if (tc.mode == COMPARE_SURF)
    {
        mfxFrameSurface1* pSurfOut0;
        pSurfOut0 = &*m_pSurfOut;

        m_pSurfOut = 0;

        tsBitstreamReader reader(sname, 1024);
        m_bs_processor = &reader;

        m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

        DecodeFrames(1);

        if (&*pSurfOut0 != &*m_pSurfOut)
            TS_FAIL_TEST("Output surfaces from DecodeFrames() before Reset and DecodeFrames() after Reset are not equal", MFX_ERR_NONE);
    }
    else if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_reset);

}

// Legacy test: behavior_h264_decode_suite, b_AVC_DECODE_Reset, b_AVC_DECODE_Reset_Ext
