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

    enum
    {
        MFXPAR = 1,
        EXT_FRM_CTRL
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
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] = 
{
    // LenSP/MaxLenSP
    {/*00*/ MFX_ERR_NONE, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 1},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MaxLenSP, 14}}
    },
    {/*01*/ MFX_ERR_NONE, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 63},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MaxLenSP, 63}}
    },
    {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 64},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MaxLenSP, 64}}
    },
    {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 16},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MaxLenSP, 10}}
    },

    // SubMBPartMask
    {/*04*/ MFX_ERR_NONE, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SubMBPartMask, 0x00} // enable all
    },
    {/*05*/ MFX_ERR_NONE, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SubMBPartMask, 0x01} // disable 16x16
    },
    {/*06*/ MFX_ERR_NONE, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SubMBPartMask, 0x40} // disable 4x4
    },
    {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SubMBPartMask, 0x7f} // disable all
    },

    // IntraPartMask
    {/*08*/ MFX_ERR_NONE, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.IntraPartMask, 0x00} // enable all
    },
    {/*09*/ MFX_ERR_NONE, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.IntraPartMask, 0x01} // disable 16x16
    },
    {/*10*/ MFX_ERR_NONE, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.IntraPartMask, 0x04} // disable 4x4
    },
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.IntraPartMask, 0x03} // disable all
    },

    // MultiPredL0/MultiPredL1
    {/*12*/ MFX_ERR_NONE, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL0, 1},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL1, 1}}
    },

    // SubPelMode
    {/*13*/ MFX_ERR_NONE, IN_FRM_CTRL, {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SubPelMode, 1}},
    {/*14*/ MFX_ERR_NONE, IN_FRM_CTRL, {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SubPelMode, 3}},

    // InterSAD/IntraSAD
    {/*15*/ MFX_ERR_NONE, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.InterSAD, 2},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.IntraSAD, 2}}
    },

    // DistortionType, AdaptiveSearch, RepartitionCheckEnable
    {/*16*/ MFX_ERR_NONE, IN_FRM_CTRL, {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.DistortionType, 1}},
    {/*17*/ MFX_ERR_NONE, IN_FRM_CTRL, {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.AdaptiveSearch, 1}},
    {/*18*/ MFX_ERR_NONE, IN_FRM_CTRL, {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RepartitionCheckEnable, 1}},

    // RefWidth, RefHeight, SearchWindow
    {/*19*/ MFX_ERR_NONE, IN_FRM_CTRL,
           {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL0, 1},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefWidth, 64},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefHeight, 32}},
    },
    {/*20*/ MFX_ERR_NONE, IN_FRM_CTRL,
           {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL1, 1},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefWidth, 64},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefHeight, 32}},
    },
    {/*21*/ MFX_ERR_NONE, IN_FRM_CTRL,
           {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL0, 1},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL1, 1},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefWidth, 32},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefHeight, 32}},
    },
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
           {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL0, 1},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MultiPredL1, 1},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefWidth, 36},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefHeight, 36}},
    },
    {/*23*/ MFX_ERR_NONE, IN_FRM_CTRL,
           {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchWindow, 1}
    },
    {/*24*/ MFX_ERR_NONE, IN_FRM_CTRL,
           {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchWindow, 5}
    },
    {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
           {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchWindow, 6}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

#define SETPARS(p, type)\
for(mfxU32 i = 0; i < n_par; i++) \
{ \
    if(tc.set_par[i].f && tc.set_par[i].ext_type == type) \
    { \
        tsStruct::set(p, *tc.set_par[i].f, tc.set_par[i].v); \
    } \
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    m_pPar->AsyncDepth = 1;
    // set up parameters for case
    SETPARS(m_pPar, MFXPAR);

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
        in_efc.LenSP = 4;
        in_efc.MaxLenSP = 8;

        in_efc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        in_efc.Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
        SETPARS(&in_efc, EXT_FRM_CTRL);
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
