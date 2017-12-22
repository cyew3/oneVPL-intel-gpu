//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.
//

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace hevce_frame_qp
{

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, tsParserHEVCAU, tsSurfaceProcessor, tsBitstreamProcessor
    {
    public:
        static const unsigned int n_cases;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , frames(0)
            , encoded(0)
            , framesToEncode(1)
        {
            srand(0);
            m_filler = this;
            m_bs_processor = this;
        }
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);
        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:

        mfxU32 frames;
        mfxU32 encoded;
        mfxU32 framesToEncode;

        std::vector<mfxU16> qp;

        enum
        {
            MFX_PAR = 1,
            RESET,
        };

        static const tc_struct test_case[];

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            //if (frames < framesToEncode)
            {
                m_pCtrl->QP = 1 + rand() % 50;
                qp.push_back(m_pCtrl->QP);

                s.Data.TimeStamp = s.Data.FrameOrder = frames++;
            }
            return MFX_ERR_NONE;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            mfxU32 checked = 0;

            if (bs.Data)
                set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

            while (checked++ < nFrames)
            {
                auto& AU = ParseOrDie();
                auto& S = AU.pic->slice[0]->slice[0];

                if (bs.TimeStamp < framesToEncode)
                {
                    mfxI16 QP  = S.slice_qp_delta + S.pps->init_qp_minus26 + 26;
                    EXPECT_EQ(qp[bs.TimeStamp], QP) << "ERROR: Frame's QP is not equal to mfxEncodeCtrl's per frame QP\n";
                    encoded++;
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

    };


    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_PAR, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  1}},
        {/*02*/ MFX_ERR_NONE, MFX_PAR, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15}},
        {/*03*/ MFX_ERR_NONE, RESET,   {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

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

        SETPARS(m_pPar, MFX_PAR);

        if (fourcc_id == MFX_FOURCC_P010)
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

        m_par.AsyncDepth = 1;
        framesToEncode = 5;

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
            else if (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 && g_tsHWtype < MFX_HW_KBL) {
                g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
                throw tsSKIP;
            }
            else if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210 || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2)
                && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 422 formats only supported on ICL+ and ENC+PAK!\n\n\n";
                throw tsSKIP;
            }
            else if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y410)
                && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 444 formats only supported on ICL+ and VDENC!\n\n\n";
                throw tsSKIP;
            }

            //HEVCE_HW need aligned width and height for 32
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
        }

        Init();

        EncodeFrames(framesToEncode);

        if (tc.mode == RESET)
        {
            Reset();
            qp.clear();
            encoded = 0;
            frames = 0;

            EncodeFrames(framesToEncode);
        }

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_frame_qp, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_hevce_frame_qp, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_hevce_frame_qp, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_hevce_frame_qp, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_hevce_frame_qp, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_hevce_frame_qp, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
};
