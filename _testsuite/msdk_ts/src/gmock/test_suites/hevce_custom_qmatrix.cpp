/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "mfx_ext_buffers.h"

#include <fstream>
#include <vector>
#include <memory>
#include <algorithm>
#include <stdlib.h>

/******************************************************/
/*Test scenarios:
- check ScenarioInfo: MFX_SCENARIO_GAME_STREAMING
- check if Flat QMatrix, Default QMatrix, Custom QMatrix
- check data in SPS
*/
/******************************************************/

//Test for verifying correctness of usage customized quantization matrices
namespace hevce_custom_qmatrix
{
    // Matrices for Game Streaming
    uint8_t gs_4x4[16] = { 16, 34, 34, 53, 53, 53, 74, 74, 74, 74, 85, 85, 85, 98, 98, 112 };
    uint8_t gs_intra_8x8[64] = { 16, 26, 26, 34, 29, 34, 42, 42, 42, 42, 48, 48, 48, 48, 48, 61,
        61, 61, 61, 61, 61, 66, 66, 66, 66, 66, 66, 66, 72, 72, 72, 72,
        72, 72, 72, 72, 77, 77, 77, 77, 77, 77, 77, 82, 82, 82, 82, 82,
        82, 88, 88, 88, 88, 88, 96, 96, 96, 96, 101, 101, 101, 106, 106, 112
    };
    uint8_t gs_inter_8x8[64] = { 16, 23, 23, 26, 23, 26, 30, 30, 30, 30, 33, 33, 33, 33, 33, 37,
        37, 37, 37, 37, 37, 39, 39, 39, 39, 39, 39, 39, 42, 42, 42, 42,
        42, 42, 42, 42, 44, 44, 44, 44, 44, 44, 44, 48, 48, 48, 48, 48,
        48, 49, 49, 49, 49, 49, 53, 53, 53, 53, 56, 56, 56, 58, 58, 62
    };
    // End of matrices for Game Streaming

    const mfxU8 ScanOrder2[4 * 4][2] =
    {
        { 0, 0},{ 0, 1},{ 1, 0},{ 0, 2},
        { 1, 1},{ 2, 0},{ 0, 3},{ 1, 2},
        { 2, 1},{ 3, 0},{ 1, 3},{ 2, 2},
        { 3, 1},{ 2, 3},{ 3, 2},{ 3, 3},
    };

    const mfxU8 ScanOrder3[8 * 8][2] =
    {
        { 0, 0},{ 0, 1},{ 1, 0},{ 0, 2},{ 1, 1},{ 2, 0},{ 0, 3},{ 1, 2},
        { 2, 1},{ 3, 0},{ 0, 4},{ 1, 3},{ 2, 2},{ 3, 1},{ 4, 0},{ 0, 5},
        { 1, 4},{ 2, 3},{ 3, 2},{ 4, 1},{ 5, 0},{ 0, 6},{ 1, 5},{ 2, 4},
        { 3, 3},{ 4, 2},{ 5, 1},{ 6, 0},{ 0, 7},{ 1, 6},{ 2, 5},{ 3, 4},
        { 4, 3},{ 5, 2},{ 6, 1},{ 7, 0},{ 1, 7},{ 2, 6},{ 3, 5},{ 4, 4},
        { 5, 3},{ 6, 2},{ 7, 1},{ 2, 7},{ 3, 6},{ 4, 5},{ 5, 4},{ 6, 3},
        { 7, 2},{ 3, 7},{ 4, 6},{ 5, 5},{ 6, 4},{ 7, 3},{ 4, 7},{ 5, 6},
        { 6, 5},{ 7, 4},{ 5, 7},{ 6, 6},{ 7, 5},{ 6, 7},{ 7, 6},{ 7, 7},
    };

    class TestSuite : public tsVideoEncoder, public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
    static const unsigned int n_cases;

    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVC2()
        , extOpt(NULL)
        , m_tsBsWriter("test.h265")
    {
        m_bs_processor = this;
        tsParserHEVC2::set_trace_level(0);
    }
    ~TestSuite()
    {
    }
    int RunTest(unsigned int id);

    private:

        struct tc_struct
        {
            mfxStatus stsInit;
            mfxU16 ScenarioInfo;
        };

        tc_struct tc;
        static const tc_struct test_case[];
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        void VerifySPSHeader(UnitType * const hdr);
        void VerifyPPSHeader(UnitType * const hdr);
        void VerifyScalingMatrices(BS_HEVC2::QM *matrix);
        void InitTestSuite(void);
        void InitScenarioInfo(void);
        void checkSupport(const tc_struct& ts);
        void CalculateScalingFactor();

