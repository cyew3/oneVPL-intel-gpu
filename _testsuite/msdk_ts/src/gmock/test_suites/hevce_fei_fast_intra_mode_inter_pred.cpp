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
Test verifies that HEVC FEI encode per-frame control flag FastIntraMode doesn't affect
Inter prediction in quantative manner when enabled for all frames or for subset
based on frame type. Difference between number of Inter base blocks from the reference
(when flag is OFF) and test bitstream shouldn't exceed threshold

*/

namespace hevce_fei_fast_intra_mode_inter_pred
{

class TestSuite :
    public tsVideoEncoder,
    public tsSurfaceProcessor,
    public tsBitstreamProcessor,
    public tsParserHEVC2
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        , tsParserHEVC2(BS_HEVC2::PARSE_SSD)
        , m_mode(0)
        , m_ctuSize(32)
    {
        m_filler = this;
        m_bs_processor = this;

        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 352;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 288;

        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_pPar->AsyncDepth = 1;                              // limitation for FEI
        m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP; // For now FEI work with CQP only

        m_pPar->mfx.GopPicSize = 30;
        m_pPar->mfx.GopRefDist = 4;

        mfxExtCodingOption2 &co2 = m_par;
        co2.BRefType = MFX_B_REF_PYRAMID;

        m_reader.reset(new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"),
            m_pPar->mfx.FrameInfo));

        g_tsStreamPool.Reg();
    }

    ~TestSuite() {}

    int RunTest(unsigned int id);
    static const unsigned int n_cases;
private:
    mfxU32           m_mode = 0;
    mfxU32           m_ctuSize = 0;
    mfxU32           m_width_in_ctu = 0;

    struct tc_struct
    {
        mfxU32 m_flagSetMode;
        mfxU32 m_ctuSize;
    };

    enum FlagSetMode
    {
        DISABLED_ON_ALL_FRAMES = 0,
        ENABLED_ON_ALL_FRAMES = 1,
        ENABLED_ON_I_FRAMES = 2,
        ENABLED_ON_P_FRAMES = 3,
        ENABLED_ON_B_FRAMES = 4,
        ENABLED_ON_B_REF_FRAMES = 5
    };

    void AdjustCaseCTUParameters();
    void SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl);
    void SetIntraModeInCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl, mfxU8 frameType);
    mfxU8 GetFrameType(const mfxVideoParam& videoPar, mfxU32 frameOrder);
    void ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src);
    mfxStatus ProcessSurface(mfxFrameSurface1& surf);

    void Count4x4InterPredBlocksInCU(Bs32u size);
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

    bool Check4x4InterPredBlocksThreshold();

    std::unique_ptr <tsRawReader> m_reader;
    std::map<mfxU32, tsExtBufType<mfxEncodeCtrl>> m_map_ctrl;

    const mfxU32 INTER_PRED_4X4_BLOCKS_THRESHOLD = 5;
    mfxU32 m_InterPred4x4BlocksNumRef = 0;
    mfxU32 m_InterPred4x4BlocksNumActual = 0;

    static const tc_struct test_case[];
};

void TestSuite::AdjustCaseCTUParameters()
{
    m_width_in_ctu = (m_pPar->mfx.FrameInfo.Width + m_ctuSize - 1) / m_ctuSize;
}

void TestSuite::SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl)
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

void TestSuite::SetIntraModeInCtrl(mfxExtFeiHevcEncFrameCtrl & ctrl, mfxU8 frameType)
{
    switch (m_mode)
    {
    case DISABLED_ON_ALL_FRAMES:
        // Frame Enc Control won't be changed
        break;
    case ENABLED_ON_ALL_FRAMES:
        ctrl.FastIntraMode = 1;
        break;
    case ENABLED_ON_I_FRAMES:
        if (frameType & MFX_FRAMETYPE_I)
        {
            ctrl.FastIntraMode = 1;
        }
        break;
    case ENABLED_ON_P_FRAMES:
        if (frameType & MFX_FRAMETYPE_P)
        {
            ctrl.FastIntraMode = 1;
        }
        break;
    case ENABLED_ON_B_FRAMES:
        if (frameType & MFX_FRAMETYPE_B)
        {
            ctrl.FastIntraMode = 1;
        }
        break;
    case ENABLED_ON_B_REF_FRAMES:
        if (frameType == (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF))
        {
            ctrl.FastIntraMode = 1;
        }
        break;
    }
}

mfxU8 TestSuite::GetFrameType(const mfxVideoParam & videoPar, mfxU32 frameOrder)
{
    mfxU32 gopPicSize = videoPar.mfx.GopPicSize;
    mfxU32 gopRefDist = videoPar.mfx.GopRefDist;

    if (frameOrder == 0)
    {
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);
    }

    mfxU32 frameInGOP = frameOrder % gopPicSize;

    if (frameInGOP == 0)
    {
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);
    }

    mfxU32 frameInPattern = (frameInGOP - 1) % gopRefDist;

    if (frameInPattern == gopRefDist - 1)
    {
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
    }

    if (frameInPattern % 2)
    {
        return (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF);
    }

    return MFX_FRAMETYPE_B;
}

void TestSuite::ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src)
{
    dst = src;
}

mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& surf)
{
    mfxStatus sts = m_reader->ProcessSurface(surf);

    surf.Data.TimeStamp = surf.Data.FrameOrder;

    mfxU8 frameType = GetFrameType(m_par, surf.Data.FrameOrder);

    tsExtBufType<mfxEncodeCtrl>& ctrl = m_map_ctrl[surf.Data.FrameOrder];

    mfxExtFeiHevcEncFrameCtrl& feiCtrl = ctrl;
    SetDefaultsToCtrl(feiCtrl);
    SetIntraModeInCtrl(feiCtrl, frameType);

    ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl));

    return sts;
}

void TestSuite::Count4x4InterPredBlocksInCU(Bs32u log2CUSize)
{
    if (log2CUSize < 3 || log2CUSize > 6)
    {
        g_tsLog << "ERROR: Count4x4InterPredBlocksInCU: log2 CU size is out of range [3;6] \n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    if (m_mode == FlagSetMode::DISABLED_ON_ALL_FRAMES)
    {
        m_InterPred4x4BlocksNumRef += 4 << (log2CUSize - 2);
    }
    else
    {
        m_InterPred4x4BlocksNumActual += 4 << (log2CUSize - 2);
    }
}

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

#ifdef MANUAL_DEBUG_MODE
    if (m_mode != FlagSetMode::DISABLED_ON_ALL_FRAMES)
    {
        WriteBitstream("debug.hevc", bs, true);
    }
    else
    {
        WriteBitstream("debug.ref.hevc", bs, true);
    }
#endif // MANUAL_DEBUG_MODE

    while (nFrames--)
    {
        mfxExtFeiHevcEncFrameCtrl& feiCtrl = m_map_ctrl[bs.TimeStamp];
        bool isAVCIntraModes = (feiCtrl.FastIntraMode == 1);

        auto& hdr = ParseOrDie();

        for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
        {
            if (!IsHEVCSlice(pNALU->nal_unit_type))
            {
                continue;
            }

            if (pNALU->slice == nullptr)
            {
                g_tsLog << "ERROR: ProcessBitstream: pNALU->slice is null\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }

            auto& slice = *pNALU->slice;

            Bs32u countCTU = 0;
            for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next, ++countCTU)
            {
                Bs16u countCU = 0;
                for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next, ++countCU)
                {
                    if (pCU->PredMode == BS_HEVC2::MODE_INTER)
                    {
                        Count4x4InterPredBlocksInCU(pCU->log2CbSize);
#ifdef MANUAL_DEBUG_MODE
                        if (m_mode != FlagSetMode::DISABLED_ON_ALL_FRAMES)
                        {
                            std::cout << "Test bitstream: ";
                        }
                        else
                        {
                            std::cout << "Reference bitstream: ";
                        }
                        std::cout << "Found Inter CU" << std::endl
                            << "in frame #" << bs.TimeStamp << " type : "
                            << (mfxU32)GetFrameType(m_par, bs.TimeStamp)
                            << " , CTU AdrX #" << countCTU % m_width_in_ctu
                            << " , CTU AdrY #" << countCTU / m_width_in_ctu
                            << " , CU #" << countCU << std::endl;
#endif // MANUAL_DEBUG_MODE
                    }
                }
            }
        }
    }
    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

bool TestSuite::Check4x4InterPredBlocksThreshold()
{
    mfxU32 difference = abs(mfxI64(m_InterPred4x4BlocksNumActual)
        - m_InterPred4x4BlocksNumRef);

    std::cout << " ; Difference in percent: "
        << 100 * ((mfxF32)difference / m_InterPred4x4BlocksNumRef) << "%" << std::endl;

    return (100 * ((mfxF32)difference / m_InterPred4x4BlocksNumRef)
        <= INTER_PRED_4X4_BLOCKS_THRESHOLD);
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ FlagSetMode::ENABLED_ON_ALL_FRAMES, 32},
    {/*01*/ FlagSetMode::ENABLED_ON_I_FRAMES, 32},
    {/*02*/ FlagSetMode::ENABLED_ON_P_FRAMES, 32},
    {/*03*/ FlagSetMode::ENABLED_ON_B_FRAMES, 32},
    {/*04*/ FlagSetMode::ENABLED_ON_B_REF_FRAMES, 32},
};
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const int frameNumber = 80;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_HEVC_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];
        m_ctuSize = tc.m_ctuSize;
        AdjustCaseCTUParameters();

        m_mode = DISABLED_ON_ALL_FRAMES;

        // Prepare reference data when FastIntraMode is disabled
        ///////////////////////////////////////////////////////////////////////////

        EncodeFrames(frameNumber);

        Close();

        m_map_ctrl.clear();

        // Test run
        ///////////////////////////////////////////////////////////////////////////

        m_mode = tc.m_flagSetMode;

        m_reader->ResetFile(true);

        EncodeFrames(frameNumber);

        Close();

        std::cout << std::endl << "Reference bitstream: "
            << m_InterPred4x4BlocksNumRef << " 4x4 Inter CU blocks found ; " << "Test bitstream: "
            << m_InterPred4x4BlocksNumActual << " 4x4 Inter CU blocks found";

        EXPECT_TRUE(Check4x4InterPredBlocksThreshold())
            << "Difference between number of 4x4 Inter blocks on reference and actual bitstream"
            << " exceeds threshold\n";

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_fast_intra_mode_inter_pred);
};
