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
        int RunTest(unsigned int id);

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



    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

            const tc_struct& tc = test_case[id];
            mfxU32 nFrameMin = 0;
            mfxU32 nFrameSuggested = 0;
            mfxU32 AsyncDepth = 0;
            mfxU32 type = 0;

            MFXInit();
            Load();

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

                //HEVCE_HW need aligned width and height for 32
                m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
                m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));


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
                if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
                {
                    if (m_par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
                        type = MFX_MEMTYPE_D3D_EXT | MFX_MEMTYPE_OPAQUE_FRAME;
                    if (nFrameMin)
                        nFrameMin = nFrameMin - 2;
                    if (nFrameSuggested)
                        nFrameSuggested = nFrameSuggested - 2;
                }

                EXPECT_EQ(type, m_request.Type)
                    << "ERROR: Wrong Type";
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


        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_query_io_surf);
}