/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: hevce_query_io_surf.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_query_io_surf
{

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
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

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:
        enum
        {
            MFX_PAR,
            NULL_SESSION,
            NULL_PAR,
            NULL_REQUEST,
            NONE
        };

        enum
        {
            MFX_IOPATTERN_IN_MASK = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY,
            MFX_MEMTYPE_D3D_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
            MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME,
            MFX_MEMTYPE_SYS_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME,
            MFX_MEMTYPE_SYS_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_INTERNAL_FRAME,
            MFX_MEMTYPE_SYS = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY,
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/* 0*/ MFX_ERR_NONE, NONE  },
        {/* 1*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 5 },
            }
        },
        {/* 2*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/* 3*/ MFX_ERR_NULL_PTR, NULL_PAR },
        {/* 4*/ MFX_ERR_NULL_PTR, NULL_REQUEST },
        {/* 5*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
            }
        },
        {/* 6*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY },
            }
        },
        {/* 7*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY },
            }
        },
        {/* 8*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY },
            }
        },
        {/* 9*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY },
            }
        },
        {/* 10*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0 },
            }
        },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

            mfxU32 nFrameMin = 0;
            mfxU32 nFrameSuggested = 0;
            mfxU32 AsyncDepth = 0;
            mfxU32 type = 0;
            bool isHW = false;

            MFXInit();
            Load();

            // since memcmp() used for verification, all optional fields must be set
            m_par.mfx.FrameInfo.AspectRatioW = m_par.mfx.FrameInfo.AspectRatioH = 1;
            m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            if (fourcc_id == MFX_FOURCC_NV12)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            }
            else if (fourcc_id == MFX_FOURCC_YUY2)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            }
            else if (fourcc_id == MFX_FOURCC_Y210)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            }
            else
            {
                g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
                return 0;
            }

            SETPARS(m_pPar, MFX_PAR);
            g_tsStatus.expect(tc.sts);

            if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
            {
                if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
                {
                    g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                    g_tsLog << "WARNING: Unsupported HW Platform!\n";
                    Query();
                    return 0;
                }

                if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210 || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2)
                    && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
                {
                    g_tsStatus.expect(MFX_ERR_NONE);
                    g_tsLog << "\n\nWARNING: 422 format only supported on ICL+ and ENC+PAK!\n\n\n";
                    throw tsSKIP;
                }

                //HEVCE_HW need aligned width and height for 32
                m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
                m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));

                isHW = true;
            }

            if ((m_par.AsyncDepth > 1) && (tc.sts == MFX_ERR_NONE))
            {
                AsyncDepth = m_par.AsyncDepth;
                m_par.AsyncDepth = 1;
                QueryIOSurf();
                m_par.AsyncDepth = AsyncDepth;
                nFrameMin = AsyncDepth - 1 + m_request.NumFrameMin + 2;
                nFrameSuggested = AsyncDepth - 1 + m_request.NumFrameSuggested + 2;
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

            //check
            if (tc.sts == MFX_ERR_NONE)
            {
                EXPECT_EQ(0, memcmp(&(m_request.Info), &(m_pPar->mfx.FrameInfo), sizeof(mfxFrameInfo)))
                    << "ERROR: QueryIOSurf didn't fill up frame info in returned mfxFrameAllocRequest structure";

                EXPECT_EQ(0, memcmp(&tmp_par, m_pPar, sizeof(mfxVideoParam)))
                    << "ERROR: Input parameters must not be changed in QueryIOSurf()";

                switch (m_par.IOPattern)
                {
                case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
                    type = MFX_MEMTYPE_SYS_EXT;
                    break;
                case MFX_IOPATTERN_IN_VIDEO_MEMORY:
                    type = MFX_MEMTYPE_D3D_EXT;
                    break;
                case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
                    type = MFX_MEMTYPE_SYS | MFX_MEMTYPE_OPAQUE_FRAME;
                    break;
                default: return MFX_ERR_INVALID_VIDEO_PARAM;
                }
                if (isHW)
                {
                    if (m_par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
                    {
                        type &= ~MFX_MEMTYPE_SYSTEM_MEMORY;
                        type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
                    }
                    if (nFrameMin)
                        nFrameMin -= 1;
                    if (nFrameSuggested)
                        nFrameSuggested -= 1;
                }

                EXPECT_EQ(type, m_request.Type)
                    << "ERROR: Wrong Type";

                // invalid test case for GACC SW and GACC
                if (0 != memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_SW.Data,   sizeof(MFX_PLUGINID_HEVCE_SW.Data)) &&
                    0 != memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data)))
                {
                    if (nFrameMin)
                    {
                        EXPECT_EQ(nFrameMin, m_request.NumFrameMin)
                            << "ERROR: Wrong NumFrameMin";
                    }
                    if (nFrameSuggested)
                    {
                        EXPECT_EQ(nFrameSuggested, m_request.NumFrameSuggested)
                            << "ERROR: Wrong NumFrameSuggested";
                    }
                }
            }


        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_query_io_surf, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_query_io_surf, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_query_io_surf, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
}