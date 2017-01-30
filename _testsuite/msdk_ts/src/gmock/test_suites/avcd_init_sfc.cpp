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
        /**/
        SFC_GET_VIDEO_PARAMS = 100,
        SFC_QUERY_1,
        SFC_QUERY_2,
        SFC_QUERY_3,
        SFC_QUERY_4,
        SFC_RESET_1,
        SFC_RESET_2,
        SFC_RESET_3,
        SFC_RESET_4,
        SFC_RESET_5,
        SFC_RESET_6,
        SFC_RESET_7,
        SFC_RESET_8,
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
    /* ok */
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
    /*Get Video Params */
    {/*05*/ MFX_ERR_NONE, SFC_GET_VIDEO_PARAMS, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_QUERY */
    {/*06*/ MFX_ERR_NONE, SFC_QUERY_1, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_QUERY - Negative test*/
    {/*07*/ MFX_ERR_NONE, SFC_QUERY_2, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_QUERY - Negative test*/
    {/*08*/ MFX_ERR_NONE, SFC_QUERY_3, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_QUERY - Negative test*/
    {/*09*/ MFX_ERR_NONE, SFC_QUERY_4, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },

    /*SFC_RESET - Negative test*/
    {/*10*/ MFX_ERR_NONE, SFC_RESET_1, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_RESET - Negative test*/
    {/*11*/ MFX_ERR_NONE, SFC_RESET_2, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_RESET - OK, (IN) crops changes*/
    {/*12*/ MFX_ERR_NONE, SFC_RESET_3, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_RESET - OK, (OUT) crops changes*/
    {/*13*/ MFX_ERR_NONE, SFC_RESET_4, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_RESET - OK, (IN) crops changes*/
    {/*14*/ MFX_ERR_NONE, SFC_RESET_5, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_RESET - OK, (OUT) crops changes*/
    {/*15*/ MFX_ERR_NONE, SFC_RESET_6, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_RESET - OK, (IN) crops changes*/
    {/*16*/ MFX_ERR_NONE, SFC_RESET_7, "forBehaviorTest/foreman_cif.h264", 1,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
    },
    /*SFC_RESET - OK, (OUT) crops changes*/
    {/*17*/ MFX_ERR_NONE, SFC_RESET_8, "forBehaviorTest/foreman_cif.h264", 1,
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

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1024);
    m_bs_processor = &reader;

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

    DecodeHeader();

    SETPARS(m_pPar, MFX_PAR);

    if ((ALLOC_OPAQUE != tc.mode) || (ALLOC_OPAQUE_SYSTEM != tc.mode) || (ALLOC_OPAQUE_D3D != tc.mode))
    {
        AllocSurfaces();
        if(!m_pFrameAllocator && (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
    }

    if ((ALLOC_OPAQUE == tc.mode) || (ALLOC_OPAQUE_SYSTEM == tc.mode) || (ALLOC_OPAQUE_D3D == tc.mode))
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
    pSfcParams->Out.CropW = pSfcParams->In.CropW/2;
    pSfcParams->Out.CropH = pSfcParams->In.CropH/2;
    pSfcParams->Out.Width = pSfcParams->In.CropW/2;
    pSfcParams->Out.Height = pSfcParams->In.CropH/2;
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
    if ( (MFX_OS_FAMILY_LINUX != g_tsOSFamily) ||
         (MFX_HW_BXT != g_tsHWtype) ||
         ( MFX_PICSTRUCT_PROGRESSIVE != m_pPar->mfx.FrameInfo.PicStruct))
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);

    Init(m_session, m_pPar);

    /* 05 - Get Video Params */
    if ((SFC_GET_VIDEO_PARAMS == tc.mode) && (MFX_ERR_NONE == g_tsStatus.get()))
    {
        mfxU16 oldCropH = pSfcParams->Out.CropH;
        pSfcParams->Out.CropH = pSfcParams->Out.CropH + 32;
        GetVideoParam(m_session, m_pPar);
        if (oldCropH != pSfcParams->Out.CropH)
        {
            g_tsStatus.m_status = MFX_ERR_INVALID_VIDEO_PARAM;
            g_tsStatus.check();
        }
    }
    /* 06 - Query */
    if (SFC_QUERY_1 == tc.mode)
    {
        tsExtBufType<mfxVideoParam> par_query = m_par;
        m_pParOut = &par_query;
        mfxExtDecVideoProcessing *pSfcParams_Query = (mfxExtDecVideoProcessing*)par_query.GetExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
        pSfcParams_Query->In.CropH = 0;
        Query();
        if (pSfcParams_Query->In.CropH != pSfcParams->In.CropH)
        {
            g_tsStatus.m_status = MFX_ERR_INVALID_VIDEO_PARAM;
            g_tsStatus.check();
        }
    }

    /* Query */
    if (SFC_QUERY_2 == tc.mode)
    {
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
        tsExtBufType<mfxVideoParam> par_query = m_par;
        m_pParOut = &par_query;
        //pSfcParams->In.CropH = m_pPar->mfx.FrameInfo.Height + 32;
        pSfcParams->Out.CropH = pSfcParams->Out.Height + 32;
        Query();
    }

    /*  Query */
    if (SFC_QUERY_3 == tc.mode)
    {
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
        tsExtBufType<mfxVideoParam> par_query = m_par;
        m_pParOut = &par_query;
        pSfcParams->Out.CropH = pSfcParams->Out.Height + 32;
        Query();
    }

    /* Query */
    if (SFC_QUERY_4 == tc.mode)
    {
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
        tsExtBufType<mfxVideoParam> par_query = m_par;
        m_pParOut = &par_query;
        pSfcParams->Out.CropW = pSfcParams->Out.Width + 32;
        Query();
    }

    /* for Reset() codec should be initialized first! */
    if (MFX_ERR_NONE == g_tsStatus.get())
    {
        /* Reset */
        if (SFC_RESET_1 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            par_reset.RemoveExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            m_pParOut = &par_reset;
            Reset(m_session, m_pParOut);
        }

        /* Reset */
        if (SFC_RESET_2 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            mfxExtDecVideoProcessing *pSfcParams_Reset = (mfxExtDecVideoProcessing*)par_reset.GetExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            pSfcParams_Reset->Out.Width = 2*pSfcParams->Out.Width;
            pSfcParams_Reset->Out.Height = 2*pSfcParams->Out.Height;
            m_pParOut = &par_reset;
            Reset(m_session, m_pParOut);
        }

        /* Reset */
        if (SFC_RESET_3 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            mfxExtDecVideoProcessing *pSfcParams_Reset = (mfxExtDecVideoProcessing*)par_reset.GetExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            pSfcParams_Reset->In.CropW = pSfcParams->In.CropW/2;
            pSfcParams_Reset->In.CropH = pSfcParams->In.CropH/2;
            m_pParOut = &par_reset;
            Reset(m_session, m_pParOut);
        }
        /* Reset */
        if (SFC_RESET_4 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            mfxExtDecVideoProcessing *pSfcParams_Reset = (mfxExtDecVideoProcessing*)par_reset.GetExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            pSfcParams_Reset->Out.CropW = pSfcParams->Out.CropW/2;
            pSfcParams_Reset->Out.CropH = pSfcParams->Out.CropH/2;
            m_pParOut = &par_reset;
            Reset(m_session, m_pParOut);
        }

        /* Reset */
        if (SFC_RESET_5 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            mfxExtDecVideoProcessing *pSfcParams_Reset = (mfxExtDecVideoProcessing*)par_reset.GetExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            pSfcParams_Reset->In.CropW = m_pPar->mfx.FrameInfo.Width + 32;
            pSfcParams_Reset->In.CropH = m_pPar->mfx.FrameInfo.Height + 32;
            m_pParOut = &par_reset;
            Reset(m_session, m_pParOut);
        }
        /* Reset */
        if (SFC_RESET_6 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            mfxExtDecVideoProcessing *pSfcParams_Reset = (mfxExtDecVideoProcessing*)par_reset.GetExtBuffer(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            pSfcParams_Reset->Out.CropW = pSfcParams->Out.Width  + 32;
            pSfcParams_Reset->Out.CropH = pSfcParams->Out.Height  + 32;
            m_pParOut = &par_reset;
            Reset(m_session, m_pParOut);
        }
        /* Reset */
        if (SFC_RESET_7 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            m_pParOut = &par_reset;
            m_pParOut->mfx.FrameInfo.Width = m_pParOut->mfx.FrameInfo.CropW =
                    m_pParOut->mfx.FrameInfo.Width - 32;
            m_pParOut->mfx.FrameInfo.Height = m_pParOut->mfx.FrameInfo.CropH =
                    m_pParOut->mfx.FrameInfo.Height - 32;
            Reset(m_session, m_pParOut);
        }
        /* Reset */
        if (SFC_RESET_8 == tc.mode)
        {
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            tsExtBufType<mfxVideoParam> par_reset = m_par;
            m_pParOut = &par_reset;
            m_pParOut->mfx.FrameInfo.Width = m_pParOut->mfx.FrameInfo.CropW =
                    m_pParOut->mfx.FrameInfo.Width + 32;
            m_pParOut->mfx.FrameInfo.Height = m_pParOut->mfx.FrameInfo.CropH =
                    m_pParOut->mfx.FrameInfo.Height + 32;
            m_pParOut = &par_reset;
            Reset(m_session, m_pParOut);
        }
    } //if (MFX_ERR_NONE == g_tsStatus.get())

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_init_sfc);

}
