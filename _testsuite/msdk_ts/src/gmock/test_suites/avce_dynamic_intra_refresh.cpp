/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

using namespace BS_AVC2;

//#define DEBUG_STREAM "avce_dynamic_intra_refresh.264"

namespace avce_dynamic_intra_refresh
{

class TestSuite
    : public tsVideoEncoder
    , public tsSurfaceProcessor
    , public tsBitstreamProcessor
    , public tsParserAVC2
{
private:
    struct tc_struct
    {
        struct irpar
        {
            mfxU16      IntRefType;
            mfxU16      IntRefCycleSize;
            mfxI16      IntRefQPDelta;
            mfxU16      IntRefCycleDist;
            mfxU32      DisplayOrder;
        } Init, RunTime, Reset;
    };
    static const tc_struct test_case[];
    static const mfxU16 NumFrames = 20;

    tsRawReader* m_reader;
#ifdef DEBUG_STREAM
    tsBitstreamWriter m_writer;
#endif
    const tc_struct* m_pTC;
    mfxI32 m_order;
    mfxI32 m_irOrder;
    mfxU16 m_mbDone;
    mfxU16 m_mbLeft;
    mfxU16 m_mbStripe;
    mfxU16 m_irType;
    mfxI16 m_irQPD;

public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_AVC)
        , tsParserAVC2(INIT_MODE_PARSE_SD)
        , m_reader(0)
#ifdef DEBUG_STREAM
        , m_writer(DEBUG_STREAM)
#endif
        , m_order(0)
        , m_irOrder(0)
        , m_mbLeft(0)
    {
        set_trace_level(0);

        m_filler = this;
        m_bs_processor = this;
    }

    ~TestSuite()
    {
        if (m_reader)
            delete m_reader;
    }

    static const unsigned int n_cases;
    int RunTest(unsigned int id);

    mfxStatus ProcessSurface(mfxFrameSurface1& s);
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
{
    if (   m_pTC->RunTime.DisplayOrder
        && m_pTC->RunTime.DisplayOrder == m_reader->m_cur)
    {
        if (m_pTC->RunTime.IntRefType)
        {
            mfxExtCodingOption2& CO2 = m_ctrl;
            CO2.IntRefType      = m_pTC->RunTime.IntRefType;
            CO2.IntRefCycleSize = m_pTC->RunTime.IntRefCycleSize;
            CO2.IntRefQPDelta   = m_pTC->RunTime.IntRefQPDelta;
        }

        if (m_pTC->RunTime.IntRefCycleDist)
        {
            mfxExtCodingOption3& CO3 = m_ctrl;
            CO3.IntRefCycleDist = m_pTC->RunTime.IntRefCycleDist;
        }

        m_pCtrl = &m_ctrl;
    }
    else
        m_pCtrl = 0;

    m_reader->ProcessSurface(s);

    return MFX_ERR_NONE;
}

inline bool IsIntra(MB* mb) { return mb->MbType <= I_PCM; };

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    SetBuffer0(bs);
    UnitType& hdr = ParseOrDie();
    bool dynIR = m_order && ((mfxU32)m_order == m_pTC->RunTime.DisplayOrder);
    auto IRPar = ((m_order < NumFrames || !m_pTC->Reset.IntRefType) ? m_pTC->Init : m_pTC->Reset);
    bool IROffCheck = (m_order >= NumFrames && !m_pTC->Reset.IntRefType);

    g_tsLog << "Frame #" << m_order << "\n";

    if (   dynIR
        || (   m_order && IRPar.IntRefCycleSize
            && !m_mbLeft
            && 0 == (m_irOrder % TS_MAX(IRPar.IntRefCycleSize, IRPar.IntRefCycleDist))))
    {
        if (dynIR && m_pTC->RunTime.IntRefType)
        {
            IRPar = m_pTC->RunTime;
            IROffCheck = false;
        }

        m_mbDone = 0;
        m_mbLeft = IRPar.IntRefType == 1 ? m_par.mfx.FrameInfo.Width / 16 : m_par.mfx.FrameInfo.Height / 16;
        m_mbStripe = (m_mbLeft + IRPar.IntRefCycleSize - 1) / IRPar.IntRefCycleSize;
        m_irType = IRPar.IntRefType;
        m_irQPD = IRPar.IntRefQPDelta;
    }


#ifdef DEBUG_STREAM
    m_writer.ProcessBitstream(bs, nFrames);
#endif

    bs.DataLength = 0;
    m_order++;
    m_irOrder++;

    g_tsLog << "Intra:\n";
    for (Bs32u iNALU = 0; iNALU < hdr.NumUnits; iNALU++)
    {
        auto& nalu = *hdr.nalu[iNALU];

        if (   nalu.nal_unit_type != SLICE_IDR
            && nalu.nal_unit_type != SLICE_NONIDR)
            continue;

        auto mb = nalu.slice->mb;

        while (mb)
        {
            if (!mb->x && mb->y)
                g_tsLog << "\n";

            g_tsLog << IsIntra(mb) << " ";

            mb = mb->next;
        }
        g_tsLog << "\n";
    }

#if 0
    g_tsLog << "QPy:\n";
    for (Bs32u iNALU = 0; iNALU < hdr.NumUnits; iNALU++)
    {
        auto& nalu = *hdr.nalu[iNALU];

        if (   nalu.nal_unit_type != SLICE_IDR
            && nalu.nal_unit_type != SLICE_NONIDR)
            continue;

        auto mb = nalu.slice->mb;

        while (mb)
        {
            if (!mb->x && mb->y)
                g_tsLog << "\n";

            g_tsLog << std::setw(3) << mfxU16(mb->QPy) << " ";

            mb = mb->next;
        }
        g_tsLog << "\n";
    }
#endif

