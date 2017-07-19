/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp9e_query_io_surf
{
    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9)
        {

        }
        ~TestSuite() {}

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);
        static const unsigned int n_cases;

        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_p010;
        static const unsigned int n_cases_ayuv;
        static const unsigned int n_cases_y410;

    private:
        enum
        {
            MFX_PAR,
            NULL_SESSION,
            NULL_PAR,
            NULL_REQUEST,
            ASYNC5,
            ASYNC10,
            NONE
        };

        enum
        {
            MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME,
            MFX_MEMTYPE_SYS_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME,
            MFX_MEMTYPE_SYS = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY,
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY },
            }
        },
        {/*01*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
            }
        },
        {/*02*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY },
            }
        },
        {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY },
            }
        },
        {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY },
            }
        },
        {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY },
            }
        },
        {/*06*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0 },
            }
        },
        {/*07*/ MFX_ERR_NONE, ASYNC5,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY },
            }
        },
        {/*08*/ MFX_ERR_NONE, ASYNC10,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY },
            }
        },
        {/*09*/ MFX_ERR_NONE, ASYNC5,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
            }
        },
        {/*10*/ MFX_ERR_NONE, ASYNC10,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
            }
        },
        {/*11*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*12*/ MFX_ERR_NULL_PTR, NULL_PAR },
        {/*13*/ MFX_ERR_NULL_PTR, NULL_REQUEST },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const unsigned int TestSuite::n_cases_nv12 = n_cases;
    const unsigned int TestSuite::n_cases_p010 = n_cases;
    const unsigned int TestSuite::n_cases_ayuv = n_cases;
    const unsigned int TestSuite::n_cases_y410 = n_cases;

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxU32 type = 0;

        MFXInit();

        Load();

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        SETPARS(m_pPar, MFX_PAR);
        g_tsStatus.expect(tc.sts);

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        if (tc.type == ASYNC5 || tc.type == ASYNC10)
        {
            m_pPar->AsyncDepth = 1;
        }

        mfxVideoParam tmp_par = m_par;

        //call tested funcion
        if (tc.type == NULL_SESSION)
        {
            QueryIOSurf(NULL, m_pPar, m_pRequest);
        }
        else if (tc.type == NULL_PAR)
        {
            QueryIOSurf(m_session, NULL, m_pRequest);
        }
        else if (tc.type == NULL_REQUEST)
        {
            QueryIOSurf(m_session, m_pPar, NULL);
        }
        else
        {
            QueryIOSurf();
        }

        //check call result
        if (tc.sts == MFX_ERR_NONE)
        {
            mfxU16 nFrameMin;
            mfxU16 nFrameSuggested;
            nFrameMin = nFrameSuggested = m_pPar->AsyncDepth;

            EXPECT_EQ(0, memcmp(&(m_request.Info), &(m_pPar->mfx.FrameInfo), sizeof(mfxFrameInfo)))
                << "ERROR: QueryIOSurf didn't fill up frame info in returned mfxFrameAllocRequest structure";

            if (memcmp(&tmp_par, m_pPar, sizeof(mfxVideoParam)) != 0)
            {
                TS_TRACE(m_pPar);
                ADD_FAILURE() << "ERROR: Input parameters were changed by QueryIOSurf(), expected not to be"; throw tsFAIL;
            }

            switch (m_par.IOPattern)
            {
            case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
                type = MFX_MEMTYPE_SYS_EXT;
                break;
            case MFX_IOPATTERN_IN_VIDEO_MEMORY:
                type = MFX_MEMTYPE_D3D_EXT;
                break;
            case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
                type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_OPAQUE_FRAME;
                break;
            default: return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            EXPECT_EQ(type, m_request.Type)
                << "ERROR: Wrong Type in returned structure (" << m_request.Type << ")";

            if (nFrameMin)
            {
                EXPECT_EQ(nFrameMin, m_request.NumFrameMin)
                    << "ERROR: NumFrameMin expected to be " << nFrameMin << ", but it is " << m_request.NumFrameMin;
            }
            if (nFrameSuggested)
            {
                EXPECT_EQ(nFrameSuggested, m_request.NumFrameSuggested)
                    << "ERROR: NumFrameSuggested expected to be " << nFrameSuggested
                    << ", but it is " << m_request.NumFrameSuggested;
            }

            if (tc.type == ASYNC5 || tc.type == ASYNC10)
            {
                mfxFrameAllocRequest  request2 = {};
                m_pPar->AsyncDepth = (tc.type == ASYNC5 ? 5 : 10);

                QueryIOSurf(m_session, m_pPar, &request2);

                mfxU16 expected_numframemin = m_pRequest->NumFrameMin + m_pPar->AsyncDepth - 1;
                EXPECT_EQ(expected_numframemin, request2.NumFrameMin)
                    << "ERROR: For AsyncDepth=" << m_pPar->AsyncDepth << " NumFrameMin expected to be "
                    << expected_numframemin << ", but it is " << request2.NumFrameMin;
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_query_io_surf,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_query_io_surf, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_query_io_surf,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases_ayuv);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_query_io_surf, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
}
