#include "ts_encoder.h"
#include "ts_struct.h"
#include "avc2_parser.h"
#include "avc2_struct.h"

namespace avce_mb_disable_skip_map
{

#define IS_MB_SKIPPED(mbtype)\
  ((mbtype == BS_AVC2::P_Skip || mbtype == BS_AVC2::B_Skip) ? true : false)

class TestSuite : public tsVideoEncoder, public tsBitstreamProcessor, public AVC2_BitStream
{
    std::vector<mfxExtMBDisableSkipMap> m_skip_map;
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_AVC)
        , AVC2_BitStream(BS_AVC2::INIT_MODE_PARSE_SD)
    {
        m_bs_processor = this;
    }
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < m_skip_map.size(); i++) {
            if (m_skip_map[i].Map) {
                delete[] m_skip_map[i].Map;
                m_skip_map[i].Map = 0;
            }
        }
        m_skip_map.clear();
    }

    int RunTest(unsigned int id);

    enum
    {
        DEFAULT = 0,
        EACH_OTHER_FRAME,   // non-skip each other frame
        EACH_WO_BORDERS,    // non-skip each first and last MS (other don't matter)
        EACH_OTHER_MB       // non-skip each other MB
    };

    enum
    {
        MFXPAR = 1
    };

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
    static const unsigned int n_cases;

    mfxStatus Init()
    {
        return tsVideoEncoder::Init(this->m_session, this->m_pPar);
    }
    mfxStatus Close()
    {
        return tsVideoEncoder::Close(this->m_session);
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);
        BS_AVC2::AU* au = new BS_AVC2::AU;
        while (nFrames--)
        {
            BSErr err = parse_next_au(au);
            mfxExtMBDisableSkipMap frm_map;
            frm_map.Header.BufferId = MFX_EXTBUFF_MB_DISABLE_SKIP_MAP;
            frm_map.Header.BufferSz = sizeof(mfxExtMBDisableSkipMap);

            // TODO: check for alignment
            frm_map.MapSize = m_par.mfx.FrameInfo.Width / 16 * m_par.mfx.FrameInfo.Height / 16;
            frm_map.Map = new mfxU8[frm_map.MapSize];
            memset(frm_map.Map, 0, sizeof(mfxU8) * frm_map.MapSize);

            mfxU32 non_skip = 0;

            for (Bs32u i = 0; i < au->NumUnits; i++)
            {
                if (au->nalu[i]->nal_unit_type == BS_AVC2::SLICE_NONIDR ||
                    au->nalu[i]->nal_unit_type == BS_AVC2::SLICE_IDR)
                {
                    for (Bs32u nmb = 0; nmb < au->nalu[i]->slice->NumMB; nmb++)
                    {
                        if (IS_MB_SKIPPED(au->nalu[i]->slice->mb[nmb].MbType))
                        {
                            frm_map.Map[nmb] = 1;
                            non_skip++;
                        }
                    }
                }
            }
            m_skip_map.push_back(frm_map);
        }

        bs.DataOffset = 0;
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

class TestEncoder : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public AVC2_BitStream
{
    std::vector<mfxExtMBDisableSkipMap> m_skip_map;
    std::vector<mfxEncodeCtrl> m_ctrls;
    mfxU32 m_curr_in_frame;
    mfxU32 m_curr_out_frame;
public:
    mfxStatus Init()
    {
        return tsVideoEncoder::Init(this->m_session, this->m_pPar);
    }

