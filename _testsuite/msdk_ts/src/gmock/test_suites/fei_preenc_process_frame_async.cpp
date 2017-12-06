/************************************************************************** *\

Copyright (C) 2017 Intel Corporation.  All rights reserved.

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

******************************************************************************/
#include "ts_preenc.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"


namespace fei_preenc_process_frame_async
{
    class TestSuite : public tsVideoPreENC, public tsSurfaceProcessor
    {
        public:
            TestSuite()
                : tsVideoPreENC(MFX_FEI_FUNCTION_PREENC, MFX_CODEC_AVC, true)
                , m_encCtrl{0}
                , m_encMVP{0}
                , m_encQP{0}
                , m_mbStat{0}
                , m_encMV{0}
            {
                m_filler = this;

                // set default parameters for mfxVideoParam
                m_pPar->mfx.FrameInfo.CropW  = m_pPar->mfx.FrameInfo.Width  = 352;
                m_pPar->mfx.FrameInfo.CropH  = m_pPar->mfx.FrameInfo.Height = 288;
                m_pPar->mfx.FrameInfo.CropX  = 0;
                m_pPar->mfx.FrameInfo.CropY  = 0;
                m_pPar->mfx.NumRefFrame      = 0;
                m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                m_pPar->AsyncDepth           = 1;

                m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/foreman_352x288_300.yuv"),
                                           m_pPar->mfx.FrameInfo);
                g_tsStreamPool.Reg();
            }

            ~TestSuite()
            {
                delete m_reader;
            }

            int RunTest(unsigned int id);
            static const unsigned int n_cases;

            mfxStatus ProcessSurface(mfxFrameSurface1& s)
            {
                mfxStatus sts = m_reader->ProcessSurface(s);

                return sts;
            }

    private:
            tsRawReader *m_reader;

            enum
            {
                IN_CTRL     = 1,
                IN_MV_PRED  = 1 << 1,
                IN_QP       = 1 << 2,
                IN_ALL      = 7,

                OUT_MV      = 8,
                OUT_MB_STAT = 8 << 1,
                OUT_ALL     = 24
            };

            enum
            {
                EXT_CTRL = 1,
                EXT_MV_PRED,
                EXT_QP,
                EXT_MV,
                EXT_MB_STAT
            };

            struct tc_struct
            {
                mfxStatus sts;
                mfxU32 mode;
                struct f_pair
                {
                    mfxU32 ext_type;
                    const tsStruct::Field* f;
                    mfxU32 v;
                } set_par[MAX_NPARS];
            };

            static const tc_struct test_case[];

