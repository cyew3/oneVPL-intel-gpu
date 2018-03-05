//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2018 Intel Corporation. All Rights Reserved.
//

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"

#include <algorithm>

/*
Description:
Test verifies that HEVC FEI encode correctly supports CQP bitrate control.
Frame's QP is compared with mfxEncodeCtrl's per frame QP. For test passed
they must be equal.
*/

namespace hevce_fei_frame_qp
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

    class TestSuite : tsVideoEncoder, tsParserHEVC2, tsSurfaceProcessor, tsBitstreamProcessor
    {
    public:
        static const mfxU32 n_cases;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
            , numFramesSubmitted(0)
        {
            srand(0);
            m_filler = this;
            m_bs_processor = this;

            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;

            m_par.AsyncDepth = 1;
            m_framesToEncode = (g_tsConfig.sim) ? 2 : 5;
            m_qp.reserve(m_framesToEncode);

            mfxExtFeiHevcEncFrameCtrl & ctrl = m_ctrl;
            ctrl.SubPelMode = 3;
            ctrl.SearchWindow = 5;
            ctrl.NumFramePartitions = 4;
            ctrl.MultiPred[0] = ctrl.MultiPred[1] = 1;

            m_Gen.seed(0);
        }
        ~TestSuite() {}

        int RunTest(const mfxU32 id);

    private:
        mfxU32 numFramesSubmitted;
        mfxU32 m_framesToEncode;
        std::mt19937 m_Gen;

        std::vector<mfxU16> m_qp;

        enum
        {
            MFX_PAR = 1,
            RESET,
        };

        static const tc_struct test_case[];

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            m_pCtrl->QP = GetRandomQP();
            m_pCtrl->QP += (m_par.mfx.FrameInfo.BitDepthLuma - 8) * 6;
            m_qp.push_back(m_pCtrl->QP);
            s.Data.TimeStamp = s.Data.FrameOrder = numFramesSubmitted++;
            return MFX_ERR_NONE;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            if (bs.Data)
                set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

            for (mfxU32 checked = 0; checked < nFrames; checked++)
            {
                auto& hdr = ParseOrDie();

                for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
                {
                    if (!IsHEVCSlice(pNALU->nal_unit_type))
                        continue;

                    EXPECT_TRUE(pNALU->slice != nullptr) << "Error: pNALU->slice is NULL\n";
                    auto& S = *pNALU->slice;

                    mfxI16 QP = S.qp_delta + S.pps->init_qp_minus26 + 26;
                    QP += (m_par.mfx.FrameInfo.BitDepthLuma - 8) * 6;
                    EXPECT_TRUE (QP >= 0 && QP <= 51) << "Frame QP is out of range [0,51]\n";
                    EXPECT_EQ(m_qp.at(bs.TimeStamp), QP) << "Error: Frame's QP is not equal to mfxEncodeCtrl's per frame QP\n";
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

        mfxU32 GetRandomQP()
        {
            std::uniform_int_distribution<mfxU32> distr(0, 51);
            return distr(m_Gen);
        }
    };

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_PAR, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  1}},
        {/*02*/ MFX_ERR_NONE, MFX_PAR, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15}},
    };

    const mfxU32 TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

    mfxI32 TestSuite::RunTest(const mfxU32 id)
    {
        TS_START;
        CHECK_HEVC_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];

        MFXInit();
        Load();
        SETPARS(m_pPar, MFX_PAR);

        Init();
        EncodeFrames(m_framesToEncode);

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_frame_qp);
};