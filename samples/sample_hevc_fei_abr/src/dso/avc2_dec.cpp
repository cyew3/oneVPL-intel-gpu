#include "avc2_parser.h"

using namespace BS_AVC2;
using namespace BsReader2;


namespace BS_AVC2
{
bool operator== (DPBPic const & l, DPBPic const & r)
{
    return l.field == r.field && l.frame == r.frame;
}

const char sliceTypeTraceMap[10][3] =
{
    "P", "B", "I", "SP", "SI",
    "P", "B", "I", "SP", "SI"
};

const char mbTypeTraceMap[74][16] =
{
    "I_NxN",          "I_16x16_0_0_0", "I_16x16_1_0_0", "I_16x16_2_0_0",
    "I_16x16_3_0_0",  "I_16x16_0_1_0", "I_16x16_1_1_0", "I_16x16_2_1_0",
    "I_16x16_3_1_0",  "I_16x16_0_2_0", "I_16x16_1_2_0", "I_16x16_2_2_0",
    "I_16x16_3_2_0",  "I_16x16_0_0_1", "I_16x16_1_0_1", "I_16x16_2_0_1",
    "I_16x16_3_0_1",  "I_16x16_0_1_1", "I_16x16_1_1_1", "I_16x16_2_1_1",
    "I_16x16_3_1_1",  "I_16x16_0_2_1", "I_16x16_1_2_1", "I_16x16_2_2_1",
    "I_16x16_3_2_1",  "I_PCM",         "MBTYPE_SI",     "P_L0_16x16",
    "P_L0_L0_16x8",   "P_L0_L0_8x16",  "P_8x8",         "P_8x8ref0",
    "B_Direct_16x16", "B_L0_16x16",    "B_L1_16x16",    "B_Bi_16x16",
    "B_L0_L0_16x8",   "B_L0_L0_8x16",  "B_L1_L1_16x8",  "B_L1_L1_8x16",
    "B_L0_L1_16x8",   "B_L0_L1_8x16", "B_L1_L0_16x8",   "B_L1_L0_8x16",
    "B_L0_Bi_16x8",   "B_L0_Bi_8x16", "B_L1_Bi_16x8",   "B_L1_Bi_8x16",
    "B_Bi_L0_16x8",   "B_Bi_L0_8x16", "B_Bi_L1_16x8",   "B_Bi_L1_8x16",
    "B_Bi_Bi_16x8",   "B_Bi_Bi_8x16", "B_8x8",          "P_Skip",
    "B_Skip",         "P_L0_8x8",     "P_L0_8x4",       "P_L0_4x8",
    "P_L0_4x4",       "B_Direct_8x8", "B_L0_8x8",       "B_L1_8x8",
    "B_Bi_8x8",       "B_L0_8x4",     "B_L0_4x8",       "B_L1_8x4",
    "B_L1_4x8",       "B_Bi_8x4",     "B_Bi_4x8",       "B_L0_4x4",
    "B_L1_4x4",       "B_Bi_4x4"
};

const char predMode4x4TraceMap[9][20] =
{
    "Vertical",
    "Horizontal",
    "DC",
    "Diagonal_Down_Left",
    "Diagonal_Down_Right",
    "Vertical_Right",
    "Horizontal_Down",
    "Vertical_Left",
    "Horizontal_Up"
};

const char predMode8x8TraceMap[9][20] =
{
    "Vertical",
    "Horizontal",
    "DC",
    "Diagonal_Down_Left",
    "Diagonal_Down_Right",
    "Vertical_Right",
    "Horizontal_Down",
    "Vertical_Left",
    "Horizontal_Up"
};

const char predMode16x16TraceMap[4][12] =
{
    "Vertical",
    "Horizontal",
    "DC",
    "Plane",
};

const char predModeChromaTraceMap[4][12] =
{
    "DC",
    "Horizontal",
    "Vertical",
    "Plane",
};

const char mmcoTraceMap[7][12] =
{
    "END", "REMOVE_ST", "REMOVE_LT", "ST_TO_LT", "MAX_LT_IDX", "CLEAR_DPB", "CUR_TO_LT"
};

const char rplmTraceMap[6][12] =
{
    "ST_SUB", "ST_ADD", "LT", "END", "VIEW_SUB", "VIEW_ADD"
};

const Bs8u Invers4x4Scan[16] =
{
     0,  1,  4,  5,
     2,  3,  6,  7,
     8,  9, 12, 13,
    10, 11, 14, 15
};
}

const char NaluTraceMap::map[21][16] =
{
    "Unspecified",
    "SLICE_NONIDR",
    "SD_PART_A",
    "SD_PART_B",
    "SD_PART_C",
    "SLICE_IDR",
    "SEI",
    "SPS",
    "PPS",
    "AUD",
    "END_OF_SEQUENCE",
    "END_OF_STREAM",
    "FILLER_DATA",
    "SPS_EXTENSION",
    "PREFIX_NALU",
    "SUBSET_SPS",
    "DPS",
    "Reserved",
    "Reserved",
    "SLICE_AUX",
    "SLICE_EXT"
};

const char* NaluTraceMap::operator[] (Bs32u i)
{
    if (i < 21) return map[i];
    if (i < 24) return map[17];
    return map[0];
}

const char SEIPayloadTraceMap::map[51][38] =
{
    "BUFFERING_PERIOD",
    "PIC_TIMING",
    "PAN_SCAN_RECT",
    "FILLER_PAYLOAD",
    "USER_DATA_REGISTERED_ITU_T_T35",
    "USER_DATA_UNREGISTERED",
    "RECOVERY_POINT",
    "DEC_REF_PIC_MARKING_REPETITION",
    "SPARE_PIC",
    "SCENE_INFO",
    "SUB_SEQ_INFO",
    "SUB_SEQ_LAYER_CHARACTERISTICS",
    "SUB_SEQ_CHARACTERISTICS",
    "FULL_FRAME_FREEZE",
    "FULL_FRAME_FREEZE_RELEASE",
    "FULL_FRAME_SNAPSHOT",
    "PROGRESSIVE_REFINEMENT_SEGMENT_START",
    "PROGRESSIVE_REFINEMENT_SEGMENT_END",
    "MOTION_CONSTRAINED_SLICE_GROUP_SET",
    "FILM_GRAIN_CHARACTERISTICS",
    "DEBLOCKING_FILTER_DISPLAY_PREFERENCE",
    "STEREO_VIDEO_INFO",
    "POST_FILTER_HINT",
    "TONE_MAPPING_INFO",
    "SCALABILITY_INFO",
    "SUB_PIC_SCALABLE_LAYER",
    "NON_REQUIRED_LAYER_REP",
    "PRIORITY_LAYER_INFO",
    "LAYERS_NOT_PRESENT",
    "LAYER_DEPENDENCY_CHANGE",
    "SCALABLE_NESTING",
    "BASE_LAYER_TEMPORAL_HRD",
    "QUALITY_LAYER_INTEGRITY_CHECK",
    "REDUNDANT_PIC_PROPERTY",
    "TL0_DEP_REP_INDEX",
    "TL_SWITCHING_POINT",
    "PARALLEL_DECODING_INFO",
    "MVC_SCALABLE_NESTING",
    "VIEW_SCALABILITY_INFO",
    "MULTIVIEW_SCENE_INFO",
    "MULTIVIEW_ACQUISITION_INFO",
    "NON_REQUIRED_VIEW_COMPONENT",
    "VIEW_DEPENDENCY_CHANGE",
    "OPERATION_POINTS_NOT_PRESENT",
    "BASE_VIEW_TEMPORAL_HRD",
    "FRAME_PACKING_ARRANGEMENT",
    "MULTIVIEW_VIEW_POSITION",
    "DISPLAY_ORIENTATION",
    "MVCD_SCALABLE_NESTING",
    "MVCD_VIEW_SCALABILITY_INFO",
    "Reserved"
};

const char* SEIPayloadTraceMap::operator[] (Bs32u i)
{
    return map[BS_MIN(i, 50)];
}

const char ProfileTraceMap::map[13][16] =
{
    "Unknown",
    "CAVLC_444",
    "BASELINE",
    "MAIN",
    "EXTENDED",
    "SVC_BASELINE",
    "SVC_HIGH",
    "HIGH",
    "HIGH_10",
    "HIGH_422",
    "HIGH_444",
    "MULTIVIEW_HIGH",
    "STEREO_HIGH",
};

const char* ProfileTraceMap::operator[] (Bs32u i)
{
    switch (i)
    {
    case CAVLC_444     : return map[ 1];
    case BASELINE      : return map[ 2];
    case MAIN          : return map[ 3];
    case EXTENDED      : return map[ 4];
    case SVC_BASELINE  : return map[ 5];
    case SVC_HIGH      : return map[ 6];
    case HIGH          : return map[ 7];
    case HIGH_10       : return map[ 8];
    case HIGH_422      : return map[ 9];
    case HIGH_444      : return map[10];
    case MULTIVIEW_HIGH: return map[11];
    case STEREO_HIGH   : return map[12];
    default:break;
    }
    return map[0];
}

const char ARIdcTraceMap::map[19][16] =
{
    "Unspecified",
    "1:1", "12:11", "10:11", "16:11",
    "40:33", "24:11", "20:11", "32:11",
    "80:33", "18:11", "15:11", "64:33",
    "160:99", "4:3", "3:2", "2:1",
    "Reserved",
    "Extended_SAR"
};

const char* ARIdcTraceMap::operator[] (Bs32u i)
{
    if (i < 17)
        return map[i];
    if (i == Extended_SAR)
        return map[18];
    return map[17];
}

inline DPBFrame::Field & GetField(DPBPic & p)
{
    return p.frame->field[p.field];
}

