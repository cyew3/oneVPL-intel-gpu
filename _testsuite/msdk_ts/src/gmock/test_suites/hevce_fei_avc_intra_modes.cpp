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
Test verifies that HEVC FEI encode per-frame control flag FastIntraMode affects
list of the available Intra Modes. If FastIntraMode is ON, bit-stream shouldn't
have HEVC specific intra modes

*/

using namespace BS_HEVC2;

namespace hevce_fei_avc_intra_modes
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
        , tsParserHEVC2(PARSE_SSD)
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

    enum FlagSetMode
    {
        /*00*/ DISABLED_ON_ALL_FRAMES = 0,
        /*01*/ ENABLED_ON_ALL_FRAMES = 1,
        /*02*/ ENABLED_ON_I_FRAMES = 2,
        /*03*/ ENABLED_ON_P_FRAMES = 3,
        /*04*/ ENABLED_ON_B_FRAMES = 4,
        /*05*/ ENABLED_ON_B_REF_FRAMES = 5
    };


    void AdjustCaseCTUParameters();
    void SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl);
    void SetIntraModeInCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl, mfxU8 frameType);
    mfxU8 GetFrameType(const mfxVideoParam& videoPar, mfxU32 frameOrder);
    void ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src);
    mfxStatus ProcessSurface(mfxFrameSurface1& surf);

    Bs32u GetIntraChromaModeFEI(Bs8u intraPredModeC_0_0);

    void ExtractIntraPredModeFEI(const CU* pCU, CUIntraTypeFEISpec& cuIntraTypeFEISpec);
    bool IsAVCSpecLumaIntraMode(mfxU32 intraMode);
    bool HasCUHevcIntraMode(const CUIntraTypeFEISpec& cuIntraTypeFEISpec);
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

    bool m_bHEVCIntraModes = false;
    std::unique_ptr <tsRawReader> m_reader;
    std::map<mfxU32, tsExtBufType<mfxEncodeCtrl>> m_map_ctrl;
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

#ifdef MANUAL_DEBUG_MODE
    std::cout << "Frame #" << surf.Data.FrameOrder << ", fast_intra_mode set to "
        << feiCtrl.FastIntraMode << std::endl;
#endif // MANUAL_DEBUG_MODE

    ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl));

    return sts;
}

Bs32u TestSuite::GetIntraChromaModeFEI(Bs8u intraPredModeC_0_0)
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

void TestSuite::ExtractIntraPredModeFEI(const CU* pCU, CUIntraTypeFEISpec& cuIntraTypeFEISpec)
{
    if (pCU == nullptr)
    {
        g_tsLog << "ERROR: ExtractIntraPredModeFEI: pCU is equal to nullptr\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    // Luma
    if (pCU->IntraPredModeY[0][0] > 34
        || pCU->IntraPredModeY[1][0] > 34
        || pCU->IntraPredModeY[0][1] > 34
        || pCU->IntraPredModeY[1][1] > 34)
    {
        g_tsLog << "ERROR: ExtractIntraPredModeFEI: incorrect IntraPredModeY[X][X]\n";
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
        g_tsLog << "ERROR: ExtractIntraPredModeFEI: incorrect IntraPredModeC[0][0]\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    cuIntraTypeFEISpec.IntraChromaMode = GetIntraChromaModeFEI(pCU->IntraPredModeC[0][0]);
}

bool TestSuite::HasCUHevcIntraMode(const CUIntraTypeFEISpec& cuIntraTypeFEISpec)
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

bool TestSuite::IsAVCSpecLumaIntraMode(mfxU32 intraMode)
{
    return (intraMode == 0 /*Planar*/ || intraMode == 1 /*DC*/
        || intraMode == 26 /*Ver*/ || intraMode == 10 /*Hor*/
        || intraMode == 34 /*DDL*/ || intraMode == 18 /*DDR*/
        || intraMode == 22 /*VR*/ || intraMode == 14 /*HD*/
        || intraMode == 30 /*VL*/ || intraMode == 6 /*HU*/);
}

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

#ifdef MANUAL_DEBUG_MODE
    WriteBitstream("debug.hevc", bs, true);
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

            CUIntraTypeFEISpec cuIntraTypeFEISpec;

            Bs32u countCTU = 0;
            for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next, ++countCTU)
            {
                Bs16u countCU = 0;
                for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next, ++countCU)
                {
                    // Extract Intra prediction mode information
                    if (pCU->PredMode == MODE_INTRA)
                    {
                        ExtractIntraPredModeFEI(pCU, cuIntraTypeFEISpec);
                        if (isAVCIntraModes)
                        {
                            EXPECT_FALSE(HasCUHevcIntraMode(cuIntraTypeFEISpec))
                                << "Found HEVC specific intra modes in case with AVC intra modes only"
                                << " for frame #" << bs.TimeStamp
                                << " , CTU AdrX #" << countCTU % m_width_in_ctu
                                << " , CTU AdrY #" << countCTU / m_width_in_ctu
                                << " , CU #" << countCU << ", (" << pCU->x << ";" << pCU->y << ")\n";
                        }
                        else
                        {
                            // Check that frame includes HEVC specific intra modes in case with enabled intra HEVC specific modes
                            if (HasCUHevcIntraMode(cuIntraTypeFEISpec))
                            {
#ifdef MANUAL_DEBUG_MODE
                                std::cout << "Found HEVC specific intra modes"
                                    << " for frame #" << bs.TimeStamp << " type : "
                                    << (mfxU32)GetFrameType(m_par, bs.TimeStamp)
                                    << " , CTU AdrX #" << countCTU % m_width_in_ctu
                                    << " , CTU AdrY #" << countCTU / m_width_in_ctu
                                    << " , CU #" << countCU << ", (" << pCU->x << ";" << pCU->y << ")"
                                    << std::endl;
#endif // MANUAL_DEBUG_MODE
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

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ FlagSetMode::DISABLED_ON_ALL_FRAMES, 32},
    {/*01*/ FlagSetMode::ENABLED_ON_ALL_FRAMES, 32},
    {/*02*/ FlagSetMode::ENABLED_ON_I_FRAMES, 32},
    {/*03*/ FlagSetMode::ENABLED_ON_P_FRAMES, 32},
    {/*04*/ FlagSetMode::ENABLED_ON_B_FRAMES, 32},
    {/*05*/ FlagSetMode::ENABLED_ON_B_REF_FRAMES, 32},
};
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const int frameNumber = 80;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_HEVC_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];
        m_mode = tc.m_flagSetMode;
        m_ctuSize = tc.m_ctuSize;

        AdjustCaseCTUParameters();

        ///////////////////////////////////////////////////////////////////////////

        MFXInit();

        EncodeFrames(frameNumber);

        Close();

        if (m_mode != ENABLED_ON_ALL_FRAMES)
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
