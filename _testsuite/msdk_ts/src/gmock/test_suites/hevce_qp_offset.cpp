//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_qp_offset
{

class TestSuite : tsVideoEncoder, tsParserHEVC, tsBitstreamProcessor
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVC()
    {
        m_bs_processor = this;
        set_trace_level(0);
    }

    ~TestSuite() { }
    int RunTest(unsigned int id);
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

    static const unsigned int n_cases;

private:

    enum
    {
        MFXPAR = 1,
        EXTCO2,
        EXTCO3,
    };

    struct tc_struct
    {
        mfxStatus sts;

        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];

        mfxI16 qpo[8];
        const mfxU16 *pattern;
        mfxU16 pattern_size;
    };

    static const tc_struct test_case[];

    const tc_struct* m_tc;
    mfxI32 m_anchorPOC;
};

const mfxU16 B3[3]   = {1,0,1};
const mfxU16 B7[7]   = {2,1,2,0,2,1,2};
const mfxU16 B15[15] = {3,2,3,1,3,2,3,0,3,2,3,1,3,2,3};
const mfxU16 P4[4]   = {0,2,1,2};

const TestSuite::tc_struct TestSuite::test_case[] =
{
/* 00 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}
        },
        {},
        P4, 4
    },
/* 01 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON}
        },
        {-10, -5, 4, },
        P4, 4
    },
/* 02 */
    {
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON},
            {EXTCO2, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_OFF}
        },
        {},
        0, 0
    },
/* 03 */
    {
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.PRefType, MFX_P_REF_SIMPLE}
        },
        {},
        0, 0
    },
/* 04 */
    {
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1000},
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0},
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON},
        },
        {},
        0, 0
    },
/* 05 */
    {
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON}
        },
        {-30, 40, 4, },
        B7, 7
    },
/* 06 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF}
        },
        {0, 2, 4, 8},
        P4, 4
    },
/* 07 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF}
        },
        {},
        P4, 4
    },
/* 08 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8},
        },
        {},
        B7, 7
    },
/* 09 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4},
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 36},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON}
        },
        {-16, 3},
        B3, 3
    },
/* 10 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 16},
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 30},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON}
        },
        {-5, 0, 3},
        B15, 15
    },
/* 11 */
    {
        MFX_ERR_NONE,
        {
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
            {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
            {EXTCO3, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_ON}
        },
        { -2, 0, 2},
        P4, 4
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

using namespace BS_HEVC;

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxExtCodingOption3& CO3 = m_par;

    SetBuffer0(bs);

    auto& AU = ParseOrDie();
    auto& S = AU.pic->slice[0]->slice[0];

    bool isP = true;
    bool BPyr = (m_par.mfx.GopRefDist > 1);
    mfxI16 QP  = S.slice_qp_delta + S.pps->init_qp_minus26 + 26;
    mfxI16 QPX = (m_par.mfx.GopRefDist > 1) ? m_par.mfx.QPB : m_par.mfx.QPP;

    if (S.type == I)
    {
        EXPECT_EQ(m_par.mfx.QPI, QP);
        m_anchorPOC = AU.pic->PicOrderCntVal;
    }
    else
    {
        if (S.type == B)
        {
            for (mfxU32 i = 0; isP && i <= S.num_ref_idx_l1_active_minus1; i++)
                isP = (AU.pic->RefPicList1[i]->PicOrderCntVal < AU.pic->PicOrderCntVal);
        }

        if (isP && BPyr)
        {
            m_anchorPOC = AU.pic->PicOrderCntVal;
            EXPECT_EQ(m_par.mfx.QPP, QP);
        }
        else if (CO3.EnableQPOffset == MFX_CODINGOPTION_ON)
        {
            mfxI32 maxIdx = BPyr ? m_tc->pattern_size : CO3.NumRefActivePRef;
            mfxI32 idx = (abs(AU.pic->PicOrderCntVal - m_anchorPOC) - BPyr) % maxIdx;
            mfxU16 layer = m_tc->pattern[idx];
            EXPECT_EQ(QPX + CO3.QPOffset[layer], QP) << " (layer = " << layer << ")";
        }
        else
        {
            EXPECT_EQ(QPX, QP);
        }
    }

    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxExtCodingOption3& CO3 = m_par;
    mfxExtCodingOption2& CO2 = m_par;

    tsExtBufType<mfxVideoParam> parOut;
    mfxExtCodingOption3& CO3out = parOut;
    mfxExtCodingOption2& CO2out = parOut;

    m_tc = test_case + id;
    parOut.mfx.CodecId = m_par.mfx.CodecId;

    MFXInit();
    Load();

    memcpy(CO3.QPOffset, tc.qpo, sizeof(tc.qpo));
    SETPARS(m_pPar, MFXPAR);
    SETPARS(&CO3, EXTCO3);
    SETPARS(&CO2, EXTCO2);

    m_par.AsyncDepth = 1; //async=1 used to simplify test logic in ProcessBitstream. Increased async will break the test.
    m_par.mfx.IdrInterval = 1;

    g_tsStatus.expect(tc.sts);
    m_pParOut = &parOut;
    Query();

    if (tc.sts)
    {
        bool par_changed = (CO3.EnableQPOffset != CO3out.EnableQPOffset);

        if (CO3out.EnableQPOffset == MFX_CODINGOPTION_ON)
        {
            mfxI16 QPX = (parOut.mfx.GopRefDist == 1) ? parOut.mfx.QPP : parOut.mfx.QPB;

            for (mfxI16 i = 0; i < 8; i++)
            {
                EXPECT_GE(CO3out.QPOffset[i],  1 - QPX) << " (i = " << i << ")";
                EXPECT_LE(CO3out.QPOffset[i], 51 - QPX) << " (i = " << i << ")";
                par_changed |= (CO3out.QPOffset[i] != CO3.QPOffset[i]);
            }
        }

        EXPECT_TRUE(par_changed);
    }

    g_tsStatus.expect(tc.sts);
    Init();

    if (tc.sts >= 0 && m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        GetVideoParam();
        EncodeFrames(50);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_qp_offset);
};