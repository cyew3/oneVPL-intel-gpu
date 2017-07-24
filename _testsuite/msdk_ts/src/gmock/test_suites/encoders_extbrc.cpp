/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

// !!! TODO: add run-time checks

namespace encoders_extbrc
{

typedef enum _eBRCFunc
{
      BRC0_Init
    , BRC0_Reset
    , BRC0_Close
    , BRC0_GetFrameCtrl
    , BRC0_Update
    , BRC1_Init
    , BRC1_Reset
    , BRC1_Close
    , BRC1_GetFrameCtrl
    , BRC1_Update
    , BRC_NoCall
} eBRCFunc;

template <mfxU32 CodecID>
class TestSuite : public tsVideoEncoder
{
public:
    static const unsigned int n_cases;
    typedef void (TestSuite::*Mod) ();

    struct tc_struct
    {
        struct
        {
            mfxU16 ExtBRC;
            Mod    mod;
            const tsStruct::Field* field;
        } init, reset;
    };

    static const tc_struct test_case[];
    const tc_struct* m_pTC;
    mfxStatus m_brcSts;
    mfxExtBRC m_brc[2];
    eBRCFunc m_lastBRCcall;

    TestSuite();

    ~TestSuite() {}
    int RunTest(unsigned int id);

    void SetBRC0()
    {
        mfxExtBRC& BRC = m_par;
        BRC = m_brc[0];
    }
    void SetBRC1()
    {
        mfxExtBRC& BRC = m_par;
        BRC = m_brc[1];
    }
    void BRCFieldSwap();
    void BRCFieldZero();
    void SetBRC0AndFieldZero()
    {
        SetBRC0();
        BRCFieldZero();
    }
};

#define TestSuite TestSuite<0>
mfxStatus BRC0Init(mfxHDL pthis, mfxVideoParam* par)
{
    TRACE_FUNC2(BRC0Init, pthis, par);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest->m_lastBRCcall = BRC0_Init;
    return pTest->m_brcSts;
}
mfxStatus BRC0Reset(mfxHDL pthis, mfxVideoParam* par)
{
    TRACE_FUNC2(BRC0Reset, pthis, par);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest->m_lastBRCcall = BRC0_Reset;
    return pTest->m_brcSts;
}
mfxStatus BRC0Close(mfxHDL pthis)
{
    TRACE_FUNC1(BRC0Close, pthis);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest->m_lastBRCcall = BRC0_Close;
    return pTest->m_brcSts;
}
mfxStatus BRC0GetFrameCtrl(mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl)
{
    TRACE_FUNC3(BRC0GetFrameCtrl, pthis, par, ctrl);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest->m_lastBRCcall = BRC0_GetFrameCtrl;
    return pTest->m_brcSts;
}
mfxStatus BRC0Update(mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status)
{
    TRACE_FUNC4(BRC0Update, pthis, par, ctrl, status);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest->m_lastBRCcall = BRC0_Update;
    return pTest->m_brcSts;
}

mfxStatus BRC1Init(mfxHDL pthis, mfxVideoParam* par)
{
    TRACE_FUNC2(BRC1Init, pthis, par);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest--;
    mfxStatus sts = BRC0Init(pTest, par);
    pTest->m_lastBRCcall = BRC1_Init;
    return sts;
}
mfxStatus BRC1Reset(mfxHDL pthis, mfxVideoParam* par)
{
    TRACE_FUNC2(BRC1Reset, pthis, par);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest--;
    mfxStatus sts = BRC0Reset(pTest, par);
    pTest->m_lastBRCcall = BRC1_Reset;
    return sts;
}
mfxStatus BRC1Close(mfxHDL pthis)
{
    TRACE_FUNC1(BRC1Close, pthis);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest--;
    mfxStatus sts = BRC0Close(pTest);
    pTest->m_lastBRCcall = BRC1_Close;
    return sts;
}
mfxStatus BRC1GetFrameCtrl(mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl)
{
    TRACE_FUNC3(BRC1GetFrameCtrl, pthis, par, ctrl);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest--;
    mfxStatus sts = BRC0GetFrameCtrl(pTest, par, ctrl);
    pTest->m_lastBRCcall = BRC1_GetFrameCtrl;
    return sts;
}
mfxStatus BRC1Update(mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status)
{
    TRACE_FUNC4(BRC1Update, pthis, par, ctrl, status);
    TestSuite* pTest = (TestSuite*)pthis;
    pTest--;
    mfxStatus sts = BRC0Update(pTest, par, ctrl, status);
    pTest->m_lastBRCcall = BRC1_Update;
    return sts;
}

#undef TestSuite

template <mfxU32 CodecID>
void TestSuite<CodecID>::BRCFieldSwap()
{
    auto* pField = m_initialized ? m_pTC->reset.field : m_pTC->init.field;
    mfxExtBRC* pBRC = GetExtBufferPtr(m_par);
    if (pBRC && pField)
    {
        auto val = tsStruct::get(pBRC, *pField);
        auto val0 = tsStruct::get(&m_brc[0], *pField);
        auto val1 = tsStruct::get(&m_brc[1], *pField);
        tsStruct::set(pBRC, *pField, (val == val0) ? val1 : val0);
    }
}

template <mfxU32 CodecID>
void TestSuite<CodecID>::BRCFieldZero()
{
    auto* pField = m_initialized ? m_pTC->reset.field : m_pTC->init.field;
    mfxExtBRC* pBRC = GetExtBufferPtr(m_par);
    if (pBRC && pField)
        tsStruct::set(pBRC, *pField, 0);
}

bool IsBRCValid(mfxVideoParam& par)
{
    mfxExtBRC* pBRC = GetExtBufferPtr(par);
    if (!pBRC)
        return true; //internal BRC is always valid
    return pBRC->pthis
        && pBRC->Init
        && pBRC->Reset
        && pBRC->Close
        && pBRC->GetFrameCtrl
        && pBRC->Update;
}
bool IsBRCSame(mfxVideoParam& par, mfxExtBRC* pOld)
{
    mfxExtBRC* pNew = GetExtBufferPtr(par);
    if (!pNew) return true;
    if (!pOld) return false;
    return (   pNew->pthis == pOld->pthis
            && pNew->Init == pOld->Init
            && pNew->Reset == pOld->Reset
            && pNew->Close == pOld->Close
            && pNew->GetFrameCtrl == pOld->GetFrameCtrl
            && pNew->Update == pOld->Update);
}

template <mfxU32 CodecID>
TestSuite<CodecID>::TestSuite()
    : tsVideoEncoder(CodecID)
    , m_brcSts(MFX_ERR_NONE)
{
    memset(&m_brc[0], 0, sizeof(mfxExtBRC));
    m_brc[0].Header.BufferId = MFX_EXTBUFF_BRC;
    m_brc[0].Header.BufferSz = sizeof(mfxExtBRC);
    m_brc[0].pthis          = this;
    m_brc[0].Init           = BRC0Init;
    m_brc[0].Reset          = BRC0Reset;
    m_brc[0].Close          = BRC0Close;
    m_brc[0].GetFrameCtrl   = BRC0GetFrameCtrl;
    m_brc[0].Update         = BRC0Update;

    memset(&m_brc[1], 0, sizeof(mfxExtBRC));
    m_brc[1].Header.BufferId = MFX_EXTBUFF_BRC;
    m_brc[1].Header.BufferSz = sizeof(mfxExtBRC);
    m_brc[1].pthis          = this + 1;
    m_brc[1].Init           = BRC1Init;
    m_brc[1].Reset          = BRC1Reset;
    m_brc[1].Close          = BRC1Close;
    m_brc[1].GetFrameCtrl   = BRC1GetFrameCtrl;
    m_brc[1].Update         = BRC1Update;

    m_lastBRCcall = BRC_NoCall;
}

template <mfxU32 CodecID>
int TestSuite<CodecID>::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    m_pTC = &tc;
    mfxExtCodingOption2& CO2 = m_par;