void Parser::decodeSlice(NALU& nalu, Slice* first)
{
    TLAuto tl(*this, TRACE_POC);
    Slice& s = *nalu.slice;
    SPS& sps = *s.pps->sps;

    newSlice(s);

    if (!first)
    {
        Bs32u ExpectedFN = (m_prev_ref.FrameNum + 1) % MaxFrameNum;

        if (sps.gaps_in_frame_num_value_allowed_flag
            && nalu.nal_unit_type != SLICE_IDR
            && s.frame_num != m_prev_ref.FrameNum
            && s.frame_num != ExpectedFN)
        {
            TLAuto tl1(*this, TRACE_MARKERS);

            NALU  fakeNalu = {};
            Slice fakseSlice = {};

            fakeNalu.nal_unit_type = SLICE_NONIDR;
            fakeNalu.nal_ref_idc = 1;
            fakeNalu.slice = &fakseSlice;
            fakseSlice.slice_type = SLICE_I;
            fakseSlice.pps = s.pps;

            for (Bs32u fn = ExpectedFN; fn != s.frame_num; fn = (fn + 1) % MaxFrameNum)
            {
                BS2_TRACE_STR(" --- NON-EXISTING --- ");
                BS2_TRACE(fn, s.frame_num);
                fakseSlice.frame_num = fn;
                decodeSlice(fakeNalu, 0);
                updateDPB(fakeNalu);
                m_dpb.back().non_existing = 1;
            }
        }

        BS2_SET(decodePOC(nalu, m_prev, m_prev_ref), s.POC);
        decodePicNum(s);
    }
    else
    {
        BS2_SET(m_prev.Poc, s.POC);
    }

    if (!isI && !isSI)
    {
        std::list<DPBPic> ListX[2];

        initRefLists(s, ListX[0], ListX[1]);

        if (s.ref_pic_list_modification_flag_l0)
            modifyRefList(s, ListX[0], 0);

        if (s.ref_pic_list_modification_flag_l1)
            modifyRefList(s, ListX[1], 1);

        if (   (ListX[0].size() < Bs32u(s.num_ref_idx_l0_active_minus1 + 1))
            || (ListX[1].size() < Bs32u(s.num_ref_idx_l1_active_minus1 + 1) && isB))
            throw InvalidSyntax();

        for (Bs32s X = 0; X < 1 + isB; X++)
        {
            Bs32u i = 0;

            s.RefPicListX[X] = alloc<Slice::RefPic>(&s, (Bs32u)ListX[X].size());

            for (std::list<DPBPic>::iterator it = ListX[X].begin();
                it != ListX[X].end(); it++, i++)
            {
                s.RefPicListX[X][i].POC = GetField(*it).POC;
                s.RefPicListX[X][i].bottom_field_flag = GetField(*it).bottom_field_flag;
                s.RefPicListX[X][i].long_term = it->frame->long_term;
                s.RefPicListX[X][i].non_existing = it->frame->non_existing;
            }
        }

        traceRefLists(s);
    }
}

void Parser::traceRefLists(Slice& s)
{
#ifdef __BS_TRACE__
    if (!TLTest(TRACE_REF_LISTS))
        return;

    for (Bs32s X = 0; X < 1 + isB; X++)
    {
        Bs32s sz = X ? s.num_ref_idx_l1_active_minus1 + 1 : s.num_ref_idx_l0_active_minus1 + 1;

        BS2_TRACE(X, Ref Pic List);

        for (Bs32s i = 0; i < sz; i++)
        {
            BS2_TRO;
            printf("[%2i] POC: %4i %s %s%s\n",
                i,
                s.RefPicListX[X][i].POC,
                s.RefPicListX[X][i].long_term ? "LT" : "ST",
                s.field_pic_flag ? s.RefPicListX[X][i].bottom_field_flag ? "BOT" : "TOP" : "FRM",
                s.RefPicListX[X][i].non_existing ? " NON-EXISTING!" : "");
        }
    }
#endif
}

Bs32s Parser::decodePOC(NALU& nalu, PocInfo& prev, PocInfo& prevRef)
{
    Slice& s = *nalu.slice;
    SPS& sps = *s.pps->sps;
    PocInfo cur = {};
    bool isIDR = (nalu.nal_unit_type == SLICE_IDR);
    bool isREF = (nalu.nal_ref_idc != 0);

    for (DRPM* p = s.drpm; p; p = p->next)
    {
        if (p->memory_management_control_operation == MMCO_CLEAR_DPB)
        {
            cur.mmco5 = true;
            break;
        }
    }

    cur.isBot = s.bottom_field_flag;
    cur.PocLsb = s.pic_order_cnt_lsb;
    cur.FrameNum = s.frame_num;

    if (sps.pic_order_cnt_type == 0)
    {
        Bs32s prevPocMsb = 0;
        Bs32s prevPocLsb = 0;
        Bs32s MaxPocLsb = (1 << (sps.log2_max_pic_order_cnt_lsb_minus4 + 4));

        if (!isIDR)
        {
            if (prevRef.mmco5)
            {
                if (!prevRef.isBot)
                    prevPocLsb = prevRef.TopPoc;
            }
            else
            {
                prevPocMsb = prevRef.PocMsb;
                prevPocLsb = prevRef.PocLsb;
            }
        }

        if ((cur.PocLsb < prevPocLsb) && (prevPocLsb - cur.PocLsb >= MaxPocLsb / 2))
            cur.PocMsb = prevPocMsb + MaxPocLsb;
        else if ((cur.PocLsb > prevPocLsb) && (cur.PocLsb - prevPocLsb > MaxPocLsb / 2))
            cur.PocMsb = prevPocMsb - MaxPocLsb;
        else
            cur.PocMsb = prevPocMsb;

        if (!cur.isBot)
            cur.TopPoc = cur.PocMsb + cur.PocLsb;

        if (!s.field_pic_flag)
            cur.BotPoc = cur.TopPoc + s.delta_pic_order_cnt_bottom;
        else if (cur.isBot)
            cur.BotPoc = cur.PocMsb + cur.PocLsb;
    }
    else if (sps.pic_order_cnt_type == 1)
    {
        Bs32s prevFNO = 0, absFN = 0, expectedPOC = 0, prevFN = 0;

        if (!isIDR && !prev.mmco5)
        {
            prevFN = prev.FrameNum;
            prevFNO = prev.FrameNumOffset;
        }

        if (isIDR)
            cur.FrameNumOffset = 0;
        else if (prevFN > cur.FrameNum)
            cur.FrameNumOffset = prevFNO + MaxFrameNum;
        else
            cur.FrameNumOffset = prevFNO;

        if (sps.num_ref_frames_in_pic_order_cnt_cycle != 0)
            absFN = cur.FrameNum + cur.FrameNumOffset;

        if (!isREF && absFN > 0)
            absFN--;

        if (absFN > 0)
        {
            Bs32s PocCycle = (absFN - 1) / sps.num_ref_frames_in_pic_order_cnt_cycle;
            Bs32s FNinPocCycle = (absFN - 1) % sps.num_ref_frames_in_pic_order_cnt_cycle;
            Bs32s d = 0;

            for (Bs32s i = 0; i < sps.num_ref_frames_in_pic_order_cnt_cycle; i++)
                d += sps.offset_for_ref_frame[i];

            expectedPOC = PocCycle * d;

            for (Bs32s i = 0; i <= FNinPocCycle; i++)
                expectedPOC += sps.offset_for_ref_frame[i];
        }

        if (!isREF)
            expectedPOC += sps.offset_for_non_ref_pic;

        if (!s.field_pic_flag)
        {
            cur.TopPoc = expectedPOC + s.delta_pic_order_cnt[0];
            cur.BotPoc = cur.TopPoc + sps.offset_for_top_to_bottom_field + s.delta_pic_order_cnt[1];
        }
        else if (!s.bottom_field_flag)
            cur.TopPoc = expectedPOC + s.delta_pic_order_cnt[0];
        else
            cur.BotPoc = expectedPOC + sps.offset_for_top_to_bottom_field + s.delta_pic_order_cnt[0];
    }
    else if (sps.pic_order_cnt_type == 2)
    {
        Bs32s prevFNO = 0, tmpPOC = 0, prevFN = 0;

        if (!isIDR)
        {
            prevFN = prev.FrameNum;

            if (!prev.mmco5)
                prevFNO = prev.FrameNumOffset;
        }

        if (isIDR)
            cur.FrameNumOffset = 0;
        else if (prevFN > cur.FrameNum)
            cur.FrameNumOffset = prevFNO + MaxFrameNum;
        else
            cur.FrameNumOffset = prevFNO;

        if (!isIDR)
            tmpPOC = 2 * (cur.FrameNumOffset + cur.FrameNum) - !isREF;

        if (!s.field_pic_flag)
            cur.TopPoc = cur.BotPoc = tmpPOC;
        else if (cur.isBot)
            cur.BotPoc = tmpPOC;
        else
            cur.TopPoc = tmpPOC;
    }
    else
        throw InvalidSyntax();

    if (!s.field_pic_flag)
        cur.Poc = std::min(cur.TopPoc, cur.BotPoc);
    else if (cur.isBot)
        cur.Poc = cur.BotPoc;
    else
        cur.Poc = cur.TopPoc;

    cur.FrameNum *= !cur.mmco5;

    prev = cur;

    if (isREF)
        prevRef = cur;

    return cur.Poc;
}

