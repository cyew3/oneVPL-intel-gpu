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

#include "ts_fei_warning.h"
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

/*
Description:
Test verifies that HEVC FEI encode per-frame control flag ForceCtuSplit affects
splitting all CTUs in order to avoid 32x32 INTER CUs. In other words, all inter CUs
should be 16x16 or smaller
*/

namespace hevce_fei_force_ctu_split
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
        , m_ctuSize(32)
#ifdef MANUAL_DEBUG_MODE
        , m_tsBsWriter("test.h265")
#endif // MANUAL_DEBUG_MODE
    {
        m_filler = this;
        m_bs_processor = this;

        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 4096;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 2160;
        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_pPar->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_pPar->AsyncDepth                 = 1;                   // Limitation for FEI
        m_pPar->mfx.RateControlMethod      = MFX_RATECONTROL_CQP; // For now, FEI work with CQP only

        m_reader.reset(new tsRawReader(g_tsStreamPool.Get("YUV/rally_4096x2160_30fps_1s_YUV.yuv"),
                                   m_pPar->mfx.FrameInfo));
        g_tsStreamPool.Reg();
    }

private:
    const mfxU32 m_framesNumber = 10;
    // Case parameters:
    mfxU32 m_ctuSize = 0;
    mfxU32 m_widthInCTU = 0;
    mfxU32 m_resolutionMode = 0;
    mfxU32 m_flagMode = 0;

#ifdef MANUAL_DEBUG_MODE
    tsBitstreamWriter m_tsBsWriter;
#endif // MANUAL_DEBUG_MODE

    enum Resolution
    {
        RESOLUTION_176x144   = 0,
        RESOLUTION_352x288   = 1,
        RESOLUTION_720x480   = 2,
        RESOLUTION_1280x720  = 3,
        RESOLUTION_1920x1088 = 4,
        RESOLUTION_4096x2160 = 5
    };

    // Specifies whether the flag will be applied or not
    // on per-frame basis
    enum FlagSetMode
    {
        ALL_FRAMES = 0,
        ROTATING = 1
    };

    struct tc_struct
    {
        mfxU32 m_resolution;
        mfxU32 m_flagSetMode;
        mfxU32 m_ctuSize;
    };

    void SetCaseFrameResolution();

    void AdjustCaseCTUParameters();

    mfxStatus ProcessSurface(mfxFrameSurface1& surf) override;

    // Fills mfxExtFeiHevcEncFrameCtrl with initial parameters for ENCODE
    void SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl);

    void SetForceCtuSplitFlag(mfxExtFeiHevcEncFrameCtrl& ctrl, mfxU32 frameOrder);

    // Attaches frame-level mfxExtFeiHevcEncFrameCtrl to mfxEncodeCtrl
    void ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src);

    // Checks that all processed CUs have approriate size
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames) override;

    bool CheckCUSize(Bs32u size);

    std::unique_ptr <tsRawReader> m_reader;

    // Frame-level controls for ENCODE ordered in DISPLAY order
    std::map<mfxU32, tsExtBufType<mfxEncodeCtrl>> m_map_ctrl;

    static const  tc_struct test_case[];

public:
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ Resolution::RESOLUTION_176x144,   FlagSetMode::ALL_FRAMES, 32 },
    {/*01*/ Resolution::RESOLUTION_176x144,   FlagSetMode::ROTATING,  32 },
    {/*02*/ Resolution::RESOLUTION_352x288,   FlagSetMode::ALL_FRAMES, 32 },
    {/*03*/ Resolution::RESOLUTION_352x288,   FlagSetMode::ROTATING,  32 },
    {/*04*/ Resolution::RESOLUTION_720x480,   FlagSetMode::ALL_FRAMES, 32 },
    {/*05*/ Resolution::RESOLUTION_720x480,   FlagSetMode::ROTATING,  32 },
    {/*06*/ Resolution::RESOLUTION_1280x720,  FlagSetMode::ALL_FRAMES, 32 },
    {/*07*/ Resolution::RESOLUTION_1280x720,  FlagSetMode::ROTATING,  32 },
    {/*08*/ Resolution::RESOLUTION_1920x1088, FlagSetMode::ALL_FRAMES, 32 },
    {/*09*/ Resolution::RESOLUTION_1920x1088, FlagSetMode::ROTATING,  32 },
    {/*10*/ Resolution::RESOLUTION_4096x2160, FlagSetMode::ALL_FRAMES, 32 },
    {/*11*/ Resolution::RESOLUTION_4096x2160, FlagSetMode::ROTATING,  32 },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


