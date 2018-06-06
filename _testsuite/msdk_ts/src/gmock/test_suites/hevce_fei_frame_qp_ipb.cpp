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

#include <random>

/*
Description:
Test verifies that HEVC FEI encode correctly supports CQP bitrate control.
For each type of frame (I, P, B) is set to the value of the QP, which is
compared with value of the frame QP. For test passed they must be equal.
Testing mfxInfoMFX::QPI, mfxInfoMFX::QPP, mfxInfoMFX::QPB.
*/

namespace hevce_fei_frame_qp_ipb
{
    using namespace BS_HEVC2;

    struct tc_struct
    {
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, tsParserHEVC2, tsBitstreamProcessor
    {
    public:
        static const mfxU32 n_cases;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        {
            m_bs_processor = this;

            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;

            m_par.AsyncDepth = 1;

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
        mfxU32 m_framesToEncode = (g_tsConfig.sim) ? 2 : 5;
        std::mt19937 m_Gen;

        enum
        {
            MFX_PAR = 1,
            EXT_COD2,
            EXT_COD3
        };

        static const tc_struct test_case[];

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
                    EXPECT_TRUE(QP >= 0 && QP <= 51) << "Frame QP is out of range [0,51]\n";

                    if(bs.FrameType & MFX_FRAMETYPE_I) {
                        EXPECT_EQ(m_par.mfx.QPI, QP) << "ERROR: Reported QPI != provided to encoder QPI\n";
                    } else if(bs.FrameType & MFX_FRAMETYPE_P) {
                        EXPECT_EQ(m_par.mfx.QPP, QP) << "ERROR: Reported QPP != provided to encoder QPP\n";
                    } else if(bs.FrameType & MFX_FRAMETYPE_B) {
                        EXPECT_EQ(m_par.mfx.QPB, QP) << "ERROR: Reported QPB != provided to encoder QPB\n";
                    }
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }
    };

    const tc_struct TestSuite::test_case[] =
    {
      {/*00*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  6}}},

      {/*01*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  2},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  10},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  10},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,  MFX_P_REF_SIMPLE}}},

      {/*02*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  3},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,  1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  20},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  21},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,  MFX_P_REF_SIMPLE}}},

      {/*03*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  5},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,  1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  0},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  0},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,  0},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,  MFX_P_REF_SIMPLE}}},

      {/*04*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  4},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  10},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  18},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,  25},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,       MFX_B_REF_PYRAMID},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF}}},

      {/*05*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  5},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,  4},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  25},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  16},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,  7},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,       MFX_B_REF_PYRAMID},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,       MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF}}},

      {/*06*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  10},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,  9},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  0},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  15},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,  30},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,       MFX_B_REF_PYRAMID},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,       MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF}}},

      {/*07*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  3},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  51},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  51},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,  51},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,  MFX_B_REF_OFF},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,  MFX_P_REF_SIMPLE}}},

      {/*08*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  5},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,  2},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  30},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  34},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,  38},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,  MFX_B_REF_OFF},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,  MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB,       MFX_CODINGOPTION_OFF}}},

      {/*09*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  30},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,  8},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,  1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,  51},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,  35},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,  MFX_B_REF_OFF},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,  MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB,       MFX_CODINGOPTION_OFF}}},
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

        mfxExtCodingOption2& co2 = m_par;
        SETPARS(&co2, EXT_COD2);

        mfxExtCodingOption3& co3 = m_par;
        SETPARS(&co3, EXT_COD3);

        mfxU16 QPI_initial = m_pPar->mfx.QPI;
        mfxU16 QPP_initial = m_pPar->mfx.QPP;
        mfxU16 QPB_initial = m_pPar->mfx.QPB;

        Init();

        mfxVideoParam par_after_init = {};
        GetVideoParam(m_session, &par_after_init);

        if (QPI_initial != 0)
            EXPECT_EQ(QPI_initial, par_after_init.mfx.QPI) << "ERROR: QPI value was changed inside the library.\n";
        if (QPP_initial != 0)
            EXPECT_EQ(QPP_initial, par_after_init.mfx.QPP) << "ERROR: QPP value was changed inside the library.\n";
        if (QPB_initial != 0)
            EXPECT_EQ(QPB_initial, par_after_init.mfx.QPB) << "ERROR: QPB value was changed inside the library.\n";

        EncodeFrames(m_framesToEncode);

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_frame_qp_ipb);
};
