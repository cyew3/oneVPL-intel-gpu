#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <vector>
#include <algorithm>

namespace fei_encode_default_ref_lists
{

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

    if ((gopOptFlag & MFX_GOP_STRICT) == 0)
    if ((frameOrder + 1) % gopPicSize == 0 /*&& (gopOptFlag & MFX_GOP_CLOSED) ||
        (idrPicDist && (frameOrder + 1) % idrPicDist == 0)*/)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    if (isBPyramid && (frameOrder % gopPicSize % gopRefDist - 1) % 2 == 1)
        return (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF);

    return (MFX_FRAMETYPE_B);
}

struct Field
{
    mfxI32 poc;
    mfxU32 numRef[2];
    mfxI32 ListX[2][33];
};

struct ExternalFrame
{
    mfxI32 order;
    mfxU32 type;
    Field  field[2];
};

class EncodeEmulator
{
private:

    struct Frame
    {
        mfxI32 order;
        mfxU32 type;
        mfxI32 poc[2];
    };

    struct GreaterOrder
    {
        bool operator() (Frame const & l, Frame const & r) { return l.order > r.order; };
    };

    typedef std::vector<Frame> FrameArray;
    typedef std::vector<Frame>::iterator FrameIterator;

    FrameArray m_queue;
    FrameArray m_dpb;
    const mfxVideoParam* m_pVideo;
    bool  m_isBPyramid;
    mfxI32 m_lastIdr;

