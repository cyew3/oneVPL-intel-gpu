/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_big_resolution
{
    #define HEVCE_2K_SIZE (2176)
    #define HEVCE_4K_SIZE (4096)
    #define HEVCE_8K_SIZE (8192)
    #define HEVCE_16K_SIZE (16384)

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;
        static const unsigned int n_frames = 5;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
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
                mfxI32 v;
            } set_par[MAX_NPARS];
    };

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:
        enum
        {
            MFX_PAR,
        };
        enum
        {
            INIT   = 1 << 1,
            ENCODE = 1 << 2,
            QUERY  = 1 << 3
        };
        bool isResolutionSupported;
        void checkSupport(tc_struct ts);
        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_4K_SIZE },
            }
        },
        {/*01*/ MFX_ERR_NONE, QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_8K_SIZE },
            }
        },
        {/*02*/ MFX_ERR_NONE, QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_16K_SIZE },
            }
        },
        {/*03*/ MFX_ERR_NONE, INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_4K_SIZE },
            }
        },
        {/*04*/ MFX_ERR_NONE, INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_8K_SIZE },
            }
        },
        {/*05*/ MFX_ERR_NONE, INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_16K_SIZE },
            }
        },
        {/*06*/ MFX_ERR_NONE, INIT | ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_4K_SIZE },
            }
        },
        {/*07*/ MFX_ERR_NONE, INIT | ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_8K_SIZE },
            }
        },
        {/*08*/ MFX_ERR_NONE, INIT | ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_16K_SIZE },
            }
        },
        {/*09*/ MFX_ERR_NONE, INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_4K_SIZE },
            }
        },
        {/*10*/ MFX_ERR_NONE, INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_8K_SIZE },
            }
        },
        {/*11*/ MFX_ERR_NONE, INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_16K_SIZE },
            }
        },
        {/*12*/ MFX_ERR_NONE, INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_8K_SIZE },
            }
        },
        {/*13*/ MFX_ERR_NONE, QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_4K_SIZE },
            }
        },
        {/*14*/ MFX_ERR_NONE, QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_8K_SIZE },
            }
        },
        {/*15*/ MFX_ERR_NONE, QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_16K_SIZE },
            }
        },
        {/*16*/ MFX_ERR_NONE, QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, HEVCE_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, HEVCE_8K_SIZE },
            }
        },
    };

    void TestSuite::checkSupport(tc_struct) {
        isResolutionSupported = true;
        int width = m_par.mfx.FrameInfo.Width;
        int height = m_par.mfx.FrameInfo.Height;
        int fourcc_id = m_par.mfx.FrameInfo.FourCC;
        if ((0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                throw tsSKIP;
            }
            if (g_tsHWtype < MFX_HW_CNL && g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
            {
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_P010)
                && (g_tsHWtype < MFX_HW_KBL)) {
                g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_Y210 || fourcc_id == MFX_FOURCC_YUY2 ||
                fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410)
                && (g_tsHWtype < MFX_HW_ICL))
            {
                g_tsLog << "\n\nWARNING: RExt formats are only supported starting from ICL!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == GMOCK_FOURCC_P012 || fourcc_id == GMOCK_FOURCC_Y212 || fourcc_id == GMOCK_FOURCC_Y412)
                && (g_tsHWtype < MFX_HW_TGL))
            {
                g_tsLog << "\n\nWARNING: 12b formats are only supported starting from TGL!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_YUY2 || fourcc_id == MFX_FOURCC_Y210 || fourcc_id == GMOCK_FOURCC_Y212)
                && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 4:2:2 formats are NOT supported on VDENC!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412)
                && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are only supported on VDENC!\n\n\n";
                throw tsSKIP;
            }
        }
    }

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
        SETPARS(m_par, MFX_PAR);

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_par.AsyncDepth = 1;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.FrameInfo.FourCC = fourcc_id;

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == GMOCK_FOURCC_P012)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y412)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y212)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        MFXInit();
        Load();

        checkSupport(tc);

        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

        mfxStatus caps_sts = GetCaps(&caps, &capSize);
        if (caps_sts != MFX_ERR_UNSUPPORTED)
            g_tsStatus.check(caps_sts);

        if (caps.MaxPicHeight < m_par.mfx.FrameInfo.Height || caps.MaxPicWidth < m_par.mfx.FrameInfo.Width)
        {
            isResolutionSupported = false;
        }

        if (tc.type & QUERY)
        {
            if (!isResolutionSupported)
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            Query();
        }

        if (tc.type & INIT)
        {
            if (!isResolutionSupported)
                g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
            Init();
            if (!isResolutionSupported)
            {
                throw tsOK;
            }
        }

        if (tc.type & ENCODE && !g_tsConfig.sim) {
            AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * n_frames);
            EncodeFrames(n_frames, true);
            DrainEncodedBitstream();
#ifdef DUMP
            const int encoded_size = m_bitstream.DataLength;
            std::string fname = "encoded_" + std::to_string(m_par.mfx.FrameInfo.Width) + "x" + std::to_string(m_par.mfx.FrameInfo.Height) + ".h265";
            static FILE *fp_hevc = fopen(fname.c_str(), "wb");
            fwrite(m_bitstream.Data, encoded_size, 1, fp_hevc);
            fflush(fp_hevc);
            fclose(fp_hevc);
#endif
        }
        else if (tc.type & ENCODE && g_tsConfig.sim) {
            g_tsLog << "\n\nINFO: Encode stage was skipped, final result taken from previous stage\n\n";
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_420_nv12_big_resolution, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_big_resolution, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_big_resolution, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_big_resolution, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_big_resolution, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_big_resolution, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_big_resolution, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_big_resolution, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_big_resolution, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
}
