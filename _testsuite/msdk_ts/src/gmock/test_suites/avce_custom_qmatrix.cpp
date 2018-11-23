/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "mfx_ext_buffers.h"

#include <fstream>
#include <vector>
#include <stdlib.h>

///<summary>Test for verifying correctness of usage customized quantization matrices</summary>
namespace avce_custom_qmatrix
{
    //Table 7-3
    uint8_t std_intra_4x4[16] = { 6,13,13,20,20,20,28,28,28,28,32,32,32,37,37,42, };
    uint8_t std_inter_4x4[16] = { 10,14,14,20,20,20,24,24,24,24,27,27,27,30,30,34 };
    //Table 7-4
    uint8_t std_intra_8x8[64] = { 6, 10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18, 18, 18, 18, 23,
    23, 23, 23, 23, 23, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27,
    27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31,
    31, 33, 33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42, };
    uint8_t std_inter_8x8[64] = { 9, 13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19, 19, 19, 19, 21,
    21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27,
    27, 28, 28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35, };

    // Matrices for Game Streaming
    uint8_t gs_intra_4x4[16] = { 16, 34, 34, 53, 53, 53, 74, 74, 74, 74, 85, 85, 85, 98, 98, 112 };
    uint8_t gs_inter_4x4[16] = { 16, 22, 22, 32, 32, 32, 38, 38, 38, 38, 43, 43, 43, 48, 48, 54 };
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

    class TestSuite : public tsVideoEncoder, public tsBitstreamProcessor, public tsParserH264NALU
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
            , tsParserH264NALU(BS_H264_INIT_MODE_CABAC | BS_H264_INIT_MODE_CAVLC)
            , spsHeader(NULL,0)
            , ppsHeader(NULL,0)
            , fout()
            , reader(NULL)
            , extOpt(NULL)
        {
            m_bs_processor = this;
            tsParserH264NALU::set_trace_level(0);
            /* Fill default quantization matrices */
            std::copy(std::begin(std_intra_4x4), std::end(std_intra_4x4), std::begin(intra_y));
            std::copy(std::begin(std_intra_4x4), std::end(std_intra_4x4), std::begin(intra_cb));
            std::copy(std::begin(std_intra_4x4), std::end(std_intra_4x4), std::begin(intra_cr));
            std::copy(std::begin(std_inter_4x4), std::end(std_inter_4x4), std::begin(inter_y));
            std::copy(std::begin(std_inter_4x4), std::end(std_inter_4x4), std::begin(inter_cb));
            std::copy(std::begin(std_inter_4x4), std::end(std_inter_4x4), std::begin(inter_cr));
            std::copy(std::begin(std_intra_8x8), std::end(std_intra_8x8), std::begin(intra_8x8));
            std::copy(std::begin(std_inter_8x8), std::end(std_inter_8x8), std::begin(inter_8x8));
        }
        ~TestSuite()
        {
            delete[] spsHeader.first;
            delete[] ppsHeader.first;
            delete reader;
        }
        int RunTest(unsigned int id);

    private:

        ///<summary>Expectations for test case, bit flags</summary>
        enum {
            TC_EXPECT_EMPTY = 0,        ///<summary>Quantization matrices for SPS and PPS have to be empty, flags have to be 0</summary>
            TC_EXPECT_SPS   = (1 << 0), ///<summary>Expect quantization matrix in the SPS</summary>
            TC_EXPECT_PPS   = (1 << 1), ///<summary>Expect quantization matrix in the PPS</summary>
        };

        ///<summary>Information about test case</summary>
        struct tc_struct
        {
            mfxStatus stsInit; ///<summary>Expecting status on Init</summary>
            char* debugOut; ///<summary>Full path for bitstream output, for debug purposes</summary>
            mfxU16 LowPower; ///<summary>Enable or disable LowPower mode</summary>
            mfxU16 ScenarioInfo; ///<summary>Selectec CO3.ScenarioInfo, leave 0 if unused</summary>
            mfxU16 Expectations; ///<summary>Expectations flag: where test case expects change</summary>
            mfxU16 UseQMBuffer; ///<summary>Define usage of undocumented QM buffer, 0 - no, 1 - contains valid buffer, 2 - contains invalid buffer</summary>
            bool  SPSPresentFlag[12]; ///<summary>List of expected scaling matrices for SPS</summary>
            bool  PPSPresentFlag[12]; ///<summary>List of expected scaling matrices for PPS</summary>
        };

