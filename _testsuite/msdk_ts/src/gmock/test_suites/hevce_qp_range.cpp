//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2018 Intel Corporation. All Rights Reserved.
//

//
// Description:
// The test verifies that the HEVC encoder correctly sets the QP when
// the MinQPI/MaxQPI/MinQPP/MaxQPP/MinQPB/MaxQPB values are set.
//

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include <random>

namespace hevce_qp_range
{

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

    class TestSuite : tsVideoEncoder, tsParserHEVC2, tsSurfaceProcessor, tsBitstreamProcessor
    {
    public:
        static const mfxU32 n_cases;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
        {
            m_bs_processor = this;

            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;

            m_par.AsyncDepth = 1;

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

                    mfxU16 QP = S.qp_delta + S.pps->init_qp_minus26 + 26;
                    QP += (m_par.mfx.FrameInfo.BitDepthLuma - 8) * 6;
                    check_qp(QP, bs.FrameType);
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

        void check_qp(mfxU32 QP, mfxU32 frame_type)
        {
            mfxExtCodingOption2& co2 = m_par;

            switch (frame_type & MFX_FRAMETYPE_I)
            {
                case MFX_FRAMETYPE_I:
                {
                    if (co2.MinQPI)
                        EXPECT_TRUE(QP >= co2.MinQPI) << "Frame QP is less than MinQPI: "    << QP << " < " << co2.MinQPI << "\n";
                    if (co2.MaxQPI)
                        EXPECT_TRUE(QP <= co2.MaxQPI) << "Frame QP is greater than MaxQPI: " << QP << " > " << co2.MaxQPI << "\n";
                    break;
                }
                case MFX_FRAMETYPE_P:
                {
                    if (co2.MinQPP)
                        EXPECT_TRUE(QP >= co2.MinQPP) << "Frame QP is less than MinQPP: "    << QP << " < " << co2.MinQPP << "\n";
                    if (co2.MaxQPP)
                        EXPECT_TRUE(QP <= co2.MaxQPP) << "Frame QP is greater than MaxQPP: " << QP << " > " << co2.MaxQPP << "\n";
                    break;
                }
                case MFX_FRAMETYPE_B:
                {
                    if (co2.MinQPB)
                        EXPECT_TRUE(QP >= co2.MinQPB) << "Frame QP is less than MinQPB: "    << QP << " < " << co2.MinQPB << "\n";
                    if (co2.MaxQPB)
                        EXPECT_TRUE(QP <= co2.MaxQPB) << "Frame QP is greater than MaxQPB: " << QP << " > " << co2.MaxQPB << "\n";
                    break;
                }
            }
        }
    };

    const tc_struct TestSuite::test_case[] =

    {
      {/*00*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          6}}},

      {/*01*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,        1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          10}}},

      {/*02*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        2},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          40},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,          30},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE}}},

      {/*03*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        3},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,          10},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,          15},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE}}},

      {/*04*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        5},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        1},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          48},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,          48},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPB,          48},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE}}},

      {/*05*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPB,          50},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,        MFX_B_REF_PYRAMID},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableQPOffset,  MFX_CODINGOPTION_OFF}}},

      {/*06*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        5},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        4},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          2},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          10},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,        MFX_B_REF_PYRAMID},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableQPOffset,  MFX_CODINGOPTION_OFF}}},

      {/*07*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        10},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        9},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          2},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          10},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,          10},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,          15},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPB,          15},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPB,          20},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,        MFX_B_REF_PYRAMID},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableQPOffset,  MFX_CODINGOPTION_OFF}}},

      {/*08*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        3},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPB,          40},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPB,          45},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,        MFX_B_REF_OFF},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE}}},

      {/*09*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        5},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        2},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,          20},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,        MFX_B_REF_OFF},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB,             MFX_CODINGOPTION_OFF}}},

      {/*10*/ MFX_PAR,
        {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,        10},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        5},
        {MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          10},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.BRefType,        MFX_B_REF_OFF},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.PRefType,        MFX_P_REF_SIMPLE},
        {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB,             MFX_CODINGOPTION_OFF}}},
    };

    const mfxU32 TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

    mfxI32 TestSuite::RunTest(const mfxU32 id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        MFXInit();
        Load();
        SETPARS(m_pPar, MFX_PAR);

        mfxExtCodingOption2& co2 = m_par;
        SETPARS(&co2, EXT_COD2);

        mfxExtCodingOption3& co3 = m_par;
        SETPARS(&co3, EXT_COD3);

        Init();
        EncodeFrames(m_framesToEncode);
        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_qp_range);
};