        BS_HEVC2::QM qm;

        std::unique_ptr<tsRawReader> reader;
        mfxExtCodingOptionSPSPPS *extOpt;
        tsBitstreamWriter m_tsBsWriter;
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /* Check if ScenarioInfo = 0 then Flat qmatrix is used */
        {
            /* 0 */                         MFX_ERR_NONE,
            /* ScenarioInfo = */            MFX_SCENARIO_UNKNOWN,
        },
        /* Check if ScenarioInfo is MFX_SCENARIO_GAME_STREAMING then custrom qmatrix is used */
        {
            /* 1 */                         MFX_ERR_NONE,
            /* ScenarioInfo = */            MFX_SCENARIO_GAME_STREAMING,
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    void TestSuite::CalculateScalingFactor()
    {
        for (mfxU32 matrixId = 0; matrixId < 6; matrixId++)
        {
            for (mfxU32 i = 0; i < 16; i++)
                qm.ScalingFactor0[matrixId][ScanOrder2[i][0]][ScanOrder2[i][1]]
                    = gs_4x4[i];

            const mfxU8 *scalingList = (matrixId < 3) ? gs_intra_8x8 : gs_inter_8x8;
            for (mfxU32 i = 0; i < 64; i++)
                qm.ScalingFactor1[matrixId][ScanOrder3[i][0]][ScanOrder3[i][1]]
                    = scalingList[i];

            for (mfxU32 i = 0; i < 64; i++)
                for (mfxU32 j = 0; j < 2; j++)
                    for (mfxU32 k = 0; k < 2; k++)
                        qm.ScalingFactor2[matrixId][ScanOrder3[i][0] * 2 + k][ScanOrder3[i][1] * 2 + j]
                            = scalingList[i];
            qm.ScalingFactor2[matrixId][0][0] = scalingList[0];
        }

        for (mfxU32 i = 0; i < 64; i++)
            for (mfxU32 j = 0; j < 4; j++)
                for (mfxU32 k = 0; k < 4; k++)
                {
                    qm.ScalingFactor3[0][ScanOrder3[i][0] * 4 + k][ScanOrder3[i][1] * 4 + j]
                        = gs_intra_8x8[i];
                    qm.ScalingFactor3[3][ScanOrder3[i][0] * 4 + k][ScanOrder3[i][1] * 4 + j]
                        = gs_inter_8x8[i];
                }
        qm.ScalingFactor3[0][0][0] = gs_intra_8x8[0];
        qm.ScalingFactor3[3][0][0] = gs_inter_8x8[0];
    }

    //Scaling matrices verification
    void TestSuite::VerifyScalingMatrices(BS_HEVC2::QM *matrix)
    {
        if (tc.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
        {
            CalculateScalingFactor();

            mfxU8 sizeQm = 0;
            mfxU8 nMode = 6;
            for (mfxU8 matrixId = 0; matrixId < nMode; ++matrixId)
            {
                sizeQm = 4;
                for (mfxU8 i = 0; i < sizeQm; ++i)
                {
                    if (!std::equal(std::begin(qm.ScalingFactor0[matrixId][i]), std::end(qm.ScalingFactor0[matrixId][i]), std::begin(matrix->ScalingFactor0[matrixId][i])))
                    {
                        g_tsLog << "ScalingFactor for sizeId = 4x4 and matrixId = " << matrixId << " is different\n";
                        throw tsFAIL;
                    }
                }
                sizeQm = 8;
                for (mfxU8 i = 0; i < sizeQm; ++i)
                {
                    if (!std::equal(std::begin(qm.ScalingFactor1[matrixId][i]), std::end(qm.ScalingFactor1[matrixId][i]), std::begin(matrix->ScalingFactor1[matrixId][i]))) {
                        g_tsLog << "ScalingFactor for sizeId = 8x8 and matrixId = " << matrixId << " is different\n";
                        throw tsFAIL;
                    }
                }
                sizeQm = 16;
                for (mfxU8 i = 0; i < sizeQm; ++i)
                {
                    if (!std::equal(std::begin(qm.ScalingFactor2[matrixId][i]), std::end(qm.ScalingFactor2[matrixId][i]), std::begin(matrix->ScalingFactor2[matrixId][i])))
                    {
                        g_tsLog << "ScalingFactor for sizeId = 16x16 and matrixId = " << matrixId << " is different\n";
                        throw tsFAIL;
                    }
                }
                if (matrixId == 0 || matrixId == 3)
                {
                    sizeQm = 32;
                    for (mfxU8 i = 0; i < sizeQm; ++i)
                    {
                        if (!std::equal(std::begin(qm.ScalingFactor3[matrixId][i]), std::end(qm.ScalingFactor3[matrixId][i]), std::begin(matrix->ScalingFactor3[matrixId][i]))) {
                            g_tsLog << " ScalingFactor for sizeId = 32x32 and matrixId = " << matrixId << " is different\n";
                            throw tsFAIL;
                        }
                    }
                }
            }
        }
    }

    void TestSuite::VerifySPSHeader(UnitType * const hdr)
    {
        if (tc.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
        {
            if (hdr->sps->scaling_list_enabled_flag != 1)
            {
                g_tsLog << "SPS: Scaling List Enabled Flag has to be 1, but it isn't\n";
                throw tsFAIL;
            }
            if (hdr->sps->scaling_list_data_present_flag != 1)
            {
                g_tsLog << "SPS: Scaling Matrix Present Flag has to be 1, but it isn't\n";
                throw tsFAIL;
            }
        }
        VerifyScalingMatrices(hdr->sps->qm);
        g_tsLog << "SPS is OK\n";
    }

    void TestSuite::VerifyPPSHeader(UnitType * const hdr)
    {
        if (tc.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
        {
            if (hdr->pps->scaling_list_data_present_flag != 0)
            {
                g_tsLog << "PPS: Scaling Matrix Present Flag has to be 0, but it isn't\n";
                throw tsFAIL;
            }
        }
        g_tsLog << "PPS is OK\n";
    }

    //Parsing bitstream and check for correct filling of headers
    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        mfxStatus sts = MFX_ERR_NONE;

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength + 1);

        sts = m_tsBsWriter.ProcessBitstream(bs, nFrames);

        UnitType *hdr = Parse();

        for (auto pNALU = hdr; pNALU; pNALU = pNALU->next)
        {
            if (BS_HEVC2::SPS_NUT == pNALU->nal_unit_type)
            {
                g_tsLog << "SPS found, verifying\n";
                VerifySPSHeader(pNALU);
            }

            if (BS_HEVC2::PPS_NUT == pNALU->nal_unit_type)
            {
                g_tsLog << "PPS found, verifying\n";
                VerifyPPSHeader(pNALU);
            }
        }

        return sts;
    }

    //Initialization of base parameters which doesn't affect workflow
    void TestSuite::InitTestSuite(void)
    {
        //file to process
        const char* file_name = NULL;
        file_name = g_tsStreamPool.Get("YUV/matrix_1920x1088_250.yuv");
        m_par.mfx.CodecId = MFX_CODEC_HEVC;
        m_par.mfx.FrameInfo.Width = 1920;
        m_par.mfx.FrameInfo.Height = 1088;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = 26;
        m_par.mfx.QPP = 26;
        m_par.mfx.QPB = 26;
        m_par.mfx.GopPicSize = 5;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.IdrInterval = 1;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

        mfxU32 maxNALU = 0;
        mfxStatus sts = MFX_ERR_NONE;
        mfxU32 expectedSliceCount = 0;

        MFXInit();
        Load();

        if (g_tsStatus.m_status == MFX_ERR_UNSUPPORTED)
        {
            m_impl = MFX_IMPL_HARDWARE2 | MFX_IMPL_VIA_ANY;
            MFXInit();
        }

        g_tsStreamPool.Reg();
        reader = std::move(std::unique_ptr<tsRawReader>(new tsRawReader(file_name, m_par.mfx.FrameInfo)));
        m_filler = reader.get();
    }

    //Fill parameters for chosen ScenarioInfo
    void TestSuite::InitScenarioInfo(void)
    {
        m_par.AddExtBuffer<mfxExtCodingOption3>(0);
        mfxExtCodingOption3     *extOpt3 = m_par;

        if (!extOpt3)
        {
            g_tsLog << "ERROR >> No CodingOption3 buffer found!\n";
            throw tsFAIL;
        }
        extOpt3->ScenarioInfo = tc.ScenarioInfo;
    }

    void TestSuite::checkSupport(const tc_struct& tc)
    {
        if (tc.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && g_tsHWtype < MFX_HW_ICL)
        {
            g_tsLog << "\n\nWARNING: SKIP test (unsupported on current platform)\n\n";
            throw tsSKIP;
        }
    }

    int TestSuite::RunTest(unsigned int id)
    {
        const mfxU32 frameCount = 11;

        TS_START;
        tc = test_case[id];

        checkSupport(tc);
        InitTestSuite();
        InitScenarioInfo();

        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 4);

        /* Encode process */
        EncodeFrames(frameCount);

        TS_END;

        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_custom_qmatrix);
}