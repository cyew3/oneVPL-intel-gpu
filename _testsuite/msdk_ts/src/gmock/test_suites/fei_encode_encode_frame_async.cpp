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

    enum
    {
        IN_FRM_CTRL = 1,
        IN_MV_PRED  = 1 << 1,
        IN_MB_CTRL  = 1 << 2,
        IN_ALL      = 7,

        OUT_MV      = 8,
        OUT_MB_STAT = 8 << 1,
        OUT_MB_CTRL = 8 << 2,
        OUT_ALL     = 56
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
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] = 
{
    // LenSP/MaxLenSP
    {/*00*/ MFX_ERR_NONE, IN_FRM_CTRL|OUT_ALL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 1},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 14}}
    },
    {/*01*/ MFX_ERR_NONE, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 63},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 63}}
    },
    {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 64},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 64}}
    },
    {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 16},
                                        {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 10}}
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

    // Parameters expecting additional ExtBuffers
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MVPredictor, 1}},
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.PerMBQp, 1}},
    {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.PerMBInput, 1}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_FRM_CTRL,
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.MBSizeCtrl, 1}},
    {/*30*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, IN_FRM_CTRL,
           {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchWindow, 1},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefHeight, 32},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefWidth, 32},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 10},
            {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 10}},
    },
    {/*31*/ MFX_ERR_NONE, IN_FRM_CTRL,
              {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchWindow, 1},
               {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefHeight, 0},
               {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.RefWidth, 0},
               {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 0},
               {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 0}},
    }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    tsRawReader stream(g_tsStreamPool.Get("YUV/iceage_720x480_491.yuv"), m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_filler = &stream;

    MFXInit();

    bool hw_surf = m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;
    if (hw_surf)
        SetFrameAllocator(m_session, m_pVAHandle);

    m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    m_pPar->AsyncDepth = 1;
    // set up parameters for case
    SETPARS(m_pPar, MFXPAR);

    Init(m_session, m_pPar);

    ///////////////////////////////////////////////////////////////////////////
    AllocSurfaces();
    AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 1024 * 1024 * 10);

    SetFrameAllocator();

    mfxU32 n = 2;
    mfxU32 num_mb = mfxU32(m_par.mfx.FrameInfo.Width / 16) * mfxU32(m_par.mfx.FrameInfo.Height / 16);

    // Attach input structures
    std::vector<mfxExtBuffer*> in_buffs;

    mfxExtFeiEncFrameCtrl in_efc = {};
    if (tc.mode & IN_FRM_CTRL)
    {
        in_efc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        in_efc.Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
        in_efc.SearchPath = 57;
        in_efc.LenSP = 57;
        in_efc.SubMBPartMask = 0x77;
        in_efc.MultiPredL0 = 0;
        in_efc.MultiPredL1 = 0;
        in_efc.SubPelMode = 3;
        in_efc.InterSAD = 2;
        in_efc.IntraSAD = 2;
        in_efc.DistortionType = 2;
        in_efc.RepartitionCheckEnable = 0;
        in_efc.AdaptiveSearch = 1;
        in_efc.MVPredictor = 0;
        in_efc.NumMVPredictors = 1; //always 4 predictors
        in_efc.PerMBQp = 0;
        in_efc.PerMBInput = 0;
        in_efc.MBSizeCtrl = 0;
        in_efc.RefHeight = 40;
        in_efc.RefWidth = 48;
        in_efc.SearchWindow = 0;

        SETPARS(&in_efc, EXT_FRM_CTRL);
        in_buffs.push_back((mfxExtBuffer*)&in_efc);
    }

    mfxExtFeiEncMVPredictors in_mvp = {};
    if (tc.mode & IN_MV_PRED)
    {
        in_mvp.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
        in_mvp.Header.BufferSz = sizeof(mfxExtFeiEncMVPredictors);
        in_mvp.MB = new mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB[num_mb];
        in_buffs.push_back((mfxExtBuffer*)&in_mvp);
        in_efc.MVPredictor = 1;
        in_efc.NumMVPredictors = 1; //always 4 predictors
    }

    mfxExtFeiEncMBCtrl in_mbc = {};
    if (tc.mode & IN_MB_CTRL)
    {
        in_mbc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
        in_mbc.Header.BufferSz = sizeof(mfxExtFeiEncMBCtrl);
        in_mbc.MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[num_mb];
        in_buffs.push_back((mfxExtBuffer*)&in_mbc);
        in_efc.PerMBInput = 1;
    }
    if (!in_buffs.empty())
    {
        m_ctrl.ExtParam = &in_buffs[0];
        m_ctrl.NumExtParam = (mfxU16)in_buffs.size();
    }

    // Create and attach output structures
    std::vector<mfxExtBuffer*> out_buffs;

    mfxExtFeiEncMV out_mv = {};
    if (tc.mode & OUT_MV)
    {
        out_mv.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
        out_mv.Header.BufferSz = sizeof(mfxExtFeiEncMV);
        out_mv.MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[num_mb];
        out_mv.NumMBAlloc = num_mb;
        memset(out_mv.MB, 0, sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));
        out_buffs.push_back((mfxExtBuffer*)&out_mv);
    }
    mfxExtFeiEncMV orig_out_mv = {};
    if (tc.mode & OUT_MV)
    {
        orig_out_mv.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
        orig_out_mv.Header.BufferSz = sizeof(mfxExtFeiEncMV);
        orig_out_mv.MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[num_mb];
        orig_out_mv.NumMBAlloc = num_mb;
        memcpy(orig_out_mv.MB, out_mv.MB, out_mv.NumMBAlloc* sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));
    }

    mfxExtFeiEncMBStat out_mbstat = {};
    if (tc.mode & OUT_MB_STAT)
    {
        out_mbstat.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
        out_mbstat.Header.BufferSz = sizeof(mfxExtFeiEncMBStat);
        out_mbstat.MB = new mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB[num_mb];
        out_mbstat.NumMBAlloc = num_mb;
        memset(out_mbstat.MB, 0, sizeof(mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB));
        out_buffs.push_back((mfxExtBuffer*)&out_mbstat);
    }
    mfxExtFeiEncMBStat orig_out_mbstat = {};
    if (tc.mode & OUT_MB_STAT)
    {
        orig_out_mbstat.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
        orig_out_mbstat.Header.BufferSz = sizeof(mfxExtFeiEncMBStat);
        orig_out_mbstat.MB = new mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB[num_mb];
        orig_out_mbstat.NumMBAlloc = num_mb;
        memcpy(orig_out_mbstat.MB, out_mbstat.MB, out_mbstat.NumMBAlloc * sizeof(mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB));
    }

    mfxExtFeiPakMBCtrl out_pakmb = {};
    if (tc.mode & OUT_MB_CTRL)
    {
        out_pakmb.Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
        out_pakmb.Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);
        out_pakmb.MB = new mfxFeiPakMBCtrl[num_mb];
        out_pakmb.NumMBAlloc = num_mb;
        memset(out_pakmb.MB, 0, sizeof(mfxFeiPakMBCtrl));
        out_buffs.push_back((mfxExtBuffer*)&out_pakmb);
    }
    mfxExtFeiPakMBCtrl orig_out_pakmb = {};
    if (tc.mode & OUT_MB_CTRL)
    {
        orig_out_pakmb.Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
        orig_out_pakmb.Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);
        orig_out_pakmb.MB = new mfxFeiPakMBCtrl[num_mb];
        orig_out_pakmb.NumMBAlloc = num_mb;
        memcpy(orig_out_pakmb.MB, orig_out_pakmb.MB, out_pakmb.NumMBAlloc * sizeof(mfxFeiPakMBCtrl));
    }

    if (!out_buffs.empty())
    {
        m_bitstream.ExtParam = &out_buffs[0];
        m_bitstream.NumExtParam = (mfxU16)out_buffs.size();
    }

    g_tsStatus.expect(tc.sts);
    mfxStatus st;
    if (tc.sts == MFX_ERR_NONE) {
        EncodeFrames(n, true);
    } else {
        m_pSurf = GetSurface();
        st = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);
        while (st == MFX_ERR_MORE_DATA)
        {
            m_pSurf = GetSurface();
            st = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);
        }
    }

    g_tsStatus.expect(MFX_ERR_NONE);

    if (tc.mode & OUT_MV)
    {
        EXPECT_NE(0, memcmp(&orig_out_mv.MB, &out_mv.MB, out_mv.NumMBAlloc *sizeof(mfxExtFeiEncMV)))
                    << "ERROR: mfxExtFeiEncMV output should be changed";
    }
    if (tc.mode & OUT_MB_STAT)
    {
        EXPECT_NE(0, memcmp(&orig_out_mbstat.MB, &out_mbstat.MB, out_mbstat.NumMBAlloc * sizeof(mfxExtFeiEncMBStat)))
                    << "ERROR: mfxExtFeiEncMBStat output should be changed";
    }
    if (tc.mode & OUT_MB_CTRL)
    {
        EXPECT_NE(0, memcmp(&orig_out_pakmb.MB, &out_pakmb.MB, out_pakmb.NumMBAlloc * sizeof(mfxExtFeiPakMBCtrl)))
                    << "ERROR: mfxExtFeiEncMBStat output should be changed";
    }

    if (NULL != out_mv.MB)
    {
        delete out_mv.MB;
        out_mv.MB = NULL;
    }
    if (NULL != orig_out_mv.MB)
    {
        delete orig_out_mv.MB;
        orig_out_mv.MB = NULL;
    }

    if (NULL != out_mbstat.MB)
    {
        delete out_mbstat.MB;
        out_mbstat.MB = NULL;
    }
    if (NULL != orig_out_mbstat.MB)
    {
        delete orig_out_mbstat.MB;
        orig_out_mbstat.MB = NULL;
    }

    if (NULL != out_pakmb.MB)
    {
        delete out_pakmb.MB;
        out_pakmb.MB = NULL;
    }
    if (NULL != orig_out_pakmb.MB)
    {
        delete orig_out_pakmb.MB;
        orig_out_pakmb.MB = NULL;
    }

    if (NULL != in_mvp.MB)
    {
        delete in_mvp.MB;
        in_mvp.MB = NULL;
    }

    if (NULL != in_mbc.MB)
    {
        delete in_mbc.MB;
        in_mbc.MB = NULL;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_encode_frame_async);
};
