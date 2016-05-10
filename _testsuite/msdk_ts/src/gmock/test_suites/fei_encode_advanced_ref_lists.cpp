/******************************************************************************* *\

Copyright (C) 2016 Intel Corporation.  All rights reserved.

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
#include "ts_parser.h"
#include "ts_struct.h"

namespace fei_encode_advanced_ref_lists
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true) {}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
    static const mfxU16 max_rl = 12;
    static const mfxU16 max_mfx_pars = 5;

    enum
    {
        SET_RPL_EXPLICITLY    = 0x0001,
        CALCULATE_RPL_IN_TEST = 0x0002,
        REORDER_FOm1          = 0x0010, // reorder frame with FrameOrder - 1 to 1st place in ref list (change nothing)
        REORDER_FOm2          = 0x0020, // reorder frame with FrameOrder - 2 to 1st place in ref list
        REORDER_FOm3          = 0x0040, // reorder frame with FrameOrder - 3 to 1st place in ref list
    };


    struct ctrl{
        const tsStruct::Field* field;
        mfxU32 value;
    };

    struct tc_struct
    {
        mfxU32 controlFlags;
        mfxU16 nFrames;
        mfxU16 BRef;
        ctrl mfx[max_mfx_pars];
        struct _rl{
            mfxU16 poc;
            ctrl f[18];

        }rl[max_rl];
    };

    static const tc_struct test_case[];

};

mfxStatus CreateRefLists(mfxVideoParam& par, mfxU32 frameOrder, mfxU32 controlFlags, mfxExtAVCRefLists& refListFF, mfxExtAVCRefLists& refListSF)
{
    mfxU16& ps = par.mfx.FrameInfo.PicStruct;
    bool field = (ps == MFX_PICSTRUCT_FIELD_TFF || ps == MFX_PICSTRUCT_FIELD_BFF);
    bool bff   = (ps == MFX_PICSTRUCT_FIELD_BFF);

    if (frameOrder % par.mfx.GopPicSize)
    {
        // not an IDR-frame - ref lists can be applied
        if (!field)
        {
            // progressive encoding
            switch (controlFlags & 0xf0)
            {
            case TestSuite::REORDER_FOm2:
                if (par.mfx.GopRefDist == 1 && (frameOrder % par.mfx.GopPicSize) >= 2)
                {
                    refListFF.NumRefIdxL0Active = 1;
                    refListFF.RefPicList0[0].PicStruct = ps;
                    refListFF.RefPicList0[0].FrameOrder = frameOrder - 2;
                }
                break;
            case TestSuite::REORDER_FOm3:
                if (par.mfx.GopRefDist == 1 && (frameOrder % par.mfx.GopPicSize) >= 3)
                {
                    refListFF.NumRefIdxL0Active = 1;
                    refListFF.RefPicList0[0].PicStruct = ps;
                    refListFF.RefPicList0[0].FrameOrder = frameOrder - 3;
                }
            default:
                break;
            }
        }
        else
        {
            // interlaced encoding
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus ClearRefLists(mfxExtAVCRefLists& lists)
{
    lists.NumRefIdxL0Active = 0;
    lists.NumRefIdxL1Active = 0;
    for (auto& x : lists.RefPicList0)
        x.FrameOrder = MFX_FRAMEORDER_UNKNOWN;
    for (auto& x : lists.RefPicList1)
         x.FrameOrder = MFX_FRAMEORDER_UNKNOWN;

    return MFX_ERR_NONE;
}

class SFiller : public tsSurfaceProcessor
{
private:
    mfxU32 m_c;
    mfxU32 m_i;
    mfxU32 m_buf_idx;
    mfxU32 m_num_buffers;
    mfxU32 m_pic_struct;
    mfxVideoParam& m_par;
    mfxEncodeCtrl& m_ctrl;
    const TestSuite::tc_struct & m_tc;
    std::vector<mfxExtAVCRefLists> m_list;
    std::vector<mfxExtBuffer*> m_pbuf;

public:
    SFiller(const TestSuite::tc_struct & tc, mfxEncodeCtrl& ctrl, mfxVideoParam& par) : m_c(0), m_i(0), m_buf_idx(0), m_par(par), m_ctrl(ctrl), m_tc(tc)
    {
        mfxExtAVCRefLists rlb = {{MFX_EXTBUFF_AVC_REFLISTS, sizeof(mfxExtAVCRefLists)}, };

        m_num_buffers = 2 * (TS_MAX(m_par.AsyncDepth, m_par.mfx.GopRefDist) + 1);
        // 2 free ext buffers will be available for every EncodeFrameAsync call
        m_list.resize(m_num_buffers, rlb);
        m_pbuf.resize(m_num_buffers, 0  );

        for (auto& ctrl : tc.mfx)
        {
            if (ctrl.field == &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct)
            {
                m_pic_struct = ctrl.value;
                break;
            }
        }
    };
    mfxStatus ProcessSurface(mfxFrameSurface1& s);
};

 mfxStatus SFiller::ProcessSurface(mfxFrameSurface1& s)
 {
    bool field = (m_pic_struct == MFX_PICSTRUCT_FIELD_TFF || m_pic_struct == MFX_PICSTRUCT_FIELD_BFF);
    bool bff   = (m_pic_struct == MFX_PICSTRUCT_FIELD_BFF);

    s.Data.FrameOrder = m_c;
    memset(&m_ctrl, 0, sizeof(m_ctrl));

    m_pbuf[m_buf_idx+0] = &m_list[m_buf_idx+0].Header;
    m_pbuf[m_buf_idx+1] = &m_list[m_buf_idx+1].Header;

    // clear ext buffers for ref lists
    ClearRefLists(m_list[m_buf_idx]);
    ClearRefLists(m_list[m_buf_idx + 1]);

    m_ctrl.ExtParam = &m_pbuf[m_buf_idx];

    if (m_tc.controlFlags & TestSuite::SET_RPL_EXPLICITLY)
    {
        // get ref lists from description of test cases
        if (m_c * 2 == m_tc.rl[m_i].poc)
        {
            m_ctrl.NumExtParam = 1;

            for (auto& f : m_tc.rl[m_i].f)
                if (f.field)
                    tsStruct::set(m_pbuf[m_buf_idx], *f.field, f.value);
            m_i ++;
        }

        if (m_c * 2 + 1 == m_tc.rl[m_i].poc)
        {
            m_ctrl.NumExtParam = 2;

            for (auto& f : m_tc.rl[m_i].f)
                if (f.field)
                    tsStruct::set(m_pbuf[m_buf_idx + 1], *f.field, f.value);

            if (bff)
                std::swap(m_ctrl.ExtParam[0], m_ctrl.ExtParam[1]);

            m_i ++;
        }
    }
    else if (m_tc.controlFlags & TestSuite::CALCULATE_RPL_IN_TEST)
    {
        // calculate ref lists using requested logic
        CreateRefLists(m_par, s.Data.FrameOrder, m_tc.controlFlags, m_list[m_buf_idx], m_list[m_buf_idx + 1]);
        if (m_list[m_buf_idx].RefPicList0[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN ||
            m_list[m_buf_idx].RefPicList1[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            m_ctrl.NumExtParam = 1;

        if (m_list[m_buf_idx + 1].RefPicList0[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN ||
            m_list[m_buf_idx + 1].RefPicList1[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            m_ctrl.NumExtParam = 1;
    }
    else
    {
        g_tsLog << "Incorrect control flags for test case...\n";
        return MFX_ERR_ABORTED;
    }

    if (m_ctrl.NumExtParam)
    {
        // if buffer is used, move forward
        m_buf_idx = (m_buf_idx + 2) % m_num_buffers;
    }
    m_c ++;

    return MFX_ERR_NONE;
}

class BitstreamChecker : public tsBitstreamProcessor, public tsParserH264AU
{
private:
    mfxI32 m_i;
    mfxU32 m_pic_struct;
    mfxVideoParam& m_par;
    const TestSuite::tc_struct & m_tc;
    mfxU32 m_IdrCnt;
public:
    BitstreamChecker(const TestSuite::tc_struct & tc, mfxVideoParam& par)
        : tsParserH264AU()
        , m_i(0)
        , m_par(par)
        , m_tc(tc)
        , m_IdrCnt(0)
    {
        set_trace_level(BS_H264_TRACE_LEVEL_REF_LIST);

        for (auto& ctrl : tc.mfx)
        {
            if (ctrl.field == &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct)
            {
                m_pic_struct = ctrl.value;
                break;
            }
        }
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

    mfxU32 GetPOC(mfxExtAVCRefLists::mfxRefPic rp, mfxU32 IdrCnt)
    {
        mfxU32 frameOrderInGOP = rp.FrameOrder - ((m_IdrCnt - 1) * m_par.mfx.GopPicSize);
        if (rp.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
            return (2 * frameOrderInGOP);
        return (2 * frameOrderInGOP + (rp.PicStruct != m_pic_struct));
    }

    mfxU32 GetFrameOrder(mfxI32 POC)
    {
        return (m_IdrCnt - 1) * m_par.mfx.GopPicSize + POC / 2; // should work for both progressive and interlace
    }
};

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxU32 checked = 0;
    SetBuffer(bs);

    nFrames *= 1 + (m_pic_struct != MFX_PICSTRUCT_PROGRESSIVE);

    while (checked++ < nFrames)
    {
        UnitType& hdr = ParseOrDie();

        for (UnitType::nalu_param* nalu = hdr.NALU; nalu < hdr.NALU + hdr.NumUnits; nalu ++)
        {
            if(nalu->nal_unit_type != 1 && nalu->nal_unit_type != 5)
                continue;

            if (nalu->nal_unit_type == 5)
                m_IdrCnt ++;

            slice_header& s = *nalu->slice_hdr;

            if (m_tc.controlFlags & TestSuite::SET_RPL_EXPLICITLY)
            {
                m_i = -1;


                for (auto& l : m_tc.rl)
                {
                    if (l.poc == s.PicOrderCnt && s.PicOrderCnt)
                    {
                        m_i = mfxI32(&l - &m_tc.rl[0]);
                        break;
                    }
                }

                if (m_i >= 0)
                {
                    mfxExtAVCRefLists rl = {};
                    for (auto& f : m_tc.rl[m_i].f)
                    {
                        if (f.field)
                            tsStruct::set(&rl, *f.field, f.value);
                    }

                    if (!s.num_ref_idx_active_override_flag)
                    {
                        s.num_ref_idx_l0_active_minus1 = s.pps_active->num_ref_idx_l0_default_active_minus1;
                        if (s.slice_type % 5 == 1)
                            s.num_ref_idx_l1_active_minus1 = s.pps_active->num_ref_idx_l1_default_active_minus1;
                    }

                    if (rl.NumRefIdxL0Active)
                    {
                        EXPECT_EQ(rl.NumRefIdxL0Active, s.num_ref_idx_l0_active_minus1+1);

                        for (mfxU16 i = 0; i <= s.num_ref_idx_l0_active_minus1; i ++)
                        {
                            EXPECT_EQ(GetPOC(rl.RefPicList0[i], m_IdrCnt), (mfxU32)s.RefPicList[0][i]->PicOrderCnt);
                        }
                    }

                    if (rl.NumRefIdxL1Active)
                    {
                        EXPECT_EQ(rl.NumRefIdxL1Active, s.num_ref_idx_l1_active_minus1+1);

                        for (mfxU16 i = 0; i <= s.num_ref_idx_l1_active_minus1; i ++)
                        {
                            EXPECT_EQ(GetPOC(rl.RefPicList1[i], m_IdrCnt), (mfxU32)s.RefPicList[1][i]->PicOrderCnt);
                        }
                    }

                    m_i ++;
                }
            } // SET_RPL_EXPLICITLY
            else if (m_tc.controlFlags & TestSuite::CALCULATE_RPL_IN_TEST)
            {
                mfxExtAVCRefLists lists[2];
                mfxU32 frameOrder = GetFrameOrder(s.PicOrderCnt);
                // clear ext buffers for ref lists
                ClearRefLists(lists[0]);
                ClearRefLists(lists[1]);
                CreateRefLists(m_par, frameOrder, m_tc.controlFlags, lists[0], lists[1]);

                if (m_par.mfx.GopRefDist == 1)
                {
                    // only progressive for now
                    if (!s.num_ref_idx_active_override_flag)
                    {
                        s.num_ref_idx_l0_active_minus1 = s.pps_active->num_ref_idx_l0_default_active_minus1;
                        if (s.slice_type % 5 == 1)
                            s.num_ref_idx_l1_active_minus1 = s.pps_active->num_ref_idx_l1_default_active_minus1;
                    }

                    if (lists[0].NumRefIdxL0Active)
                    {
                        EXPECT_EQ(lists[0].NumRefIdxL0Active, s.num_ref_idx_l0_active_minus1+1);

                        for (mfxU16 i = 0; i <= s.num_ref_idx_l0_active_minus1; i ++)
                        {
                            EXPECT_EQ(GetPOC(lists[0].RefPicList0[i], m_IdrCnt), (mfxU32)s.RefPicList[0][i]->PicOrderCnt);
                        }
                    }

                    if (lists[0].NumRefIdxL1Active)
                    {
                        EXPECT_EQ(lists[0].NumRefIdxL1Active, s.num_ref_idx_l1_active_minus1+1);

                        for (mfxU16 i = 0; i <= s.num_ref_idx_l1_active_minus1; i ++)
                        {
                            EXPECT_EQ(GetPOC(lists[0].RefPicList1[i], m_IdrCnt), (mfxU32)s.RefPicList[1][i]->PicOrderCnt);
                        }
                    }
                }
            } // CALCULATE_RPL_IN_TEST
            else
            {
                g_tsLog << "Incorrect control flags for test case...\n";
                return MFX_ERR_ABORTED;
            }
        }
    }
    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

#define PROGR MFX_PICSTRUCT_PROGRESSIVE
#define TFF   MFX_PICSTRUCT_FIELD_TFF
#define BFF   MFX_PICSTRUCT_FIELD_BFF

#define FRM MFX_PICSTRUCT_PROGRESSIVE
#define TF  MFX_PICSTRUCT_FIELD_TFF
#define BF  MFX_PICSTRUCT_FIELD_BFF

#define RPL0(idx,fo,ps) \
    {&tsStruct::mfxExtAVCRefLists.RefPicList0[idx].FrameOrder, fo}, \
    {&tsStruct::mfxExtAVCRefLists.RefPicList0[idx].PicStruct,  ps}

#define RPL1(idx,fo,ps) \
    {&tsStruct::mfxExtAVCRefLists.RefPicList1[idx].FrameOrder, fo}, \
    {&tsStruct::mfxExtAVCRefLists.RefPicList1[idx].PicStruct,  ps}

#define NRAL0(x) {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, x}
#define NRAL1(x) {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active, x}

#define MFX_PARS(ps,gps,grd,nrf,tu) {\
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, ps}, \
            {&tsStruct::mfxVideoParam.mfx.GopPicSize,          gps}, \
            {&tsStruct::mfxVideoParam.mfx.GopRefDist,          grd}, \
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame,         nrf}, \
            {&tsStruct::mfxVideoParam.mfx.TargetUsage,         tu}, \
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*0*/
        SET_RPL_EXPLICITLY, 6, 1,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 0, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 4,  {NRAL0(1), RPL0(0, 1, FRM)} },
            { 6,  {NRAL0(3), RPL0(0, 0, FRM), RPL0(1, 2, FRM), RPL0(2, 1, FRM)}},
            { 10, {NRAL0(2), RPL0(0, 2, FRM), RPL0(1, 4, FRM)}}
        }
    },
    {/*1*/
        SET_RPL_EXPLICITLY, 10, 2,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 2,  {NRAL0(1), RPL0(0, 0, FRM), NRAL1(1), RPL1(0, 4, FRM)}},
            { 10, {NRAL0(1), RPL0(0, 2, FRM), NRAL1(1), RPL1(0, 8, FRM)}},
        }
    },
    {/*2*/
        SET_RPL_EXPLICITLY, 4, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            {4,  {NRAL0(3), RPL0(0, 0, TF), RPL0(1, 0, BF), RPL0(2, 1, TF)}},
            {5,  {NRAL0(2), RPL0(0, 1, BF), RPL0(1, 0, TF)}},
            {6,  {NRAL0(3), RPL0(0, 0, BF), RPL0(1, 0, TF), RPL0(2, 1, BF)}},
            {7,  {NRAL0(2), RPL0(0, 2, BF), RPL0(1, 2, TF)}}
        }
    },
    {/*3*/
        SET_RPL_EXPLICITLY, 10, 2,
        MFX_PARS(/*PicStruct*/ BFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            {10, {NRAL0(2), RPL0(0, 4, BF), RPL0(1, 2, TF), NRAL1(1), RPL1(0, 8, BF)}},
            {11, {NRAL0(1), RPL0(0, 4, TF), NRAL1(2), RPL1(0, 6, BF), RPL1(1, 8, TF)}},
        }
    },
    {/*4*/
        CALCULATE_RPL_IN_TEST | REORDER_FOm2, 40, 1,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 9, /*TU*/ 1),
    },
    {/*5*/
        CALCULATE_RPL_IN_TEST | REORDER_FOm2, 40, 1,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 4),
    },
    {/*6*/
        CALCULATE_RPL_IN_TEST | REORDER_FOm2, 40, 1,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 2, /*TU*/ 7),
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxU32 num_mb = mfxU32(m_par.mfx.FrameInfo.Width / 16) * mfxU32(m_par.mfx.FrameInfo.Height / 16);

    ///////////////////////////////////////////////////////////////////////////
    /// FEI buffers (start)
    ///////////////////////////////////////////////////////////////////////////
    // Attach input structures
    std::vector<mfxExtBuffer*> in_buffs;

    mfxExtFeiEncFrameCtrl in_efc = {};
    in_efc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
    in_efc.Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
    in_efc.SearchPath = 57;
    in_efc.LenSP = 57;
    in_efc.SubMBPartMask = 0x77;
    in_efc.MultiPredL0 = 0;
    in_efc.MultiPredL1 = 0;
    in_efc.SubPelMode = 3;
    in_efc.InterSAD = 2;
    in_efc.IntraSAD = 2;
    in_efc.DistortionType = 2;
    in_efc.RepartitionCheckEnable = 0;
    in_efc.AdaptiveSearch = 1;
    in_efc.MVPredictor = 0;
    in_efc.NumMVPredictors[0] = 1; //always 4 predictors
    in_efc.PerMBQp = 0;
    in_efc.PerMBInput = 0;
    in_efc.MBSizeCtrl = 0;
    in_efc.RefHeight = 40;
    in_efc.RefWidth = 48;
    in_efc.SearchWindow = 0;
    in_buffs.push_back((mfxExtBuffer*)&in_efc);

    mfxExtFeiEncMVPredictors in_mvp = {};
    in_mvp.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
    in_mvp.Header.BufferSz = sizeof(mfxExtFeiEncMVPredictors);
    in_mvp.MB = new mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB[num_mb];
    in_buffs.push_back((mfxExtBuffer*)&in_mvp);
    in_efc.MVPredictor = 1;
    in_efc.NumMVPredictors[0] = 1;

    mfxExtFeiEncMBCtrl in_mbc = {};
    in_mbc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
    in_mbc.Header.BufferSz = sizeof(mfxExtFeiEncMBCtrl);
    in_mbc.MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[num_mb];
    in_buffs.push_back((mfxExtBuffer*)&in_mbc);
    in_efc.PerMBInput = 1;

    if (!in_buffs.empty())
    {
        m_ctrl.ExtParam = &in_buffs[0];
        m_ctrl.NumExtParam = (mfxU16)in_buffs.size();
    }
    ///////////////////////////////////////////////////////////////////////////
    /// FEI buffers (end)
    ///////////////////////////////////////////////////////////////////////////

    m_par.AsyncDepth   = 1;
    m_par.mfx.NumSlice = 1;
    m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    for (auto& ctrl : tc.mfx)
        if (ctrl.field)
            tsStruct::set(m_pPar, *ctrl.field, ctrl.value);

    ((mfxExtCodingOption2&)m_par).BRefType = tc.BRef;

    SFiller sf(tc, *m_pCtrl, *m_pPar);
    BitstreamChecker c(tc, *m_pPar);
    m_filler = &sf;
    m_bs_processor = &c;

    EncodeFrames(tc.nFrames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_advanced_ref_lists);
}
