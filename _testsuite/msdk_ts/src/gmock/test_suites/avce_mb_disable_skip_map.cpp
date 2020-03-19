/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019-2020 Intel Corporation. All Rights Reserved.

File Name: avce_mb_disable_skip_map.cpp

\* ****************************************************************************** */


#include "ts_encoder.h"
#include "ts_struct.h"
#include "bs_parser++.h"

namespace avce_mb_disable_skip_map
{

#define IS_MB_SKIPPED(mbtype)\
  ((mbtype == BS_AVC2::P_Skip || mbtype == BS_AVC2::B_Skip) ? true : false)
#define WIDTH 352
#define HEIGHT 288
#define N_FRAMES 10

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public BS_AVC2_parser
{
    std::vector<mfxExtMBDisableSkipMap> m_non_skip_map;
    tsRawReader* m_reader;
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_AVC)
        , BS_AVC2_parser(BS_AVC2::INIT_MODE_PARSE_SD)
        , m_reader(nullptr)
    {
        m_filler = this;
        m_bs_processor = this;
    }
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < m_non_skip_map.size(); i++) {
            if (m_non_skip_map[i].Map) {
                delete[] m_non_skip_map[i].Map;
                m_non_skip_map[i].Map = nullptr;
            }
        }
        m_non_skip_map.clear();
        if (m_reader)
            delete m_reader;
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
            memset(frm_map.reserved, 0, sizeof(frm_map.reserved));

            // TODO: check for alignment
            frm_map.MapSize = m_par.mfx.FrameInfo.Width / 16 * m_par.mfx.FrameInfo.Height / 16;
            frm_map.Map = new mfxU8[frm_map.MapSize];
            memset(frm_map.Map, 0, sizeof(mfxU8) * frm_map.MapSize);

            for (Bs32u i = 0; i < au->NumUnits; i++)
            {
                if (au->nalu[i]->nal_unit_type == BS_AVC2::SLICE_NONIDR)
                {
                    for (Bs32u nmb = 0; nmb < au->nalu[i]->slice->NumMB; nmb++)
                    {
                        if (!IS_MB_SKIPPED(au->nalu[i]->slice->mb[nmb].MbType))
                            frm_map.Map[nmb] = 1;
                    }
                }
            }
            m_non_skip_map.push_back(frm_map);
        }

        bs.DataOffset = 0;
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        m_reader->ProcessSurface(s);
        return MFX_ERR_NONE;
    }
};

class TestEncoder : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public BS_AVC2_parser
{
    std::vector<mfxExtMBDisableSkipMap> m_non_skip_map;
    std::vector<mfxEncodeCtrl> m_ctrls;
    mfxU32 m_curr_in_frame;
    mfxU32 m_curr_out_frame;
public:
    mfxU32 m_unexpected_skipped_MB;
    tsRawReader* m_reader;
    mfxStatus Init()
    {
        return tsVideoEncoder::Init(this->m_session, this->m_pPar);
    }

    TestEncoder(std::vector<mfxExtMBDisableSkipMap> skip_map)
        : tsVideoEncoder(MFX_CODEC_AVC)
        , BS_AVC2_parser(BS_AVC2::INIT_MODE_PARSE_SD)
        , m_non_skip_map(skip_map)
        , m_reader(nullptr)
        , m_unexpected_skipped_MB(0)
    {
        m_curr_in_frame = 0;
        m_curr_out_frame = 0;
        m_filler = this;
        m_bs_processor = this;
    }
    ~TestEncoder()
    {
        if (m_reader)
            delete m_reader;
    }

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

                        if (m_non_skip_map[m_curr_out_frame].Map[nmb] == 1)
                            exp_nonskip++;

