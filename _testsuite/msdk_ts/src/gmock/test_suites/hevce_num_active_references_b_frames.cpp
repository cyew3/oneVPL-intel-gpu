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
#include "ts_parser.h"

/*!

Description:
Test verifies that HEVC encoder works correctly with NumRefActiveBL0[layer] and NumRefActiveBL1[layer] keys.
Current test allows to check from 1 (no b-pyramid) to 5 layers for b-pyramid

*/

#define DEF_SKL_L0_ACT_REF 3
#define DEF_SKL_L1_ACT_REF 1

using namespace BS_HEVC2;

namespace hevce_num_active_references_b_frames
{
    class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC2(PARSE_SSD)
        {
            m_filler = this;
            m_bs_processor = this;

            m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 352;
            m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 288;
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_pPar->AsyncDepth = 1;
            m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;

            m_pPar->mfx.IdrInterval = 0; // Only first I frame is IDR
            m_pPar->mfx.NumRefFrame = 7; // It is a minimum number of reference frames,
                                         // which test needs to have for case with 5 layers of b-pyramid and 3 L0 reference

            m_reader.reset(new tsRawReader(g_tsStreamPool.Get("/forBehaviorTest/foreman_cif.nv12"),
                m_pPar->mfx.FrameInfo));

            g_tsStreamPool.Reg();

            m_Gen.seed(0);
        }

        ~TestSuite() {}

        typedef enum
        {
            SYMMETRIC = 0,
            ASYMMETRIC = 1
        } PyramidType;

        typedef enum
        {
            CONSTANT = 0,
            NONCONSTANT = 1
        } ActiveRefPerLayer;

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxStatus sts = m_reader->ProcessSurface(s);

            s.Data.TimeStamp = s.Data.FrameOrder;

