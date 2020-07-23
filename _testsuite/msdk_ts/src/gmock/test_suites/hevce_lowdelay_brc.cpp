/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_lowdelay_brc
{

    class TestSuite : tsVideoEncoder
    {
    private:
        static const mfxU32 max_num_ctrl = 10;
        static const mfxU32 max_num_ctrl_par = 10;
        struct tc_struct
        {
            mfxStatus sts;
            struct tctrl {
                mfxU32 type;
                const tsStruct::Field* field;
                mfxU32 par[max_num_ctrl_par];
            } ctrl[max_num_ctrl];
        };

        static const tc_struct test_case[];
        enum CTRL_TYPE
        {
            STAGE = 0xF0000000
            , INIT = 0x80000000
            , RESET = 0x40000000
            , QUERY = 0x20000000
            , MFXVPAR = 1 << 1
            , BUFPAR_2 = 1 << 2
            , BUFPAR_3 = 1 << 3
        };

    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() { }
        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);
        int RunTest(tc_struct tc, unsigned int fourcc_id);
        static const unsigned int n_cases;

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 0 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 0 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 3000 } } }
        },
        {/*01*/ MFX_ERR_NONE,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 0 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 0 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 3000 } } }
        },
        {/*02*/ MFX_ERR_NONE,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 0 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 0 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 0 } } }
        },
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 0 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 0 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 3 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 3000 } } }
        },
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 0 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 3000 } } }
        },
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 0 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 10000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 3000 } } }
        },
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 0 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 0 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_CBR } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 3000 } } }
        }
        // Driver does not support
        /*{ MFX_ERR_NONE,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.LowDelayBRC,{ MFX_CODINGOPTION_ON } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 0 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 0 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VCM } },
        { BUFPAR_2, &tsStruct::mfxExtCodingOption2.MaxFrameSize,{ 3000 } } }
        }*/
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

        MFXInit();
        Load();
        //m_session = tsSession::m_session;

        mfxExtCodingOption2& ext2 = m_par;
        mfxExtCodingOption3& ext3 = m_par;

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
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        for (mfxU32 i = 0; i < max_num_ctrl; i++)
        {
            if (tc.ctrl[i].type & MFXVPAR)
            {
                if (tc.ctrl[i].field)
                {
                    tsStruct::set(&m_par, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
                }

            }

            if (tc.ctrl[i].type & BUFPAR_2)
            {
                if (tc.ctrl[i].field)
                {
                    tsStruct::set(&ext2, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
                }
            }

            if (tc.ctrl[i].type & BUFPAR_3)
            {
                if (tc.ctrl[i].field)
                {
                    tsStruct::set(&ext3, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
                }
            }
        }

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if (ext3.LowDelayBRC == MFX_CODINGOPTION_ON && g_tsHWtype < MFX_HW_ICL) {
                g_tsLog << "\n\nWARNING: LowDelayBRC is supported starting from Gen11!\n\n\n";
                g_tsStatus.disable();
                throw tsSKIP;
            }

            if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_VCM || m_par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
            {
                if (g_tsOSFamily == MFX_OS_FAMILY_LINUX && g_tsHWtype < MFX_HW_KBL)
                {
                    g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                    g_tsLog << "WARNING: Unsupported HW Platform!\n";
                    return 0;
                }
            }

            if ((g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
                && (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2 ||
                    m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210))
            {
                g_tsLog << "\n\nWARNING: 4:2:2 formats are supported in HEVCe DualPipe only!\n\n\n";
                throw tsSKIP;
            }

            if ((g_tsConfig.lowpower == MFX_CODINGOPTION_OFF)
                && (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV ||
                    m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y410))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are supported in HEVCe VDENC only!\n\n\n";
                throw tsSKIP;
            }
        }

        g_tsStatus.expect(tc.sts);
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_VCM) {
            m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }

        m_par.mfx.TargetKbps = 600;
        m_par.mfx.MaxKbps = ext3.WinBRCMaxAvgKbps;
        mfxVideoParam out = m_par;

        if (tc.ctrl[0].type & QUERY)
        {
            Query(m_session, &m_par, &out);
        }

        if (tc.ctrl[0].type & INIT)
        {
            Init(m_session, &out);
        }

        if (tc.ctrl[0].type & RESET)
        {
            Reset(m_session, &out);
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_420_nv12_lowdelay_brc, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_lowdelay_brc, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_lowdelay_brc, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_lowdelay_brc, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_lowdelay_brc, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_lowdelay_brc, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
}