#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace avce_mbqp
{
enum
{
    QP_INVALID = 255,
    QP_BY_FRM_TYPE = 254,
    QP_DO_NOT_CHECK = 253
};

class AUWrap : public H264AUWrapper
{
private:
    mfxU8 m_prevqp;
public:

    AUWrap(BS_H264_au& au)
        : H264AUWrapper(au)
        , m_prevqp(QP_INVALID)
    {
    }

    ~AUWrap()
    {
    }

    mfxU32 GetFrameType()
    {
        if (!m_sh)
            return 0;

        mfxU32 type = 0;
        switch (m_sh->slice_type % 5)
        {
        case 0:
            type = MFX_FRAMETYPE_P;
            break;
        case 1:
            type = MFX_FRAMETYPE_B;
            break;
        case 2:
            type = MFX_FRAMETYPE_I;
            break;
        default:
            break;
        }

        return type;
    }

    bool IsBottomField()
    {
        return m_sh && m_sh->bottom_field_flag;
    }

    mfxU8 NextQP()
    {
        if (!NextMB())
            return QP_INVALID;

        //assume bit-depth=8
        if (m_mb == m_sh->mb)
            m_prevqp = 26 + m_sh->pps_active->pic_init_qp_minus26 + m_sh->slice_qp_delta;

        m_prevqp = (m_prevqp + m_mb->mb_qp_delta + 52) % 52;

        if (m_sh->slice_type % 5 == 2)  // Intra
        {
            // 7-11
            if (m_mb->mb_type == 0 && m_mb->coded_block_pattern == 0 || m_mb->mb_type == 25)
                return QP_DO_NOT_CHECK;
        } else
        {
            if (m_mb->mb_skip_flag)
                return QP_DO_NOT_CHECK;

            if (m_sh->slice_type % 5 == 0)  // P
            {
                // 7-13 + 7-11
                if ((m_mb->mb_type <= 5 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 30)
                    return QP_DO_NOT_CHECK;
            } else
            {   // 7-14 + 7-11
                if ((m_mb->mb_type <= 23 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 48)
                    return QP_DO_NOT_CHECK;
            }
        }

        return m_prevqp;
    }
};

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU
{
private:
    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
        std::vector<mfxU8>          buf;
    };

    std::map<mfxU32, Ctrl> m_ctrl;
    mfxU32 m_fo;
    bool mbqp_on;
    mfxU32 mode;
    tsNoiseFiller m_reader;
    //tsBitstreamWriter m_writer;
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_AVC)
        , tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        , m_fo(0)
        , mbqp_on(true)
        , mode(0)
        , m_reader()
        //, m_writer("debug.264")
    {
        set_trace_level(0);
        srand(0);

        m_filler = this;
        m_bs_processor = this;
        m_par.AsyncDepth = 1;
    }

    ~TestSuite() {}

    enum
    {
        MFXPAR = 1
    };
    enum
    {
        INIT            = 1,
        QUERY           = 1 << 1,
        RESET_ON        = 1 << 2,
        RESET_OFF       = 1 << 3,
        RANDOM          = 1 << 4
    };

    static const unsigned int n_cases;
    int RunTest(unsigned int id);
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        m_reader.ProcessSurface(s);
        mfxU32 numMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16) * ((m_par.mfx.FrameInfo.CropH + 15) / 16);
        Ctrl& ctrl = m_ctrl[m_fo];

        ctrl.buf.resize(numMB);
        bool send_buffer = mbqp_on;
        if (mode & RANDOM)
            send_buffer = ((rand() % 9000) % 2 == 0);

        for (mfxU32 i = 0; i < ctrl.buf.size(); i++)
        {
            if (mbqp_on && send_buffer)
            {
                ctrl.buf[i] = 1 + rand() % 50;
                if (i) {
                    // The decoded value of mb_qp_delta shall be in the range of −( 26 + QpBdOffsetY / 2) to +( 25 + QpBdOffsetY / 2 ), inclusive
                    mfxI16 delta = ctrl.buf[i-1] - ctrl.buf[i];
                    if (delta >= 25)
                        ctrl.buf[i] = ctrl.buf[i] + delta - rand() % 10;
                    else if (delta <= -25)
                        ctrl.buf[i] = ctrl.buf[i] + delta + rand() % 10;

                    if (ctrl.buf[i] > 51) ctrl.buf[i] = 51;
                    if (ctrl.buf[i] <  1) ctrl.buf[i] = 1;
                }
            } else
            {
                ctrl.buf[i] = QP_BY_FRM_TYPE;
            }
        }

        mfxExtMBQP& mbqp = ctrl.ctrl;
        mbqp.QP = &ctrl.buf[0];
        mbqp.NumQPAlloc = mfxU32(ctrl.buf.size());

        if (mbqp_on)
        {
            m_pCtrl = &ctrl.ctrl;
        }
        else
        {
            m_pCtrl->NumExtParam = 0;
        }

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
            nFrames *= 2;

        //m_writer.ProcessBitstream(bs, nFrames);

        while (checked++ < nFrames)
        {
            UnitType& hdr = ParseOrDie();
            AUWrap au(hdr);
            mfxU8 qp = au.NextQP();
            mfxU32 i = 0;
            mfxU32 wMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16);
            mfxU32 hMB = ((m_par.mfx.FrameInfo.CropH + 15) / 16);
            mfxExtMBQP& mbqp = m_ctrl[(mfxU32)bs.TimeStamp].ctrl;

            mfxU32 fieldOffset = au.IsBottomField() ? (mfxU32)(hMB / 2 * wMB) : 0;

            g_tsLog << "Frame #"<< bs.TimeStamp <<" QPs:\n";

            while (qp != QP_INVALID)
            {
                mfxU8 expected_qp = mbqp.QP[fieldOffset + i];
                if (expected_qp == QP_BY_FRM_TYPE)
                {
                    switch(au.GetFrameType())
                    {
                    case MFX_FRAMETYPE_I:
                        expected_qp = (mfxU8)m_par.mfx.QPI;
                        break;
                    case MFX_FRAMETYPE_P:
                        expected_qp = (mfxU8)m_par.mfx.QPP;
                        break;
                    case MFX_FRAMETYPE_B:
                        expected_qp = (mfxU8)m_par.mfx.QPB;
                        break;
                    default:
                        expected_qp = 0;
                        break;
                    }
                }

                if (qp != QP_DO_NOT_CHECK && expected_qp != qp)
                {
                    g_tsLog << "\nFAIL: Expected QP (" << mfxU16(expected_qp) << ") != real QP (" << mfxU16(qp) << ")\n";
                    g_tsLog << "\nERROR: Expected QP != real QP\n";
                    return MFX_ERR_ABORTED;
                }

                if (i++ % wMB == 0)
                    g_tsLog << "\n";

                g_tsLog << std::setw(3) << mfxU16(qp) << " ";

                qp = au.NextQP();
            }
            g_tsLog << "\n";
        }

        m_ctrl.erase((mfxU32)bs.TimeStamp);
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