    TestEncoder(std::vector<mfxExtMBDisableSkipMap> skip_map)
        : tsVideoEncoder(MFX_CODEC_AVC)
        , AVC2_BitStream(BS_AVC2::INIT_MODE_PARSE_SD)
        , m_skip_map(skip_map)
    {
        m_curr_in_frame = 0;
        m_curr_out_frame = 0;
        m_filler = this;
        m_bs_processor = this;
    }
    ~TestEncoder()
    {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);
        BS_AVC2::AU* au = new BS_AVC2::AU;
        while (nFrames--)
        {
            BSErr err = parse_next_au(au);

            mfxU32 real_nonskip = 0;
            mfxU32 exp_nonskip = 0;
            for (Bs32u i = 0; i < au->NumUnits; i++)
            {
                if (au->nalu[i]->nal_unit_type == BS_AVC2::SLICE_NONIDR)
                {
                    for (Bs32u nmb = 0; nmb < au->nalu[i]->slice->NumMB; nmb++)
                    {
                        if (!IS_MB_SKIPPED(au->nalu[i]->slice->mb[nmb].MbType))
                            real_nonskip++;
                        if (m_skip_map[m_curr_out_frame].Map[nmb] == 1)
                            exp_nonskip++;

                        if (!IS_MB_SKIPPED(au->nalu[i]->slice->mb[nmb].MbType) &&
                            (m_skip_map[m_curr_out_frame].Map[nmb] == 1))
                        {
                            g_tsLog << "ERROR: unexpected skipped MB" << "\n"
                                    << "frame#" << m_curr_out_frame << " MB#" << nmb
                                    << " (MbType=" << au->nalu[i]->slice->mb[nmb].MbType << ")\n";
                            g_tsStatus.check(MFX_ERR_ABORTED);
                        }
                    }
                }
            }
            g_tsLog << "Frame#" << m_curr_out_frame << ": " << real_nonskip << "/" << exp_nonskip << "\n";
            m_curr_out_frame++;
        }

        bs.DataOffset = 0;
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_curr_in_frame >= m_skip_map.size())
            return MFX_ERR_NONE;

        m_ctrls.push_back(m_ctrl);

        m_pCtrl = &m_ctrls[m_curr_in_frame];

        m_pCtrl->NumExtParam = 1;
        if (!m_pCtrl->ExtParam)
            m_pCtrl->ExtParam = new mfxExtBuffer*[1];

        m_pCtrl->ExtParam[0] = (mfxExtBuffer*)&m_skip_map[m_curr_in_frame++];

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, DEFAULT, {}},
    {/*00*/ MFX_ERR_NONE, EACH_OTHER_FRAME, {}},
    {/*00*/ MFX_ERR_NONE, EACH_WO_BORDERS, {}},
    {/*00*/ MFX_ERR_NONE, EACH_OTHER_MB, {}},

    {/*00*/ MFX_ERR_NONE, DEFAULT, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}},
    {/*00*/ MFX_ERR_NONE, EACH_OTHER_FRAME, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}},
    {/*00*/ MFX_ERR_NONE, EACH_WO_BORDERS, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}},
    {/*00*/ MFX_ERR_NONE, EACH_OTHER_MB, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    MFXInit();

    AllocSurfaces();

    // m_par.AsyncDepth = 1;
    mfxU32 nframes = 10;

    SETPARS(m_pPar, MFXPAR);

    // run encoder with default parameters and collect skipped MB indexes per-frame
    Init();
    EncodeFrames(nframes, true);
    Close();
    MFXClose();

    if (tc.mode == EACH_OTHER_FRAME) {
        for (mfxU32 i = 1; i < m_skip_map.size(); i++) {    // skip first I
            memset(m_skip_map[i].Map, i%2, sizeof(mfxU8)*m_skip_map[i].MapSize);
        }
    } else if (tc.mode == EACH_WO_BORDERS) {
        for (mfxU32 i = 1; i < m_skip_map.size(); i++) {    // skip first I
            memset(m_skip_map[i].Map, 0, sizeof(mfxU8)*m_skip_map[i].MapSize);
            m_skip_map[i].Map[0] = 1;
            m_skip_map[i].Map[m_skip_map[i].MapSize-1] = 1;
        }
    } else if (tc.mode == EACH_OTHER_MB) {
        for (mfxU32 i = 1; i < m_skip_map.size(); i++) {    // skip first I
            for (mfxU32 n = 0; n < m_skip_map[i].MapSize; n++)
                m_skip_map[i].Map[n] = n%2;
        }
    }

    MFXInit();
    AllocSurfaces();

    // configure pipeline to use MBDisableSkipMap obtained on prev step
    TestEncoder test_encoder(m_skip_map);
    test_encoder.MFXInit();
    test_encoder.AllocSurfaces();
    mfxExtCodingOption3& co3 = test_encoder.m_par;
    co3.MBDisableSkipMap = MFX_CODINGOPTION_ON;

    // run test pipeline
    g_tsStatus.expect(tc.sts);
    test_encoder.Init();
    test_encoder.EncodeFrames(nframes, true);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_mb_disable_skip_map)
};