void TestSuite::SetCaseFrameResolution()
{
    switch (m_resolutionMode)
    {
    case RESOLUTION_176x144:
        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 176;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 144;
        break;
    case RESOLUTION_352x288:
        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 352;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 288;
        break;
    case RESOLUTION_720x480:
        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 720;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 480;
        break;
    case RESOLUTION_1280x720:
        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 1280;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 720;
        break;
    case RESOLUTION_1920x1088:
        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 1920;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 1088;
        break;
    case RESOLUTION_4096x2160:
        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 4096;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 2160;
        break;
    default:
        g_tsLog << "ERROR: SetCaseFrameResolution: Case is set with incorrect resolution mode" << "\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }
}

void TestSuite::AdjustCaseCTUParameters()
{
    m_widthInCTU = (m_pPar->mfx.FrameInfo.Width + m_ctuSize - 1) / m_ctuSize;
}

void TestSuite::SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl)
{
    memset(&ctrl, 0, sizeof(ctrl));

    ctrl.Header.BufferId = MFX_EXTBUFF_HEVCFEI_ENC_CTRL;
    ctrl.Header.BufferSz = sizeof(mfxExtFeiHevcEncFrameCtrl);
    ctrl.SubPelMode = 3;         // Use quarter-pixel motion estimation
    ctrl.SearchWindow = 5;       // 48 SUs 48x40 window full search
    ctrl.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
    // enable internal L0/L1 predictors: 1 - spatial predictors to preserve quality
    ctrl.MultiPred[0] = ctrl.MultiPred[1] = 1;
}

void TestSuite::SetForceCtuSplitFlag(mfxExtFeiHevcEncFrameCtrl & ctrl, mfxU32 frameOrder)
{
    switch (m_flagMode)
    {
    case ALL_FRAMES:
        ctrl.ForceCtuSplit = 1;
        break;
    case ROTATING:
        // Enabling flag for some subset of frames
        // For example, affect only even frames
        if (!(frameOrder % 2))
        {
            ctrl.ForceCtuSplit = 1;
        }
        break;
    default:
        g_tsLog << "ERROR: SetForceCtuSplitFlag: Case is set with incorrect flag setting mode" << "\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }
}

mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& surf)
{
    mfxStatus sts = m_reader->ProcessSurface(surf);

    surf.Data.TimeStamp = surf.Data.FrameOrder;

    tsExtBufType<mfxEncodeCtrl>& ctrl = m_map_ctrl[surf.Data.FrameOrder];

    mfxExtFeiHevcEncFrameCtrl& feiCtrl = ctrl;
    SetDefaultsToCtrl(feiCtrl);
    SetForceCtuSplitFlag(feiCtrl, surf.Data.FrameOrder);

    ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl));

    return sts;
}

void TestSuite::ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src)
{
    dst = src;
}

bool TestSuite::CheckCUSize(Bs32u log2CUSize)
{
    return log2CUSize < 5;
}

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

    mfxStatus sts = MFX_ERR_NONE;

#ifdef MANUAL_DEBUG_MODE
    sts = m_tsBsWriter.ProcessBitstream(bs, nFrames);
#endif // MANUAL_DEBUG_MODE

    while (nFrames--)
    {
        mfxExtFeiHevcEncFrameCtrl& feiCtrl = m_map_ctrl[bs.TimeStamp];

        if (!feiCtrl.ForceCtuSplit)
        {
            // ForceCtuSplit flag is disabled on this frame, nothing to check
            continue;
        }

        auto& hdr = ParseOrDie();

        for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
        {
            if (!pNALU)
            {
                g_tsLog << "ERROR: ProcessBitstream: Some NAL unit is undefined" << "\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }

            if (!IsHEVCSlice(pNALU->nal_unit_type))
            {
                continue;
            }

            auto& slice = *pNALU->slice;

            Bs32u countCTU = 0;

            for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next, ++countCTU)
            {
                for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next)
                {
                    // Flag affects Inter CUs only
                    if (pCU->PredMode == BS_HEVC2::MODE_INTER)
                    {
                        EXPECT_TRUE(CheckCUSize(pCU->log2CbSize))
                            << "CU size exceeds expected bounds"
                            << " on frame #" << bs.TimeStamp
                            << " ; CTU AdrX:" << countCTU % m_widthInCTU
                            << " ; CTU AdrY:" << countCTU / m_widthInCTU;
                    }
                }
            }
        }
    }

    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_HEVC_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];
    m_ctuSize        = tc.m_ctuSize;
    m_flagMode       = tc.m_flagSetMode;
    m_resolutionMode = tc.m_resolution;

    SetCaseFrameResolution();

    AdjustCaseCTUParameters();

    ///////////////////////////////////////////////////////////////////////////

    MFXInit();

    EncodeFrames(m_framesNumber);

    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_fei_force_ctu_split);
};