    if (!m_mbLeft)
        return MFX_ERR_NONE;

    m_mbStripe = TS_MIN(m_mbStripe, m_mbLeft);
    m_mbLeft -= m_mbStripe;

    g_tsLog << "Expected refresh line: " << m_mbDone << " .. " << m_mbDone + m_mbStripe
            << ", QPDelta = "<< m_irQPD << "\n";

    for (Bs32u iNALU = 0; iNALU < hdr.NumUnits; iNALU++)
    {
        auto& nalu = *hdr.nalu[iNALU];

        if (   nalu.nal_unit_type != SLICE_IDR
            && nalu.nal_unit_type != SLICE_NONIDR)
            continue;

        auto mb = nalu.slice->mb;

        if (IROffCheck)
        {
            IROffCheck = false;

            while (mb)
            {
                auto X = m_irType == 1 ? mb->x / 16 : mb->y / 16;

                if (X >= m_mbDone && X < (m_mbDone + m_mbStripe) && !IsIntra(mb))
                {
                    IROffCheck = true;
                    break;
                }

                mb = mb->next;
            }

            EXPECT_TRUE(IROffCheck) << "Intra Refresh wasn't disabled by Reset()\n";
        }
        else
        {
            bool IRCheck = true;
            bool QPCheck = true;

            while (mb)
            {
                auto X = m_irType == 1 ? mb->x / 16 : mb->y / 16;

                if (X >= m_mbDone && X < (m_mbDone + m_mbStripe))
                {
                    if (!IsIntra(mb))
                    {
                        IRCheck = false;
                        break;
                    }
                    EXPECT_TRUE(IsIntra(mb)) << "ERROR: MB is not encoded as Intra" << std::endl
                        << "DETAILS:" << " in Frame #" << (m_order - 1) << " MB #" << mb->Addr << std::endl;

                    if (mb->coded_block_pattern && m_par.mfx.QPP + m_irQPD != mb->QPy)
                    {
                        EXPECT_EQ(m_par.mfx.QPP + m_irQPD, mb->QPy) << "ERROR: MB encoded with unexpected QP" << std::endl
                            << "DETAILS: unexpected QP value " << mb->QPy << " MB #" << mb->Addr << "Expected QP" << (m_par.mfx.QPP + m_irQPD) << std::endl;
                        QPCheck = false;
                        break;
                    }
                }

                mb = mb->next;
            }

            EXPECT_TRUE(IRCheck);
            EXPECT_TRUE(QPCheck);
        }
    }

    m_mbDone += m_mbStripe;