        tc_struct tc;
        static const tc_struct test_case[];
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        void VerifySPSHeader(UnitType * const hdr);
        void VerifyPPSHeader(UnitType * const hdr);
        void VerifyScalingMatrices(scaling_matrix *matrix);
        void InitTestSuite(void);
        void InitScenarioInfo(void);
        void InitQMBuffer(uint8_t bufferType);
        void InitExpectations(void);

        std::pair<mfxU8 *, size_t> spsHeader;
        std::pair<mfxU8 *, size_t> ppsHeader;
        uint8_t intra_y[16];
        uint8_t intra_cb[16];
        uint8_t intra_cr[16];
        uint8_t inter_y[16];
        uint8_t inter_cb[16];
        uint8_t inter_cr[16];
        uint8_t intra_8x8[64];
        uint8_t inter_8x8[64];
        std::fstream fout;
        tsRawReader *reader;
        mfxExtCodingOptionSPSPPS *extOpt;
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /* By default change of quantization matrix is disabled, verifying */
        {
            /* 0 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_EMPTY,
            /* UseQMBuffer = */     0,
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using ScenarioInfo */
        {
            /* 1 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    MFX_SCENARIO_GAME_STREAMING,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     0,
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 2 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     0,
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 3 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 4 */                 MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     2, //Invalid buffer
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 5 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    MFX_SCENARIO_GAME_STREAMING,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined PPS buffer */
        {
            /* 6 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_PPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
            /* PPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined PPS buffer */
        {
            /* 7 */                 MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_PPS,
            /* UseQMBuffer = */     2, //Invalid buffer
            /* SPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
            /* PPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
        },
        /* By default change of quantization matrix is disabled, verifying */
        {
            /* 8 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_EMPTY,
            /* UseQMBuffer = */     0,
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using ScenarioInfo */
        {
            /* 9 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    MFX_SCENARIO_GAME_STREAMING,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     0,
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 10 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     0,
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 11 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 12 */                 MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     2, //Invalid buffer
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 13 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    MFX_SCENARIO_GAME_STREAMING,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined PPS buffer */
        {
            /* 14 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_PPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
            /* PPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined PPS buffer */
        {
            /* 15 */                 MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_ON,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_PPS,
            /* UseQMBuffer = */     2, //Invalid buffer
            /* SPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
            /* PPSPresentFlag = */  {true,true,true,true,true,true,true,true,false,false,false,false},
        },
        //////////////////////////////////////////////////////////////////////////////////////////////////////
        //Not full matrices on input
        //////////////////////////////////////////////////////////////////////////////////////////////////////
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 16 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {false,true,true,false,true,true,false,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined PPS buffer */
        {
            /* 17 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_PPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
            /* PPSPresentFlag = */  {false,true,true,false,true,true,false,true,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined SPS buffer */
        {
            /* 18 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_SPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {false,false,true,false,true,false,false,true,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
        },
        /* Try to change quantization matrix using externally defined PPS buffer */
        {
            /* 19 */                 MFX_ERR_NONE,
            /* debugOut = */        NULL,
            /* LowPower = */        MFX_CODINGOPTION_OFF,
            /* ScenarioInfo = */    0,
            /* Expectations = */    TC_EXPECT_PPS,
            /* UseQMBuffer = */     1, //Valid Buffer
            /* SPSPresentFlag = */  {false,false,false,false,false,false,false,false,false,false,false,false},
            /* PPSPresentFlag = */  {false,false,true,false,true,false,false,true,false,false,false,false},
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    ///<summary>Scaling matrices verification</summary>
    void TestSuite::VerifyScalingMatrices(scaling_matrix *matrix)
    {
        /*
        if we expect usage of ScenarioInfo - replace matrices by hardcoded in 
        mdp_msdk-lib\_studio\mfx_lib\encode_hw\h264\src\mfx_h264_encode_hw_utils_new.cpp
        MfxHwH264Encode::FillCustomScalingLists
        */
        if (tc.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && tc.UseQMBuffer == 0)
        {
            std::copy(std::begin(gs_intra_4x4), std::end(gs_intra_4x4), std::begin(intra_y));
            std::copy(std::begin(gs_intra_4x4), std::end(gs_intra_4x4), std::begin(intra_cb));
            std::copy(std::begin(gs_intra_4x4), std::end(gs_intra_4x4), std::begin(intra_cr));
            std::copy(std::begin(gs_inter_4x4), std::end(gs_inter_4x4), std::begin(inter_y));
            std::copy(std::begin(gs_inter_4x4), std::end(gs_inter_4x4), std::begin(inter_cb));
            std::copy(std::begin(gs_inter_4x4), std::end(gs_inter_4x4), std::begin(inter_cr));
            std::copy(std::begin(gs_intra_8x8), std::end(gs_intra_8x8), std::begin(intra_8x8));
            std::copy(std::begin(gs_inter_8x8), std::end(gs_inter_8x8), std::begin(inter_8x8));
        }

        if (matrix->scaling_list_present_flag[0] && memcmp(intra_y, matrix->ScalingList4x4[0], sizeof(intra_y)) != 0)
        {
            g_tsLog << "Intra Y matrix unexpectedly different\n";
            throw tsFAIL;
        }
        if (matrix->scaling_list_present_flag[1] && memcmp(intra_cb, matrix->ScalingList4x4[1], sizeof(intra_cb)) != 0)
        {
            g_tsLog << "Intra CB matrix unexpectedly different\n";
            throw tsFAIL;
        }
        if (matrix->scaling_list_present_flag[2] && memcmp(intra_cr, matrix->ScalingList4x4[2], sizeof(intra_cr)) != 0)
        {
            g_tsLog << "Intra CR matrix unexpectedly different\n";
            throw tsFAIL;
        }
        if (matrix->scaling_list_present_flag[3] && memcmp(inter_y, matrix->ScalingList4x4[3], sizeof(inter_y)) != 0)
        {
            g_tsLog << "Inter Y matrix unexpectedly different\n";
            throw tsFAIL;
        }
        if (matrix->scaling_list_present_flag[4] && memcmp(inter_cb, matrix->ScalingList4x4[4], sizeof(inter_cb)) != 0)
        {
            g_tsLog << "Inter CB matrix unexpectedly different\n";
            throw tsFAIL;
        }
        if (matrix->scaling_list_present_flag[5] && memcmp(inter_cr, matrix->ScalingList4x4[5], sizeof(inter_cr)) != 0)
        {
            g_tsLog << "Inter CR matrix unexpectedly different\n";
            throw tsFAIL;
        }

        if (matrix->scaling_list_present_flag[6] && memcmp(intra_8x8, matrix->ScalingList8x8[0], sizeof(intra_8x8)) != 0)
        {
            g_tsLog << "Intra 8x8 matrix unexpectedly different\n";
            throw tsFAIL;
        }
        if (matrix->scaling_list_present_flag[7] && memcmp(inter_8x8, matrix->ScalingList8x8[1], sizeof(inter_8x8)) != 0)
        {
            g_tsLog << "Inter 8x8 matrix unexpectedly different\n";
            throw tsFAIL;
        }
        //*/
    }

    ///<summary>Verification of SPS header in stream</summary>
    void TestSuite::VerifySPSHeader(UnitType * const hdr)
    {
        if (tc.Expectations & TC_EXPECT_EMPTY)
        {
            if (!!hdr->SPS->seq_scaling_matrix_present_flag) //Has to be empty
            {
                g_tsLog << "SPS: Scaling Matrix Present Flag has to be 0, but it isn't\n";
                throw tsFAIL;
            }
        }
        if (tc.Expectations & TC_EXPECT_SPS)
        {
            uint8_t cmpList[64] = {};
            if (!hdr->SPS->seq_scaling_matrix_present_flag) //Has to be filled
            {
                g_tsLog << "SPS: Scaling Matrix Present Flag has to be 1, but it isn't\n";
                throw tsFAIL;
            }
            for (uint8_t i = 0; i < 12; ++i)
            {
                if (!(hdr->SPS->seq_scaling_matrix->scaling_list_present_flag[i] == 1) == tc.SPSPresentFlag[i])
                {
                    g_tsLog << "Scaling matrix present at " << static_cast<unsigned int>(i) << " flag is " << (hdr->SPS->seq_scaling_matrix->scaling_list_present_flag[i] == 1) << " expected " << tc.SPSPresentFlag[i] << "\n";
                    throw tsFAIL;
                }
            }
            g_tsLog << "SPS Scaling Matrices verification\n";
            VerifyScalingMatrices(hdr->SPS->seq_scaling_matrix);
        }
        g_tsLog << "SPS is OK\n";
    }

    ///<summary>Verification of PPS header in stream</summary>
    void TestSuite::VerifyPPSHeader(UnitType * const hdr)
    {
        if (tc.Expectations & TC_EXPECT_EMPTY)
        {
            if (!!hdr->PPS->pic_scaling_matrix_present_flag) //Has to be empty
            {
                g_tsLog << "PPS: Scaling Matrix Present Flag has to be 0, but it isn't\n";
                throw tsFAIL;
            }
        }
        if (tc.Expectations & TC_EXPECT_PPS)
        {
            if (!hdr->PPS->pic_scaling_matrix_present_flag) //Has to be filled
            {
                g_tsLog << "PPS: Scaling Matrix Present Flag has to be 1, but it isn't\n";
                throw tsFAIL;
            }
            for (uint8_t i = 0; i < 12; ++i)
            {
                if (!(hdr->PPS->pic_scaling_matrix->scaling_list_present_flag[i] == 1) == tc.PPSPresentFlag[i])
                {
                    g_tsLog << "Scaling matrix present at " << static_cast<unsigned int>(i) << " flag is " << (hdr->PPS->pic_scaling_matrix->scaling_list_present_flag[i] == 1) << " expected " << tc.PPSPresentFlag[i] << "\n";
                    throw tsFAIL;
                }
            }
            g_tsLog << "PPS Scaling Matrices verification\n";
            VerifyScalingMatrices(hdr->PPS->pic_scaling_matrix);
        }
        g_tsLog << "PPS is OK\n";
    }

    ///<summary>Parsing bitstream and check for correct filling of headers</summar>
    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength + 1);

        UnitType *hdr = Parse();

        while (hdr && tsParserH264NALU::m_sts == BS_ERR_NONE)
        {
            switch (hdr->nal_unit_type)
            {
            case 0x07: //SPS Found
                g_tsLog << "SPS found, verifying\n";
                VerifySPSHeader(hdr);
                if (!spsHeader.first)
                {
                    spsHeader.second = hdr->NumBytesInNALunit - 4;
                    spsHeader.first = new mfxU8[spsHeader.second];
                    memcpy(spsHeader.first, bs.Data + bs.DataOffset + tsParserH264NALU::get_offset() - spsHeader.second, spsHeader.second);
                }
                break;
            case 0x08: //PPS Found
                g_tsLog << "PPS found, verifying\n";
                VerifyPPSHeader(hdr);
                if (!ppsHeader.first)
                {
                    ppsHeader.second = hdr->NumBytesInNALunit - 4;
                    ppsHeader.first = new mfxU8[ppsHeader.second];
                    memcpy(ppsHeader.first, bs.Data + bs.DataOffset + tsParserH264NALU::get_offset() - ppsHeader.second, ppsHeader.second);
                }
                break;
            default:
                g_tsLog << "Ignored NAL Unit with type: " << static_cast<unsigned int>(hdr->nal_unit_type) << "\n";
                break;
            }
            hdr = Parse();
        }
        //We don't need to empty buffer because we can write it to dump later
        return MFX_ERR_NONE;
    }

    ///<summary>Initialization of base parameters which doesn't affect workflow</summary>
    void TestSuite::InitTestSuite(void)
    {
        //file to process
        const char* file_name = NULL;
        file_name = g_tsStreamPool.Get("YUV/matrix_1920x1088_250.nv12");
        m_par.mfx.CodecId = MFX_CODEC_AVC;
        m_par.mfx.FrameInfo.Width = 1920;
        m_par.mfx.FrameInfo.Height = 1088;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_par.mfx.TargetKbps = 2000;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        m_par.mfx.GopPicSize = 5;
        m_par.mfx.GopRefDist = 2;
        m_par.mfx.IdrInterval = 1;
        m_par.mfx.MaxKbps = m_par.mfx.TargetKbps;

        //frame count to process
        mfxU32 maxNALU = 0;
        mfxStatus sts = MFX_ERR_NONE;
        mfxU32 expectedSliceCount = 0;

        if (tc.debugOut)
            fout.open(tc.debugOut, std::fstream::binary | std::ios_base::out);

        g_tsStatus.disable_next_check();
        MFXInit();

        if (g_tsStatus.m_status == MFX_ERR_UNSUPPORTED)
        {
            m_impl = MFX_IMPL_HARDWARE2 | MFX_IMPL_VIA_ANY;
            MFXInit();
        }

        g_tsStreamPool.Reg();
        reader = new tsRawReader(file_name, m_par.mfx.FrameInfo);
        m_filler = reader;
    }

    ///<summary>Fill parameters for chosen ScenarioInfo</summary>
    void TestSuite::InitScenarioInfo(void)
    {
        m_par.AddExtBuffer<mfxExtCodingOption3>(0);
        mfxExtCodingOption3     *extOpt3 = m_par; /* GetExtBuffer */
        extOpt3->ScenarioInfo = tc.ScenarioInfo;

        if (!extOpt3)
        {
            g_tsLog << "ERROR >> No CodingOption3 buffer found!\n";
            throw tsFAIL;
        }
    }

    ///<summary>Initialization of mfxExtAVCScalingMatrix buffer</summary>
    ///<param name="bufferType">Chosen buffer type:
    ///1 - with default matrices (depends on tc.*PresentFlag)
    ///2 - with default matrices and broken ScalingListPresent value</param>
    void TestSuite::InitQMBuffer(uint8_t bufferType)
    {
        if (!bufferType || bufferType > 2)
        {
            g_tsLog << "Undefined bufferType value in behavior test\n";
            throw tsFAIL;
        }
        m_par.AddExtBuffer<mfxExtAVCScalingMatrix>(0);
        mfxExtAVCScalingMatrix *extQM = m_par; /* GetExtBuffer */
        extQM->Type = (tc.Expectations & TC_EXPECT_SPS) ? MFX_SCALING_MATRIX_SPS : MFX_SCALING_MATRIX_PPS;

        switch (bufferType)
        {
        case 1: //valid buffer
            for (uint8_t i = 0; i < 8; ++i)
                extQM->ScalingListPresent[i] = ((tc.Expectations & TC_EXPECT_SPS) ? tc.SPSPresentFlag[i] : tc.PPSPresentFlag[i]) == true ? 1 : 0;
            memcpy(extQM->ScalingList4x4[0], intra_y, sizeof(intra_y));
            memcpy(extQM->ScalingList4x4[1], intra_cb, sizeof(intra_cb));
            memcpy(extQM->ScalingList4x4[2], intra_cr, sizeof(intra_cr));
            memcpy(extQM->ScalingList4x4[3], inter_y, sizeof(inter_y));
            memcpy(extQM->ScalingList4x4[4], inter_cb, sizeof(inter_cb));
            memcpy(extQM->ScalingList4x4[5], inter_cr, sizeof(inter_cr));
            memcpy(extQM->ScalingList8x8[0], intra_8x8, sizeof(intra_8x8));
            memcpy(extQM->ScalingList8x8[1], inter_8x8, sizeof(inter_8x8));
            break;
        case 2: //invalid buffer, corrupts previously filled valid buffer
            for (uint8_t i = 0; i < 12; ++i)
                extQM->ScalingListPresent[i] = ((tc.Expectations & TC_EXPECT_SPS) ? tc.SPSPresentFlag[i] : tc.PPSPresentFlag[i]) == true ? 2 : 0;
            memcpy(extQM->ScalingList4x4[0], intra_y, sizeof(intra_y));
            memcpy(extQM->ScalingList4x4[1], intra_cb, sizeof(intra_cb));
            memcpy(extQM->ScalingList4x4[2], intra_cr, sizeof(intra_cr));
            memcpy(extQM->ScalingList4x4[3], inter_y, sizeof(inter_y));
            memcpy(extQM->ScalingList4x4[4], inter_cb, sizeof(inter_cb));
            memcpy(extQM->ScalingList4x4[5], inter_cr, sizeof(inter_cr));
            memcpy(extQM->ScalingList8x8[0], intra_8x8, sizeof(intra_8x8));
            memcpy(extQM->ScalingList8x8[1], inter_8x8, sizeof(inter_8x8));
            break;
        }
    }

    void TestSuite::InitExpectations(void)
    {
        InitQMBuffer(1);
        /* For extracting sps and pps */
        Init();
        m_par.AddExtBuffer<mfxExtCodingOptionSPSPPS>(0);
        extOpt = m_par;
        if (!extOpt)
        {
            g_tsLog << "ERROR >> No mfxExtCodingOptionSPSPPS buffer found!\n";
            throw tsFAIL;
        }
        extOpt->SPSBuffer = new mfxU8[1024];
        extOpt->SPSBufSize = 1024;
        extOpt->PPSBuffer = new mfxU8[1024];
        extOpt->PPSBufSize = 1024;
        GetVideoParam();
        Close();
        m_par.RemoveExtBuffer<mfxExtAVCScalingMatrix>();
    }

    int TestSuite::RunTest(unsigned int id)
    {
        const mfxU32 frameCount = 11;

        TS_START;
        tc = test_case[id];

        InitTestSuite();

        /* Trying to use Game Streaming option */
        if (tc.ScenarioInfo != 0)
        {
            InitScenarioInfo();
        }
        /* Trying to use mfxExtAVCScalingMatrix option */
        if (tc.UseQMBuffer)
        {
            InitQMBuffer(tc.UseQMBuffer);
        }
        /* Trying to use externally provided SPS or PPS */
        if (tc.Expectations && tc.UseQMBuffer==0 && tc.ScenarioInfo==0)
        {
            InitExpectations();
        }

        if (tc.stsInit != MFX_ERR_NONE)
        {
            mfxExtAVCScalingMatrix tmp = {};
            tmp = *(mfxExtAVCScalingMatrix*)m_par;
            g_tsStatus.expect(tc.stsInit == MFX_ERR_INVALID_VIDEO_PARAM ? MFX_ERR_UNSUPPORTED:tc.stsInit);
            Query();
            *(mfxExtAVCScalingMatrix*)m_par = tmp;
            g_tsStatus.expect(tc.stsInit);
            Init();
        }
        else
        {
            g_tsStatus.expect(tc.stsInit);
            Init();
        }
        //GetVideoParam();

        if (g_tsStatus.get() < 0)
            return 0;

        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 4);

        /* Encode process */
        for (mfxU32 i = 0, c = 0; i < frameCount; ++c) {
            m_pSurf = GetSurface();

            mfxStatus sts = EncodeFrameAsync();

            if (sts == MFX_ERR_NONE) {
                SyncOperation();
                if (tc.debugOut)
                    fout.write((char*)m_bitstream.Data, m_bitstream.DataLength);
                m_bitstream.DataLength = 0;
                ++i;
            }
            else if (sts == MFX_ERR_MORE_DATA) {
                if (reader->m_eos)
                    break;
            }
            else {
                break;
            }
        }

        if (tc.debugOut)
            fout.close();

        TS_END;

        if (extOpt)
        {
            delete[] extOpt->SPSBuffer;
            delete[] extOpt->PPSBuffer;
        }

        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_custom_qmatrix);
}
