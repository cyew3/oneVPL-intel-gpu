#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace avce_advanced_ref_lists
{

#define FRAME_WIDTH         720
#define FRAME_HEIGHT        576

#define MAX_REF_LIST_COUNT  12
#define MAX_MFX_PARAM_COUNT 6

enum
{
    SET_RPL_EXPLICITLY    = 0x0001,
    CALCULATE_RPL_IN_TEST = 0x0002,
    REORDER_TO_1st_PLACE  = 0x0004, // reorder frame with FrameOrder - X to 1st place in ref list
};


typedef struct{
    const tsStruct::Field* field;
    mfxU32 value;
} ctrl;

typedef struct
{
    mfxU32 controlFlags;
    mfxU16 nFrames;
    mfxU16 BRef;
    ctrl mfx[MAX_MFX_PARAM_COUNT];
    struct _rl{
        mfxU16 poc;
        ctrl f[18];

    }rl[MAX_REF_LIST_COUNT];
} tc_struct;

typedef struct {
    mfxU16 caseNum;
    mfxU16 subCaseNum;
    struct {
        mfxU16 reorder;
        mfxU16 numrRefIdxActive;
    } reorderListL[2];
} ReorderConfig;

 mfxU16 GetMaxNumRefActivePL0(mfxU32 tu)
{
    mfxU16 const DEFAULT_BY_TU[] = { 0, 2, 2, 2, 2, 2, 1, 1 };
    return DEFAULT_BY_TU[tu];
}

mfxU16 GetMaxNumRefActiveBL0(mfxU32 tu)
{
    mfxU16 const DEFAULT_BY_TU[] = { 0, 4, 4, 3, 2, 2, 1, 1 };
    return DEFAULT_BY_TU[tu];
}

mfxU16 GetMaxNumRefActiveBL1(mfxU32 tu)
{
    mfxU16 const DEFAULT_BY_TU[] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    return DEFAULT_BY_TU[tu];
}


mfxStatus CreateRefLists(mfxVideoParam& par, mfxU32 frameOrder, mfxU16 frameType, ReorderConfig conf, mfxExtAVCRefLists& refList, mfxU32 fieldOrder, std::vector<mfxU32>& dpbFrameOrders)
{
    frameOrder;
    mfxU16& ps = par.mfx.FrameInfo.PicStruct;
    bool field = (ps == MFX_PICSTRUCT_FIELD_TFF || ps == MFX_PICSTRUCT_FIELD_BFF);
    bool bff   = (ps == MFX_PICSTRUCT_FIELD_BFF);

    if (!field && fieldOrder)
        return MFX_ERR_NONE;

    bool isBFrame = !!(frameType & MFX_FRAMETYPE_B);

    mfxU16 maxSupportedL0Refs = isBFrame ?
        GetMaxNumRefActiveBL0(par.mfx.TargetUsage) : GetMaxNumRefActivePL0(par.mfx.TargetUsage);
    mfxU16 maxSupportedL1Refs = GetMaxNumRefActiveBL1(par.mfx.TargetUsage);

    if ((frameType & MFX_FRAMETYPE_I) == 0)
    {
        // not an IDR-frame - ref lists can be applied
        if (!field)
        {
            // progressive encoding
            refList.NumRefIdxL0Active = IPP_MIN(conf.reorderListL[0].numrRefIdxActive, IPP_MIN(maxSupportedL0Refs, dpbFrameOrders.size()));
            if (conf.reorderListL[0].reorder < dpbFrameOrders.size())
            {
                refList.RefPicList0[0].PicStruct = ps;
                refList.RefPicList0[0].FrameOrder = dpbFrameOrders[dpbFrameOrders.size() - 1 - conf.reorderListL[0].reorder];
            }
            if (isBFrame)
            {
                refList.NumRefIdxL1Active = IPP_MIN(conf.reorderListL[1].numrRefIdxActive, IPP_MIN(maxSupportedL1Refs, dpbFrameOrders.size()));
                if (conf.reorderListL[1].reorder < dpbFrameOrders.size())
                {
                    refList.RefPicList1[0].PicStruct = ps;
                    refList.RefPicList1[0].FrameOrder = dpbFrameOrders[dpbFrameOrders.size() - 1 - conf.reorderListL[1].reorder];
                }
            }
        }
        else
        {
            if (fieldOrder == 0)
            {
                // first field in the pair
                refList.NumRefIdxL0Active = IPP_MIN(conf.reorderListL[0].numrRefIdxActive, IPP_MIN(maxSupportedL0Refs, dpbFrameOrders.size() * 2));
            }
            else
            {
                // second field in the pair
                refList.NumRefIdxL0Active = IPP_MIN(conf.reorderListL[0].numrRefIdxActive, IPP_MIN(maxSupportedL0Refs, dpbFrameOrders.size() * 2 - 1));
            }
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

bool SomethingInRefLists(mfxExtAVCRefLists& lists)
{
    bool isThereSomething = false;

    if (lists.NumRefIdxL0Active || lists.NumRefIdxL1Active ||
        lists.RefPicList0[0].FrameOrder != MFX_FRAMEORDER_UNKNOWN ||
        lists.RefPicList1[0].FrameOrder != MFX_FRAMEORDER_UNKNOWN)
        isThereSomething = true;

    return isThereSomething;
}

mfxU8 CalcL0Entries(mfxExtAVCRefLists& lists)
{
    mfxU8 i = 0;
    while (lists.RefPicList0[i].FrameOrder !=  MFX_FRAMEORDER_UNKNOWN)
        i ++;

    return i;
}

mfxU8 CalcL1Entries(mfxExtAVCRefLists& lists)
{
    mfxU8 i = 0;
    while (lists.RefPicList1[i].FrameOrder !=  MFX_FRAMEORDER_UNKNOWN)
        i ++;

    return i;
}

// functionality for reordering
mfxU8 GetFrameType(
    mfxVideoParam const & video,
    mfxU32                frameOrder,
    bool                  isBPyramid)
{
    mfxU32 gopOptFlag = video.mfx.GopOptFlag;
    mfxU32 gopPicSize = video.mfx.GopPicSize;
    mfxU32 gopRefDist = video.mfx.GopRefDist;
    mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval + 1);

    if (gopPicSize == 0xffff) //infinite GOP
        idrPicDist = gopPicSize = 0xffffffff;

    if (idrPicDist && frameOrder % idrPicDist == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (!idrPicDist && frameOrder == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (frameOrder % gopPicSize == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

    if (frameOrder % gopPicSize % gopRefDist == 0)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

    if ( (gopOptFlag & MFX_GOP_STRICT) == 0 && (frameOrder + 1) % gopPicSize == 0)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    if (isBPyramid && (frameOrder % gopPicSize % gopRefDist - 1) % 2 == 1)
        return (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF);

    return (MFX_FRAMETYPE_B);
}

mfxU32 BRefOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter)
{
    mfxU32 pivot = (begin + end) / 2;
    if (displayOrder == pivot)
        return counter;
    else if (displayOrder < pivot)
        return BRefOrder(displayOrder, begin, pivot, counter + 1);
    else
        return BRefOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin);
}

struct Frame
{
    mfxFrameSurface1* surf;
    mfxU32 type;
};


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
    const tc_struct & m_tc;
    std::vector<mfxExtAVCRefLists> m_list;
    std::vector<mfxExtBuffer*> m_pbuf;
    ReorderConfig m_reorderConfig;
    tsRawReader m_raw_reader;

    tsSurfacePool& m_spool;
    bool m_isBPyramid;
    std::vector<Frame> m_dpb;
    std::vector<Frame> m_queue;
    std::vector<mfxU32> m_dpbFrameOrders;

    typedef std::vector<Frame>::iterator FrameIterator;

    bool FindL1(mfxU32 order)
    {
        for (FrameIterator it = m_dpb.begin(); it < m_dpb.end(); it++)
            if (it->surf->Data.FrameOrder > order)
                return true;
        return false;
    }

    FrameIterator Reorder(bool flush)
    {
        FrameIterator begin = m_queue.begin();
        FrameIterator end = m_queue.end();
        FrameIterator top = begin;
        FrameIterator b0 = end; // 1st non-ref B with L1 > 0
        std::vector<FrameIterator> brefs;

        while (top != end && (top->type & MFX_FRAMETYPE_B))
        {
            if (FindL1(top->surf->Data.FrameOrder))
            {
                if (m_isBPyramid)
                    brefs.push_back(top);
                else if (b0 == end)
                    b0 = top;
            }

            top++;
        }

        if (!brefs.empty())
        {
            mfxU32 idx = 0;
            mfxU32 prevMG = brefs[0]->surf->Data.FrameOrder % m_par.mfx.GopPicSize / m_par.mfx.GopRefDist;

            for (mfxU32 i = 1; i < brefs.size(); i++)
            {
                mfxU32 mg = brefs[i]->surf->Data.FrameOrder % m_par.mfx.GopPicSize / m_par.mfx.GopRefDist;

                if (mg != prevMG)
                    break;

                prevMG = mg;

                mfxU32 order0 = brefs[i]->surf->Data.FrameOrder   % m_par.mfx.GopPicSize % m_par.mfx.GopRefDist - 1;
                mfxU32 order1 = brefs[idx]->surf->Data.FrameOrder % m_par.mfx.GopPicSize % m_par.mfx.GopRefDist - 1;

                if (  BRefOrder(order0, 0, m_par.mfx.GopRefDist - 1, 0)
                    < BRefOrder(order1, 0, m_par.mfx.GopRefDist - 1, 0))
                    idx = i;
            }

            return brefs[idx];
        }

        if (b0 != end)
            return b0;

        if (flush && top == end && begin != end)
        {
            top--;
            top->type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }

        return top;
    }

public:
    SFiller(const tc_struct & tc,
        mfxEncodeCtrl& ctrl,
        mfxVideoParam& par,
        ReorderConfig conf,
        const char* fname,
        mfxFrameInfo& fi,
        mfxU32 n_frames,
        tsSurfacePool& pool)
        : m_c(0)
        , m_i(0)
        , m_buf_idx(0)
        , m_par(par)
        , m_ctrl(ctrl)
        , m_tc(tc)
        , m_reorderConfig(conf)
        , m_raw_reader(fname, fi, n_frames)
        , m_spool(pool)
        , m_isBPyramid(tc.BRef == MFX_B_REF_PYRAMID)
    {
        mfxExtAVCRefLists rlb = {{MFX_EXTBUFF_AVC_REFLISTS, sizeof(mfxExtAVCRefLists)}, };

        m_num_buffers = 2 * (m_par.AsyncDepth + m_par.mfx.GopRefDist - 1);
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

        m_cur = 0;
    };
    mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
};

bool isOlder(Frame& i, Frame& j)
{
    return i.surf->Data.FrameOrder < j.surf->Data.FrameOrder;
}

 mfxFrameSurface1* SFiller::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
 {
    bool field = (m_pic_struct == MFX_PICSTRUCT_FIELD_TFF || m_pic_struct == MFX_PICSTRUCT_FIELD_BFF);
    bool bff   = (m_pic_struct == MFX_PICSTRUCT_FIELD_BFF);

    memset(&m_ctrl, 0, sizeof(m_ctrl));
    if (m_par.mfx.EncodedOrder == 0)
    {
        // read raw surface from input file
        m_raw_reader.ProcessSurface(*ps);
        ps->Data.FrameOrder = m_c++;
    }
    else
    {
        // do frame buffering and reordering
        while (ps)
        {
            mfxStatus sts = m_raw_reader.ProcessSurface(*ps);
            Frame f = {ps, 0};

            if (f.surf)
            {
                f.surf->Data.Locked++;
                f.surf->Data.FrameOrder = m_c++;
                f.type = GetFrameType(m_par, f.surf->Data.FrameOrder, m_isBPyramid);
                m_queue.push_back(f);
            }

            FrameIterator it = Reorder(!f.surf);

            if (it != m_queue.end())
            {
                f = *it;

                if (it->type & MFX_FRAMETYPE_REF)
                {
                    m_dpb.push_back(f);

                    if (m_dpb.size() > m_par.mfx.NumRefFrame)
                    {
                        FrameIterator toRemove = m_isBPyramid ? 
                            std::min_element(m_dpb.begin(), m_dpb.end(), isOlder) : m_dpb.begin();
                        toRemove->surf->Data.Locked--;
                        m_dpb.erase(toRemove);
                    }
                }
                else
                    f.surf->Data.Locked--;

                m_queue.erase(it);

                if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
                {
                    f.type |= (f.type << 8);

                    if (f.type & MFX_FRAMETYPE_I)
                    {
                        f.type &= ~(MFX_FRAMETYPE_xI | MFX_FRAMETYPE_xIDR);
                        f.type |= MFX_FRAMETYPE_xP;
                    }
                }

                m_ctrl.FrameType = f.type;

                ps = f.surf;
                break;
            }

            if (!f.surf)
                break;

            ps = m_spool.GetSurface();
        }
    }

    // prepare and attach ext buffers with ref lists
    m_pbuf[m_buf_idx+0] = &m_list[m_buf_idx+0].Header;
    m_pbuf[m_buf_idx+1] = &m_list[m_buf_idx+1].Header;

    ClearRefLists(m_list[m_buf_idx]);
    ClearRefLists(m_list[m_buf_idx + 1]);

    m_ctrl.ExtParam = &m_pbuf[m_buf_idx];

    bool alreadyInDpb = false;

    if (m_tc.controlFlags & SET_RPL_EXPLICITLY)
    {
        // get ref lists from description of test cases
        if (ps->Data.FrameOrder * 2 == m_tc.rl[m_i].poc)
        {
            m_ctrl.NumExtParam = 1;

            for (auto& f : m_tc.rl[m_i].f)
                if (f.field)
                    tsStruct::set(m_pbuf[m_buf_idx], *f.field, f.value);
            m_i ++;
        }

        if (ps->Data.FrameOrder * 2 + 1 == m_tc.rl[m_i].poc)
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
    else if (m_tc.controlFlags & CALCULATE_RPL_IN_TEST)
    {
        mfxU16 frameType = m_par.mfx.EncodedOrder ?
            m_ctrl.FrameType :
        ps->Data.FrameOrder % m_par.mfx.GopPicSize ? MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF :
            MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;

        // calculate ref lists using requested logic
        CreateRefLists(m_par, ps->Data.FrameOrder, frameType, m_reorderConfig, m_list[m_buf_idx], 0, m_dpbFrameOrders);
        if (SomethingInRefLists(m_list[m_buf_idx]))
        {
            // ref list for 1st field (or whole frame) is specified, attach it to mfxEncodeCtrl
            m_ctrl.NumExtParam = 1;
        }

        if (field)
        {
            if ((m_ctrl.FrameType & MFX_FRAMETYPE_IDR) || m_par.mfx.EncodedOrder == 0 && (ps->Data.FrameOrder % m_par.mfx.GopPicSize == 0))
            {
                m_dpbFrameOrders.clear();
            }

            if ((m_ctrl.FrameType & MFX_FRAMETYPE_REF) || m_par.mfx.EncodedOrder == 0)
            {
                m_dpbFrameOrders.push_back(ps->Data.FrameOrder);
            }

            if (m_dpbFrameOrders.size() > m_par.mfx.NumRefFrame)
            {
                std::vector<mfxU32>::iterator toRemove = m_isBPyramid ?
                    std::min_element(m_dpbFrameOrders.begin(), m_dpbFrameOrders.end()) : m_dpbFrameOrders.begin();
                m_dpbFrameOrders.erase(toRemove);
            }

            alreadyInDpb = true;
        }

        CreateRefLists(m_par, ps->Data.FrameOrder, frameType, m_reorderConfig, m_list[m_buf_idx + 1], 1, m_dpbFrameOrders);

        if (SomethingInRefLists(m_list[m_buf_idx + 1]))
        {
            // ref list for 2nd field is specified, attach it to mfxEncodeCtrl
            m_ctrl.NumExtParam = 2;
        }
    }
    else
    {
        g_tsLog << "Incorrect control flags for test case...\n";
        return 0;
    }

    if (alreadyInDpb == false)
    {
        if ((m_ctrl.FrameType & MFX_FRAMETYPE_IDR) || m_par.mfx.EncodedOrder == 0 && (ps->Data.FrameOrder % m_par.mfx.GopPicSize == 0))
        {
            m_dpbFrameOrders.clear();
        }

        if ((m_ctrl.FrameType & MFX_FRAMETYPE_REF) || m_par.mfx.EncodedOrder == 0)
        {
            m_dpbFrameOrders.push_back(ps->Data.FrameOrder);
        }

        if (m_dpbFrameOrders.size() > m_par.mfx.NumRefFrame)
        {
            std::vector<mfxU32>::iterator toRemove = m_isBPyramid ?
                std::min_element(m_dpbFrameOrders.begin(), m_dpbFrameOrders.end()) : m_dpbFrameOrders.begin();
            m_dpbFrameOrders.erase(toRemove);
        }
    }

    if (m_ctrl.NumExtParam)
    {
        // if buffer is used, move forward
        m_buf_idx = (m_buf_idx + 2) % m_num_buffers;
    }

    return ps;
}

class BitstreamChecker : public tsBitstreamProcessor, public tsParserH264AU
{
private:
    mfxI32 m_i;
    mfxU32 m_pic_struct;
    mfxVideoParam& m_par;
    const tc_struct & m_tc;
    mfxU32 m_IdrCnt;
    ReorderConfig m_reorderConfig;
    //tsBitstreamWriter m_writer;
    std::vector<mfxU32> m_dpbFrameOrders;
    bool m_isBPyramid;
public:
    BitstreamChecker(const tc_struct & tc, mfxVideoParam& par, ReorderConfig conf)
        : tsParserH264AU()
        , m_i(0)
        , m_par(par)
        , m_tc(tc)
        , m_IdrCnt(0)
        , m_reorderConfig(conf)
        //, m_writer("debug.264")
        , m_isBPyramid(tc.BRef == MFX_B_REF_PYRAMID)
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

    mfxU32 GetPOC(mfxExtAVCRefLists::mfxRefPic rp)
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
    bool field = (m_pic_struct == MFX_PICSTRUCT_FIELD_TFF || m_pic_struct == MFX_PICSTRUCT_FIELD_BFF);
    bool bff   = (m_pic_struct == MFX_PICSTRUCT_FIELD_BFF);
    SetBuffer(bs);

    nFrames *= 1 + (m_pic_struct != MFX_PICSTRUCT_PROGRESSIVE);

    //m_writer.ProcessBitstream(bs, nFrames);

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

            m_i = -1;

            mfxU8 fieldId = 0;
            if (field && s.field_pic_flag == 0)
            {
                g_tsLog << "Coded stream doesn't comply with input encoding configuration...\n";
                return MFX_ERR_ABORTED;
            }

            if (field)
                fieldId == (bff && s.bottom_field_flag) || (!bff && s.bottom_field_flag == 0) ? 0 : 1;

            mfxExtAVCRefLists rl = {};
            // clear ext buffers for ref lists
            ClearRefLists(rl);
            if (m_tc.controlFlags & SET_RPL_EXPLICITLY)
            {

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
                    for (auto& f : m_tc.rl[m_i].f)
                    {
                        if (f.field)
                            tsStruct::set(&rl, *f.field, f.value);
                    }

                    m_i ++;
                }
            } // SET_RPL_EXPLICITLY
            else if (m_tc.controlFlags & CALCULATE_RPL_IN_TEST)
            {
                mfxU32 frameOrder = GetFrameOrder(s.PicOrderCnt);
                CreateRefLists(m_par, frameOrder, bs.FrameType, m_reorderConfig, rl, fieldId, m_dpbFrameOrders);
            } // CALCULATE_RPL_IN_TEST
            else
            {
                g_tsLog << "Incorrect control flags for test case...\n";
                return MFX_ERR_ABORTED;
            }

            g_tsLog << "\n";
            g_tsLog << "Check of bitstream for Case " << m_reorderConfig.caseNum << " Subcase " << m_reorderConfig.subCaseNum << " FrameOrder " << GetFrameOrder(s.PicOrderCnt) << "FieldId " << fieldId << " \n";
            g_tsLog << "----------------------------------------------------- \n";
            g_tsLog << "Reorder config: \n";
            g_tsLog << "L0: reorder = " << m_reorderConfig.reorderListL[0].reorder << ", numrRefIdxActive = " << m_reorderConfig.reorderListL[0].numrRefIdxActive << " \n";
            g_tsLog << "L1: reorder = " << m_reorderConfig.reorderListL[1].reorder << ", numrRefIdxActive = " << m_reorderConfig.reorderListL[1].numrRefIdxActive << " \n";
            g_tsLog << "----------------------------------------------------- \n";
            g_tsLog << "Encoding parameters: \n";
            g_tsLog << "TU = " << m_par.mfx.TargetUsage << ", GopRefDist = " << m_par.mfx.GopRefDist << ", NumRefFrame = " << m_par.mfx.NumRefFrame << ", EncodedOrder = " <<m_par.mfx.EncodedOrder <<" \n";
            g_tsLog << "----------------------------------------------------- \n";
            g_tsLog << "\n";

            if (!s.num_ref_idx_active_override_flag)
            {
                s.num_ref_idx_l0_active_minus1 = s.pps_active->num_ref_idx_l0_default_active_minus1;
                if (s.slice_type % 5 == 1)
                    s.num_ref_idx_l1_active_minus1 = s.pps_active->num_ref_idx_l1_default_active_minus1;
            }

            if (rl.NumRefIdxL0Active)
            {
                EXPECT_EQ(rl.NumRefIdxL0Active, s.num_ref_idx_l0_active_minus1+1);
            }

            if (rl.NumRefIdxL1Active)
            {
                EXPECT_EQ(rl.NumRefIdxL1Active, s.num_ref_idx_l1_active_minus1+1);
            }


            mfxU8 numL0Entries = CalcL0Entries(rl);
            mfxU8 numL1Entries = CalcL1Entries(rl);

            if (numL0Entries)
            {
                for (mfxU16 i = 0; i <= s.num_ref_idx_l0_active_minus1; i ++)
                {
                    if (rl.RefPicList0[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN)
                    {
                        EXPECT_EQ(GetPOC(rl.RefPicList0[i]), s.RefPicList[0][i]->PicOrderCnt);
                    }
                }
            }

            if (numL1Entries)
            {
                for (mfxU16 i = 0; i <= s.num_ref_idx_l1_active_minus1; i ++)
                {
                    if (rl.RefPicList1[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN)
                    {
                        EXPECT_EQ(GetPOC(rl.RefPicList1[i]), s.RefPicList[1][i]->PicOrderCnt);
                    }
                }
            }

            bool isIDR = (fieldId == 0) ? !!(bs.FrameType & MFX_FRAMETYPE_IDR) : !!(bs.FrameType & MFX_FRAMETYPE_xIDR);

            if (isIDR)
            {
                m_dpbFrameOrders.clear();
            }

            if (fieldId == 0)
            {
                if (bs.FrameType & MFX_FRAMETYPE_REF)
                {
                    m_dpbFrameOrders.push_back(GetFrameOrder(s.PicOrderCnt));
                }

                if (m_dpbFrameOrders.size() > m_par.mfx.NumRefFrame)
                {
                    std::vector<mfxU32>::iterator toRemove = m_isBPyramid ?
                        std::min_element(m_dpbFrameOrders.begin(), m_dpbFrameOrders.end()) : m_dpbFrameOrders.begin();
                    m_dpbFrameOrders.erase(toRemove);
                }
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

#define MFX_PARS(ps,gps,grd,nrf,tu, eo) {\
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, ps}, \
            {&tsStruct::mfxVideoParam.mfx.GopPicSize,          gps}, \
            {&tsStruct::mfxVideoParam.mfx.GopRefDist,          grd}, \
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame,         nrf}, \
            {&tsStruct::mfxVideoParam.mfx.TargetUsage,         tu}, \
            {&tsStruct::mfxVideoParam.mfx.EncodedOrder,        eo}, \
}

const tc_struct test_case[] =
{
    {/*0*/
        SET_RPL_EXPLICITLY, 6, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 0, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0, /*EncodedOrder*/ 0),
        {
            { 4,  {NRAL0(1), RPL0(0, 1, FRM)} },
            { 6,  {NRAL0(3), RPL0(0, 0, FRM), RPL0(1, 2, FRM), RPL0(2, 1, FRM)}},
            { 10, {NRAL0(2), RPL0(0, 2, FRM), RPL0(1, 4, FRM)}}
        }
    },
    {/*1*/
        SET_RPL_EXPLICITLY, 10, MFX_B_REF_PYRAMID,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 4, /*TU*/ 0, /*EncodedOrder*/ 0),
        {
            { 2,  {NRAL0(1), RPL0(0, 0, FRM), NRAL1(1), RPL1(0, 4, FRM)}},
            { 10, {NRAL0(1), RPL0(0, 2, FRM), NRAL1(1), RPL1(0, 8, FRM)}},
        }
    },
    {/*2*/
        SET_RPL_EXPLICITLY, 4, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 0, /*EncodedOrder*/ 0),
        {
            {4,  {NRAL0(3), RPL0(0, 0, TF), RPL0(1, 0, BF), RPL0(2, 1, TF)}},
            {5,  {NRAL0(2), RPL0(0, 1, BF), RPL0(1, 0, TF)}},
            {6,  {NRAL0(3), RPL0(0, 0, BF), RPL0(1, 0, TF), RPL0(2, 1, BF)}},
            {7,  {NRAL0(2), RPL0(0, 2, BF), RPL0(1, 2, TF)}}
        }
    },
    {/*3*/
        SET_RPL_EXPLICITLY, 10, MFX_B_REF_PYRAMID,
        MFX_PARS(/*PicStruct*/ BFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 4, /*TU*/ 0, /*EncodedOrder*/ 0),
        {
            {10, {NRAL0(2), RPL0(0, 4, BF), RPL0(1, 2, TF), NRAL1(1), RPL1(0, 8, BF)}},
            {11, {NRAL0(1), RPL0(0, 4, TF), NRAL1(2), RPL1(0, 6, BF), RPL1(1, 8, TF)}},
        }
    },
    // TU: 1; EncodedOrder: 0, 1; GopRefDist: 1, 2, 4, 8; NumRefFrame 9;
    {/*4*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 9, /*TU*/ 1, /*EncodedOrder*/ 0),
    },
    {/*5*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 9, /*TU*/ 1, /*EncodedOrder*/ 1),
    },
    {/*6*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 2, /*NumRefFrame*/ 9, /*TU*/ 1, /*EncodedOrder*/ 1),
    },
    {/*7*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 4, /*NumRefFrame*/ 9, /*TU*/ 1, /*EncodedOrder*/ 1),
    },
    {/*8*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 8, /*NumRefFrame*/ 9, /*TU*/ 1, /*EncodedOrder*/ 1),
    },
    {/*9*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_PYRAMID,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 4, /*NumRefFrame*/ 9, /*TU*/ 1, /*EncodedOrder*/ 1),
    },
    {/*10*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_PYRAMID,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 8, /*NumRefFrame*/ 9, /*TU*/ 1, /*EncodedOrder*/ 1),
    },

    // TU: 4; EncodedOrder: 0, 1; GopRefDist: 1, 2, 4, 8; NumRefFrame 4;
    {/*11*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 4, /*EncodedOrder*/ 0),
    },
    {/*12*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 4, /*TU*/ 4, /*EncodedOrder*/ 1),
    },
    {/*13*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 2, /*NumRefFrame*/ 4, /*TU*/ 4, /*EncodedOrder*/ 1),
    },
    {/*14*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 4, /*NumRefFrame*/ 4, /*TU*/ 4, /*EncodedOrder*/ 1),
    },
    {/*15*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 8, /*NumRefFrame*/ 4, /*TU*/ 4, /*EncodedOrder*/ 1),
    },
    {/*16*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_PYRAMID,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 4, /*NumRefFrame*/ 4, /*TU*/ 4, /*EncodedOrder*/ 1),
    },
    {/*17*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_PYRAMID,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 8, /*NumRefFrame*/ 4, /*TU*/ 4, /*EncodedOrder*/ 1),
    },

    // TU: 7; EncodedOrder: 0, 1; GopRefDist: 1, 2; NumRefFrame 2;
    {/*18*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 2, /*TU*/ 7, /*EncodedOrder*/ 0),
    },
    {/*19*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 1, /*NumRefFrame*/ 2, /*TU*/ 7, /*EncodedOrder*/ 1),
    },
    {/*20*/
        CALCULATE_RPL_IN_TEST | REORDER_TO_1st_PLACE, 40, MFX_B_REF_OFF,
        MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 20, /*GopRefDist*/ 2, /*NumRefFrame*/ 2, /*TU*/ 7, /*EncodedOrder*/ 1),
    },
};

