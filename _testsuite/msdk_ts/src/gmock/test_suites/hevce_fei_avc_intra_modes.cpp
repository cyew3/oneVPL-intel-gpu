/******************************************************************************* *\

Copyright (C) 2018 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"
#include "ts_parser.h"

/*!

Description:
Test verifies that HEVC FEI encode per-frame control flag SkipHEVCIntraKernel affects
list of the available Intra Modes. If SkipHEVCIntraKernel is ON, bit-stream shouldn't
have HEVC specific intra modes

*/

using namespace BS_HEVC2;

namespace hevce_fei_avc_intra_modes
{

    class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
            , tsParserHEVC2(PARSE_SSD)
            , m_mode(0)
            , m_ctu_size(32)
        {
            m_filler = this;
            m_bs_processor = this;

            m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 352;
            m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 288;
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_pPar->AsyncDepth = 1;                              //limitation for FEI
            m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP; //For now FEI work with CQP only

            m_width_in_ctu = (m_pPar->mfx.FrameInfo.Width + m_ctu_size - 1) / m_ctu_size;
            m_height_in_ctu = (m_pPar->mfx.FrameInfo.Height + m_ctu_size - 1) / m_ctu_size;

            m_reader.reset(new tsRawReader(g_tsStreamPool.Get("/forBehaviorTest/foreman_cif.nv12"),
                m_pPar->mfx.FrameInfo));

            g_tsStreamPool.Reg();

            m_Gen.seed(0);

        }

        ~TestSuite() {}

        typedef enum
        {
            INTRA_AVC_PRED_FRAMES = 0,
            INTRA_HEVC_PRED_FRAMES = 1,
            INTRA_AVC_HEVC_PRED_FRAMES = 2
        } TestMode;

        struct CUIntraTypeFEISpec
        {
            // Luma
            mfxU32 IntraMode0 = 0;
            mfxU32 IntraMode1 = 0;
            mfxU32 IntraMode2 = 0;
            mfxU32 IntraMode3 = 0;

            // Chroma
            mfxU32 IntraChromaMode = 0;
        };