void Parser::decodePicNum(Slice& s)
{
    for (DPB::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
    {
        DPBFrame& ref = *it;

        if (!ref.long_term)
        {
            if (ref.FrameNum > s.frame_num)
                ref.FrameNumWrap = ref.FrameNum - MaxFrameNum;
            else
                ref.FrameNumWrap = ref.FrameNum;
        }

        if (s.field_pic_flag)
        {
            ref.field[0].PicNum = 2 * ref.FrameNumWrap
                + (ref.field[0].bottom_field_flag == s.bottom_field_flag);
            ref.field[1].PicNum = 2 * ref.FrameNumWrap
                + (ref.field[1].bottom_field_flag == s.bottom_field_flag);
        }
        else
            ref.field[0].PicNum = ref.field[1].PicNum = ref.FrameNumWrap;
    }
}

inline DPBFrame::Field const & GetField(DPBPic const & p)
{
    return p.frame->field[p.field];
}

struct PicNumDescending
{
    bool operator () (DPBPic const & l, DPBPic const & r)
    {
        return GetField(l).PicNum > GetField(r).PicNum;
    }
};

struct PicNumAscending
{
    bool operator () (DPBPic const & l, DPBPic const & r)
    {
        return GetField(l).PicNum < GetField(r).PicNum;
    }
};

struct FrameNumWrapDescending
{
    bool operator () (DPBPic const & l, DPBPic const & r)
    {
        return l.frame->FrameNumWrap > r.frame->FrameNumWrap;
    }
};

struct LTIdxAscending
{
    bool operator () (DPBPic const & l, DPBPic const & r)
    {
        return l.frame->LongTermFrameIdx < r.frame->LongTermFrameIdx;
    }
};

struct PocDescending
{
    bool operator () (DPBPic const & l, DPBPic const & r)
    {
        return GetField(l).POC > GetField(r).POC;
    }
};

struct PocAscending
{
    bool operator () (DPBPic const & l, DPBPic const & r)
    {
        return GetField(l).POC < GetField(r).POC;
    }
};

enum SELECT_FRAMES
{
    ANY = 0x00,
    FOR_L0 = 0x01,
    FOR_L1 = 0x02,
    FOR_LT = 0x04,
    COMPLETE = 0x08
};

void selectFrames(DPB& dpb, std::list<DPBPic>& l, Bs32s poc, Bs16u mode)
{
    bool checkList = !!(mode & (FOR_L0 | FOR_L1));
    bool targetList = !!(mode & FOR_L1);
    bool lt = !!(mode & FOR_LT);
    bool complete = !!(mode & COMPLETE);

    for (DPB::iterator it = dpb.begin(); it != dpb.end(); it++)
    {
        DPBPic pic = { it, 0 };
        bool   list = poc < it->field[0].POC || poc < it->field[1].POC;

        if (lt != it->long_term)
            continue;
        if (!lt && list != targetList && checkList)
            continue;

        if (complete && it->field[0].available && it->field[1].available)
            l.push_back(pic);
        else if (it->field[0].available || it->field[1].available)
            l.push_back(pic);
    }
}

void selectFields(std::list<DPBPic>& frameList, std::list<DPBPic>& l, bool parity)
{
    std::list<DPBPic>::iterator it[2] = { frameList.begin(), frameList.begin() };

    //8.2.4.2.5
    for (bool i = false; it[0] != frameList.end() || it[1] != frameList.end(); i = !i)
    {
        if (it[i] == frameList.end())
        {
            parity = !parity;
            continue;
        }

        if (!it[i]->frame->field[i].available)
        {
            it[i]++;
            continue;
        }

        if (it[i]->frame->field[i].bottom_field_flag == parity)
        {
            l.push_back(*it[i]);
            l.back().field = i;
            parity = !parity;
            it[i]++;
        }
    }
}

void Parser::initRefLists(Slice& s, std::list<DPBPic>& List0, std::list<DPBPic>& List1)
{
    std::list<DPBPic> ListX[4]; // list0, list1 + 2 tmp lists
    std::list<DPBPic>::iterator it;

    if (!s.field_pic_flag && (isP || isSP))
    {
        //8.2.4.2.1
        selectFrames(m_dpb, ListX[0], s.POC, COMPLETE);
        ListX[0].sort(PicNumDescending());

        selectFrames(m_dpb, ListX[2], s.POC, COMPLETE | FOR_LT);
        ListX[2].sort(PicNumAscending());

        ListX[0].splice(ListX[0].end(), ListX[2]);
    }
    else if (s.field_pic_flag && (isP || isSP))
    {
        //8.2.4.2.2
        selectFrames(m_dpb, ListX[2], s.POC, ANY);
        ListX[2].sort(FrameNumWrapDescending());
        selectFields(ListX[2], ListX[0], s.bottom_field_flag);
        ListX[2].resize(0);

        selectFrames(m_dpb, ListX[2], s.POC, FOR_LT);
        ListX[2].sort(LTIdxAscending());
        selectFields(ListX[2], ListX[0], s.bottom_field_flag);
    }
    else if (!s.field_pic_flag && isB)
    {
        //8.2.4.2.3
        selectFrames(m_dpb, ListX[0], s.POC, COMPLETE | FOR_L0);
        ListX[0].sort(PocDescending());

        selectFrames(m_dpb, ListX[1], s.POC, COMPLETE | FOR_L1);
        ListX[1].sort(PocAscending());

        ListX[2].insert(ListX[2].end(), ListX[1].begin(), ListX[1].end());
        ListX[0].splice(ListX[0].end(), ListX[2]);

        ListX[2].insert(ListX[2].end(), ListX[0].begin(), ListX[0].end());
        ListX[1].splice(ListX[1].end(), ListX[2]);

        selectFrames(m_dpb, ListX[2], s.POC, COMPLETE | FOR_LT);
        ListX[2].sort(PicNumAscending());

        ListX[0].insert(ListX[0].end(), ListX[2].begin(), ListX[2].end());
        ListX[1].splice(ListX[1].end(), ListX[2]);
    }
    else if (s.field_pic_flag && isB)
    {
        //8.2.4.2.4
        selectFrames(m_dpb, ListX[2], s.POC, FOR_L0);
        ListX[2].sort(PocDescending());

        selectFrames(m_dpb, ListX[3], s.POC, FOR_L1);
        ListX[3].sort(PocAscending());

        ListX[2].insert(ListX[2].end(), ListX[3].begin(), ListX[3].end());
        it = ListX[2].begin();
        std::advance(it, ListX[2].size() - ListX[3].size());
        ListX[3].insert(ListX[3].end(), ListX[2].begin(), it);

        selectFields(ListX[2], ListX[0], s.bottom_field_flag);
        selectFields(ListX[3], ListX[1], s.bottom_field_flag);

        ListX[2].resize(0);
        selectFrames(m_dpb, ListX[2], s.POC, FOR_LT);
        ListX[2].sort(LTIdxAscending());

        selectFields(ListX[2], ListX[0], s.bottom_field_flag);
        selectFields(ListX[2], ListX[1], s.bottom_field_flag);
    }

    if (ListX[0].size() > Bs32u(s.num_ref_idx_l0_active_minus1 + 1))
        ListX[0].resize(s.num_ref_idx_l0_active_minus1 + 1);

    if (isB && ListX[1].size() > Bs32u(s.num_ref_idx_l1_active_minus1 + 1))
        ListX[1].resize(s.num_ref_idx_l1_active_minus1 + 1);

    if (isB && ListX[1].size() > 1 && ListX[1] == ListX[0])
    {
        //8.2.4.2.3 && 8.2.4.2.4
        it = ListX[1].begin();
        std::swap(*ListX[1].begin(), *(++it));
    }

    List0.swap(ListX[0]);
    List1.swap(ListX[1]);
}

struct PicNumEq
{
    Bs32s m_picNum;
    bool  m_lt;

    PicNumEq(Bs32s PicNum, bool lt) { m_picNum = PicNum; m_lt = lt; }

    bool operator() (DPBPic const & l)
    {
        return m_picNum == GetField(l).PicNum && m_lt == l.frame->long_term;
    }

    bool operator() (DPBFrame const & l)
    {
        return (m_picNum == l.field[0].PicNum || m_picNum == l.field[1].PicNum)
            && m_lt == l.long_term;
    }
};

struct LTIdxEq
{
    Bs32s m_idx;

    LTIdxEq(Bs32s LTIdx) { m_idx = LTIdx; }

    bool operator() (DPBFrame const & l)
    {
        return l.long_term && l.LongTermFrameIdx == m_idx;
    }
};

struct LTIdxGt
{
    Bs32s m_idx;

    LTIdxGt(Bs32s LTIdx) { m_idx = LTIdx; }

    bool operator() (DPBFrame const & l)
    {
        return l.long_term && l.LongTermFrameIdx > m_idx;
    }
};

struct FrameNumWrapLess
{
    bool operator () (DPBFrame const & l, DPBFrame const & r)
    {
        if (l.long_term)
            return false;
        return l.FrameNumWrap < r.FrameNumWrap || r.long_term;
    }
};

DPBPic GetPicByPicNum(DPB& dpb, Bs32s PicNum, bool lt)
{
    DPB::iterator it = std::find_if(dpb.begin(), dpb.end(), PicNumEq(PicNum, lt));

    if (it == dpb.end())
        throw InvalidSyntax();

    DPBPic pic = { it, it->field[0].PicNum != PicNum };

    if (!GetField(pic).available)
        throw InvalidSyntax();

    return pic;
}

std::list<DPBPic>::iterator GetIt(std::list<DPBPic>& list, Bs32u idx)
{
    std::list<DPBPic>::iterator it = list.begin();
    std::advance(it, idx);
    return it;
}

void Parser::modifyRefList(Slice& s, std::list<DPBPic>& ListX, Bs8u X)
{
    Bs32s MaxPicNum = MaxFrameNum * (1 + s.field_pic_flag);
    Bs32s CurrPicNum = (s.field_pic_flag) ? (2 * s.frame_num + 1) : s.frame_num;
    Bs32s picNumLXPred = CurrPicNum;
    Bs32u refIdx = 0;
    std::list<DPBPic>::iterator it;

    for (RefPicListMod* mod = s.rplm[X]; mod; mod = mod->next)
    {
        bool flag0;
        Bs32s picNumLXNoWrap, picNumLX, sign = 1;

        switch (mod->modification_of_pic_nums_idc)
        {
        case RPLM_END:
            return;
        case RPLM_ST_SUB:
            sign = -1;
        case RPLM_ST_ADD:
            //8.2.4.3.1
            flag0 = (sign == -1) ? (picNumLXPred - (mod->abs_diff_pic_num_minus1 + 1) < 0)
                : (picNumLXPred + (mod->abs_diff_pic_num_minus1 + 1) >= MaxPicNum);

            picNumLXNoWrap = picNumLXPred + (mod->abs_diff_pic_num_minus1 + 1) * sign
                - sign * MaxPicNum * flag0;
            picNumLXPred = picNumLXNoWrap;
            picNumLX = picNumLXNoWrap - MaxPicNum * (picNumLXNoWrap > CurrPicNum);

            it = std::find_if(GetIt(ListX, refIdx), ListX.end(), PicNumEq(picNumLX, false));

            if (it != ListX.end())
                ListX.erase(it);

            ListX.insert(GetIt(ListX, refIdx), GetPicByPicNum(m_dpb, picNumLX, false));

            refIdx++;
            break;
        case RPLM_LT:
            //8.2.4.3.2
            it = std::find_if(GetIt(ListX, refIdx), ListX.end(), PicNumEq(mod->long_term_pic_num, true));

            if (it != ListX.end())
                ListX.erase(it);

            ListX.insert(GetIt(ListX, refIdx), GetPicByPicNum(m_dpb, mod->long_term_pic_num, true));

            refIdx++;
            break;
        default:
            throw InvalidSyntax();
        }
    }
}

DPB::iterator removePic(DPB& dpb, DPBPic& pic)
{
    Bs32s picNumX = GetField(pic).PicNum;

    GetField(pic).available = 0;
    pic.field = !pic.field;

    if (GetField(pic).PicNum == picNumX || !GetField(pic).available)
        return pic.frame;

    return dpb.end();
}

void Parser::updateDPB(NALU& nalu)
{
    Slice& s = *nalu.slice;
    bool isIDR = (nalu.nal_unit_type == SLICE_IDR);
    bool isLT = false;
    bool mmco5 = false;
    Bs32s LTIdx = 0;
    DPB::iterator it;

    if (isIDR)
    {
        dpbClear();
    }
    else if (s.adaptive_ref_pic_marking_mode_flag)
    {
        //8.2.5.4
        Bs32s CurrPicNum = (s.field_pic_flag) ? (2 * s.frame_num + 1) : s.frame_num;
        Bs32s picNumX;
        DPBPic pic;

        for (DRPM* drpm = s.drpm; drpm; drpm = drpm->next)
        {
            switch (drpm->memory_management_control_operation)
            {
            case MMCO_END:
                break;
            case MMCO_REMOVE_ST:
                picNumX = CurrPicNum - (drpm->difference_of_pic_nums_minus1 + 1);
                pic = GetPicByPicNum(m_dpb, picNumX, false);
                dpbRemove(removePic(m_dpb, pic));
                break;
            case MMCO_REMOVE_LT:
                pic = GetPicByPicNum(m_dpb, drpm->long_term_pic_num, true);
                dpbRemove(removePic(m_dpb, pic));
                break;
            case MMCO_ST_TO_LT:
                picNumX = CurrPicNum - (drpm->difference_of_pic_nums_minus1 + 1);

                it = std::find_if(m_dpb.begin(), m_dpb.end(), LTIdxEq(drpm->long_term_frame_idx));

                if (it != m_dpb.end())
                {
                    if (
                           m_dpb.end() == std::find_if(m_dpb.begin(), m_dpb.end(), PicNumEq(picNumX, false))
                        && (it->field[0].available && it->field[1].available)
                        && 1 == ((it->field[0].PicNum == picNumX) + (it->field[1].PicNum == picNumX)))
                    {
                        break;
                    }
                    else
                    {
                        pic = GetPicByPicNum(m_dpb, picNumX, false);

                        auto& pf0 = pic.frame->field[pic.field];
                        auto& pf1 = pic.frame->field[!pic.field];
                        auto& lf0 = it->field[0];
                        auto& lf1 = it->field[1];

                        if (!pf1.available)
                        {
                            if (lf0.available && !lf1.available
                                && (lf0.bottom_field_flag != pf0.bottom_field_flag)
                                && (lf0.POC / 2) == (pf0.POC / 2))
                            {
                                pf1 = lf0;
                            }
                            else if (lf1.available && !lf0.available
                                && (lf1.bottom_field_flag != pf0.bottom_field_flag)
                                && (lf1.POC / 2) == (pf0.POC / 2))
                            {
                                pf1 = lf1;
                            }
                        }

                        dpbRemove(it);
                    }
                }
                else
                    pic = GetPicByPicNum(m_dpb, picNumX, false);

                pic.frame->long_term = 1;
                pic.frame->LongTermFrameIdx = drpm->long_term_frame_idx;
                break;
            case MMCO_MAX_LT_IDX:
                while (dpbRemove(std::find_if(m_dpb.begin(),
                    m_dpb.end(), LTIdxGt(drpm->max_long_term_frame_idx_plus1 - 1))));
                break;
            case MMCO_CLEAR_DPB:
                dpbClear();
                mmco5 = true;
                break;
            case MMCO_CUR_TO_LT:
                isLT = true;
                LTIdx = drpm->long_term_frame_idx;
                dpbRemove(std::find_if(m_dpb.begin(),
                    m_dpb.end(), LTIdxEq(drpm->long_term_frame_idx)));
                break;
            default:
                throw InvalidSyntax();
            }
        }
    }
    else
    {
        //8.2.5.3
        if (s.field_pic_flag
            && !m_dpb.empty()
            && m_dpb.back().field[0].available
            && !m_dpb.back().field[1].available
            && m_dpb.back().field[0].bottom_field_flag != s.bottom_field_flag
            && m_dpb.back().long_term == isLT
            && m_dpb.back().FrameNum == s.frame_num)
        {
            m_dpb.back().field[1].POC = s.POC;
            m_dpb.back().field[1].bottom_field_flag = s.bottom_field_flag;
            m_dpb.back().field[1].available = 1;

            if (isLT)
                m_dpb.back().LongTermFrameIdx = LTIdx;

            return;
        }

        if (m_dpb.size() == s.pps->sps->max_num_ref_frames)
        {
            it = std::min_element(m_dpb.begin(), m_dpb.end(), FrameNumWrapLess());

            if (it == m_dpb.end() || it->long_term)
                throw InvalidSyntax();

            dpbRemove(it);
        }
    }

    if (m_dpb.size() >= s.pps->sps->max_num_ref_frames)
        throw InvalidSyntax();

    DPBFrame f = {};

    if (isLT)
    {
        f.long_term = 1;
        f.LongTermFrameIdx = LTIdx;
    }
    else
        f.FrameNum = s.frame_num * !mmco5;

    f.field[0].POC = s.POC * !mmco5;
    f.field[0].bottom_field_flag = s.bottom_field_flag;
    f.field[0].available = 1;

    if (!s.field_pic_flag)
    {
        f.field[0].bottom_field_flag = !(s.POC == m_prev.TopPoc);
        f.field[1].bottom_field_flag = !f.field[0].bottom_field_flag;
        f.field[1].POC = (f.field[1].bottom_field_flag ?
            m_prev.BotPoc : m_prev.TopPoc) * !mmco5;
        f.field[1].available = 1;
    }

    dpbStore(f);
}


void Info::newSlice(Slice& s)
{
    const Bs8u SubWC[5] = { 1, 2, 2, 1, 1 };
    const Bs8u SubHC[5] = { 1, 2, 1, 1, 1 };
    bool updateSPS = true;
    bool updatePPS = true;

    SPS& sps = *s.pps->sps;
    PPS& pps = *s.pps;
    Bs32s i = 0, j = 0, k = 0, x = 0, y = 0;
    std::vector<Bs32u> mapUnitToSliceGroupMap;

    m_prevSPS = &sps;
    m_prevPPS = &pps;
    m_slice = &s;

    isP = isB = isI = isSP = isSI = false;

    switch (s.slice_type % 5)
    {
    case SLICE_P:  isP  = true; break;
    case SLICE_B:  isB  = true; break;
    case SLICE_I:  isI  = true; break;
    case SLICE_SP: isSP = true; break;
    case SLICE_SI: isSI = true; break;
    default: break;
    }

    if(updateSPS)
    {
        MaxFrameNum         = (1 << (sps.log2_max_frame_num_minus4 + 4));
        SubWidthC           = SubWC[sps.chroma_format_idc + sps.separate_colour_plane_flag];
        SubHeightC          = SubHC[sps.chroma_format_idc + sps.separate_colour_plane_flag];
        MbWidthC            = 16 / SubWidthC;
        MbHeightC           = 16 / SubHeightC;
        NumC8x8             = 4 / (SubWidthC * SubHeightC);
        BitDepthY           = 8 + sps.bit_depth_luma_minus8;
        QpBdOffsetY         = 6 * sps.bit_depth_luma_minus8;
        BitDepthC           = 8 + sps.bit_depth_chroma_minus8;
        QpBdOffsetC         = 6 * sps.bit_depth_chroma_minus8;
        RawMbBits           = 256 * BitDepthY + 2 * MbWidthC * MbHeightC * BitDepthC;
        PicWidthInMbs       = sps.pic_width_in_mbs_minus1 + 1;
        PicWidthInSamplesL  = PicWidthInMbs * 16;
        PicWidthInSamplesC  = PicWidthInMbs * MbWidthC;
        PicHeightInMapUnits = sps.pic_height_in_map_units_minus1 + 1;
        PicSizeInMapUnits   = PicWidthInMbs * PicHeightInMapUnits;
        PicHeightInSamplesL = PicHeightInMbs * 16;
        PicHeightInSamplesC = PicHeightInMbs * MbHeightC;
        FrameHeightInMbs    = (2 - sps.frame_mbs_only_flag) * PicHeightInMapUnits;
        ChromaArrayType     = sps.separate_colour_plane_flag ? 0 : sps.chroma_format_idc;

        mapUnitToSliceGroupMap.resize(PicSizeInMapUnits);
    }

    MbaffFrameFlag = ( sps.mb_adaptive_frame_field_flag && !s.field_pic_flag );
    PicHeightInMbs = FrameHeightInMbs / (1 + s.field_pic_flag);
    PicSizeInMbs = PicWidthInMbs * PicHeightInMbs;
    m_firstMB  = true;
    m_prevAddr = s.first_mb_in_slice * (1 + MbaffFrameFlag);

    MbByAddr.resize(PicSizeInMbs);
    MbByAddr.assign(MbByAddr.size(), 0);

    if (!updatePPS)
        return;

    //8.2.2
    if (0 == pps.num_slice_groups_minus1)
        mapUnitToSliceGroupMap.assign(PicSizeInMapUnits, 0);
    else
    {
        Bs32s SliceGroupChangeRate = pps.slice_group_change_rate_minus1 + 1;
        Bs32s MapUnitsInSliceGroup0 = BS_MIN(s.slice_group_change_cycle * SliceGroupChangeRate, PicSizeInMapUnits);
        Bs32s sizeOfUpperLeftGroup = (pps.slice_group_change_direction_flag ? (PicSizeInMapUnits - MapUnitsInSliceGroup0) : MapUnitsInSliceGroup0);

        switch (pps.slice_group_map_type)
        {
        case 0:
            i = 0;
            do
            {
                for (Bs32u iGroup = 0; iGroup <= pps.num_slice_groups_minus1 && i < PicSizeInMapUnits;
                    i += pps.run_length_minus1[iGroup++] + 1)
                {
                    for (j = 0; j <= (Bs32s)pps.run_length_minus1[iGroup] && i + j < PicSizeInMapUnits; j++)
                        mapUnitToSliceGroupMap[i + j] = iGroup;
                }
            } while (i < PicSizeInMapUnits);
            break;
        case 1:
            for (i = 0; i < PicSizeInMapUnits; i++)
                mapUnitToSliceGroupMap[i] = ((i % PicWidthInMbs) + (((i / PicWidthInMbs) * (pps.num_slice_groups_minus1 + 1)) / 2)) % (pps.num_slice_groups_minus1 + 1);
            break;
        case 2:
            for (i = 0; i < PicSizeInMapUnits; i++)
            {
                mapUnitToSliceGroupMap[i] = pps.num_slice_groups_minus1;

                for (Bs32s iGroup = pps.num_slice_groups_minus1 - 1; iGroup >= 0; iGroup--)
                {
                    Bs32s yTopLeft = pps.top_left[iGroup] / PicWidthInMbs;
                    Bs32s xTopLeft = pps.top_left[iGroup] % PicWidthInMbs;
                    Bs32s yBottomRight = pps.bottom_right[iGroup] / PicWidthInMbs;
                    Bs32s xBottomRight = pps.bottom_right[iGroup] % PicWidthInMbs;

                    for (y = yTopLeft; y <= yBottomRight; y++)
                        for (x = xTopLeft; x <= xBottomRight; x++)
                            mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = iGroup;
                }
            }
            break;
        case 3:
            for (i = 0; i < PicSizeInMapUnits; i++)
            {
                mapUnitToSliceGroupMap[i] = 1;
                x = (PicWidthInMbs - pps.slice_group_change_direction_flag) / 2;
                y = (PicHeightInMapUnits - pps.slice_group_change_direction_flag) / 2;

                Bs32s leftBound = x, topBound = y, rightBound = x, bottomBound = y,
                    xDir = pps.slice_group_change_direction_flag - 1,
                    yDir = pps.slice_group_change_direction_flag;
                bool  mapUnitVacant = false;

                for (k = 0; k < MapUnitsInSliceGroup0; k += mapUnitVacant)
                {
                    mapUnitVacant = (mapUnitToSliceGroupMap[y * PicWidthInMbs + x] == 1);

                    if (mapUnitVacant)
                        mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = 0;

                    if (xDir == -1 && x == leftBound)
                    {
                        leftBound = std::max(leftBound - 1, 0);
                        x = leftBound;
                        xDir = 0;
                        yDir = 2 * pps.slice_group_change_direction_flag - 1;
                    }
                    else if (xDir == 1 && x == rightBound)
                    {
                        rightBound = std::min(rightBound + 1, PicWidthInMbs - 1);
                        x = rightBound;
                        xDir = 0;
                        yDir = 1 - 2 * pps.slice_group_change_direction_flag;
                    }
                    else if (yDir == -1 && y == topBound)
                    {
                        topBound = std::max(topBound - 1, 0);
                        y = topBound;
                        xDir = 1 - 2 * pps.slice_group_change_direction_flag;
                        yDir = 0;
                    }
                    else if (yDir == 1 && y == bottomBound)
                    {
                        bottomBound = std::min(bottomBound + 1, PicHeightInMapUnits - 1);
                        y = bottomBound;
                        xDir = 2 * pps.slice_group_change_direction_flag - 1;
                        yDir = 0;
                    }
                    else
                    {
                        x += xDir;
                        y += yDir;
                    }
                }
            }
            break;
        case 4:
            for (i = 0; i < PicSizeInMapUnits; i++)
            {
                if (i < sizeOfUpperLeftGroup)
                    mapUnitToSliceGroupMap[i] = pps.slice_group_change_direction_flag;
                else
                    mapUnitToSliceGroupMap[i] = 1 - pps.slice_group_change_direction_flag;
            }
            break;
        case 5:
            for (j = 0; j < PicWidthInMbs; j++)
            {
                for (i = 0; i < PicHeightInMapUnits; i++)
                {
                    if (k++ < sizeOfUpperLeftGroup)
                        mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = pps.slice_group_change_direction_flag;
                    else
                        mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = 1 - pps.slice_group_change_direction_flag;
                }
            }
            break;
        case 6:
            for (i = 0; i < PicSizeInMapUnits; i++)
                mapUnitToSliceGroupMap[i] = pps.slice_group_id[i];
            break;
        }
    }

    MbToSliceGroupMap.resize(PicSizeInMbs);

    if (sps.frame_mbs_only_flag || s.field_pic_flag)
        MbToSliceGroupMap.assign(mapUnitToSliceGroupMap.begin(), mapUnitToSliceGroupMap.end());
    else if (MbaffFrameFlag)
    {
        for (i = 0; i < PicSizeInMbs; i++)
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[i / 2];
    }
    else
    {
        for (i = 0; i < PicSizeInMbs; i++)
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[(i / (2 * PicWidthInMbs)) * PicWidthInMbs + (i % PicWidthInMbs)];
    }
}

Bs32u Info::nextMbAddress(Bs32u n)
{
    Bs32u i = n + 1;
    while (i < PicSizeInMbs && MbToSliceGroupMap[i] != MbToSliceGroupMap[n])
        i++;
    return i;
}

void Info::newMB(MB& mb)
{
    if (m_firstMB)
    {
        m_firstMB = false;
        mb.Addr = m_prevAddr;
    }
    else
        mb.Addr = nextMbAddress(m_prevAddr);

    m_prevAddr = mb.Addr;

    if (MbaffFrameFlag)
    {
        mb.x = ((mb.Addr / 2 % PicWidthInMbs) << 4);
        mb.y = ((mb.Addr / 2 / PicWidthInMbs) << 5) + (mb.Addr & 1) * 16;
    }
    else
    {
        mb.x = ((mb.Addr % PicWidthInMbs) << 4);
        mb.y = ((mb.Addr / PicWidthInMbs) << 4);
    }

    mb.mb_field_decoding_flag = defaultMBFDF(mb);

    m_cMB = &mb;

    if (mb.Addr > (Bs32s)MbByAddr.size())
        throw InvalidSyntax();

    MbByAddr[mb.Addr] = &mb;
}

bool Info::defaultMBFDF(MB& mb)
{
    if (!MbaffFrameFlag)
        return m_slice->field_pic_flag;

    MB* prevMB = getMB(mb.Addr - 1);

    if ((mb.Addr & 1) && !prevMB->mb_skip_flag)
        return prevMB->mb_field_decoding_flag;

    MB* mbA = getMB(neighbMbA(mb));
    if (mbA)
        return mbA->mb_field_decoding_flag;

    MB* mbB = getMB(neighbMbB(mb));
    if (mbB)
        return mbB->mb_field_decoding_flag;

    return false;
}

MBLoc Info::getNeighbLoc(MB& mb, bool isLuma, Bs16s xN, Bs16s yN)
{
    //6.4.12
    MBLoc  loc = {0,0,0};
    Bs16u  maxW    = isLuma ? 16 : MbWidthC;
    Bs16u  maxH    = isLuma ? 16 : MbHeightC;
    Bs16u& xW      = loc.x;
    Bs16u& yW      = loc.y;
    Bs32s& mbAddrN = loc.Addr;

    mbAddrN = -1;

    if (MbaffFrameFlag == 0)
    {
        //6.4.12.1
        if (xN < 0 && yN < 0)
        {
            if (mb.Addr % PicWidthInMbs != 0)
                mbAddrN = mb.Addr - PicWidthInMbs - 1;
        }
        else if (xN < 0 && yN < maxH)
        {
            if (mb.Addr % PicWidthInMbs != 0)
                mbAddrN = mb.Addr - 1;
        }
        else if (xN < maxW && yN < 0)
        {
            mbAddrN = mb.Addr - PicWidthInMbs;
        }
        else if (xN < maxW && yN < maxH)
        {
            mbAddrN = mb.Addr;
        }
        else if (xN >= maxW && yN < 0)
        {
            if ((mb.Addr + 1) % PicWidthInMbs != 0)
                mbAddrN = mb.Addr - PicWidthInMbs + 1;
        }

        if (!Available(mbAddrN))
            mbAddrN = -1;

        xW = ( xN + maxW ) % maxW;
        yW = ( yN + maxH ) % maxH;

        return loc;
    }

    //6.4.12.2
    Bs8u  currMbFrameFlag = !mb.mb_field_decoding_flag;
    Bs8u  mbIsTopMbFlag   = mb.Addr % 2 == 0;
    Bs8u  mbAddrXFrameFlag = 1;
    Bs32s mbAddrX = -1;
    Bs16s yM = 0;

    if (xN <  0 && yN < 0)
    {
        bool availX = currMbFrameFlag && !mbIsTopMbFlag ? AvailableA(mb) : AvailableD(mb);

        if (!availX)
            return loc;

        mbAddrX = currMbFrameFlag && !mbIsTopMbFlag ? mbAddrA(mb) : mbAddrD(mb);
        mbAddrXFrameFlag = !getMB(mbAddrX)->mb_field_decoding_flag;

        switch ((currMbFrameFlag << 2)|(mbIsTopMbFlag << 1)|mbAddrXFrameFlag)
        {
        case 7:
        case 6: mbAddrN = mbAddrX + 1; yM = yN;                 break;
        case 5: mbAddrN = mbAddrX;     yM = yN;                 break;
        case 4: mbAddrN = mbAddrX + 1; yM = ((yN + maxH) >> 1); break;
        case 3: mbAddrN = mbAddrX + 1; yM = 2 * yN;             break;
        case 2: mbAddrN = mbAddrX;     yM = yN;                 break;
        case 1:
        case 0: mbAddrN = mbAddrX + 1; yM = yN;                 break;
        }
    }
    else if (xN <  0 && yN < maxH)
    {
        if (!AvailableA(mb))
            return loc;

        mbAddrX = mbAddrA(mb);
        mbAddrXFrameFlag = !getMB(mbAddrX)->mb_field_decoding_flag;

        switch((currMbFrameFlag << 3)
            |(mbIsTopMbFlag     << 2)
            |(mbAddrXFrameFlag  << 1)
            |(currMbFrameFlag ? (yN % 2 == 0) : (Bs8u)(yN < (maxH / 2))))
        {
        case 15:
        case 14: mbAddrN = mbAddrX;     yM = yN;                   break;
        case 13: mbAddrN = mbAddrX;     yM = (yN >> 1);            break;
        case 12: mbAddrN = mbAddrX + 1; yM = (yN >> 1);            break;
        case 11:
        case 10: mbAddrN = mbAddrX + 1; yM = yN;                   break;
        case  9: mbAddrN = mbAddrX;     yM = ((yN + maxH) >> 1);   break;
        case  8: mbAddrN = mbAddrX + 1; yM = ((yN + maxH) >> 1);   break;
        case  7: mbAddrN = mbAddrX;     yM = (yN << 1);            break;
        case  6: mbAddrN = mbAddrX + 1; yM = (yN << 1) - maxH;     break;
        case  5:
        case  4: mbAddrN = mbAddrX;     yM = yN;                   break;
        case  3: mbAddrN = mbAddrX;     yM = (yN << 1) + 1;        break;
        case  2: mbAddrN = mbAddrX + 1; yM = (yN << 1) + 1 - maxH; break;
        case  1:
        case  0: mbAddrN = mbAddrX + 1; yM = yN;                   break;
        }
    }
    else if (xN < maxW && yN < 0)
    {
        bool availX = (currMbFrameFlag && !mbIsTopMbFlag) ? true : AvailableB(mb);

        if (!availX)
            return loc;

        if (currMbFrameFlag && !mbIsTopMbFlag)
        {
            mbAddrX = mb.Addr;
            mbAddrXFrameFlag = !mb.mb_field_decoding_flag;
        }
        else
        {
            mbAddrX = mbAddrB(mb);
            mbAddrXFrameFlag = !getMB(mbAddrX)->mb_field_decoding_flag;
        }
        //mbAddrX = (currMbFrameFlag && !mbIsTopMbFlag) ? mb.Addr : mbAddrB(mb);
        //mbAddrXFrameFlag = !getMB(mbAddrX)->mb_field_decoding_flag;

        switch ((currMbFrameFlag << 2)|(mbIsTopMbFlag << 1)|mbAddrXFrameFlag)
        {
        case 7:
        case 6: mbAddrN = mbAddrX + 1; yM = yN;     break;
        case 5:
        case 4: mbAddrN = mbAddrX - 1; yM = yN;     break;
        case 3: mbAddrN = mbAddrX + 1; yM = 2 * yN; break;
        case 2: mbAddrN = mbAddrX;     yM = yN;     break;
        case 1:
        case 0: mbAddrN = mbAddrX + 1; yM = yN;     break;
        }

    }
    else if (xN < maxW && yN < maxH)
    {
        mbAddrN = mb.Addr;
        yM = yN;
    }
    else if (xN >= maxW && yN < 0 )
    {
        bool availX = (currMbFrameFlag && !mbIsTopMbFlag) ? false : AvailableC(mb);

        if (!availX)
            return loc;

        mbAddrX = mbAddrC(mb);
        mbAddrXFrameFlag = !getMB(mbAddrX)->mb_field_decoding_flag;

        switch ((currMbFrameFlag << 2)|(mbIsTopMbFlag << 1)|mbAddrXFrameFlag)
        {
        case 7:
        case 6: mbAddrN = mbAddrX + 1; yM = yN;     break;
        case 5:
        case 4:                                     break;
        case 3: mbAddrN = mbAddrX + 1; yM = 2 * yN; break;
        case 2: mbAddrN = mbAddrX;     yM = yN;     break;
        case 1:
        case 0: mbAddrN = mbAddrX + 1; yM = yN;     break;
        }
    }
    else
        return loc;

    xW = ( xN + maxW ) % maxW;
    yW = ( yM + maxH ) % maxH;

    return loc;
}

BlkLoc Info::neighb8x8Y(MB& mb, Bs32s idx, Bs16s xD, Bs16s yD)
{
    MBLoc  loc = getNeighbLoc(mb, true, (idx % 2) * 8 + xD, (idx / 2) * 8 + yD);
    BlkLoc blk = { -1, loc.Addr };

    if (blk.addr >= 0)
        blk.idx = 2 * (loc.y / 8) + (loc.x / 8); //6.4.13.3

    return blk;
}

BlkLoc Info::neighb8x8C(MB& mb, Bs32s idx, Bs16s xD, Bs16s yD)
{
    MBLoc  loc = getNeighbLoc(mb, false, (idx % 2) * 8 + xD, (idx / 2) * 8 + yD);
    BlkLoc blk = { -1, loc.Addr };

    if (blk.addr >= 0)
        blk.idx = 2 * (loc.y / 8) + (loc.x / 8); //6.4.13.3

    return blk;
}

//#define InverseRasterScan(a, b, c, d, e) (((e) == 0) ? (((a) % ((d) / (b))) * (b)) : (((a) / ((d) / (b))) * (c)))
template<class T> inline T InverseRasterScan(T a, T b, T c, T d, T e)
{
    return (((e) == 0) ? (((a) % ((d) / (b))) * (b)) : (((a) / ((d) / (b))) * (c)));
}

BlkLoc Info::neighb4x4Y(MB& mb, Bs32s idx, Bs16s xD, Bs16s yD)
{
    // 6.4.3 -> 6.4.12
    MBLoc  loc = getNeighbLoc(mb, true,
        InverseRasterScan(idx / 4, 8, 8, 16, 0) + InverseRasterScan(idx % 4, 4, 4, 8, 0) + xD,
        InverseRasterScan(idx / 4, 8, 8, 16, 1) + InverseRasterScan(idx % 4, 4, 4, 8, 1) + yD);
    BlkLoc blk = { -1, loc.Addr };

    if (blk.addr >= 0)
        blk.idx = 8 * (loc.y / 8) + 4 * (loc.x / 8) +
            2 * ((loc.y % 8) / 4) + ((loc.x % 8) / 4); //6.4.13.1

    return blk;
}

BlkLoc Info::neighb4x4C(MB& mb, Bs32s idx, Bs16s xD, Bs16s yD)
{
    // 6.4.3 -> 6.4.12
    MBLoc  loc = getNeighbLoc(mb, false,
        InverseRasterScan(idx, 4, 4, 8, 0) + xD,
        InverseRasterScan(idx, 4, 4, 8, 1) + yD);
    BlkLoc blk = { -1, loc.Addr };

    if (blk.addr >= 0)
        blk.idx = 2 * (loc.y / 4) + (loc.x / 4);//6.4.13.2

    return blk;
}

Bs8s Info::MbPartPredMode(MB& mb, Bs8u x)
{

    if (isP || isSP)
    {
        if (x == 0)
        {
            switch (mb.MbType)
            {
            case P_L0_16x16:
            case P_L0_L0_16x8:
            case P_L0_L0_8x16:
            case P_Skip:
                return Pred_L0;
            case P_8x8:
            case P_8x8ref0:
                return MODE_NA;
            default:
                break;
            }
        }
        else if (x == 1)
        {
            switch (mb.MbType)
            {
            case P_L0_L0_16x8:
            case P_L0_L0_8x16:
                return Pred_L0;
            case P_L0_16x16:
            case P_8x8:
            case P_8x8ref0:
            case P_Skip:
                return MODE_NA;
            default:
                break;
            }
        }
    }

    if (isB)
    {
        if (x == 0)
        {
            switch (mb.MbType)
            {
            case B_Direct_16x16: return Direct ;
            case B_L0_16x16    : return Pred_L0;
            case B_L1_16x16    : return Pred_L1;
            case B_Bi_16x16    : return BiPred ;
            case B_L0_L0_16x8  : return Pred_L0;
            case B_L0_L0_8x16  : return Pred_L0;
            case B_L1_L1_16x8  : return Pred_L1;
            case B_L1_L1_8x16  : return Pred_L1;
            case B_L0_L1_16x8  : return Pred_L0;
            case B_L0_L1_8x16  : return Pred_L0;
            case B_L1_L0_16x8  : return Pred_L1;
            case B_L1_L0_8x16  : return Pred_L1;
            case B_L0_Bi_16x8  : return Pred_L0;
            case B_L0_Bi_8x16  : return Pred_L0;
            case B_L1_Bi_16x8  : return Pred_L1;
            case B_L1_Bi_8x16  : return Pred_L1;
            case B_Bi_L0_16x8  : return BiPred ;
            case B_Bi_L0_8x16  : return BiPred ;
            case B_Bi_L1_16x8  : return BiPred ;
            case B_Bi_L1_8x16  : return BiPred ;
            case B_Bi_Bi_16x8  : return BiPred ;
            case B_Bi_Bi_8x16  : return BiPred ;
            case B_8x8         : return MODE_NA;
            case B_Skip        : return Direct ;
            default: break;
            }
        }
        else if (x == 1)
        {
            switch (mb.MbType)
            {
            case B_Direct_16x16: return MODE_NA;
            case B_L0_16x16    : return MODE_NA;
            case B_L1_16x16    : return MODE_NA;
            case B_Bi_16x16    : return MODE_NA;
            case B_L0_L0_16x8  : return Pred_L0;
            case B_L0_L0_8x16  : return Pred_L0;
            case B_L1_L1_16x8  : return Pred_L1;
            case B_L1_L1_8x16  : return Pred_L1;
            case B_L0_L1_16x8  : return Pred_L1;
            case B_L0_L1_8x16  : return Pred_L1;
            case B_L1_L0_16x8  : return Pred_L0;
            case B_L1_L0_8x16  : return Pred_L0;
            case B_L0_Bi_16x8  : return BiPred ;
            case B_L0_Bi_8x16  : return BiPred ;
            case B_L1_Bi_16x8  : return BiPred ;
            case B_L1_Bi_8x16  : return BiPred ;
            case B_Bi_L0_16x8  : return Pred_L0;
            case B_Bi_L0_8x16  : return Pred_L0;
            case B_Bi_L1_16x8  : return Pred_L1;
            case B_Bi_L1_8x16  : return Pred_L1;
            case B_Bi_Bi_16x8  : return BiPred ;
            case B_Bi_Bi_8x16  : return BiPred ;
            case B_8x8         : return MODE_NA;
            case B_Skip        : return MODE_NA;
            default: break;
            }
        }
    }

    if (mb.MbType == I_NxN)
        return mb.transform_size_8x8_flag ? Intra_8x8 : Intra_4x4;

    if (mb.MbType < I_PCM)
        return Intra_16x16;

    if (isSI && mb.MbType == MBTYPE_SI)
        return Intra_4x4;

    return MODE_NA;
}

Bs8u Info::NumMbPart(MB& mb)
{
    if (isP || isSP)
    {
        switch (mb.MbType)
        {
        case P_L0_16x16:
        case P_Skip:
            return 1;
        case P_L0_L0_16x8:
        case P_L0_L0_8x16:
            return 2;
        case P_8x8:
        case P_8x8ref0:
            return 4;
        default:
            break;
        }
    }

    if (isB && mb.MbType >= MBTYPE_B_start)
    {
        if (mb.MbType == B_Direct_16x16 || mb.MbType == B_Skip)
            return 0;

        if (mb.MbType == B_8x8)
            return 4;

        if (mb.MbType < B_L0_L0_16x8)
            return 1;

        return 2;
    }

    if (mb.MbType == I_NxN)
        return mb.transform_size_8x8_flag ? 4 : 16;

    if (mb.MbType < I_PCM)
        return 1;

    if (isSI && mb.MbType == MBTYPE_SI)
        return 16;

    return 0;
}

namespace BS_AVC2
{

const Bs8u SubMbTblP[4][5] =
{
//   Name      NumSubMbPart  SubMbPredMode SubMbPartWidth SubMbPartHeight
    {P_L0_8x8,            1,       Pred_L0,             8,              8},
    {P_L0_8x4,            2,       Pred_L0,             8,              4},
    {P_L0_4x8,            2,       Pred_L0,             4,              8},
    {P_L0_4x4,            4,       Pred_L0,             4,              4},
};

const Bs8u SubMbTblB[13][5] =
{
//           Name      NumSubMbPart  SubMbPredMode SubMbPartWidth SubMbPartHeight
    {B_Direct_8x8,                4,        Direct,             4,              4},
    {    B_L0_8x8,                1,       Pred_L0,             8,              8},
    {    B_L1_8x8,                1,       Pred_L1,             8,              8},
    {    B_Bi_8x8,                1,        BiPred,             8,              8},
    {    B_L0_8x4,                2,       Pred_L0,             8,              4},
    {    B_L0_4x8,                2,       Pred_L0,             4,              8},
    {    B_L1_8x4,                2,       Pred_L1,             8,              4},
    {    B_L1_4x8,                2,       Pred_L1,             4,              8},
    {    B_Bi_8x4,                2,        BiPred,             8,              4},
    {    B_Bi_4x8,                2,        BiPred,             4,              8},
    {    B_L0_4x4,                4,       Pred_L0,             4,              4},
    {    B_L1_4x4,                4,       Pred_L1,             4,              4},
    {    B_Bi_4x4,                4,        BiPred,             4,              4},
};

}

Bs8u Info::NumSubMbPart(MB& mb, Bs8u idx)
{
    if (idx > 3)
        throw InvalidSyntax();

    if (isP || isSP || isB)
    {
        if (isB && mb.SubMbType[idx] >= MBTYPE_SubB_start)
            return SubMbTblB[mb.SubMbType[idx] - MBTYPE_SubB_start][1];

        if (   mb.SubMbType[idx] >= MBTYPE_SubP_start
            && mb.SubMbType[idx] < MBTYPE_SubB_start)
            return SubMbTblP[mb.SubMbType[idx] - MBTYPE_SubP_start][1];

        return 0;
    }

    return 4;
}

Bs8s Info::SubMbPredMode(MB& mb, Bs8u idx)
{
    if (idx > 3)
        throw InvalidSyntax();

    if (mb.SubMbType[idx] >= MBTYPE_SubB_start)
        return SubMbTblB[mb.SubMbType[idx] - MBTYPE_SubB_start][2];

    if (   mb.SubMbType[idx] >= MBTYPE_SubP_start
        && mb.SubMbType[idx] < MBTYPE_SubB_start)
        return SubMbTblP[mb.SubMbType[idx] - MBTYPE_SubP_start][2];

    if (isB)
        return Direct;

    if (mb.SubMbType[idx] < MBTYPE_SubP_start)
        return MbPartPredMode(mb, idx & 1);

    return MODE_NA;
}

Bs8u Info::SubMbPartWidth(MB& mb, Bs8u idx)
{
    if (idx > 3)
        throw InvalidSyntax();

    if (isP || isSP || isB)
    {
        if (isB && mb.SubMbType[idx] >= MBTYPE_SubB_start)
            return SubMbTblB[mb.SubMbType[idx] - MBTYPE_SubB_start][3];

        if (   mb.SubMbType[idx] >= MBTYPE_SubP_start
            && mb.SubMbType[idx] < MBTYPE_SubB_start)
            return SubMbTblP[mb.SubMbType[idx] - MBTYPE_SubP_start][3];

        return 0;
    }

    return 4;
}

Bs8u Info::SubMbPartHeight(MB& mb, Bs8u idx)
{
    if (idx > 3)
        throw InvalidSyntax();

    if (isP || isSP || isB)
    {
        if (isB && mb.SubMbType[idx] >= MBTYPE_SubB_start)
            return SubMbTblB[mb.SubMbType[idx] - MBTYPE_SubB_start][4];

        if (mb.SubMbType[idx] >= MBTYPE_SubP_start
            && mb.SubMbType[idx] < MBTYPE_SubB_start)
            return SubMbTblP[mb.SubMbType[idx] - MBTYPE_SubP_start][4];

        return 0;
    }

    return 4;
}

Bs8u Info::cbpY(MB& mb)
{
    if (mb.MbType == I_NxN || mb.MbType == MBTYPE_SI)
        return mb.coded_block_pattern % 16;

    if (mb.MbType < I_16x16_0_0_1)
        return 0;

    if (mb.MbType < I_PCM)
        return 15;

    return mb.coded_block_pattern % 16;
}

Bs8u Info::cbpC(MB& mb)
{
    switch (mb.MbType)
    {
    case I_16x16_0_0_0:
    case I_16x16_1_0_0:
    case I_16x16_2_0_0:
    case I_16x16_3_0_0: return 0;
    case I_16x16_0_1_0:
    case I_16x16_1_1_0:
    case I_16x16_2_1_0:
    case I_16x16_3_1_0: return 1;
    case I_16x16_0_2_0:
    case I_16x16_1_2_0:
    case I_16x16_2_2_0:
    case I_16x16_3_2_0: return 2;
    case I_16x16_0_0_1:
    case I_16x16_1_0_1:
    case I_16x16_2_0_1:
    case I_16x16_3_0_1: return 0;
    case I_16x16_0_1_1:
    case I_16x16_1_1_1:
    case I_16x16_2_1_1:
    case I_16x16_3_1_1: return 1;
    case I_16x16_0_2_1:
    case I_16x16_1_2_1:
    case I_16x16_2_2_1:
    case I_16x16_3_2_1: return 2;
    default: break;
    }

    return mb.coded_block_pattern / 16;
}


Bs8u Info::MbPartWidth(Bs8u MbType)
{
    switch(MbType){
    case P_L0_16x16     : return 16;
    case P_L0_L0_16x8   : return 16;
    case P_L0_L0_8x16   : return  8;
    case P_8x8          : return  8;
    case P_8x8ref0      : return  8;
    case P_Skip         : return 16;
    case B_Direct_16x16 : return  8;
    case B_L0_16x16     : return 16;
    case B_L1_16x16     : return 16;
    case B_Bi_16x16     : return 16;
    case B_L0_L0_16x8   : return 16;
    case B_L0_L0_8x16   : return  8;
    case B_L1_L1_16x8   : return 16;
    case B_L1_L1_8x16   : return  8;
    case B_L0_L1_16x8   : return 16;
    case B_L0_L1_8x16   : return  8;
    case B_L1_L0_16x8   : return 16;
    case B_L1_L0_8x16   : return  8;
    case B_L0_Bi_16x8   : return 16;
    case B_L0_Bi_8x16   : return  8;
    case B_L1_Bi_16x8   : return 16;
    case B_L1_Bi_8x16   : return  8;
    case B_Bi_L0_16x8   : return 16;
    case B_Bi_L0_8x16   : return  8;
    case B_Bi_L1_16x8   : return 16;
    case B_Bi_L1_8x16   : return  8;
    case B_Bi_Bi_16x8   : return 16;
    case B_Bi_Bi_8x16   : return  8;
    case B_8x8          : return  8;
    case B_Skip         : return  8;
    default: break;
    }
    return 0;
}


Bs8u Info::MbPartHeight(Bs8u MbType)
{
    switch(MbType)
    {
    case P_L0_16x16     : return 16;
    case P_L0_L0_16x8   : return  8;
    case P_L0_L0_8x16   : return 16;
    case P_8x8          : return  8;
    case P_8x8ref0      : return  8;
    case P_Skip         : return 16;
    case B_Direct_16x16 : return  8;
    case B_L0_16x16     : return 16;
    case B_L1_16x16     : return 16;
    case B_Bi_16x16     : return 16;
    case B_L0_L0_16x8   : return  8;
    case B_L0_L0_8x16   : return 16;
    case B_L1_L1_16x8   : return  8;
    case B_L1_L1_8x16   : return 16;
    case B_L0_L1_16x8   : return  8;
    case B_L0_L1_8x16   : return 16;
    case B_L1_L0_16x8   : return  8;
    case B_L1_L0_8x16   : return 16;
    case B_L0_Bi_16x8   : return  8;
    case B_L0_Bi_8x16   : return 16;
    case B_L1_Bi_16x8   : return  8;
    case B_L1_Bi_8x16   : return 16;
    case B_Bi_L0_16x8   : return  8;
    case B_Bi_L0_8x16   : return 16;
    case B_Bi_L1_16x8   : return  8;
    case B_Bi_L1_8x16   : return 16;
    case B_Bi_Bi_16x8   : return  8;
    case B_Bi_Bi_8x16   : return 16;
    case B_8x8          : return  8;
    case B_Skip         : return  8;
    default: break;
    }
    return 0;
}

PartLoc Info::neighbPart(MB& mb, Bs8s mbPartIdx, Bs8s subMbPartIdx, Bs16s xD, Bs16s yD)
{
    PartLoc loc = { -1, -1, -1 };
    Bs32s& mbAddrN = loc.mbAddr;
    Bs8s&  mbPartIdxN = loc.mbPartIdx;
    Bs8s&  subMbPartIdxN = loc.subMbPartIdx;
    Bs8u   MbType = mb.MbType;
    Bs16s  x = 0, y = 0, xS = 0, yS = 0, xN = 0, yN = 0, xP = 0, yP = 0;

    // (x,y) = 6.4.2.1
    x = InverseRasterScan<Bs16s>(mbPartIdx, MbPartWidth(MbType), MbPartHeight(MbType), 16, 0);// (6-11)
    y = InverseRasterScan<Bs16s>(mbPartIdx, MbPartWidth(MbType), MbPartHeight(MbType), 16, 1);// (6-12)

    if (P_8x8 == MbType || P_8x8ref0 == MbType || B_8x8 == MbType)
    {
        // (xS, yS) = 6.4.2.2
        xS = InverseRasterScan<Bs16s>(subMbPartIdx, SubMbPartWidth(mb, mbPartIdx), SubMbPartHeight(mb, mbPartIdx), 8, 0);// (6-13)
        yS = InverseRasterScan<Bs16s>(subMbPartIdx, SubMbPartWidth(mb, mbPartIdx), SubMbPartHeight(mb, mbPartIdx), 8, 1);// (6-14)
    }

    xN = x + xS + xD;
    yN = y + yS + yD;

    //mbAddrN and ( xW, yW ) = 6.4.12 (xN, yN)
    MBLoc locN = getNeighbLoc(mb, true, xN, yN);
    mbAddrN = locN.Addr;
    xP = locN.x;
    yP = locN.y;

    if (!getMB(mbAddrN))
        return loc;

    //(mbPartIdxN, subMbPartIdxN) = 6.4.13.4 ( xW, yW, mbTypeN(mbAddrN),  subMbTypeN[4](mbAddrN))
    MB& mbN = *getMB(mbAddrN);
    Bs8u mbTypeN = mbN.MbType;

    if (mbTypeN < MBTYPE_P_start)
        mbPartIdxN = 0;
    else
        mbPartIdxN = (16 / MbPartWidth(mbTypeN)) * (yP / MbPartHeight(mbTypeN)) + (xP / MbPartWidth(mbTypeN));

    if (B_Skip == mbTypeN || B_Direct_16x16 == mbTypeN)
        subMbPartIdxN = 2 * ((yP % 8) / 4) + ((xP % 8) / 4);
    else if (P_8x8 == mbTypeN || P_8x8ref0 == mbTypeN || B_8x8 == mbTypeN)
        subMbPartIdxN = (8 / SubMbPartWidth(mbN, mbPartIdxN))
            * ((yP % 8) / SubMbPartHeight(mbN, mbPartIdxN))
            + ((xP % 8) / SubMbPartWidth(mbN, mbPartIdxN));
    else
        subMbPartIdxN = 0;

    return loc;
}

Bs8u Info::MbType(Bs8u mb_type)
{
    if (mb_type <= B_Bi_4x4)
    {
        if (isB)
            return (mb_type < 23) ? (MBTYPE_B_start + mb_type) : (mb_type - 23);

        if (isP || isSP)
            return (mb_type < 5) ? (MBTYPE_P_start + mb_type) : (mb_type - 5);

        if (isI)
            return mb_type;

        if (isSI)
            return mb_type == 0 ? MBTYPE_SI : (mb_type - 1);
    }

    throw InvalidSyntax();
}

inline Bs32u _Idx4x4(MB& X, Bs32s idx)
{
    if (X.transform_size_8x8_flag) return (idx >> 2);
    return Invers4x4Scan[idx];
};

Bs8u Info::Intra4x4PredMode(MB& mb, Bs8u idx, bool prev, Bs8u rem)
{
    BlkLoc A = neighb4x4YblkA(mb, idx);
    BlkLoc B = neighb4x4YblkB(mb, idx);
    MB* mbA = getMB(A.addr);
    MB* mbB = getMB(B.addr);
    Bs8u intraMxMPredModeA = Intra_4x4_DC;
    Bs8u intraMxMPredModeB = Intra_4x4_DC;

    bool dcPredModePredictedFlag =
        ( !mbA ||
          !mbB ||
          (m_prevPPS->constrained_intra_pred_flag && (isInter(mbA->MbType) || isInter(mbB->MbType))));

    if (!dcPredModePredictedFlag && mbA->MbType == I_NxN)
        intraMxMPredModeA = mbA->pred.IntraPredMode[_Idx4x4(*mbA, A.idx)];

    if (!dcPredModePredictedFlag && mbB->MbType == I_NxN)
        intraMxMPredModeB = mbB->pred.IntraPredMode[_Idx4x4(*mbB, B.idx)];

    Bs8u predIntra4x4PredMode = BS_MIN(intraMxMPredModeA, intraMxMPredModeB);

    if (prev)
        return predIntra4x4PredMode;

    if (rem < predIntra4x4PredMode)
        return rem;

    return rem + 1;
}

Bs8u Info::Intra8x8PredMode(MB& mb, Bs8u idx, bool prev, Bs8u rem)
{
    BlkLoc A = neighb8x8YblkA(mb, idx);
    BlkLoc B = neighb8x8YblkB(mb, idx);
    MB* mbA = getMB(A.addr);
    MB* mbB = getMB(B.addr);
    Bs8u intraMxMPredModeA = Intra_8x8_DC;
    Bs8u intraMxMPredModeB = Intra_8x8_DC;

    bool dcPredModePredictedFlag =
        (!mbA ||
         !mbB ||
         (m_prevPPS->constrained_intra_pred_flag && (isInter(mbA->MbType) || isInter(mbB->MbType))));

    if (!dcPredModePredictedFlag && mbA->MbType == I_NxN)
    {
        if (mbA->transform_size_8x8_flag)
            intraMxMPredModeA = mbA->pred.IntraPredMode[A.idx];
        else
        {
            Bs8u n = (MbaffFrameFlag
                && !mb.mb_field_decoding_flag
                && mbA->mb_field_decoding_flag
                && idx == 2) * 2 + 1;
            intraMxMPredModeA = mbA->pred.IntraPredMode[Invers4x4Scan[A.idx * 4 + n]];
        }
    }

    if (!dcPredModePredictedFlag && mbB->MbType == I_NxN)
    {
        if (mbB->transform_size_8x8_flag)
            intraMxMPredModeB = mbB->pred.IntraPredMode[B.idx];
        else
            intraMxMPredModeB = mbB->pred.IntraPredMode[Invers4x4Scan[B.idx * 4 + 2]];
    }

    Bs8u predIntra4x4PredMode = BS_MIN(intraMxMPredModeA, intraMxMPredModeB);

    if (prev)
        return predIntra4x4PredMode;

    if (rem < predIntra4x4PredMode)
        return rem;

    return rem + 1;
}

Bs8u Info::QPy(MB& mb)
{
    Bs8u QpYprev;
    MB*  MBprev = getMB(mb.Addr - 1);

    while (MBprev && !haveResidual(*MBprev))
        MBprev = getMB(MBprev->Addr - 1);

    if (MBprev)
        QpYprev = MBprev->QPy;
    else
        QpYprev = 26 + cPPS().pic_init_qp_minus26 + cSlice().slice_qp_delta;

    return ((QpYprev + mb.mb_qp_delta + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
}
