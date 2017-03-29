/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

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
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"

// This test verified the ref list reordering in slice header;
// There is also quality check (PSNR) to verify if there is artifacts in the encoded stream.

// Swith "DEBUG" on to have encoded stream stored in file, we may manually verify if there is
// quality issue in encoded stream.
//#define DEBUG


namespace fei_encode_advanced_ref_lists
{

typedef std::vector<mfxExtBuffer*> InBuf;

// This threshold is the number I tried out. If there is
// suggested data to use, please update it.
const mfxF64 PSNR_THRESHOLD = 36.0;

class Verifier : public tsSurfaceProcessor
{
public:
    tsRawReader* m_reader;
    mfxFrameSurface1 m_ref;
    mfxU32 m_frames;

    Verifier(mfxFrameSurface1& ref, mfxU16 nFrames, const char* sn)
        : tsSurfaceProcessor()
        , m_reader(0)
    {
        m_reader = new tsRawReader(sn, ref.Info, nFrames);
        m_max = nFrames;
        m_ref = ref;
        m_frames = 0;
    }

    ~Verifier()
    {
        if (m_reader)
        {
            delete m_reader;
            m_reader = NULL;
        }
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        m_reader->ProcessSurface(m_ref);

        tsFrame ref_frame(m_ref);
        tsFrame s_frame(s);
        mfxF64 psnr_y = PSNR(ref_frame, s_frame, 0);
        mfxF64 psnr_u = PSNR(ref_frame, s_frame, 1);
        mfxF64 psnr_v = PSNR(ref_frame, s_frame, 2);
        if ((psnr_y < PSNR_THRESHOLD) ||
            (psnr_u < PSNR_THRESHOLD) ||
            (psnr_v < PSNR_THRESHOLD))
        {
            g_tsLog << "ERROR: Low PSNR on frame " << m_frames << "\n";
            return MFX_ERR_ABORTED;
        }
        m_frames++;
        return MFX_ERR_NONE;
    }
};

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true) {}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
    static const mfxU16 max_rl = 20;
    static const mfxU16 max_fn = 60; // encode 60 frames at most;
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
        struct _ps{
            mfxU16 fo; //frame order
            mfxU16 ps;
        }ps[max_fn]; //pic struct specified per frame, to support PAFF
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
    mfxU32 m_c; // frame order
    mfxU32 m_i; // index to rl
    mfxU32 m_buf_idx; // index to buf sets
    mfxU32 m_num_buffer_sets;
    mfxU32 m_pic_struct; // pic struct specified during initialization
    mfxVideoParam& m_par;
    mfxEncodeCtrl& m_ctrl;
    const TestSuite::tc_struct & m_tc;
    std::vector<mfxExtAVCRefLists> m_ref_list;
    std::vector<mfxExtFeiEncFrameCtrl> m_frm_ctl_list;
    std::vector<InBuf> m_InBufs;
    std::vector<mfxU16> m_ps_frm; // pic struct specified per frame, to support PAFF
    tsRawReader* m_reader;