    m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR; //Reset is unsupported by internal SWBRC in CBR, not a target for this test
    m_par.mfx.TargetKbps = 1000;
    m_par.mfx.MaxKbps = 1000;
    m_par.mfx.InitialDelayInKB = 64;

    MFXInit();
    if (!m_loaded)
        Load();

    CO2.ExtBRC = tc.init.ExtBRC;
    if (tc.init.mod)
        (this->*tc.init.mod)();

    bool invalid = (CO2.ExtBRC == MFX_CODINGOPTION_ON) && !IsBRCValid(m_par);
    bool brcSet = !!((mfxExtBRC*)GetExtBufferPtr(m_par));
    if (invalid)
        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    auto parOut(m_par);
    mfxExtCodingOption2& outCO2 = parOut;
    m_pParOut = &parOut;
    Query();

    if (invalid)
    {
        EXPECT_EQ(0, outCO2.ExtBRC);
    }

    if (invalid)
        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);

    Init();
    if (!invalid)
        GetVideoParam();

    if (CO2.ExtBRC == MFX_CODINGOPTION_ON && brcSet && !invalid)
    {
        EXPECT_EQ(BRC0_Init, m_lastBRCcall);
    }

    if (invalid)
    {
        return 0;
    }

    mfxExtBRC prevBRC, *pPrevBRC = GetExtBufferPtr(m_par);

    if (pPrevBRC)
    {
        prevBRC = *pPrevBRC;
        pPrevBRC = &prevBRC;
    }

    invalid = ((CO2.ExtBRC != MFX_CODINGOPTION_ON) && (tc.reset.ExtBRC == MFX_CODINGOPTION_ON))
        || ((CO2.ExtBRC == MFX_CODINGOPTION_ON) && (tc.reset.ExtBRC == MFX_CODINGOPTION_OFF));

    CO2.ExtBRC = tc.reset.ExtBRC;
    if (tc.reset.mod)
        (this->*tc.reset.mod)();

    invalid |= (CO2.ExtBRC == MFX_CODINGOPTION_ON) && !IsBRCSame(m_par, pPrevBRC);

    if (invalid)
        g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    Reset();
    GetVideoParam();

    if (CO2.ExtBRC == MFX_CODINGOPTION_ON && brcSet && !invalid)
    {
        EXPECT_EQ(BRC0_Reset, m_lastBRCcall);
    }

    Close();

    if (CO2.ExtBRC == MFX_CODINGOPTION_ON && brcSet)
    {
        EXPECT_EQ(BRC0_Close, m_lastBRCcall);
    }

    TS_END;
    return 0;
}