#define mfx_PicStruct tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
const TestSuite::tc_struct TestSuite::test_case[] =
{
    // not CQP: Init
    {/*00/00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}},
    {/*01/01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR}},
#ifndef LINUX32
    {/*02/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR}},
    {/*03/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ}},
    {/*04/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA}},
    {/*05/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT}},
    {/*06/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD}},
    {/*07/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ}},
    {/*08/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR}},
    {/*09/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM}},
    {/*10/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VME}},
#endif
    // not CQP: Query
    {/*11/02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}},
    {/*12/03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR}},
#ifndef LINUX32
    {/*13/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR}},
    {/*14/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ}},
    {/*15/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA}},
    {/*16/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT}},
    {/*17/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD}},
    {/*18/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ}},
    {/*19/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR}},
    {/*20/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM}},
    {/*21/--*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VME}},
#endif
    // progressive
    {/*22/04*/ MFX_ERR_NONE, 0},
    {/*23/05*/ MFX_ERR_NONE, RESET_ON},
    {/*24/06*/ MFX_ERR_NONE, RESET_OFF},
    {/*25/07*/ MFX_ERR_NONE, RANDOM},
    {/*26/08*/ MFX_ERR_NONE, RANDOM|RESET_ON},
    {/*27/09*/ MFX_ERR_NONE, RANDOM|RESET_OFF},
    // tff
    {/*28/10*/ MFX_ERR_NONE, 0,                  { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*29/11*/ MFX_ERR_NONE, RESET_ON,           { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*30/12*/ MFX_ERR_NONE, RESET_OFF,          { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*31/13*/ MFX_ERR_NONE, RANDOM,             { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*32/14*/ MFX_ERR_NONE, RANDOM | RESET_ON,  { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*33/15*/ MFX_ERR_NONE, RANDOM | RESET_OFF, { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    // bff
    {/*34/16*/ MFX_ERR_NONE, 0,                  { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*35/17*/ MFX_ERR_NONE, RESET_ON,           { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*36/18*/ MFX_ERR_NONE, RESET_OFF,          { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*37/19*/ MFX_ERR_NONE, RANDOM,             { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*38/20*/ MFX_ERR_NONE, RANDOM | RESET_ON,  { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*39/21*/ MFX_ERR_NONE, RANDOM | RESET_OFF, { MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
};

#undef mfx_PicStruct

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];
    mode = tc.mode;

    MFXInit();

    if (!(tc.mode & RESET_ON))
    {
        mfxExtCodingOption3& co3 = m_par;
        co3.EnableMBQP = MFX_CODINGOPTION_ON;
    }

    SETPARS(m_pPar, MFXPAR);
    set_brc_params(&m_par);

    g_tsStatus.expect(tc.sts);

    if (tc.mode & QUERY)
    {
        Query();
    }
    if (tc.mode & INIT)
    {
        Init();
    }

    AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 30 / 18);

    if (tc.sts == MFX_ERR_NONE)
    {
        if (tc.mode & RESET_ON)
        {
            mbqp_on = false;
        }

        tsSurfaceProcessor::m_max = 30; // to auto-drain buffered frames before reset
        EncodeFrames(30);

        if (tc.mode & RESET_ON || tc.mode & RESET_OFF)
        {
            if (tc.mode & RESET_ON)
            {
                mfxExtCodingOption3& co3 = m_par;
                co3.EnableMBQP = MFX_CODINGOPTION_ON;

#if defined(_WIN32)
                g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
#endif
            }
            else if (tc.mode & RESET_OFF)
            {
                mfxExtCodingOption3& co3 = m_par;
                co3.EnableMBQP = MFX_CODINGOPTION_OFF;
            }

            mbqp_on = !mbqp_on;

            tsSurfaceProcessor::m_cur = 0;
            tsSurfaceProcessor::m_eos = false;
            m_pCtrl = &(tsVideoEncoder::m_ctrl);
            Reset();
            if (g_tsStatus.get() < 0)
                return 0;

            EncodeFrames(30);
        }
    }
    else if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && (tc.mode & QUERY))
    {
        mfxExtCodingOption3* co3 = (mfxExtCodingOption3*)m_par.ExtParam[0];
        if (co3->EnableMBQP != MFX_CODINGOPTION_OFF)
        {
            g_tsLog << "ERROR: EnableMBQP should be set to OFF (" << MFX_CODINGOPTION_OFF
                    << "), real is " << co3->EnableMBQP << "\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_mbqp);
};