            return sts;
        }

        bool IsGPB(const Slice & slice)
        {
            mfxU32 L0Size = slice.num_ref_idx_l0_active;
            mfxU32 L1Size = slice.num_ref_idx_l1_active;

            auto IsInL0 = [&](const RefPic & frame)
            {
                const RefPic* begin = &slice.L0[0];
                const RefPic* end = &slice.L0[L0Size];
                const RefPic* it = std::find_if(begin, end, [&](const RefPic& L0Frame) { return L0Frame.POC == frame.POC; });
                return it != end;
            };

            return (slice.type == SLICE_TYPE::B) && std::all_of(&slice.L1[0], &slice.L1[L1Size], IsInL0);
        }

        void CheckActiveL0(const Slice & slice, mfxU32 layer, mfxU32 expected_active_ref)
        {
            EXPECT_EQ(slice.num_ref_idx_l0_active, expected_active_ref)
                << "Found NumRefActiveL0 value for layer #"
                << layer << " which is greater then expected\n"
                << "POC #" << slice.POC << "\n";
        }

        void CheckActiveL1(const Slice & slice, mfxU32 layer, mfxU32 expected_active_ref)
        {
            EXPECT_EQ(slice.num_ref_idx_l1_active, expected_active_ref)
                << "Found NumRefActiveL1 value for layer #"
                << layer << " which is greater then expected\n"
                << "POC #" << slice.POC << "\n";
        }

        void SetLayerForBFrame(const Slice & slice, mfxU32 & currLayer, bool isFirstBFrame)
        {
            // Max layer for for reference frames
            mfxU32 maxRefFrameLayerL0 = 0, maxRefFrameLayerL1 = 0;

            // Check only nearest reference frames (With the bigest layer)
            if (slice.num_ref_idx_l0_active)
            {
                RefPic* list0 = slice.L0;
                if (list0 == nullptr)
                {
                    g_tsLog << "ERROR: SetLayerForBFrame: list0 is nullptr\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }
                if (m_map_layer[list0->POC] > maxRefFrameLayerL0)
                    maxRefFrameLayerL0 = m_map_layer[list0->POC];
            }
            if (slice.num_ref_idx_l1_active)
            {
                RefPic* list1 = slice.L1;
                if (list1 == nullptr)
                {
                    g_tsLog << "ERROR: SetLayerForBFrame: list1 is nullptr\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }
                if (m_map_layer[list1->POC] > maxRefFrameLayerL1)
                    maxRefFrameLayerL1 = m_map_layer[list1->POC];
            }

            currLayer = std::max(maxRefFrameLayerL0, maxRefFrameLayerL1);
            if (!isFirstBFrame)
                currLayer++;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

            //WriteBitstream("debug.hevc", bs, true); // DEBUG

            while (nFrames--)
            {
                mfxU32 & currLayer = m_map_layer[bs.TimeStamp];

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

                    // Is frame GPB?
                    if (IsGPB(*pNALU->slice))
                        pNALU->slice->type = SLICE_TYPE::P;

                    if (pNALU->slice->type != SLICE_TYPE::B)
                        m_IsBpyrTop = true;

                    if (pNALU->slice->type == SLICE_TYPE::B)
                    {
                        SetLayerForBFrame(*pNALU->slice, currLayer, m_IsBpyrTop);
                        m_IsBpyrTop = false;

                        if (m_EncodedOrderIdx > 7) //Skip first 8 frames in the encoded order
                            // It is a minimum number of frames, which test needs to skip for accumulation of 3 L0 reference
                            // in case with 5 layers of b-pyramid
                        {
                            CheckActiveL0(*pNALU->slice, currLayer, m_active_BL0[currLayer] ? m_active_BL0[currLayer] : DEF_SKL_L0_ACT_REF);
                            CheckActiveL1(*pNALU->slice, currLayer, m_active_BL1[currLayer] ? m_active_BL1[currLayer] : DEF_SKL_L1_ACT_REF);
                        }
                    }
                }

                m_EncodedOrderIdx++;
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

        mfxU16 GetGopRefDistFromNumLayers(mfxU16 numBLayers)
        {
            return 1 << numBLayers;
        }

        mfxI32 GetRandomNumber(mfxI32 min, mfxI32 max)
        {
            std::uniform_int_distribution<> distr(min, max);
            return distr(m_Gen);
        }

        void SetNumActiveRef(mfxExtCodingOption3& cod3, mfxU16 active_P, mfxU16 active_BL0, mfxU16 active_BL1, ActiveRefPerLayer act_ref_per_lay)
        {
            if (act_ref_per_lay == CONSTANT)
            {
                std::fill(&m_active_P[0], &m_active_P[7], active_P);
                std::fill(&cod3.NumRefActiveP[0], &cod3.NumRefActiveP[7], active_P);
                std::fill(&m_active_BL0[0], &m_active_BL0[7], active_BL0);
                std::fill(&cod3.NumRefActiveBL0[0], &cod3.NumRefActiveBL0[7], active_BL0);
                std::fill(&m_active_BL1[0], &m_active_BL1[7], active_BL1);
                std::fill(&cod3.NumRefActiveBL1[0], &cod3.NumRefActiveBL1[7], active_BL1);
            }
            else if (act_ref_per_lay == NONCONSTANT)
            {
                for (mfxU16 layer = 0; layer < 8; layer++)
                {
                    m_active_P[layer]   = cod3.NumRefActiveP[layer]   = GetRandomNumber(1, std::max(1, (mfxI32)active_P));
                    m_active_BL0[layer] = cod3.NumRefActiveBL0[layer] = GetRandomNumber(1, std::max(1, (mfxI32)active_BL0));
                    m_active_BL1[layer] = cod3.NumRefActiveBL1[layer] = GetRandomNumber(1, std::max(1, (mfxI32)active_BL1));
                }
            }
        }

        struct tc_struct
        {
            tc_struct(PyramidType pyr_type, ActiveRefPerLayer act_ref_per_lay, mfxU16 active_P, mfxU16 active_BL0, mfxU16 active_BL1, mfxU16 numBLayers) :
                m_pyr_type(pyr_type),
                m_act_ref_per_lay(act_ref_per_lay),
                m_numBLayers(numBLayers),
                m_active_P(active_P),
                m_active_BL0(active_BL0),
                m_active_BL1(active_BL1) {}
            PyramidType m_pyr_type = SYMMETRIC;
            ActiveRefPerLayer m_act_ref_per_lay = CONSTANT;
            mfxU16 m_numBLayers = 0;
            mfxU16 m_active_P   = 0;
            mfxU16 m_active_BL0 = 0;
            mfxU16 m_active_BL1 = 0;
        };

        static const tc_struct test_case[];

        std::unique_ptr <tsRawReader> m_reader;

        mfxU16 m_active_P[8]   = {};
        mfxU16 m_active_BL0[8] = {};
        mfxU16 m_active_BL1[8] = {};

        std::map <mfxU32, mfxU32> m_map_layer;

        bool             m_IsBpyrTop = false; // Pointer to top of the B-pyramid with layer 0
        mfxU32           m_EncodedOrderIdx = 0;
        std::mt19937     m_Gen;
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        //Next 5 cases should work with L0 - 1, L1 - 1 active references
        {/*00*/ SYMMETRIC, CONSTANT, 0, 1, 1, 1 },// 1 b-layer
        {/*01*/ SYMMETRIC, CONSTANT, 0, 1, 1, 2 },// 2 b-layers
        {/*02*/ SYMMETRIC, CONSTANT, 0, 1, 1, 3 },// 3 b-layers
        {/*03*/ SYMMETRIC, CONSTANT, 0, 1, 1, 4 },// 4 b-layers
        {/*04*/ SYMMETRIC, CONSTANT, 0, 1, 1, 5 },// 5 b-layers
        //Next 5 cases should work with L0 - 2, L1 - 1 active references
        {/*05*/ SYMMETRIC, CONSTANT, 0, 2, 1, 1 },// 1 b-layer
        {/*06*/ SYMMETRIC, CONSTANT, 0, 2, 1, 2 },// 2 b-layers
        {/*07*/ SYMMETRIC, CONSTANT, 0, 2, 1, 3 },// 3 b-layers
        {/*08*/ SYMMETRIC, CONSTANT, 0, 2, 1, 4 },// 4 b-layers
        {/*09*/ SYMMETRIC, CONSTANT, 0, 2, 1, 5 },// 5 b-layers
        //Next 5 cases should work with default active values for reference (L0 - 3, L1 - 1)
        {/*10*/ SYMMETRIC, CONSTANT, 0, 0, 0, 1 },// 1 b-layer
        {/*11*/ SYMMETRIC, CONSTANT, 0, 0, 0, 2 },// 2 b-layers
        {/*12*/ SYMMETRIC, CONSTANT, 0, 0, 0, 3 },// 3 b-layers
        {/*13*/ SYMMETRIC, CONSTANT, 0, 0, 0, 4 },// 4 b-layers
        {/*14*/ SYMMETRIC, CONSTANT, 0, 0, 0, 5 },// 5 b-layers
        //Next case should work with with L0 - 2, L1 - 1 active references and asymmetric b-pyramid
        {/*15*/ ASYMMETRIC, CONSTANT, 0, 2, 1, 5 },// 5 b-layers
        //Next case should work with with L0 - 3, L1 - 1 active references and symmetric b-pyramid
        // but with different active references per layer
        {/*16*/ SYMMETRIC, NONCONSTANT, 3, 3, 1, 5 },// 5 b-layers
        //Next case chacks that number of active references for p-frames won't affect b-frames control
        {/*17*/ SYMMETRIC, CONSTANT, 1, 3, 1, 5 }   // 5 b-layers
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const int frameNumber = 33;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        m_pPar->mfx.GopRefDist = GetGopRefDistFromNumLayers(tc.m_numBLayers);
        // Asymmetric b-pyramid
        if (tc.m_pyr_type == ASYMMETRIC)
            m_pPar->mfx.GopRefDist--;

        // Set number of active references
        mfxExtCodingOption3& cod3 = m_par;
        SetNumActiveRef(cod3, tc.m_active_P, tc.m_active_BL0, tc.m_active_BL1, tc.m_act_ref_per_lay);
        //

        MFXInit();

        EncodeFrames(frameNumber);

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_num_active_references_b_frames);
};