        void SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl)
        {
            memset(&ctrl, 0, sizeof(ctrl));

            ctrl.Header.BufferId = MFX_EXTBUFF_HEVCFEI_ENC_CTRL;
            ctrl.Header.BufferSz = sizeof(mfxExtFeiHevcEncFrameCtrl);
            ctrl.SubPelMode = 3; // quarter-pixel motion estimation
            ctrl.SearchWindow = 5; // 48 SUs 48x40 window full search
            ctrl.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
                                         // enable internal L0/L1 predictors: 1 - spatial predictors
            ctrl.MultiPred[0] = ctrl.MultiPred[1] = 1;
        }

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        void SetIntraModeInCtrl(mfxU32 seed, mfxExtFeiHevcEncFrameCtrl& ctrl)
        {
            switch (m_mode)
            {
            case INTRA_AVC_PRED_FRAMES:
                ctrl.reserved0[2] = 1; // SkipHEVCIntraKernel
                break;

            case INTRA_HEVC_PRED_FRAMES:
                // Frame Enc Control won't be changed
                break;

            case INTRA_AVC_HEVC_PRED_FRAMES:
            {
                m_Gen.seed(seed);
                if (GetRandomBit())
                    ctrl.reserved0[2] = 1; // SkipHEVCIntraKernel
            }
            break;
            }
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxStatus sts = m_reader->ProcessSurface(s);

            s.Data.TimeStamp = s.Data.FrameOrder;
            tsExtBufType<mfxEncodeCtrl>& ctrl = m_map_ctrl[s.Data.FrameOrder];

            mfxExtFeiHevcEncFrameCtrl& feiCtrl = ctrl;
            SetDefaultsToCtrl(feiCtrl);

            SetIntraModeInCtrl(s.Data.FrameOrder, feiCtrl);

            ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl));

            return sts;
        }

        void ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src)
        {
            dst = src;
        }

        bool GetRandomBit()
        {
            std::bernoulli_distribution distr(0.33);
            return distr(m_Gen);
        }

        Bs32u GetIntraChromaModeFEI(Bs8u intraPredModeC_0_0)
        {
            switch (intraPredModeC_0_0)
            {
            case 0://Planar
                return 2;
            case 1://DC
                return 5;
            case 10://Horiz
                return 4;
            case 26://Vertic
                return 3;
            default://DM
                return 0;
            }
        }

        void SetIntraPredModeFEI(const CU* pCU, CUIntraTypeFEISpec& cuIntraTypeFEISpec)
        {
            if (pCU == nullptr)
            {
                g_tsLog << "ERROR: SetIntraPredModeFEI: pCU is equal to nullptr\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }

            // Luma
            if (pCU->IntraPredModeY[0][0] > 34
                || pCU->IntraPredModeY[1][0] > 34
                || pCU->IntraPredModeY[0][1] > 34
                || pCU->IntraPredModeY[1][1] > 34)
            {
                g_tsLog << "ERROR: SetIntraPredModeFEI: incorrect IntraPredModeY[X][X]\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }

            cuIntraTypeFEISpec.IntraMode0 = pCU->IntraPredModeY[0][0];
            cuIntraTypeFEISpec.IntraMode1 = pCU->IntraPredModeY[1][0];
            cuIntraTypeFEISpec.IntraMode2 = pCU->IntraPredModeY[0][1];
            cuIntraTypeFEISpec.IntraMode3 = pCU->IntraPredModeY[1][1];

            // Chroma
            // For ChromaArrayType != 3 (4:4:4) all elements of the array intra_chroma_pred_mode[2][2] are equal
            // Table 7.3.8.5 from ITU-T H.265 (V4)
            // HEVC FEI ENCODE on SKL supports 4:2:0 mode only
            if (pCU->IntraPredModeC[0][0] > 34)
            {
                g_tsLog << "ERROR: SetIntraPredModeFEI: incorrect IntraPredModeC[0][0]\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }

            cuIntraTypeFEISpec.IntraChromaMode = GetIntraChromaModeFEI(pCU->IntraPredModeC[0][0]);
        }

        bool IsAVCSpecLumaIntraMode(mfxU32 intraMode)
        {
            return (intraMode == 0 /*Planar*/ || intraMode == 1 /*DC*/
                || intraMode == 26 /*Ver*/ || intraMode == 10 /*Hor*/
                || intraMode == 34 /*DDL*/ || intraMode == 18 /*DDR*/
                || intraMode == 22 /*VR*/ || intraMode == 14 /*HD*/
                || intraMode == 30 /*VL*/ || intraMode == 6 /*HU*/);
        }

        bool HasCUHevcIntraMode(const CUIntraTypeFEISpec& cuIntraTypeFEISpec)
        {
            bool lumaAVCIntraMode = true;
            // Luma
            if (!IsAVCSpecLumaIntraMode(cuIntraTypeFEISpec.IntraMode0)
                || !IsAVCSpecLumaIntraMode(cuIntraTypeFEISpec.IntraMode1)
                || !IsAVCSpecLumaIntraMode(cuIntraTypeFEISpec.IntraMode2)
                || !IsAVCSpecLumaIntraMode(cuIntraTypeFEISpec.IntraMode3))
                lumaAVCIntraMode = false;

            // Chroma
            bool chromaAVCIntraMode = cuIntraTypeFEISpec.IntraChromaMode != 0; // Derived mode

            return !(lumaAVCIntraMode && chromaAVCIntraMode);

        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

            //WriteBitstream("debug.hevc", bs, true); // DEBUG

            while (nFrames--)
            {
                mfxExtFeiHevcEncFrameCtrl& feiCtrl = m_map_ctrl[bs.TimeStamp];
                bool isAVCIntraModes = (feiCtrl.reserved0[2] == 1);

                auto& hdr = ParseOrDie();

                for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
                {
                    if (pNALU == nullptr)
                    {
                        g_tsLog << "ERROR: ProcessBitstream: pNALU is nullptr\n";
                        g_tsStatus.check(MFX_ERR_ABORTED);
                    }

                    if (!IsHEVCSlice(pNALU->nal_unit_type))
                        continue;

                    auto& S = *pNALU->slice;

                    CUIntraTypeFEISpec cuIntraTypeFEISpec;

                    Bs32u countCTU = 0;
                    for (auto pCTU = S.ctu; pCTU; pCTU = pCTU->Next, ++countCTU)
                    {
                        Bs16u countCU = 0;
                        for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next, ++countCU)
                        {
                            // Set information about Intra prediction mode
                            if (pCU->PredMode == MODE_INTRA)
                            {
                                SetIntraPredModeFEI(pCU, cuIntraTypeFEISpec);
                                if (isAVCIntraModes)
                                {
                                    EXPECT_FALSE(HasCUHevcIntraMode(cuIntraTypeFEISpec))
                                        << "Found HEVC specific intra modes in case with AVC intra modes only\n"
                                        << "for frame #" << bs.TimeStamp
                                        << " , CTU AdrX #" << countCTU % m_width_in_ctu
                                        << " , CTU AdrY #" << countCTU / m_width_in_ctu
                                        << " , CU #" << countCU << "\n";
                                }
                                else
                                {
                                    // Check that frame includes HEVC specific intra modes in case with enabled intra HEVC specific modes
                                    if (HasCUHevcIntraMode(cuIntraTypeFEISpec))
                                    {
                                        m_bHEVCIntraModes = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

        struct tc_struct
        {
            tc_struct(TestMode _mode, mfxU32 _ctu_size) :
                m_mode(_mode),
                m_ctu_size(_ctu_size) {}
            TestMode  m_mode;
            mfxU32 m_ctu_size;
        };

        static const tc_struct test_case[];

        std::unique_ptr <tsRawReader> m_reader;

        mfxU32           m_mode = 0;
        mfxU32           m_ctu_size = 0;
        mfxU32           m_width_in_ctu = 0;
        mfxU32           m_height_in_ctu = 0;

        std::mt19937     m_Gen;

        std::map<mfxU32, tsExtBufType<mfxEncodeCtrl>> m_map_ctrl;
        bool m_bHEVCIntraModes = false;
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ INTRA_AVC_PRED_FRAMES, 32},
        {/*01*/ INTRA_HEVC_PRED_FRAMES, 32},
        {/*02*/ INTRA_AVC_HEVC_PRED_FRAMES, 32}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const int frameNumber = 30;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_HEVC_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];
        m_mode = tc.m_mode;
        m_ctu_size = tc.m_ctu_size;

        ///////////////////////////////////////////////////////////////////////////

        MFXInit();

        EncodeFrames(frameNumber);

        Close();

        if (m_mode == INTRA_HEVC_PRED_FRAMES || m_mode == INTRA_AVC_HEVC_PRED_FRAMES)
        {
            if (!m_bHEVCIntraModes)
            {
                g_tsLog << "ERROR: RunTest: HEVC stream hasn't got HEVC specific intra modes\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_avc_intra_modes);
};