                        if (IS_MB_SKIPPED(au->nalu[i]->slice->mb[nmb].MbType) &&
                            (m_non_skip_map[m_curr_out_frame].Map[nmb] == 1))
                        {
                            g_tsLog << "ERROR: unexpected skipped MB" << "\n"
                                    << "frame#" << m_curr_out_frame << " MB#" << nmb
                                    << " (MbType=" << au->nalu[i]->slice->mb[nmb].MbType << ")\n";
                            m_unexpected_skipped_MB++;
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
        if (m_curr_in_frame >= m_non_skip_map.size())
            return MFX_ERR_NONE;

        m_ctrls.push_back(m_ctrl);

        m_pCtrl = &m_ctrls[m_curr_in_frame];

        m_pCtrl->NumExtParam = 1;
        if (!m_pCtrl->ExtParam)
            m_pCtrl->ExtParam = new mfxExtBuffer*[1];

        m_pCtrl->ExtParam[0] = (mfxExtBuffer*)&m_non_skip_map[m_curr_in_frame++];
        m_reader->ProcessSurface(s);
        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, DEFAULT, {}},
    {/*01*/ MFX_ERR_NONE, EACH_OTHER_FRAME, {}},
    {/*02*/ MFX_ERR_NONE, EACH_WO_BORDERS, {}},
    {/*03*/ MFX_ERR_NONE, EACH_OTHER_MB, {}},

    {/*04*/ MFX_ERR_NONE, DEFAULT, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}},
    {/*05*/ MFX_ERR_NONE, EACH_OTHER_FRAME, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}},
    {/*06*/ MFX_ERR_NONE, EACH_WO_BORDERS, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}},
    {/*07*/ MFX_ERR_NONE, EACH_OTHER_MB, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    MFXInit();

    AllocSurfaces();

    SETPARS(m_pPar, MFXPAR);

    m_par.AsyncDepth = 1;
    m_par.mfx.GopRefDist = 1;
    m_par.mfx.NumRefFrame = 1;
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_par.mfx.FrameInfo.Width = WIDTH;
    m_par.mfx.FrameInfo.Height = HEIGHT;
    m_par.mfx.FrameInfo.CropW = WIDTH;
    m_par.mfx.FrameInfo.CropH = HEIGHT;

    m_reader = (new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif_10.yuv"), m_par.mfx.FrameInfo, N_FRAMES));
    g_tsStreamPool.Reg();
    // run encoder with default parameters and collect skipped MB indexes per-frame
    Init();
    EncodeFrames(N_FRAMES, true);
    Close();
    MFXClose();

    if (tc.mode == EACH_OTHER_FRAME) {
        for (mfxU32 i = 1; i < m_non_skip_map.size(); i++) {    // skip first I
            memset(m_non_skip_map[i].Map, i%2, sizeof(mfxU8)*m_non_skip_map[i].MapSize);
        }
    } else if (tc.mode == EACH_WO_BORDERS) {
        for (mfxU32 i = 1; i < m_non_skip_map.size(); i++) {    // skip first I
            memset(m_non_skip_map[i].Map, 0, sizeof(mfxU8)*m_non_skip_map[i].MapSize);
            m_non_skip_map[i].Map[0] = 1;
            m_non_skip_map[i].Map[m_non_skip_map[i].MapSize-1] = 1;
        }
    } else if (tc.mode == EACH_OTHER_MB) {
        for (mfxU32 i = 1; i < m_non_skip_map.size(); i++) {    // skip first I
            for (mfxU32 n = 0; n < m_non_skip_map[i].MapSize; n++)
                m_non_skip_map[i].Map[n] = n%2;
        }
    }

    MFXInit();
    AllocSurfaces();

    // configure pipeline to use MBDisableSkipMap obtained on prev step
    TestEncoder test_encoder(m_non_skip_map);
    test_encoder.MFXInit();
    test_encoder.AllocSurfaces();
    mfxExtCodingOption3& co3 = test_encoder.m_par;
    co3.MBDisableSkipMap = MFX_CODINGOPTION_ON;

    test_encoder.m_par = m_par;
    test_encoder.m_reader = (new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif_10.yuv"), test_encoder.m_par.mfx.FrameInfo, N_FRAMES));
    g_tsStreamPool.Reg();

    // run test pipeline
    g_tsStatus.expect(tc.sts);
    test_encoder.Init();
    test_encoder.EncodeFrames(N_FRAMES, true);

    if (test_encoder.m_unexpected_skipped_MB > 0)
    {
        g_tsLog << "ERROR: unexpected skipped MBs: " << test_encoder.m_unexpected_skipped_MB << '\n';
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_mb_disable_skip_map)
};