public:
    SFiller(const TestSuite::tc_struct & tc, mfxEncodeCtrl& ctrl, mfxVideoParam& par, const char *sn)
        : m_c(0)
        , m_i(0)
        , m_buf_idx(0)
        , m_par(par)
        , m_ctrl(ctrl)
        , m_tc(tc)
        , m_reader(0)
    {
        m_reader = new tsRawReader(sn, m_par.mfx.FrameInfo, tc.nFrames);

        // set default value for mfxExtAVCRefLists, mfxExtFeiEncFrameCtrl
        mfxExtAVCRefLists     rlb = {{MFX_EXTBUFF_AVC_REFLISTS, sizeof(mfxExtAVCRefLists)}, };
        mfxExtFeiEncFrameCtrl fcb = {{MFX_EXTBUFF_FEI_ENC_CTRL, sizeof(mfxExtFeiEncFrameCtrl)}, };
        fcb.SearchPath = 0;
        fcb.LenSP = 57;
        fcb.SubMBPartMask = 0;
        fcb.MultiPredL0 = 0;
        fcb.MultiPredL1 = 0;
        fcb.SubPelMode = 3;
        fcb.InterSAD = 2;
        fcb.IntraSAD = 2;
        fcb.DistortionType = 0;
        fcb.RepartitionCheckEnable = 0;
        fcb.AdaptiveSearch = 0;
        fcb.MVPredictor = 0;
        fcb.NumMVPredictors[0] = 0;
        fcb.NumMVPredictors[1] = 0;
        fcb.PerMBQp = 0;
        fcb.PerMBInput = 0;
        fcb.MBSizeCtrl = 0;
        fcb.RefHeight = 32;
        fcb.RefWidth = 32;
        fcb.SearchWindow = 5;

        m_num_buffer_sets = tc.nFrames;
        // 2 - paired ext buf for interlaced frame
        m_ref_list.resize(2 * m_num_buffer_sets, rlb);
        m_frm_ctl_list.resize(2 * m_num_buffer_sets, fcb);

        for (auto& ctrl : tc.mfx)
        {
            if (ctrl.field == &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct)
            {
                m_pic_struct = ctrl.value;
                break;
            }
        }

        m_max = tc.nFrames;

        // specify pic struct per frame. For MIXED, set default value "PROGRESSIVE" which by default
        // may no be explicitely specified in cases
        if (m_pic_struct == MFX_PICSTRUCT_UNKNOWN)
        {
            m_ps_frm.resize(tc.nFrames, MFX_PICSTRUCT_PROGRESSIVE);

            // specify the pic struct per frame
            for (auto& set : tc.ps)
            {
                if (set.ps > MFX_PICSTRUCT_UNKNOWN) // MFX_PICSTRUCT_UNKNOWN is invalid entry
                {
                    m_ps_frm[set.fo] = set.ps;
                }
            }
        }
        else
        {
            m_ps_frm.resize(tc.nFrames, m_pic_struct);
        }
    };

    ~SFiller()
    {
        if (m_reader)
        {
            delete m_reader;
            m_reader = NULL;
        }
    };

    mfxStatus ProcessSurface(mfxFrameSurface1& s);
};

 mfxStatus SFiller::ProcessSurface(mfxFrameSurface1& s)
 {
    mfxU16 pic_struct = m_ps_frm[m_c];
    bool field = (pic_struct == MFX_PICSTRUCT_FIELD_TFF || pic_struct == MFX_PICSTRUCT_FIELD_BFF);
    bool bff   = (pic_struct == MFX_PICSTRUCT_FIELD_BFF);

    mfxU8 ffid = !(pic_struct & MFX_PICSTRUCT_PROGRESSIVE) && (pic_struct & MFX_PICSTRUCT_FIELD_BFF);

    // PAFF requires per-frame setting of s.Info.PicStruct
    s.Info.PicStruct = pic_struct;
    s.Data.FrameOrder = m_c;
    memset(&m_ctrl, 0, sizeof(m_ctrl));

    // clear ext buffers for ref lists
    ClearRefLists(m_ref_list[m_buf_idx * 2]);
    ClearRefLists(m_ref_list[m_buf_idx * 2 + 1]);

    InBuf in_buffs;

    // attach mfxExtFeiEncFrameCtrl
    in_buffs.push_back((mfxExtBuffer *)&m_frm_ctl_list[m_buf_idx * 2 + 0]);
    if (field)
        in_buffs.push_back((mfxExtBuffer *)&m_frm_ctl_list[m_buf_idx * 2 + 1]);

    if (m_tc.controlFlags & TestSuite::SET_RPL_EXPLICITLY)
    {
        // indicates if there is user-defined ref list
        bool bAtt = false;

        // get ref lists from description of test cases
        if (m_c * 2 == m_tc.rl[m_i].poc)
        {
            for (auto& f : m_tc.rl[m_i].f)
            {
                if (f.field)
                {
                    tsStruct::set(&m_ref_list[m_buf_idx * 2 + ffid], *f.field, f.value);
                    bAtt = true;
                }
            }
            m_i ++;
        }

        if (m_c * 2 + 1 == m_tc.rl[m_i].poc)
        {
            for (auto& f : m_tc.rl[m_i].f)
            {
                if (f.field)
                {
                    tsStruct::set(&m_ref_list[m_buf_idx * 2 + 1 - ffid], *f.field, f.value);
                    bAtt = true;
                }
            }
            m_i ++;
        }

        if (bAtt)
        {
            in_buffs.push_back((mfxExtBuffer *)&m_ref_list[m_buf_idx * 2 + 0]);
            if (field)
            {
                in_buffs.push_back((mfxExtBuffer *)&m_ref_list[m_buf_idx * 2 + 1]);
            }
        }
    }
    else if (m_tc.controlFlags & TestSuite::CALCULATE_RPL_IN_TEST)
    {
        // calculate ref lists using requested logic
        CreateRefLists(m_par, s.Data.FrameOrder, m_tc.controlFlags, m_ref_list[m_buf_idx * 2], m_ref_list[m_buf_idx * 2 + 1]);
        if (m_ref_list[m_buf_idx * 2].RefPicList0[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN ||
            m_ref_list[m_buf_idx * 2].RefPicList1[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            in_buffs.push_back((mfxExtBuffer *)&m_ref_list[m_buf_idx * 2]);

        if (m_ref_list[m_buf_idx * 2 + 1].RefPicList0[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN ||
            m_ref_list[m_buf_idx * 2 + 1].RefPicList1[0].FrameOrder != (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            in_buffs.push_back((mfxExtBuffer *)&m_ref_list[m_buf_idx * 2 + 1]);
    }
    else
    {
        g_tsLog << "Incorrect control flags for test case...\n";
        return MFX_ERR_ABORTED;
    }

    m_InBufs.push_back(in_buffs);
    m_ctrl.ExtParam = &(m_InBufs[m_c])[0];
    m_ctrl.NumExtParam = (mfxU16)in_buffs.size();

    if (m_ctrl.NumExtParam)
    {
        // if buffer is used, move forward
        m_buf_idx = (m_buf_idx + 1) % m_num_buffer_sets;
    }

    m_c ++;

    m_reader->ProcessSurface(s);
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
    std::vector<mfxU16> m_ps_frm; // pic struct specified per frame, to support PAFF
#ifdef DEBUG
    tsBitstreamWriter m_writer;
#endif
public:
    mfxU8  *m_buf;
    mfxU32 m_len;    //data length in m_buf
    mfxU32 m_buf_sz; //buf size

    BitstreamChecker(const TestSuite::tc_struct & tc, mfxVideoParam& par)
        : tsParserH264AU()
        , m_i(0)
        , m_par(par)
        , m_tc(tc)
        , m_IdrCnt(0)
        , m_len(0)
#ifdef DEBUG
        , m_writer("/tmp/debug.264")
#endif
    {
        set_trace_level(BS_H264_TRACE_LEVEL_REF_LIST);

        m_buf_sz = m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 4 * tc.nFrames;
        m_buf = new mfxU8[m_buf_sz];

        for (auto& ctrl : tc.mfx)
        {
            if (ctrl.field == &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct)
            {
                m_pic_struct = ctrl.value;
                break;
            }
        }

        // specify pic struct per frame. For MIXED, set default value of PROGRESSIVE
        if (m_pic_struct == MFX_PICSTRUCT_UNKNOWN)
        {
            m_ps_frm.resize(tc.nFrames, MFX_PICSTRUCT_PROGRESSIVE);

            // specify the pic struct per frame
            for (auto& set : tc.ps)
            {
                if (set.ps > MFX_PICSTRUCT_UNKNOWN) // MFX_PICSTRUCT_UNKNOWN is invalid entry
                {
                    m_ps_frm[set.fo] = set.ps;
                }
            }
        }
        else
        {
            m_ps_frm.resize(tc.nFrames, m_pic_struct);
        }
    }

    ~BitstreamChecker()
    {
        if(m_buf)
        {
            delete [] m_buf;
            m_buf = NULL;
        }
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

    mfxU32 GetPOC(mfxExtAVCRefLists::mfxRefPic rp, mfxU32 IdrCnt)
    {
        mfxU32 frameOrderInGOP = rp.FrameOrder - ((m_IdrCnt - 1) * m_par.mfx.GopPicSize);
        mfxU16 pic_struct = m_ps_frm[rp.FrameOrder];
        if (rp.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || pic_struct == MFX_PICSTRUCT_PROGRESSIVE)
            return (2 * frameOrderInGOP);
        return (2 * frameOrderInGOP + (rp.PicStruct != pic_struct));
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
    memcpy(&m_buf[m_len], bs.Data + bs.DataOffset, bs.DataLength - bs.DataOffset);
    m_len += bs.DataLength - bs.DataOffset;

    nFrames *= 1 + !(bs.PicStruct & MFX_PICSTRUCT_PROGRESSIVE);
#ifdef DEBUG
    m_writer.ProcessBitstream(bs, nFrames);
#endif

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
#define MIXED MFX_PICSTRUCT_UNKNOWN

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
    // Added cases below for arbitrary field polarity ref order
    {/*7, TFF, no B*/
        SET_RPL_EXPLICITLY, 2, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 30, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 1,  {NRAL0(1), RPL0(0, 0, TF)} },
            { 2,  {NRAL0(2), RPL0(0, 0, BF), RPL0(1, 0, TF)}},
            { 3,  {NRAL0(3), RPL0(0, 1, TF), RPL0(1, 0, TF), RPL0(2, 0, BF)}}
        }
    },
    {/*8, BFF, B, no B-Pyramid, simulate telecine_1920x1080_BDWTU4_20_BFF.264 by Ryan*/
        SET_RPL_EXPLICITLY, 7, 1,
        MFX_PARS(/*PicStruct*/ BFF, /*GopPicSize*/ 30, /*GopRefDist*/ 3, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 1,  {NRAL0(1), RPL0(0, 0, BF)} },
            { 2,  {NRAL0(4), RPL0(0, 0, TF), RPL0(1, 0, BF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 3, BF), RPL1(1, 3, TF)}},
            { 3,  {NRAL0(4), RPL0(0, 0, TF), RPL0(1, 0, BF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 3, BF), RPL1(1, 3, TF)}},
            { 4,  {NRAL0(4), RPL0(0, 0, TF), RPL0(1, 0, BF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 3, BF), RPL1(1, 3, TF)}},
            { 5,  {NRAL0(4), RPL0(0, 0, TF), RPL0(1, 0, BF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 3, BF), RPL1(1, 3, TF)}},
            { 6,  {NRAL0(2), RPL0(0, 0, TF), RPL0(1, 0, BF)}},
            { 7,  {NRAL0(3), RPL0(0, 3, BF), RPL0(1, 0, TF), RPL0(2, 0, BF)}},
            { 8,  {NRAL0(4), RPL0(0, 3, TF), RPL0(1, 3, BF), RPL0(2, 0, TF), RPL0(3, 0, BF), NRAL1(2), RPL1(0, 6, BF), RPL1(1, 6, TF)}},
            { 9,  {NRAL0(4), RPL0(0, 3, TF), RPL0(1, 3, BF), RPL0(2, 0, TF), RPL0(3, 0, BF), NRAL1(2), RPL1(0, 6, BF), RPL1(1, 6, TF)}},
            {10,  {NRAL0(4), RPL0(0, 3, TF), RPL0(1, 3, BF), RPL0(2, 0, TF), RPL0(3, 0, BF), NRAL1(2), RPL1(0, 6, BF), RPL1(1, 6, TF)}},
            {11,  {NRAL0(4), RPL0(0, 3, TF), RPL0(1, 3, BF), RPL0(2, 0, TF), RPL0(3, 0, BF), NRAL1(2), RPL1(0, 6, BF), RPL1(1, 6, TF)}},
            {12,  {NRAL0(4), RPL0(0, 3, TF), RPL0(1, 3, BF), RPL0(2, 0, TF), RPL0(3, 0, BF)}},
            {13,  {NRAL0(4), RPL0(0, 6, BF), RPL0(1, 3, TF), RPL0(2, 3, BF), RPL0(3, 0, TF)}},
        }
    },
    {/*9, TFF, B, no B-Pyramid, simulate telecine_1920x1080_BDWTU4_20_TFF.264 by Ryan*/
        SET_RPL_EXPLICITLY, 7, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 30, /*GopRefDist*/ 3, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 1,  {NRAL0(1), RPL0(0, 0, TF)} },
            { 2,  {NRAL0(4), RPL0(0, 0, BF), RPL0(1, 0, TF), RPL0(2, 3, TF), RPL0(3, 3, BF), NRAL1(2), RPL1(0, 3, TF), RPL1(1, 3, BF)}},
            { 3,  {NRAL0(4), RPL0(0, 0, BF), RPL0(1, 0, TF), RPL0(2, 3, TF), RPL0(3, 3, BF), NRAL1(2), RPL1(0, 3, TF), RPL1(1, 3, BF)}},
            { 4,  {NRAL0(4), RPL0(0, 0, BF), RPL0(1, 0, TF), RPL0(2, 3, TF), RPL0(3, 3, BF), NRAL1(2), RPL1(0, 3, TF), RPL1(1, 3, BF)}},
            { 5,  {NRAL0(4), RPL0(0, 0, BF), RPL0(1, 0, TF), RPL0(2, 3, TF), RPL0(3, 3, BF), NRAL1(2), RPL1(0, 3, TF), RPL1(1, 3, BF)}},
            { 6,  {NRAL0(2), RPL0(0, 0, BF), RPL0(1, 0, TF)}},
            { 7,  {NRAL0(3), RPL0(0, 3, TF), RPL0(1, 0, BF), RPL0(2, 0, TF)}},
            { 8,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            { 9,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {10,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {11,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {12,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF)}},
            {13,  {NRAL0(4), RPL0(0, 6, TF), RPL0(1, 3, BF), RPL0(2, 3, TF), RPL0(3, 0, BF)}},
        }
    },
    {/*10, BFF, B-Pyramid*/
        SET_RPL_EXPLICITLY, 10, 2,
        MFX_PARS(/*PicStruct*/ BFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            {10, {NRAL0(4), RPL0(0, 2, TF), RPL0(1, 2, BF), RPL0(2, 4, TF), RPL0(3, 4, BF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {11, {NRAL0(4), RPL0(0, 2, BF), RPL0(1, 2, TF), RPL0(2, 4, BF), RPL0(3, 4, TF), NRAL1(2), RPL1(0, 6, BF), RPL1(1, 6, TF)}},
        }
    },
    {/*11, PAFF, no B*/
        SET_RPL_EXPLICITLY, 2, 1,
        MFX_PARS(/*PicStruct*/ MIXED, /*GopPicSize*/ 30, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 2,  {NRAL0(1), RPL0(0, 0, TF)}},
            { 3,  {NRAL0(2), RPL0(0, 1, TF), RPL0(1, 0, BF)}}
        },
        {
            { 0, FRM},
            { 1, TFF}
        }
    },
    {/*12, PAFF, no B, simulate telecine_1920x1080_BDWTU4_20_PAFF.264*/
        SET_RPL_EXPLICITLY, 10, 1,
        MFX_PARS(/*PicStruct*/ MIXED, /*GopPicSize*/ 30, /*GopRefDist*/ 3, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 8,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            { 9,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {10,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {11,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {12,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF)}},
            {13,  {NRAL0(4), RPL0(0, 6, TF), RPL0(1, 3, BF), RPL0(2, 3, TF), RPL0(3, 0, BF)}},
            {16,  {NRAL0(4), RPL0(0, 6, BF), RPL0(1, 6, TF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 9, TF), RPL1(1, 9, BF)}},
            {17,  {NRAL0(4), RPL0(0, 6, BF), RPL0(1, 6, TF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 9, TF), RPL1(1, 9, BF)}},
        },
        {
            { 3, TFF},
            { 4, BFF},
            { 5, BFF},
            { 6, TFF},
            { 8, TFF},
        }
    },
    {/*13, one variable to #11, the first frame be interlaced*/
        SET_RPL_EXPLICITLY, 10, 1,
        MFX_PARS(/*PicStruct*/ MIXED, /*GopPicSize*/ 30, /*GopRefDist*/ 3, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 8,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            { 9,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {10,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {11,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF), NRAL1(2), RPL1(0, 6, TF), RPL1(1, 6, BF)}},
            {12,  {NRAL0(4), RPL0(0, 3, BF), RPL0(1, 3, TF), RPL0(2, 0, BF), RPL0(3, 0, TF)}},
            {13,  {NRAL0(4), RPL0(0, 6, TF), RPL0(1, 3, BF), RPL0(2, 3, TF), RPL0(3, 0, BF)}},
            {16,  {NRAL0(4), RPL0(0, 6, BF), RPL0(1, 6, TF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 9, TF), RPL1(1, 9, BF)}},
            {17,  {NRAL0(4), RPL0(0, 6, BF), RPL0(1, 6, TF), RPL0(2, 3, BF), RPL0(3, 3, TF), NRAL1(2), RPL1(0, 9, TF), RPL1(1, 9, BF)}},
        },
        {
            { 0, TFF},
            { 4, BFF},
            { 5, BFF},
            { 6, TFF},
            { 8, TFF},
        }
    },
    {/*14, one case for existing features*/
        CALCULATE_RPL_IN_TEST | REORDER_FOm1, 2, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 30, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0),
    },
    {/*15, used as benchmark, similar to #7, but no arbitrary ref order specified*/
        SET_RPL_EXPLICITLY, 2, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 30, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0),
        {
            { 1,  {NRAL0(1), RPL0(0, 0, TF)} },
            { 2,  {NRAL0(2), RPL0(0, 0, TF), RPL0(1, 0, BF)}},
            { 3,  {NRAL0(3), RPL0(0, 0, BF), RPL0(1, 1, TF), RPL0(2, 0, TF)}}
        }
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];

    m_par.AsyncDepth   = 1;
    m_par.mfx.NumSlice = 1;
    m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    for (auto& ctrl : tc.mfx)
        if (ctrl.field)
            tsStruct::set(m_pPar, *ctrl.field, ctrl.value);

    ((mfxExtCodingOption2&)m_par).BRefType = tc.BRef;

    const char* sn = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
    g_tsStreamPool.Reg();

    m_par.mfx.FrameInfo.Width     = 352;
    m_par.mfx.FrameInfo.Height    = 288;
    m_par.mfx.FrameInfo.CropW     = 352;
    m_par.mfx.FrameInfo.CropH     = 288;

    SFiller sf(tc, *m_pCtrl, *m_pPar, sn);
    BitstreamChecker c(tc, *m_pPar);
    m_filler = &sf;
    m_bs_processor = &c;

    EncodeFrames(tc.nFrames);

    // to check the quality of encoded stream
    tsVideoDecoder dec(MFX_CODEC_AVC);

    mfxBitstream bs;
    memset(&bs, 0, sizeof(bs));
    bs.Data = c.m_buf;
    bs.DataLength = c.m_len;
    bs.MaxLength = c.m_buf_sz;
    tsBitstreamReader reader(bs, c.m_buf_sz);
    dec.m_bs_processor = &reader;

    // Borrow one surface from encoder which is already done execution.
    // This surface will be used in verifier only.
    mfxFrameSurface1 *ref = GetSurface();
    Verifier v(*ref, tc.nFrames, sn);
    dec.m_surf_processor = &v;

    dec.Init();
    dec.AllocSurfaces();
    dec.DecodeFrames(tc.nFrames);
    dec.Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_advanced_ref_lists);
}
