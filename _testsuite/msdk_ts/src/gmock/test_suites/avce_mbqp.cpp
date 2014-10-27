#include "ts_encoder.h"
#include "ts_parser.h"

namespace avce_mbqp
{
enum
{
    QP_INVALID = 255
};

class AUWrap
{
private:
    BS_H264_au& m_au;
    mfxI32 m_idx;
    slice_header* m_sh;
    macro_block* m_mb;
    mfxU8 m_prevqp;
public:

    AUWrap(BS_H264_au& au)
        : m_au(au)
        , m_mb(0)
        , m_sh(0)
        , m_idx(-1)
        , m_prevqp(QP_INVALID)
    {
    }
    ~AUWrap()
    {
    }

    slice_header* NextSlice()
    {
        m_sh = 0;

        while ((mfxU32)++m_idx < m_au.NumUnits)
        {
            if (   m_au.NALU[m_idx].nal_unit_type == 1
                || m_au.NALU[m_idx].nal_unit_type == 5)
            {
                m_sh = m_au.NALU[m_idx].slice_hdr;
                break;
            }
        }
        return m_sh;
    }

    macro_block* NextMB()
    {
        if (!m_sh)
        {
            m_sh = NextSlice();
            if (!m_sh)
                return 0;
        }

        if (!m_mb)
            m_mb = m_sh->mb;
        else
        {
            m_mb ++;

            if (m_mb >= m_sh->mb + m_sh->NumMb)
            {
                m_mb = 0;
                m_sh = 0;
                return NextMB();
            }
        }

        return m_mb;
    }

    mfxU8 NextQP()
    {
        if (!NextMB())
            return QP_INVALID;

        //assume bit-depth=8
        if (m_mb == m_sh->mb)
            m_prevqp = 26 + m_sh->pps_active->pic_init_qp_minus26 + m_sh->slice_qp_delta;
    
        m_prevqp += m_mb->mb_qp_delta;

        return m_prevqp;
    }
};

class Encoder : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU
{
private:
    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
        std::vector<mfxU8>          buf;
    };

    std::map<mfxU32, Ctrl> m_ctrl;
    mfxU32 m_fo;
public:
    Encoder()
        : tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        , tsVideoEncoder(MFX_CODEC_AVC)
        , m_fo(0)
    {
        set_trace_level(0);
        srand(0);

        m_filler = this;
        m_bs_processor = this;
    }

    ~Encoder()
    {
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxU32 numMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16) * ((m_par.mfx.FrameInfo.CropH + 15) / 16);
        Ctrl& ctrl = m_ctrl[m_fo];
        mfxExtMBQP& mbqp = ctrl.ctrl;

        ctrl.buf.resize(numMB);
        for (mfxU32 i = 0; i < ctrl.buf.size(); i++)
            ctrl.buf[i] = 1 + rand() % 50;

        mbqp.QP = &ctrl.buf[0];
        mbqp.NumQPAlloc = mfxU32(ctrl.buf.size());
        m_pCtrl = &ctrl.ctrl;

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        while (checked++ < nFrames)
        {
            UnitType& hdr = ParseOrDie();
            AUWrap au(hdr);
            mfxU8 qp = au.NextQP();
            mfxU32 i = 0;
            mfxU32 wMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16);
            mfxU32 hMB = ((m_par.mfx.FrameInfo.CropH + 15) / 16);
            mfxExtMBQP& mbqp = m_ctrl[(mfxU32)bs.TimeStamp].ctrl;

            g_tsLog << "Frame #"<< bs.TimeStamp <<" QPs:";

            while (qp != QP_INVALID)
            {
                EXPECT_EQ(mbqp.QP[i], qp);

                if (i++ % wMB == 0)
                    g_tsLog << "\n";

                g_tsLog << mfxU16(qp) << " ";


                qp = au.NextQP();
            }
            g_tsLog << "\n";
        }

        m_ctrl.erase((mfxU32)bs.TimeStamp);
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};


    
int test(unsigned int id)
{
    TS_START;
    Encoder enc;
    mfxInfoMFX& mfx = enc.m_par.mfx;
    mfxExtCodingOption3& co3 = enc.m_par;

    co3.EnableMBQP = MFX_CODINGOPTION_ON;
    mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    mfx.QPI = mfx.QPP = mfx.QPB = 26;
    
    enc.EncodeFrames(30);
    
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_mbqp, test, 1);
};