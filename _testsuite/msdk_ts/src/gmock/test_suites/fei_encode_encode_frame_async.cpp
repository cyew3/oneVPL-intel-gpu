#include "ts_encoder.h"
#include "ts_struct.h"

namespace fei_encode_encode_frame_async
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        IN_FRM_CTRL = 1,
        IN_MV_PRED  = 1 << 1,
        IN_MB_CTRL  = 1 << 2,
        IN_ALL      = 7,

        OUT_MV      = 8,
        OUT_MB_STAT = 8 << 1,
        OUT_MB_CTRL = 8 << 2,
        OUT_ALL     = 63
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/*00*/ MFX_ERR_NONE, IN_FRM_CTRL, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    m_pPar->AsyncDepth = 1;
    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    Init(m_session, m_pPar);

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    AllocSurfaces();
    SetFrameAllocator();

    mfxU32 encoded = 0;
    mfxU32 submitted = 0;
    mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
    mfxSyncPoint sp;

    mfxU32 n = 2;
    mfxU32 num_mb = mfxU32(m_par.mfx.FrameInfo.Width / 16) * mfxU32(m_par.mfx.FrameInfo.Height / 16);
    async = TS_MIN(n, async - 1);

    AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 1024 * 1024 * n);

    // Attach input structures
    std::vector<mfxExtBuffer*> in_buffs;

    mfxExtFeiEncFrameCtrl in_efc = {};
    if (tc.mode & IN_FRM_CTRL)
    {
        in_efc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        in_efc.Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
        in_buffs.push_back((mfxExtBuffer*)&in_efc);
    }

    mfxExtFeiEncMVPredictors in_mvp = {};
    if (tc.mode & IN_MV_PRED)
    {
        in_mvp.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
        in_mvp.Header.BufferSz = sizeof(mfxExtFeiEncMVPredictors);
        in_mvp.MB = new mfxExtFeiEncMVPredictors::mfxMB[num_mb];
        in_buffs.push_back((mfxExtBuffer*)&in_mvp);
    }

    mfxExtFeiEncMBCtrl in_mbc = {};
    if (tc.mode & IN_MB_CTRL)
    {
        in_mbc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
        in_mbc.Header.BufferSz = sizeof(mfxExtFeiEncMBCtrl);
        in_mbc.MB = new mfxExtFeiEncMBCtrl::mfxMB[num_mb];
        in_buffs.push_back((mfxExtBuffer*)&in_mbc);
    }
    if (!in_buffs.empty())
    {
        m_ctrl.ExtParam = &in_buffs[0];
        m_ctrl.NumExtParam = in_buffs.size();
    }

    // Create and attach output structures
    std::vector<mfxExtBuffer*> out_buffs;

    mfxExtFeiEncMV out_mv = {};
    if (tc.mode & OUT_MV)
    {
        out_mv.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
        out_mv.Header.BufferSz = sizeof(mfxExtFeiEncMV);
        out_mv.MB = new mfxExtFeiEncMV::mfxMB[num_mb];
        out_buffs.push_back((mfxExtBuffer*)&out_mv);
    }
    if (tc.mode & OUT_MB_STAT)
    {
        mfxExtFeiEncMBStat out_mbstat = {};
        out_mbstat.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
        out_mbstat.Header.BufferSz = sizeof(mfxExtFeiEncMBStat);
        out_mbstat.MB = new mfxExtFeiEncMBStat::mfxMB[num_mb];
        out_buffs.push_back((mfxExtBuffer*)&out_mbstat);
    }
    if (tc.mode & OUT_MB_CTRL)
    {
        mfxExtFeiPakMBCtrl out_pakmb = {};
        out_pakmb.Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
        out_pakmb.Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);
        out_pakmb.MB = new mfxFeiPakMBCtrl[num_mb];
        out_buffs.push_back((mfxExtBuffer*)&out_pakmb);
    }

    if (!out_buffs.empty())
    {
        m_bitstream.ExtParam = &out_buffs[0];
        m_bitstream.NumExtParam = out_buffs.size();
    }

    m_pSurf = GetSurface();
    while(encoded < n)
    {
        mfxStatus mfxRes = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);

        if(MFX_ERR_MORE_DATA == mfxRes)
        {
            if(!m_pSurf)
            {
                m_pSurf = GetSurface();
                if(submitted)
                {
                    encoded += submitted;
                    SyncOperation(sp);
                }
                break;
            }
            continue;
        }

        g_tsStatus.check();
        sp = m_syncpoint;

        if(++submitted >= async)
        {
            SyncOperation();
            encoded += submitted;
            submitted = 0;
            async = TS_MIN(async, (n - encoded));
        }
    }

    g_tsLog << encoded << " FRAMES ENCODED\n";
    g_tsStatus.check();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_encode_frame_async);
};