            std::vector<mfxExtBuffer*>  m_in_buffs;
            std::vector<mfxExtBuffer*>  m_out_buffs;
            mfxExtFeiPreEncCtrl         m_encCtrl;
            mfxExtFeiPreEncMVPredictors m_encMVP;
            mfxExtFeiEncQP              m_encQP;
            mfxExtFeiPreEncMBStat       m_mbStat;
            mfxExtFeiPreEncMV           m_encMV;
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // SubMBPartMask
        {/*00*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SubMBPartMask, 0x00} // enable all
        },
        {/*01*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SubMBPartMask, 0x01} // disable 16x16
        },
        {/*02*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SubMBPartMask, 0x40} // disable 4x4
        },
        {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SubMBPartMask, 0x7f} // disable all
        },

        // IntraPartMask
        {/*04*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.IntraPartMask, 0x00} // enable all
        },
        {/*05*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.IntraPartMask, 0x01} // disable 16x16
        },
        {/*06*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.IntraPartMask, 0x04} // disable 4x4
        },
        {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.IntraPartMask, 0x07} // disable all
        },

        // SearchWindow
        {/*08*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SearchWindow, 1} // Tiny Diamond
        },
        {/*09*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SearchWindow, 2} // small Diamond
        },
        {/*10*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SearchWindow, 3} // Diamond
        },
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SearchWindow, 0} // Wrong param
        },

        // SubPelMode
        {/*12*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SubPelMode, 0x03} // quarter-pixel motion estimation
        },
        {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.SubPelMode, 0x02} // invalid
        },

        // InterSAD
        {/*14*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.InterSAD, 0x00} // none
        },
        {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.InterSAD, 0x01} // invalid
        },

        // InterSAD
        {/*16*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.IntraSAD, 0x00} // none
        },
        {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.IntraSAD, 0x01} // invalid
        },

        // MVPredictor
        {/*18*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MVPredictor, 0x00} // disable usage of predictor
        },
        {/*19*/ MFX_ERR_NONE, IN_MV_PRED,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MVPredictor, 0x01} // enable predictor for L0 reference
        },
        {/*20*/ MFX_ERR_NONE, IN_MV_PRED,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MVPredictor, 0x02} // enable predictor for L1 reference
        },
        {/*21*/ MFX_ERR_NONE, IN_MV_PRED,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MVPredictor, 0x03} // enable both
        },
        {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MVPredictor, 0x04} // invalid
        },

        // DownsampleInput
        {/*23*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DownsampleInput, MFX_CODINGOPTION_ON}
        },
        {/*24*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DownsampleInput, MFX_CODINGOPTION_OFF}
        },
        {/*25*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DownsampleInput, MFX_CODINGOPTION_UNKNOWN}
        },
        {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DownsampleInput, 0x30}
        },

        // Picturetype
        {/*27*/ MFX_ERR_NONE, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.PictureType, MFX_PICTYPE_FRAME}
        },
        {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.PictureType, 0x03}
        },

        // Parameters expecting additional ExtBuffers
        {/*29*/ MFX_ERR_NONE, IN_MV_PRED,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MVPredictor, 1},
                {EXT_MV_PRED, &tsStruct::mfxExtFeiPreEncMVPredictors.NumMBAlloc, 396}}
        },
        {/*30*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IN_MV_PRED,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MVPredictor, 1},
                {EXT_MV_PRED, &tsStruct::mfxExtFeiPreEncMVPredictors.NumMBAlloc, 197}}
        },
        {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.FTEnable, 1},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MBQp, 0},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.Qp, 0}}
        },
        {/*32*/ MFX_ERR_NONE, IN_CTRL,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.FTEnable, 1},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MBQp, 0},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.Qp, 26}}
        },
        {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, IN_CTRL,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.FTEnable, 1},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MBQp, 0},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.Qp, 52}}
        },
        {/*34*/ MFX_ERR_NONE, IN_QP,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.FTEnable, 1},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MBQp, 1},
                {EXT_QP, &tsStruct::mfxExtFeiEncQP.NumMBAlloc, 396}}
        },
        {/*35*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IN_QP,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.FTEnable, 1},
                {EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.MBQp, 1},
                {EXT_QP, &tsStruct::mfxExtFeiEncQP.NumMBAlloc, 197}}
        },

        //DisableMVOutput
        {/*36*/ MFX_ERR_NONE, OUT_MV,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DisableMVOutput, false}}
        },
        {/*36*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, OUT_MV,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DisableMVOutput, false},
                {EXT_MV, &tsStruct::mfxExtFeiPreEncMV.NumMBAlloc, 196}}
        },
        {/*37*/ MFX_ERR_UNDEFINED_BEHAVIOR, 0,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DisableMVOutput, false}}
        },

        //DisableStatisticsOutput
        {/*38*/ MFX_ERR_NONE, OUT_MB_STAT,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DisableStatisticsOutput, false}}
        },
        {/*39*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, OUT_MB_STAT,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DisableStatisticsOutput, false},
                {EXT_MB_STAT, &tsStruct::mfxExtFeiPreEncMBStat.NumMBAlloc, 195}}
        },
        {/*40*/ MFX_ERR_UNDEFINED_BEHAVIOR, 0,
            {{EXT_CTRL, &tsStruct::mfxExtFeiPreEncCtrl.DisableStatisticsOutput, false}}
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        mfxStatus sts;

        //Calc the number of MB in one frame, only PROG case
        mfxU16 widthMB  = MSDK_ALIGN16(m_par.mfx.FrameInfo.Width);
        mfxU16 heightMB = MSDK_ALIGN16(m_par.mfx.FrameInfo.Height);
        mfxU32 numMB    = (widthMB * heightMB) >> 8;

        // Init in and out parameter for preenc call
        m_encCtrl.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
        m_encCtrl.Header.BufferSz = sizeof(mfxExtFeiPreEncCtrl);
        m_encCtrl.PictureType             = MFX_PICTYPE_FRAME;
        m_encCtrl.RefPictureType[0]       = MFX_PICTYPE_FRAME;
        m_encCtrl.RefPictureType[1]       = MFX_PICTYPE_FRAME;
        m_encCtrl.DownsampleReference[0]  = MFX_CODINGOPTION_ON;
        m_encCtrl.DownsampleReference[1]  = MFX_CODINGOPTION_ON;
        m_encCtrl.DisableMVOutput         = true;
        m_encCtrl.DisableStatisticsOutput = true;
        m_encCtrl.LenSP                   = 48;
        m_encCtrl.RefHeight               = 32;
        m_encCtrl.RefWidth                = 32;
        m_encCtrl.SubPelMode              = 0x03;
        m_encCtrl.SearchWindow            = 5;
        m_encCtrl.Qp                      = 26;
        m_encCtrl.DownsampleInput         = MFX_CODINGOPTION_ON;
        //Apply the parameter configuration for this case
        SETPARS(&m_encCtrl, EXT_CTRL);

        m_in_buffs.push_back((mfxExtBuffer*)&m_encCtrl);

        if (m_encCtrl.MVPredictor && (tc.mode & IN_MV_PRED))
        {
            m_encMVP.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV_PRED;
            m_encMVP.Header.BufferSz = sizeof(mfxExtFeiPreEncMVPredictors);
            m_encMVP.NumMBAlloc = numMB;
            m_encMVP.MB = new mfxExtFeiPreEncMVPredictors::mfxExtFeiPreEncMVPredictorsMB[numMB];
            memset(m_encMVP.MB, 0, sizeof(m_encMVP.MB[0]) * numMB);
            //Do this (might include setting of NumMBAlloc) after memory allocation for "MB".
            SETPARS(&m_encMVP, EXT_MV_PRED);
            m_in_buffs.push_back((mfxExtBuffer*)&m_encMVP);
        }

        if (m_encCtrl.FTEnable)
        {
            if (m_encCtrl.MBQp && (tc.mode & IN_QP))
            {
                m_encQP.Header.BufferId = MFX_EXTBUFF_FEI_ENC_QP;
                m_encQP.Header.BufferSz = sizeof(mfxExtFeiEncQP);
                m_encQP.NumMBAlloc = numMB;
                m_encQP.MB = new mfxU8[numMB];
                memset(m_encQP.MB, 0, sizeof(m_encQP.MB[0]) * numMB);
                SETPARS(&m_encQP, EXT_QP);
                m_in_buffs.push_back((mfxExtBuffer*)&m_encQP);
            }
        }

        if (!m_in_buffs.empty())
        {
            m_pPreENCInput->NumExtParam = (mfxU16)m_in_buffs.size();
            m_pPreENCInput->ExtParam    = m_in_buffs.data();
        }

        // Create and attach output structures
        if (!m_encCtrl.DisableMVOutput && (tc.mode & OUT_MV))
        {
            m_encMV.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
            m_encMV.Header.BufferSz = sizeof(mfxExtFeiPreEncMV);
            m_encMV.NumMBAlloc = numMB;
            m_encMV.MB = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[numMB];
            memset(m_encMV.MB, 0, sizeof(m_encMV.MB[0]) * numMB);
            SETPARS(&m_encMV, EXT_MV);
            m_out_buffs.push_back((mfxExtBuffer*)&m_encMV);
        }

        if (!m_encCtrl.DisableStatisticsOutput && (tc.mode & OUT_MB_STAT))
        {
            m_mbStat.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
            m_mbStat.Header.BufferSz = sizeof(mfxExtFeiPreEncMBStat);
            m_mbStat.NumMBAlloc = numMB;
            m_mbStat.MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[numMB];
            memset(m_mbStat.MB, 0, sizeof(m_mbStat.MB[0]) * numMB);
            SETPARS(&m_mbStat, EXT_MB_STAT);
            m_out_buffs.push_back((mfxExtBuffer*)&m_mbStat);
        }

        if (!m_out_buffs.empty())
        {
            m_pPreENCOutput->NumExtParam = (mfxU16)m_out_buffs.size();
            m_pPreENCOutput->ExtParam    = m_out_buffs.data();
        }


        Init();

        // one surface is enough for this test
        m_request.NumFrameMin       = 1;
        m_request.NumFrameSuggested = 1;
        m_request.Type = MFX_MEMTYPE_FROM_ENC |
                         MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET |
                         MFX_MEMTYPE_EXTERNAL_FRAME;
        m_request.Info    = m_par.mfx.FrameInfo;
        m_request.AllocId = m_par.AllocId;
        AllocSurfaces();

        m_pPreENCInput->InSurface = GetSurface();
        m_filler->ProcessSurface(m_pPreENCInput->InSurface, m_pFrameAllocator);

        g_tsStatus.expect(tc.sts);
        sts = ProcessFrameAsync(m_session, m_pPreENCInput, m_pPreENCOutput, m_pSyncPoint);
        g_tsStatus.check(sts);

        g_tsStatus.expect(MFX_ERR_NONE);
        Close();

        if (m_encMVP.MB) {
            delete[] m_encMVP.MB;
            m_encMVP.MB = NULL;
        }
        if (m_encQP.MB) {
            delete[] m_encQP.MB;
            m_encQP.MB = NULL;
        }
        if (m_encMV.MB) {
            delete[] m_encMV.MB;
            m_encMV.MB = NULL;
        }
        if (m_mbStat.MB) {
            delete[] m_mbStat.MB;
            m_mbStat.MB = NULL;
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(fei_preenc_process_frame_async);
};
