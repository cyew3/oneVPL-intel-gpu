#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <vector>

namespace avce_inter_mbs
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

class Reorderer : public tsSurfaceProcessor
{
private:
    tsSurfaceProcessor& m_r;
    mfxEncodeCtrl& m_ctrl;
    tsSurfacePool& m_spool;
    mfxVideoParam m_par;
    bool m_isBPyramid;
    std::vector<Frame> m_dpb;
    std::vector<Frame> m_queue;

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
    Reorderer(
        tsSurfaceProcessor& sp,
        mfxVideoParam const & par,
        bool isBPyramid,
        mfxEncodeCtrl& ctrl,
        tsSurfacePool& pool)
        : m_r(sp)
        , m_par(par)
        , m_isBPyramid(isBPyramid)
        , m_ctrl(ctrl)
        , m_spool(pool)
    {
        m_cur = 0;
    }
    ~Reorderer()
    {
    }

    mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        while (ps)
        {
            Frame f = { m_r.ProcessSurface(ps, pfa), 0 };

            if (f.surf)
            {
                f.surf->Data.Locked++;
                f.surf->Data.FrameOrder = m_cur++;
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
                        m_dpb.begin()->surf->Data.Locked--;
                        m_dpb.erase(m_dpb.begin());
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

                return f.surf;
            }

            if (!f.surf)
                break;

            ps = m_spool.GetSurface();
        }

        return 0;
    }
};


using namespace BS_AVC2;

class Test
    : public tsBitstreamProcessor
    , public tsParserAVC2
    , public tsVideoEncoder
{
private:
    //tsBitstreamWriter m_w;
public:
    Test()
        : tsVideoEncoder(MFX_CODEC_AVC)
        , tsParserAVC2(INIT_MODE_PARSE_SD)
        //, m_w("debug.264")
    {
        m_bs_processor = this;
        //set_trace_level(TRACE_MB_TYPE|TRACE_SLICE);
    }

    ~Test(){}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
            nFrames *= 2;

        SetBuffer0(bs);

        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            bool allIntra = true;
            
            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                if (   au.nalu[i]->nal_unit_type != SLICE_NONIDR
                    || au.nalu[i]->slice->slice_type % 5 == SLICE_I)
                    continue;

                MB* mb = au.nalu[i]->slice->mb;

                while (mb && allIntra)
                {
                    allIntra = !(mb->MbType >= MBTYPE_P_start && mb->MbType < P_Skip);
                    mb = mb->next;
                }
                EXPECT_FALSE(allIntra) << "ERROR: all MBs of non-Intra slice coded in Intra or Skip mode";
            }
        }

        //m_w.ProcessBitstream(bs, 1);
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

typedef struct
{
    mfxU16 EncodedOrder;
    mfxU16 PicStruct;
    mfxU16 GopPicSize;
    mfxU16 GopRefDist;
    mfxU16 BRefType;
    mfxU16 NumRefFrame;
    mfxU32 nFrames;
} tc_struct;

tc_struct test_case[] =
{
//       EncodedOrder PicStruct                 GopPicSize GopRefDist BRefType NumRefFrame nFrames
/* 00 */{           1, MFX_PICSTRUCT_FIELD_TFF,          24,         4,       2,          3,    60 },
/* 01 */{           0, MFX_PICSTRUCT_FIELD_TFF,          24,         4,       2,          3,    60 },
};

int test(unsigned int id)
{
    TS_START;
    tc_struct& tc = test_case[id];
    const char* stream = g_tsStreamPool.Get("/YUV/720x480i29_jetcar_CC60f.nv12");
    g_tsStreamPool.Reg();

    Test enc;
    mfxExtCodingOption2& co2 = enc.m_par;

    co2.BRefType = tc.BRefType;
    enc.m_par.mfx.FrameInfo.PicStruct = tc.PicStruct;
    enc.m_par.mfx.GopRefDist = tc.GopRefDist;
    enc.m_par.mfx.GopPicSize = tc.GopPicSize;
    enc.m_par.mfx.GopOptFlag = MFX_GOP_STRICT;
    enc.m_par.mfx.EncodedOrder = tc.EncodedOrder;

    tsRawReader f(stream, enc.m_par.mfx.FrameInfo);
    enc.m_filler = &f;

    enc.Init();
    enc.GetVideoParam();

    Reorderer r(f, enc.m_par, co2.BRefType == MFX_B_REF_PYRAMID, enc.m_ctrl, enc);
    if (enc.m_par.mfx.EncodedOrder)
    {
        enc.m_filler = &r;
        enc.QueryIOSurf();
        enc.m_request.NumFrameMin = enc.m_request.NumFrameSuggested = 
            enc.m_par.mfx.GopRefDist - 1 + enc.m_par.mfx.NumRefFrame + enc.m_par.AsyncDepth;
    }

    enc.EncodeFrames(tc.nFrames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_inter_mbs, test, sizeof(test_case) / sizeof(tc_struct));

}