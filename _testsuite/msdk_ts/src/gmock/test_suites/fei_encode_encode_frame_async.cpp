/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"

namespace fei_encode_encode_frame_async
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true) {}

    ~TestSuite()
    {
        if (m_initialized)
        {
            Close();
        }

        std::vector<mfxExtBuffer*> tmp;
        tmp.reserve(in_buffs.size() + out_buffs.size() + cmp_buffs.size());

        tmp.insert(tmp.end(),  in_buffs.begin(),  in_buffs.end());
        tmp.insert(tmp.end(), out_buffs.begin(), out_buffs.end());
        tmp.insert(tmp.end(), cmp_buffs.begin(), cmp_buffs.end());

        for (mfxU32 i = 0; i < tmp.size(); ++i)
        {
            switch (tmp[i]->BufferId)
            {
            case MFX_EXTBUFF_FEI_ENC_CTRL:
            {
                mfxExtFeiEncFrameCtrl* ptr = reinterpret_cast<mfxExtFeiEncFrameCtrl*>(tmp[i]);
                delete ptr;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            {
                mfxExtFeiEncMVPredictors* ptr = reinterpret_cast<mfxExtFeiEncMVPredictors*>(tmp[i]);
                delete[] ptr->MB;
                delete ptr;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MB:
            {
                mfxExtFeiEncMBCtrl* ptr = reinterpret_cast<mfxExtFeiEncMBCtrl*>(tmp[i]);
                delete[] ptr->MB;
                delete ptr;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MV:
            {
                mfxExtFeiEncMV* ptr = reinterpret_cast<mfxExtFeiEncMV*>(tmp[i]);
                delete[] ptr->MB;
                delete ptr;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            {
                mfxExtFeiEncMBStat* ptr = reinterpret_cast<mfxExtFeiEncMBStat*>(tmp[i]);
                delete[] ptr->MB;
                delete ptr;
            }
            break;

            case MFX_EXTBUFF_FEI_PAK_CTRL:
            {
                mfxExtFeiPakMBCtrl* ptr = reinterpret_cast<mfxExtFeiPakMBCtrl*>(tmp[i]);
                delete[] ptr->MB;
                delete ptr;
            }
            break;

            default:
            break;
            }
        }

        tmp.clear();
        in_buffs.clear();
        out_buffs.clear();
        cmp_buffs.clear();
    }

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

    std::vector<mfxExtBuffer*> in_buffs;
    std::vector<mfxExtBuffer*> out_buffs;
    std::vector<mfxExtBuffer*> cmp_buffs;
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

    CHECK_FEI_SUPPORT();

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


    mfxExtFeiEncFrameCtrl* in_efc = new mfxExtFeiEncFrameCtrl;
    memset(in_efc, 0, sizeof(mfxExtFeiEncFrameCtrl));

    in_efc->Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
    in_efc->Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
    in_efc->SearchPath             = 57;
    in_efc->LenSP                  = 57;
    in_efc->SubMBPartMask          = 0x77;
    in_efc->MultiPredL0            = 0;
    in_efc->MultiPredL1            = 0;
    in_efc->SubPelMode             = 3;
    in_efc->InterSAD               = 2;
    in_efc->IntraSAD               = 2;
    in_efc->DistortionType         = 2;
    in_efc->RepartitionCheckEnable = 0;
    in_efc->AdaptiveSearch         = 1;
    in_efc->MVPredictor            = 0;
    in_efc->NumMVPredictors[0]     = 1;
    in_efc->PerMBQp                = 0;
    in_efc->PerMBInput             = 0;
    in_efc->MBSizeCtrl             = 0;
    in_efc->RefHeight              = 40;
    in_efc->RefWidth               = 48;
    in_efc->SearchWindow           = 2;

    SETPARS(in_efc, EXT_FRM_CTRL);
    in_buffs.push_back((mfxExtBuffer*)in_efc);

    if (tc.mode & IN_MV_PRED)
    {
        mfxExtFeiEncMVPredictors* in_mvp = new mfxExtFeiEncMVPredictors;
        memset(in_mvp, 0, sizeof(mfxExtFeiEncMVPredictors));

        in_mvp->Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
        in_mvp->Header.BufferSz = sizeof(mfxExtFeiEncMVPredictors);

        in_mvp->MB = new mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB[num_mb];
        in_mvp->NumMBAlloc = num_mb;
        memset(in_mvp->MB, 0, in_mvp->NumMBAlloc*sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB));

        in_buffs.push_back((mfxExtBuffer*)in_mvp);

        in_efc->MVPredictor        = 1;
        in_efc->NumMVPredictors[0] = 1;
    }

    if (tc.mode & IN_MB_CTRL)
    {
        mfxExtFeiEncMBCtrl* in_mbc = new mfxExtFeiEncMBCtrl;
        memset(in_mbc, 0, sizeof(mfxExtFeiEncMBCtrl));

        in_mbc->Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
        in_mbc->Header.BufferSz = sizeof(mfxExtFeiEncMBCtrl);

        in_mbc->MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[num_mb];
        in_mbc->NumMBAlloc = num_mb;
        memset(in_mbc->MB, 0, in_mbc->NumMBAlloc*sizeof(mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB));

        in_buffs.push_back((mfxExtBuffer*)in_mbc);

        in_efc->PerMBInput = 1;
    }

    if (!in_buffs.empty())
    {
        m_ctrl.ExtParam    = &in_buffs[0];
        m_ctrl.NumExtParam = (mfxU16)in_buffs.size();
    }

    // Create and attach output structures
    mfxExtFeiEncMV *out_mv = NULL, *orig_out_mv = NULL;

    if (tc.mode & OUT_MV)
    {
        // to encode with
        out_mv = new mfxExtFeiEncMV;
        memset(out_mv, 0, sizeof(mfxExtFeiEncMV));

        out_mv->Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
        out_mv->Header.BufferSz = sizeof(mfxExtFeiEncMV);

        out_mv->MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[num_mb];
        out_mv->NumMBAlloc = num_mb;

        memset(out_mv->MB, 0, out_mv->NumMBAlloc * sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));
        out_buffs.push_back((mfxExtBuffer*)out_mv);

        // to compare with
        orig_out_mv = new mfxExtFeiEncMV;
        memset(orig_out_mv, 0, sizeof(mfxExtFeiEncMV));

        orig_out_mv->Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
        orig_out_mv->Header.BufferSz = sizeof(mfxExtFeiEncMV);

        orig_out_mv->MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[num_mb];
        orig_out_mv->NumMBAlloc = num_mb;
        memcpy(orig_out_mv->MB, out_mv->MB, out_mv->NumMBAlloc* sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));

        cmp_buffs.push_back((mfxExtBuffer*)orig_out_mv);
    }


    mfxExtFeiEncMBStat *out_mbstat = NULL, *orig_out_mbstat = NULL;
    if (tc.mode & OUT_MB_STAT)
    {
        // to encode with
        out_mbstat = new mfxExtFeiEncMBStat;
        memset(out_mbstat, 0, sizeof(mfxExtFeiEncMBStat));

        out_mbstat->Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
        out_mbstat->Header.BufferSz = sizeof(mfxExtFeiEncMBStat);

        out_mbstat->MB = new mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB[num_mb];
        out_mbstat->NumMBAlloc = num_mb;
        memset(out_mbstat->MB, 0, sizeof(mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB));

        out_buffs.push_back((mfxExtBuffer*)out_mbstat);

        // to compare with
        orig_out_mbstat = new mfxExtFeiEncMBStat;
        memset(orig_out_mbstat, 0, sizeof(mfxExtFeiEncMBStat));

        orig_out_mbstat->Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
        orig_out_mbstat->Header.BufferSz = sizeof(mfxExtFeiEncMBStat);

        orig_out_mbstat->MB = new mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB[num_mb];
        orig_out_mbstat->NumMBAlloc = num_mb;

        memcpy(orig_out_mbstat->MB, out_mbstat->MB, out_mbstat->NumMBAlloc * sizeof(mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB));

        cmp_buffs.push_back((mfxExtBuffer*)orig_out_mbstat);
    }

    mfxExtFeiPakMBCtrl* out_pakmb = NULL, *orig_out_pakmb = NULL;
    if (tc.mode & OUT_MB_CTRL)
    {
        // to encode with
        out_pakmb = new mfxExtFeiPakMBCtrl;
        memset(out_pakmb, 0, sizeof(mfxExtFeiPakMBCtrl));

        out_pakmb->Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
        out_pakmb->Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);

        out_pakmb->MB = new mfxFeiPakMBCtrl[num_mb];
        out_pakmb->NumMBAlloc = num_mb;
        memset(out_pakmb->MB, 0, out_pakmb->NumMBAlloc*sizeof(mfxFeiPakMBCtrl));

        out_buffs.push_back((mfxExtBuffer*)out_pakmb);

        // to compare with
        orig_out_pakmb = new mfxExtFeiPakMBCtrl;
        memset(orig_out_pakmb, 0, sizeof(mfxExtFeiPakMBCtrl));

        orig_out_pakmb->Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
        orig_out_pakmb->Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);

        orig_out_pakmb->MB = new mfxFeiPakMBCtrl[num_mb];
        orig_out_pakmb->NumMBAlloc = num_mb;
        memcpy(orig_out_pakmb->MB, orig_out_pakmb->MB, out_pakmb->NumMBAlloc * sizeof(mfxFeiPakMBCtrl));

        cmp_buffs.push_back((mfxExtBuffer*)orig_out_pakmb);
    }

    if (!out_buffs.empty())
    {
        m_bitstream.ExtParam    = &out_buffs[0];
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
        EXPECT_NE(0, memcmp(&orig_out_mv->MB, &out_mv->MB, out_mv->NumMBAlloc *sizeof(mfxExtFeiEncMV)))
                    << "ERROR: mfxExtFeiEncMV output should be changed";
    }
    if (tc.mode & OUT_MB_STAT)
    {
        EXPECT_NE(0, memcmp(&orig_out_mbstat->MB, &out_mbstat->MB, out_mbstat->NumMBAlloc * sizeof(mfxExtFeiEncMBStat)))
                    << "ERROR: mfxExtFeiEncMBStat output should be changed";
    }
    if (tc.mode & OUT_MB_CTRL)
    {
        EXPECT_NE(0, memcmp(&orig_out_pakmb->MB, &out_pakmb->MB, out_pakmb->NumMBAlloc * sizeof(mfxExtFeiPakMBCtrl)))
                    << "ERROR: mfxExtFeiEncMBStat output should be changed";
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_encode_frame_async);
};