    return MFX_ERR_NONE;
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
/*
{
    IntRefType;
    IntRefCycleSize;
    IntRefQPDelta;
    IntRefCycleDist;
    DisplayOrder;
}
*/
//         Init              RT   Reset
/*0000*/{ { 0,  0,  0,  0}, { }, { 1,  5,  2, 10} },
/*0001*/{ { 1,  5,  2, 10}, { }, { 0,  0,  0,  0} },
/*0002*/{ { 1,  3,  2,  0}, { }, { 1,  5, -1,  0} },
/*0003*/{ { 1,  6,  2,  0}, { }, { 1,  3, -1,  0} },
/*0004*/{ { 1,  6,  2,  0}, { }, { 2,  3, -1,  0} },
/*0005*/{ { 2,  6,  2,  0}, { }, { 1,  3, -1,  6} },
/*0006*/{ { 2,  6,  2,  6}, { }, { 1,  3, -1,  0} },
/*0007*/{ { 2,  6,  2,  6}, { }, { 1,  3, -1, 10} },
/*0008*/{ { 2,  6,  2, 10}, { }, { 1,  3, -1, 11} },
/*0009*/{ { 2,  6,  2, 10}, { }, { 1,  3, -1,  9} },
/*0010*/{ { 1,  5,  2,  6}, { }, { 1,  5, -1,  0} },
/*0011*/{ { 1,  5,  2,  0}, { }, { 1,  5, -1, 10} },
/*0012*/{ { 1,  3,  2,  6}, { }, { 1,  3, -1,  5} },
/*0013*/{ { 1,  1,  2,  1}, { }, { 1,  5, -1,  0} },
/*0014*/{ { 1,  1,  2,  1}, { }, { 1,  5, -1, 10} },
/*0015*/{ { 1,  1,  2,  1}, { }, { 1,  3, -1,  5} },
/*0016*/{ { 1,  3,  2,  8}, { }, { 1,  5, -1,  0} },
/*0017*/{ { 1,  3,  2,  8}, { }, { 1,  5, -1, 10} },
/*0018*/{ { 1,  3,  2,  8}, { }, { 1,  3, -1,  5} },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];
    m_pTC = &tc;
    mfxExtCodingOption2& CO2 = m_par;
    mfxExtCodingOption3& CO3 = m_par;

    CO2.IntRefType      = tc.Init.IntRefType;
    CO2.IntRefCycleSize = tc.Init.IntRefCycleSize;
    CO2.IntRefQPDelta   = tc.Init.IntRefQPDelta;
    CO3.IntRefCycleDist = tc.Init.IntRefCycleDist;

    m_irOrder = tc.Init.IntRefCycleDist == 0 ? -1 : 0;

    m_par.AsyncDepth              = 1;
    m_par.mfx.GopRefDist          = 1;
    m_par.mfx.NumRefFrame         = 1;
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_par.mfx.FrameInfo.Width     = 352;
    m_par.mfx.FrameInfo.Height    = 288;
    m_par.mfx.FrameInfo.CropW     = 352;
    m_par.mfx.FrameInfo.CropH     = 288;

    m_reader = new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo, 30);
    g_tsStreamPool.Reg();

    EncodeFrames(NumFrames);

    CO2.IntRefType      = tc.Reset.IntRefType;
    CO2.IntRefCycleSize = tc.Reset.IntRefCycleSize;
    CO2.IntRefQPDelta   = tc.Reset.IntRefQPDelta;
    CO3.IntRefCycleDist = tc.Reset.IntRefCycleDist;

    Reset();

    if (!tc.Reset.IntRefType)
    {
        m_mbLeft = 0;
    }
    else
    {
        m_irQPD  = tc.Reset.IntRefQPDelta;
        m_irType = tc.Reset.IntRefType;

        mfxU16 irPos = tc.Init.IntRefCycleDist ? (m_order % tc.Init.IntRefCycleDist) : 0;

        if (!irPos || m_mbLeft)
        {
            irPos += (mfxU16)TS_MIN(tc.Init.IntRefCycleDist, m_order);
        }

        if (tc.Reset.IntRefType != tc.Init.IntRefType)
        {
            m_mbLeft  = 0;

            if (!tc.Reset.IntRefCycleDist)
            {
                m_irOrder = 0;
            }
            else
            {
                if (tc.Reset.IntRefCycleDist < irPos)
                {
                    m_irOrder = 0;
                }
                else
                {
                    m_irOrder = irPos;
                }
            }
        }
        else if (tc.Init.IntRefCycleDist || tc.Reset.IntRefCycleDist)
        {
            bool firstInCycle = !m_mbLeft && 0 == (m_irOrder % TS_MAX(tc.Init.IntRefCycleSize, tc.Init.IntRefCycleDist));

            if (!m_mbLeft)
                m_mbDone = 0;

            m_mbLeft = tc.Reset.IntRefType == 1 ? m_par.mfx.FrameInfo.Width / 16 : m_par.mfx.FrameInfo.Height / 16;
            m_mbStripe = (m_mbLeft + tc.Reset.IntRefCycleSize - 1) / tc.Reset.IntRefCycleSize;

            if (!firstInCycle && !m_mbDone)
            {
                m_mbLeft  = 0;

                if (tc.Reset.IntRefCycleDist < irPos)
                {
                    m_irOrder = 0;
                }
                else
                {
                    m_irOrder = irPos;
                }
            }
            else
            {
                if (m_mbDone)
                {
                    m_irOrder = m_order - ((m_mbDone + m_mbStripe - 1) / m_mbStripe);
                    m_mbDone /= m_mbStripe;
                    m_mbDone *= m_mbStripe;
                    m_mbLeft -= m_mbDone;
                }
                else //if (!firstInCycle)
                {
                    m_irOrder -= (m_irOrder % tc.Init.IntRefCycleDist);
                    m_mbLeft = 0;
                }

                if (   tc.Reset.IntRefCycleDist > tc.Init.IntRefCycleDist
                    && tc.Init.IntRefCycleDist > tc.Init.IntRefCycleSize)
                {
                    m_irOrder += tc.Reset.IntRefCycleDist - tc.Init.IntRefCycleDist;
                }

                if (tc.Reset.IntRefCycleDist)
                {
                    m_irOrder = tc.Reset.IntRefCycleDist - m_irOrder % tc.Reset.IntRefCycleDist;
                }
                else if (!firstInCycle)
                {
                    m_irOrder--;
                }
            }
        }
        else
        {
            m_mbLeft = tc.Reset.IntRefType == 1 ? m_par.mfx.FrameInfo.Width / 16 : m_par.mfx.FrameInfo.Height / 16;
            m_mbStripe = (m_mbLeft + tc.Reset.IntRefCycleSize - 1) / tc.Reset.IntRefCycleSize;
            m_mbDone /= m_mbStripe;
            m_irOrder = m_mbDone;
            m_mbDone *= m_mbStripe;
            m_mbLeft -= m_mbDone;
        }
    }

    EncodeFrames(NumFrames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_dynamic_intra_refresh);
};
