/******************************************************************************* *\

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

*******************************************************************************/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"

namespace hevce_fei_invalid_runtime_parameters
{

    const mfxU32 frameToChangeCtrl = 2;

    void SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl)
    {
        memset(&ctrl, 0, sizeof(ctrl));

        ctrl.Header.BufferId = MFX_EXTBUFF_HEVCFEI_ENC_CTRL;
        ctrl.Header.BufferSz = sizeof(mfxExtFeiHevcEncFrameCtrl);
        ctrl.SubPelMode         = 3; // quarter-pixel motion estimation
        ctrl.SearchWindow       = 5; // 48 SUs 48x40 window full search
        ctrl.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
        // enable internal L0/L1 predictors: 1 - spatial predictors
        ctrl.MultiPred[0] = ctrl.MultiPred[1] = 1;
    }

    class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
        {
            m_filler       = this;
            m_bs_processor = this;

            m_pPar->mfx.FrameInfo.Width  = m_pPar->mfx.FrameInfo.CropW = 720;
            m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 480;
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;

            m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/iceage_720x480_491.yuv"),
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

            if (s.Data.FrameOrder >= frameToChangeCtrl) // Just to no crash on first frame
            {
                mfxExtFeiHevcEncFrameCtrl& rt_control = m_ctrl;

                // Replace "good" runtime buffer with the corrupted one
                rt_control = EncCrtlInvalidParam;

                m_encode_status = MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            return sts;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

    private:
        enum
        {
            MFX_PAR = 1,
            EXT_COD2,
            EXT_COD3,
            EXT_ENCCTRL
        };

        struct tc_struct
        {
            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        tsRawReader *m_reader = nullptr;

#ifdef DEBUG_TEST
        tsBitstreamWriter* m_writer = nullptr;
#endif

        mfxStatus m_encode_status = MFX_ERR_NONE;

        mfxExtFeiHevcEncFrameCtrl EncCrtlInvalidParam;

    };

#define mfx_GopPicSize tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_PRefType tsStruct::mfxExtCodingOption3.PRefType
#define mfx_BRefType tsStruct::mfxExtCodingOption2.BRefType

#define EncCtrl_SearchPath         tsStruct::mfxExtFeiHevcEncFrameCtrl.SearchPath
#define EncCtrl_LenSP              tsStruct::mfxExtFeiHevcEncFrameCtrl.LenSP
#define EncCtrl_RefWidth           tsStruct::mfxExtFeiHevcEncFrameCtrl.RefWidth
#define EncCtrl_RefHeight          tsStruct::mfxExtFeiHevcEncFrameCtrl.RefHeight
#define EncCtrl_SearchWindow       tsStruct::mfxExtFeiHevcEncFrameCtrl.SearchWindow
#define EncCtrl_NumMvPredictors_0  tsStruct::mfxExtFeiHevcEncFrameCtrl.NumMvPredictors[0]
#define EncCtrl_NumMvPredictors_1  tsStruct::mfxExtFeiHevcEncFrameCtrl.NumMvPredictors[1]
#define EncCtrl_MultiPred_0        tsStruct::mfxExtFeiHevcEncFrameCtrl.MultiPred[0]
#define EncCtrl_MultiPred_1        tsStruct::mfxExtFeiHevcEncFrameCtrl.MultiPred[1]
#define EncCtrl_SubPelMode         tsStruct::mfxExtFeiHevcEncFrameCtrl.SubPelMode
#define EncCtrl_AdaptiveSearch     tsStruct::mfxExtFeiHevcEncFrameCtrl.AdaptiveSearch
#define EncCtrl_MVPredictor        tsStruct::mfxExtFeiHevcEncFrameCtrl.MVPredictor
#define EncCtrl_PerCuQp            tsStruct::mfxExtFeiHevcEncFrameCtrl.PerCuQp
#define EncCtrl_PerCtuInput        tsStruct::mfxExtFeiHevcEncFrameCtrl.PerCtuInput
#define EncCtrl_ForceCtuSplit      tsStruct::mfxExtFeiHevcEncFrameCtrl.ForceCtuSplit
#define EncCtrl_NumFramePartitions tsStruct::mfxExtFeiHevcEncFrameCtrl.NumFramePartitions

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /* If PerCuQp is 1, mfxExtFeiHevcEncQP buffer should be attached as well */
        {/*00*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 1 }}},

        /* If PerCtuInput is 1, mfxFeiHevcEncCtuCtrl buffer should be attached as well */
        {/*01*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_PerCtuInput, 1 }}},

        /* SubPelMode must be in {0,1,3} */
        {/*02*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SubPelMode, 2 }}},

        {/*03*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SubPelMode, 4 }}},

        /* MVPredictor must be in {0,1,2,7} ({0,1,2,3,7} for ICL+) */
        {/*04*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_MVPredictor, 3 }}},

        {/*05*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_MVPredictor, 8 }}},

        /* If MVPredictor is valid, mfxExtFeiHevcEncMVPredictors buffer should be attached */
        {/*06*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_MVPredictor, 7 }}},

        /* ForceCtuSplit must be in range [0,1] ({0} for ICL+) */
        {/*07*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_ForceCtuSplit, 2 }}},

#if 0 // For ICL+
        {/*0X*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_ForceCtuSplit, 1 }}},
#endif

        /* NumFramePartitions must be in {1,2,4,8,16} */
        {/*08*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_NumFramePartitions, 5 }}},

        {/*09*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_NumFramePartitions, 9 }}},

        {/*10*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_NumFramePartitions, 17 }}},

        /* NumMVPredictorsL0 / L1 must be in range [0,4]*/
        {/*11*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_NumMvPredictors_0, 5 }}},

        {/*12*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_NumMvPredictors_1, 5 }}},

        /* MultiPredL0 / L1 must be in range [0,1] ([0,2] for ICL+) */
        {/*13*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_MultiPred_0, 2 }}},

        {/*14*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_MultiPred_0, 3 }}},

        {/*15*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_MultiPred_1, 2 }}},

        {/*16*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_MultiPred_1, 3 }}},

        /* SearchWindow (SW) must be in range [0,6] */
        {/*17*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 7 }}},

        /* If SW != 0; LenSP, AdaptiveSearch, SearchPath, RefWidth, RefHeight should be 0 */
        {/*18*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 6 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 1 }}},

        {/*19*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 6 },
                 { EXT_ENCCTRL, &EncCtrl_AdaptiveSearch, 1 }}},

        {/*20*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 6 },
                 { EXT_ENCCTRL, &EncCtrl_SearchPath, 1 }}},

        {/*21*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 6 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 1 }}},

        {/*22*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 6 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 1 }}},

        /* If SW == 0; LenSP, AdaptiveSearch, SearchPath, RefWidth, RefHeight should have valid value */

        {/*23*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 0 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 0 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 32 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 32 }}},

        /* If adaptive search enabled, LenSP should be in [2,63] */
        {/*24*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 0 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 1 },
                 { EXT_ENCCTRL, &EncCtrl_AdaptiveSearch, 1 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 32 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 32 }}},

        /* LenSP must be in range [1,63] */
        {/*25*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 0 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 64 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 32 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 32 }}},

        /* AdaptiveSearch must be in range [0,1] */
        {/*26*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 0 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 1 },
                 { EXT_ENCCTRL, &EncCtrl_AdaptiveSearch, 2 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 32 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 32 }}},

        /* SearchPath must be in range [0,2] */
        {/*27*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 0 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 1 },
                 { EXT_ENCCTRL, &EncCtrl_SearchPath, 3 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 32 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 32 }}},

        /* RefWidth / RefHeight must be multiple of 4 */
        {/*28*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 0 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 1 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 32 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 33 }}},

        {/*29*/ {{ MFX_PAR,     &mfx_GopPicSize, 30 },
                 { MFX_PAR,     &mfx_GopRefDist, 1 },
                 { EXT_COD3,    &mfx_PRefType, MFX_P_REF_SIMPLE },
                 { EXT_COD2,    &mfx_BRefType, MFX_B_REF_OFF },
                 { EXT_ENCCTRL, &EncCtrl_SearchWindow, 0 },
                 { EXT_ENCCTRL, &EncCtrl_LenSP, 1 },
                 { EXT_ENCCTRL, &EncCtrl_RefWidth, 33 },
                 { EXT_ENCCTRL, &EncCtrl_RefHeight, 32 }}},
    };

    const mfxU32 TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const mfxU32 frameNumber = 30;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_HEVC_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];

        SetDefaultsToCtrl(EncCrtlInvalidParam);

        // Set parameters for mfxVideoParam
        SETPARS(m_pPar, MFX_PAR);
        m_pPar->AsyncDepth              = 1;
        m_pPar->mfx.RateControlMethod   = MFX_RATECONTROL_CQP;
        m_pPar->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        // Set CodingOption2 parameters
        mfxExtCodingOption2& CO2 = m_par;
        SETPARS(&CO2, EXT_COD2);

        // Set CodingOption3 parameters
        mfxExtCodingOption3& CO3 = m_par;
        SETPARS(&CO3, EXT_COD3);

        SETPARS(&EncCrtlInvalidParam, EXT_ENCCTRL);

        ///////////////////////////////////////////////////////////////////////////
        MFXInit();

        g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, MFX_PLUGINID_HEVC_FEI_ENCODE);
        m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
        m_loaded = false;
        Load();

        // ENCODE frames
        mfxExtFeiHevcEncFrameCtrl& rt_control = m_ctrl;
        SetDefaultsToCtrl(rt_control);

        // ENCODE frames
        for (mfxU32 encoded = 0; encoded < frameNumber; ++encoded)
        {
            if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
            {
                continue;
            }

            g_tsStatus.expect(m_encode_status);

            g_tsStatus.check(); TS_CHECK_MFX;

            g_tsStatus.expect(MFX_ERR_NONE);

            if (m_encode_status != MFX_ERR_NONE)
                break;

            SyncOperation();
            TS_CHECK_MFX;
        }

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_invalid_runtime_parameters);
};