    bool FindL1(mfxI32 order)
    {
        for (FrameIterator it = m_dpb.begin(); it < m_dpb.end(); it++)
            if (it->order > order)
                return true;
        return false;
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

    FrameIterator Reorder(bool flush)
    {
        FrameIterator begin = m_queue.begin();
        FrameIterator end = m_queue.end();
        FrameIterator top = begin;
        FrameIterator b0 = end; // 1st non-ref B with L1 > 0
        std::vector<FrameIterator> brefs;

        while (top != end && (top->type & MFX_FRAMETYPE_B))
        {
            if (FindL1(top->order))
            {
                /*if (top->type & MFX_FRAMETYPE_REF)
                {
                    if (m_isBPyramid)
                        brefs.push_back(top);
                    else if (b0 == end || (top->order - b0->order < 2))
                        return top;
                }*/

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
            mfxU32 prevMG = brefs[0]->order % m_pVideo->mfx.GopPicSize / m_pVideo->mfx.GopRefDist;

            for (mfxU32 i = 1; i < brefs.size(); i++)
            {
                mfxU32 mg = brefs[i]->order % m_pVideo->mfx.GopPicSize / m_pVideo->mfx.GopRefDist;

                if (mg != prevMG)
                    break;

                prevMG = mg;

                mfxU32 order0 = brefs[i]->order % m_pVideo->mfx.GopPicSize % m_pVideo->mfx.GopRefDist - 1;
                mfxU32 order1 = brefs[idx]->order % m_pVideo->mfx.GopPicSize % m_pVideo->mfx.GopRefDist - 1;

                if (  BRefOrder(order0, 0, m_pVideo->mfx.GopRefDist - 1, 0)
                    < BRefOrder(order1, 0, m_pVideo->mfx.GopRefDist - 1, 0))
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
    EncodeEmulator() : m_lastIdr (0){};
    ~EncodeEmulator() {};


    void Init(mfxVideoParam const & par, bool isBPyramid)
    {
        m_isBPyramid = isBPyramid;
        m_pVideo = &par;
    }

    ExternalFrame NextFrame(mfxI32 order, bool fields, mfxU32 type = 0)
    {
        ExternalFrame out = { -1, 0 };
        FrameIterator itOut = m_queue.end();
        bool isIdr, isI, isB, isRef;

        if (order >= 0)
        {
            Frame in = {order, type};

            if (type == 0)
                in.type = GetFrameType(*m_pVideo, order, m_isBPyramid);

            m_queue.push_back(in);
        }

        if (m_pVideo->mfx.EncodedOrder)
        {
            if (!type || !m_queue.size())
                return out;

            itOut = m_queue.end() - 1;
        }
        else
            itOut = Reorder(order < 0);

        if (itOut == m_queue.end())
            return out;

        isIdr   = !!(itOut->type & MFX_FRAMETYPE_IDR);
        isRef   = !!(itOut->type & MFX_FRAMETYPE_REF);
        isI     = !!(itOut->type & MFX_FRAMETYPE_I);
        isB     = !!(itOut->type & MFX_FRAMETYPE_B);

        if (isIdr)
            m_lastIdr = itOut->order;

        out.field[0].poc = itOut->poc[0] = (itOut->order - m_lastIdr) * 2;
        out.field[1].poc = itOut->poc[1] = itOut->poc[0] + 1;


        out.order = itOut->order;
        out.type  = itOut->type;

        if (isIdr)
            m_dpb.resize(0);

        if (fields)
        {
            for (mfxU32 fid = isI; fid < 2; fid++)
            {
                Field& field = out.field[fid];
                bool selfref = fid && isRef;

                mfxI32 ListX[2][2][16] = {};
                mfxI32 lx[2] = {}, x;

                for (FrameIterator it = m_dpb.begin(); it < m_dpb.end(); it++)
                {
                    x = it->order > out.order;
                    ListX[0][x][lx[x]] = it->poc[0];
                    ListX[1][x][lx[x]] = it->poc[1];
                    lx[x]++;
                }

                std::sort(ListX[0][0], ListX[0][0] + lx[0], std::greater<mfxI32>());
                std::sort(ListX[1][0], ListX[1][0] + lx[0], std::greater<mfxI32>());
                std::sort(ListX[0][1], ListX[0][1] + lx[1], std::less<mfxI32>());
                std::sort(ListX[1][1], ListX[1][1] + lx[1], std::less<mfxI32>());

                if (selfref)
                    field.ListX[0][field.numRef[0]++] = out.field[0].poc;

                for (x = 0; x < 1 + isB; x++)
                {
                    for (mfxI32 i = 0; i < lx[x]; i++)
                    {
                        field.ListX[x][field.numRef[x]++] = ListX[fid][x][i];
                        field.ListX[x][field.numRef[x]++] = ListX[!fid][x][i];
                    }
                }

                if (selfref && field.numRef[0] > 1)
                {
                    for (mfxU32 i = 0; i < field.numRef[0]-1; i += 2)
                        std::swap(field.ListX[0][i], field.ListX[0][i+1]);
                }

                if (isB)
                {
                    memcpy(field.ListX[0] + field.numRef[0], field.ListX[1], field.numRef[1] * sizeof(field.ListX[0][0]));
                    memcpy(field.ListX[1] + field.numRef[1], field.ListX[0], field.numRef[0] * sizeof(field.ListX[0][0]));
                    std::reverse(field.ListX[0] + field.numRef[0], field.ListX[0] + field.numRef[0] + field.numRef[1]);
                    std::reverse(field.ListX[1] + field.numRef[1], field.ListX[1] + field.numRef[1] + field.numRef[0]);
                }

                if (!fid && isRef && m_dpb.size() == m_pVideo->mfx.NumRefFrame)
                    if (m_isBPyramid)
                        m_dpb.erase(std::max_element(m_dpb.begin(), m_dpb.end(), GreaterOrder()));
                    else
                        m_dpb.erase(m_dpb.begin());
            }
        }
        else if (!isI)
        {
            Field& frame = out.field[0];

            for (FrameIterator it = m_dpb.begin(); it != m_dpb.end(); it++)
            {
                bool x = it->order > out.order;
                frame.ListX[x][frame.numRef[x]++] = it->poc[0];
            }

            std::sort(frame.ListX[0], frame.ListX[0] + frame.numRef[0], std::greater<mfxI32>());
            std::sort(frame.ListX[1], frame.ListX[1] + frame.numRef[1], std::less<mfxI32>());

            if (isB)
            {
                memcpy(frame.ListX[0] + frame.numRef[0], frame.ListX[1], frame.numRef[1] * sizeof(frame.ListX[0][0]));
                memcpy(frame.ListX[1] + frame.numRef[1], frame.ListX[0], frame.numRef[0] * sizeof(frame.ListX[0][0]));
                std::reverse(frame.ListX[0] + frame.numRef[0], frame.ListX[0] + frame.numRef[0] + frame.numRef[1]);
                std::reverse(frame.ListX[1] + frame.numRef[1], frame.ListX[1] + frame.numRef[1] + frame.numRef[0]);
            }
        }

        if (isRef)
        {
            if (m_dpb.size() == m_pVideo->mfx.NumRefFrame)
                if (m_isBPyramid)
                    m_dpb.erase(std::max_element(m_dpb.begin(), m_dpb.end(), GreaterOrder()));
                else
                    m_dpb.erase(m_dpb.begin());
            m_dpb.push_back(*itOut);
        }

        m_queue.erase(itOut);

        return out;
    }
};

class Test
    : public tsBitstreamProcessor
    , public tsSurfaceProcessor
    , public tsParserH264AU
    , public EncodeEmulator
    , public tsVideoEncoder
{
private:

    struct FindByPoc
    {
        mfxI32 m_poc;
        FindByPoc(mfxI32 poc) : m_poc(poc) {}
        bool operator () (ExternalFrame const & f) { return f.field[m_poc & 1].poc == m_poc; }
    };

    std::vector<ExternalFrame> m_emu;
    std::vector<slice_header*> m_dpb_emu;

    void Lock(slice_header* sh)
    {
        if (m_dpb_emu.size() > mfxU32((m_par.mfx.NumRefFrame+1) * 2))
        {
            unlock(m_dpb_emu.front());
            m_dpb_emu.erase(m_dpb_emu.begin());
        }
        lock(sh);
        m_dpb_emu.push_back(sh);
    }

public:
    Test()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true)
    {
        //set_trace_level(BS_H264_TRACE_LEVEL_REF_LIST /*| BS_H264_TRACE_LEVEL_NALU | BS_H264_TRACE_LEVEL_AU*/);
        m_bs_processor = this;
        m_filler = this;
    }

    ~Test(){}

    mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        if (ps)
        {
            ExternalFrame f = NextFrame(m_cur < m_max ? m_cur : -1, !!(ps->Info.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)), m_ctrl.FrameType);

            if (f.order >= 0)
                m_emu.push_back(f);
        }

        if (m_cur >= m_max)
            return 0;

        m_cur++;

        return ps;
    }

    void Init()
    {
        GetVideoParam();
        EncodeEmulator::Init(m_par, ((mfxExtCodingOption2&)m_par).BRefType == MFX_B_REF_PYRAMID);
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
            nFrames *= 2;

        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        while (nFrames--)
        {
            H264AUWrapper au(ParseOrDie());
            slice_header* sh;

            while (sh = au.NextSlice())
            {
                if (sh->first_mb_in_slice)
                    continue;

                g_tsLog << "POC " << sh->PicOrderCnt << "\n";

                std::vector<ExternalFrame>::iterator frame = std::find_if(m_emu.begin(), m_emu.end(), FindByPoc(sh->PicOrderCnt));

                if (frame == m_emu.end())
                {
                    g_tsLog << "ERROR: Test Problem: encoded frame emulation not found, check test logic\n";
                    return MFX_ERR_ABORTED;
                }

                Field& emu = frame->field[sh->PicOrderCnt & 1];
                mfxU32 lx[2] = { sh->num_ref_idx_l0_active_minus1 + 1, sh->num_ref_idx_l1_active_minus1 + 1 };

                if (sh->slice_type % 5 != 1) lx[1] = 0;
                if (sh->slice_type % 5 == 2) lx[0] = 0;

                for (mfxU32 list = 0; list < 2; list++)
                {
                    g_tsLog << " List " << list << ":\n";
                    for (mfxU32 idx = 0; idx < lx[list]; idx++)
                    {
                        g_tsLog << " - Actual: " << sh->RefPicList[list][idx]->PicOrderCnt
                            << " - Expected: " << emu.ListX[list][idx] << "\n";
                        EXPECT_EQ(emu.ListX[list][idx], sh->RefPicList[list][idx]->PicOrderCnt)
                            << " list = " << list
                            << " idx = " << idx << "\n";
                    }
                }

                if (!(m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) || (sh->PicOrderCnt & 1))
                {
                    m_emu.erase(frame);
                }

                if (sh->isReferencePicture)
                    Lock(sh);
            }
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

typedef struct
{
    mfxU16 PicStruct;
    mfxU16 GopPicSize;
    mfxU16 GopRefDist;
    mfxU16 BRefType;
    mfxU16 NumRefFrame;
    mfxU32 nFrames;
    mfxU16 IOPattern;
} tc_struct;

tc_struct test_case[] =
{
//       PicStruct                 GopPicSize GopRefDist BRefType NumRefFrame nFrames    IOPattern
/* 00 */{MFX_PICSTRUCT_PROGRESSIVE,         0,         0,       0,          0,    300,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 01 */{MFX_PICSTRUCT_FIELD_TFF,           0,         0,       0,          0,    300,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 02 */{MFX_PICSTRUCT_FIELD_BFF,           0,         0,       0,          0,    300,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 03 */{MFX_PICSTRUCT_PROGRESSIVE,        30,         3,       0,          0,     70,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 04 */{MFX_PICSTRUCT_FIELD_TFF,          30,         3,       0,          0,     70,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 05 */{MFX_PICSTRUCT_FIELD_BFF,          30,         3,       0,          0,     70,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 06 */{MFX_PICSTRUCT_PROGRESSIVE,        30,         4,       2,          0,     70,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 07 */{MFX_PICSTRUCT_FIELD_TFF,          30,         4,       2,          0,     70,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 08 */{MFX_PICSTRUCT_FIELD_BFF,          30,         4,       2,          0,     70,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 09 */{MFX_PICSTRUCT_PROGRESSIVE,       256,         8,       2,          0,    300,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 10 */{MFX_PICSTRUCT_FIELD_TFF,         256,         8,       2,          0,    300,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 11 */{MFX_PICSTRUCT_FIELD_BFF,         256,         8,       2,          0,    300,   MFX_IOPATTERN_IN_VIDEO_MEMORY },
/* 12 */{MFX_PICSTRUCT_PROGRESSIVE,         0,         0,       0,          0,    300,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 13 */{MFX_PICSTRUCT_FIELD_TFF,           0,         0,       0,          0,    300,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 14 */{MFX_PICSTRUCT_FIELD_BFF,           0,         0,       0,          0,    300,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 15 */{MFX_PICSTRUCT_PROGRESSIVE,        30,         3,       0,          0,     70,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 16 */{MFX_PICSTRUCT_FIELD_TFF,          30,         3,       0,          0,     70,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 17 */{MFX_PICSTRUCT_FIELD_BFF,          30,         3,       0,          0,     70,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 18 */{MFX_PICSTRUCT_PROGRESSIVE,        30,         4,       2,          0,     70,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 19 */{MFX_PICSTRUCT_FIELD_TFF,          30,         4,       2,          0,     70,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 20 */{MFX_PICSTRUCT_FIELD_BFF,          30,         4,       2,          0,     70,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 21 */{MFX_PICSTRUCT_PROGRESSIVE,       256,         8,       2,          0,    300,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 22 */{MFX_PICSTRUCT_FIELD_TFF,         256,         8,       2,          0,    300,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
/* 23 */{MFX_PICSTRUCT_FIELD_BFF,         256,         8,       2,          0,    300,   MFX_IOPATTERN_IN_SYSTEM_MEMORY },
};

int test(unsigned int id)
{
    TS_START;
    tc_struct& tc = test_case[id];

    Test enc;
    mfxExtCodingOption2& co2 = enc.m_par;
    mfxU32 nFrames = tc.nFrames;

    co2.BRefType = tc.BRefType;
    enc.m_par.mfx.FrameInfo.PicStruct = tc.PicStruct;
    enc.m_par.mfx.GopRefDist = tc.GopRefDist;
    enc.m_par.mfx.GopPicSize = tc.GopPicSize;
    enc.m_par.IOPattern = tc.IOPattern;
    enc.m_max = nFrames;
    enc.m_par.AsyncDepth = 1; //current limitation for FEI

    //tsBitstreamWriter bw("out.264");
    //enc.m_bs_processor = &bw;

    if (enc.m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        enc.MFXInit();
        enc.m_pFrameAllocator = enc.m_pVAHandle;
        enc.SetFrameAllocator();
    }

    enc.m_par.mfx.FrameInfo.Width  = enc.m_par.mfx.FrameInfo.CropW = 352;
    enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = 288;

    //set mfxEncodeCtrl ext buffers
    std::vector<mfxExtBuffer*> in_buffs;

    mfxExtFeiEncFrameCtrl in_efc = {};
    in_efc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
    in_efc.Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
    in_efc.SearchPath = 2;
    in_efc.LenSP = 57;
    in_efc.SubMBPartMask = 0;
    in_efc.MultiPredL0 = 0;
    in_efc.MultiPredL1 = 0;
    in_efc.SubPelMode = 3;
    in_efc.InterSAD = 2;
    in_efc.IntraSAD = 2;
    in_efc.DistortionType = 0;
    in_efc.RepartitionCheckEnable = 0;
    in_efc.AdaptiveSearch = 0;
    in_efc.MVPredictor = 0;
    in_efc.NumMVPredictors = 1;
    in_efc.PerMBQp = 0;
    in_efc.PerMBInput = 0;
    in_efc.MBSizeCtrl = 0;
    in_efc.RefHeight = 32;
    in_efc.RefWidth = 32;
    in_efc.SearchWindow = 5;
    in_buffs.push_back((mfxExtBuffer*)&in_efc);

    if (!in_buffs.empty())
    {
        enc.m_ctrl.ExtParam = &in_buffs[0];
        enc.m_ctrl.NumExtParam = (mfxU16)in_buffs.size();
    }

    enc.Init();
    enc.EncodeFrames(nFrames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(fei_encode_default_ref_lists, test, sizeof(test_case) / sizeof(tc_struct));

}