#define RUN_SUBCASE(c, s) if (conf.caseNum != c || conf.subCaseNum != s) continue;
#define RUN_CASE(c) if (conf.caseNum != c) continue;

int RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxVideoParam testPar;
    memset(&testPar, 0, sizeof(mfxVideoParam));

    for (auto& ctrl : tc.mfx)
        if (ctrl.field)
            tsStruct::set(&testPar, *ctrl.field, ctrl.value);

    std::vector<ReorderConfig> configs;
    ReorderConfig conf;

    if (tc.controlFlags & SET_RPL_EXPLICITLY)
    {
        // dummy config - will not be used
        // it's required to run the loop for EncodeFrames() below
        memset(&conf, 0 , sizeof(ReorderConfig));
        conf.caseNum = id;
        configs.push_back(conf);
    }
    else if (tc.controlFlags & CALCULATE_RPL_IN_TEST)
    {
        conf.caseNum = id;
        mfxU16 subCaseNum = 0;

        // for every case test is trying many different ref list modification scenarios
        // thus every test case has many subcases
        for (mfxU8 i = 0; i < testPar.mfx.NumRefFrame; i ++)
        {
            // set values for num_ref_idx_active
            std::vector<mfxU8> numRefIdxActive;
            // don't specify num_ref_idx_active (use default)
            numRefIdxActive.push_back(0);
            // set num_ref_idx_active = 1 (limit ref list with 1 reference)
            numRefIdxActive.push_back(1);
            // set num_ref_idx_active to maximum possible value for every particular frame
            if (testPar.mfx.NumRefFrame > 1)
                numRefIdxActive.push_back(testPar.mfx.NumRefFrame - 1);

            if (tc.controlFlags & REORDER_TO_1st_PLACE)
            {
                for (mfxU8 j = 0; j < numRefIdxActive.size(); j ++)
                {
                    // for now same ref list mod syntax is applied for both L0 and L1 lists
                    conf.reorderListL[0].reorder = conf.reorderListL[1].reorder = i;
                    conf.reorderListL[0].numrRefIdxActive = conf.reorderListL[1].numrRefIdxActive = numRefIdxActive[j];
                    // specify number of sub case
                    conf.subCaseNum = subCaseNum;
                    configs.push_back(conf);
                    subCaseNum ++;
                }
            }
        }
    }

    // go through all subcases
    for (mfxU8 i = 0; i < configs.size(); i ++)
    {
        conf = configs[i];
        /*RUN_SUBCASE(4, 0);*/
        /*RUN_CASE(9);*/
        tsVideoEncoder enc(MFX_CODEC_AVC);

        enc.m_par.AsyncDepth   = 1;
        enc.m_par.mfx.NumSlice = 1;
        enc.m_par.mfx.FrameInfo.Width  = enc.m_par.mfx.FrameInfo.CropW = FRAME_WIDTH;
        enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = FRAME_HEIGHT;

        for (auto& ctrl : tc.mfx)
            if (ctrl.field)
                tsStruct::set(enc.m_pPar, *ctrl.field, ctrl.value);

        ((mfxExtCodingOption2&)enc.m_par).BRefType = tc.BRef;
        std::string inputName;
        if (enc.m_par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE)
            inputName = "/YUV/iceage_720x576_491.yuv";
        else
            inputName = "/YUV/stockholm_720x576i_252.yuv";

        SFiller sf(tc, *enc.m_pCtrl, *enc.m_pPar, conf, /*inputName.c_str()*/ g_tsStreamPool.Get(inputName.c_str()), enc.m_par.mfx.FrameInfo, tc.nFrames, enc);
        g_tsStreamPool.Reg();
        BitstreamChecker c(tc, *enc.m_pPar, conf);
        enc.m_filler = &sf;
        enc.m_bs_processor = &c;

        if (enc.m_par.mfx.EncodedOrder)
        {
            enc.QueryIOSurf();
            enc.m_request.NumFrameMin = enc.m_request.NumFrameSuggested = 
                enc.m_par.mfx.GopRefDist - 1 + enc.m_par.mfx.NumRefFrame + enc.m_par.AsyncDepth;
        }


        // for better navigation index of every case and subcase are printed in the log
        g_tsLog << "--------------------> Case: " << conf.caseNum << " subcase: " << conf.subCaseNum << "\n";
        enc.EncodeFrames(tc.nFrames);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_advanced_ref_lists, RunTest, sizeof(test_case) / sizeof(tc_struct));
}