template <mfxU32 CodecID>
const typename TestSuite<CodecID>::tc_struct TestSuite<CodecID>::test_case[] =
{
    {// 0000
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, 0 }
    },
    {// 0001
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_OFF, 0 }
    },
    {// 0002
        { MFX_CODINGOPTION_OFF, 0 },
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 }
    },
    {// 0003
        { MFX_CODINGOPTION_ON, 0 },
    },
    {// 0004
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC1 }
    },
    {// 0005
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0AndFieldZero, &tsStruct::mfxExtBRC.Init },
    },
    {// 0006
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0AndFieldZero, &tsStruct::mfxExtBRC.Reset },
    },
    {// 0007
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0AndFieldZero, &tsStruct::mfxExtBRC.Close },
    },
    {// 0008
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0AndFieldZero, &tsStruct::mfxExtBRC.GetFrameCtrl },
    },
    {// 0009
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0AndFieldZero, &tsStruct::mfxExtBRC.Update },
    },
    {// 0010
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldZero, &tsStruct::mfxExtBRC.Init },
    },
    {// 0011
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldZero, &tsStruct::mfxExtBRC.Reset },
    },
    {// 0012
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldZero, &tsStruct::mfxExtBRC.Close },
    },
    {// 0013
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldZero, &tsStruct::mfxExtBRC.GetFrameCtrl },
    },
    {// 0014
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldZero, &tsStruct::mfxExtBRC.Update },
    },
    {// 0015
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldSwap, &tsStruct::mfxExtBRC.pthis },
    },
    {// 0016
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldSwap, &tsStruct::mfxExtBRC.Init },
    },
    {// 0017
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldSwap, &tsStruct::mfxExtBRC.Reset },
    },
    {// 0018
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldSwap, &tsStruct::mfxExtBRC.Close },
    },
    {// 0019
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldSwap, &tsStruct::mfxExtBRC.GetFrameCtrl },
    },
    {// 0020
        { MFX_CODINGOPTION_ON, &TestSuite::SetBRC0 },
        { MFX_CODINGOPTION_ON, &TestSuite::BRCFieldSwap, &tsStruct::mfxExtBRC.Update },
    },
};

template <mfxU32 CodecID>
const unsigned int TestSuite<CodecID>::n_cases = 21;// sizeof(TestSuite<CodecID>::test_case) / sizeof(TestSuite<CodecID>::tc_struct);

#define TestSuite TestSuite<MFX_CODEC_HEVC>
TS_REG_TEST_SUITE_CLASS(hevce_extbrc);
#undef TestSuite

#define TestSuite TestSuite<MFX_CODEC_AVC>
TS_REG_TEST_SUITE_CLASS(avce_extbrc);
#undef TestSuite

}