/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace avcd_init_sfc
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
        NULL_PAR,
        NULL_SESSION,
        NULL_BS,
        PAR_ACCEL,
        NO_EXT_ALLOCATOR,
        ALLOC_OPAQUE = 8,
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
        mfxI32 num_calls;
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
    {/*00*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    {/*01*/ MFX_ERR_UNSUPPORTED, 0, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
    },
    {/*02*/ MFX_ERR_UNSUPPORTED, ALLOC_OPAQUE_SYSTEM, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*03*/ MFX_ERR_UNSUPPORTED, ALLOC_OPAQUE_D3D, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*04*/ MFX_ERR_UNSUPPORTED, 0, "forBehaviorTest/foreman_cif.h264", 1,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}
        },
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

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1024);
    m_bs_processor = &reader;

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

    DecodeHeader();

    SETPARS(m_pPar, MFX_PAR);

    if (tc.mode != NO_EXT_ALLOCATOR && tc.mode != PAR_ACCEL && !(tc.mode & ALLOC_OPAQUE))
    {
        AllocSurfaces();
        if(!m_pFrameAllocator && (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
    }

    if (tc.mode & ALLOC_OPAQUE)
    {
        AllocSurfaces();

        m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
        mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        MFXVideoDECODE_QueryIOSurf(m_session, m_pPar, &m_request);

        if (tc.mode == ALLOC_OPAQUE_SYSTEM || tc.mode == ALLOC_OPAQUE_LESS_SYSTEM || tc.mode == ALLOC_OPAQUE_MORE_SYSTEM)
            m_request.Type = MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_DECODE|MFX_MEMTYPE_OPAQUE_FRAME;

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

    m_par.AddExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING, sizeof(mfxExtDecVideoProcessing));
    mfxExtDecVideoProcessing *pSfcParams = (mfxExtDecVideoProcessing*)m_par.GetExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    pSfcParams->In.CropX = 0;
    pSfcParams->In.CropY = 0;
    pSfcParams->In.CropW = m_pPar->mfx.FrameInfo.Width;
    pSfcParams->In.CropH = m_pPar->mfx.FrameInfo.Height;
    pSfcParams->Out.CropX = 0;
    pSfcParams->Out.CropY = 0;
    pSfcParams->Out.CropW = 352;
    pSfcParams->Out.CropH = 288;
    pSfcParams->Out.Width = 352;
    pSfcParams->Out.Height = 288;
    pSfcParams->Out.ChromaFormat = m_pPar->mfx.FrameInfo.ChromaFormat;
    pSfcParams->Out.FourCC = m_pPar->mfx.FrameInfo.FourCC;

    mfxSession m_session_tmp = m_session;

    if (tc.mode == NULL_PAR)
        m_pPar = 0;
    else if (tc.mode == NULL_SESSION)
        m_session = 0;
    else if (tc.mode == PAR_ACCEL)
    {
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsLog << "Expected status changed to MFX_ERR_NONE \n";
        }
    }

    g_tsStatus.expect(tc.sts);
    /* Only BXT support SFC
     * and only for AVC
     * and only for progressive */
    if ((MFX_HW_BXT != g_tsHWtype) || ( MFX_PICSTRUCT_PROGRESSIVE != m_pPar->mfx.FrameInfo.PicStruct))
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);

    Init(m_session, m_pPar);

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_init_sfc);

}
