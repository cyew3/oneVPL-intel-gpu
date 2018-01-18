/******************************************************************************* *\

Copyright (C) 2018 Intel Corporation.  All rights reserved.

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
#include "fei_buffer_allocator.h"

namespace hevce_fei_ctuqp_invalid_params
{

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
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        {
            m_filler       = this;
            m_bs_processor = this;

            m_pPar->mfx.FrameInfo.Width        = m_pPar->mfx.FrameInfo.CropW = 352;
            m_pPar->mfx.FrameInfo.Height       = m_pPar->mfx.FrameInfo.CropH = 288;
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
            m_pPar->AsyncDepth                 = 1;
            m_pPar->mfx.RateControlMethod      = MFX_RATECONTROL_CQP;

            m_request.Width  = m_pPar->mfx.FrameInfo.Width;
            m_request.Height = m_pPar->mfx.FrameInfo.Height;

            m_cuqp_width  = (m_pPar->mfx.FrameInfo.Width  + 32 - 1) / 32;
            m_cuqp_height = (m_pPar->mfx.FrameInfo.Height + 32 - 1) / 32;

            m_reader = new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"),
                m_pPar->mfx.FrameInfo);
            g_tsStreamPool.Reg();
        }

        ~TestSuite()
        {
            delete m_reader;
            if (m_mode != QP_NO_BUFFER)
            {
                mfxExtFeiHevcEncQP& hevcFeiEncQp = m_ctrl;
                m_hevcFeiAllocator->Free(&hevcFeiEncQp);
            }
        }

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxStatus sts = m_reader->ProcessSurface(s);

            mfxExtFeiHevcEncFrameCtrl& rt_control = m_ctrl;

            // Replace "good" runtime buffer with the corrupted one
            rt_control = EncCtrlInvalidParam;

            return sts;
        }

        /* To prevent increase of length of bitstream,
        setting DataLength to 0 is just saying that don't care about produced bitstream,
        other way it is possible run out of pre alloced memory and have to realloc it.*/
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

    private:
        enum
        {
            MFX_PAR = 1,
            EXT_COD3,
            EXT_ENCCTRL
        };

        enum MODE
        {
            QP_INVALID          = 1,
            QP_NO_BUFFER        = 2,
            QP_ENABLE_MBQP      = 3,
            QP_ENABLE_MBQP_RT   = 4
        };

        struct tc_struct
        {
            MODE m_mode;
            mfxStatus init_status;
            mfxStatus encode_status;
            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
            mfxU32 m_qp_value;
        };

        static const tc_struct test_case[];

        tsRawReader *m_reader = nullptr;
        mfxU32 m_qp_value     = 0;
        mfxU32 m_cuqp_width   = 0;
        mfxU32 m_cuqp_height  = 0;
        MODE m_mode;
        BufferAllocRequest m_request;
        std::unique_ptr <FeiBufferAllocator> m_hevcFeiAllocator;

        mfxStatus m_encode_status = MFX_ERR_NONE;
        mfxStatus m_init_status   = MFX_ERR_NONE;

        mfxExtFeiHevcEncFrameCtrl EncCtrlInvalidParam;

    };

#define mfx_EnableMBQP      tsStruct::mfxExtCodingOption3.EnableMBQP

#define EncCtrl_PerCuQp     tsStruct::mfxExtFeiHevcEncFrameCtrl.PerCuQp

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /* If PerCuQp is 1 and QP buffer is not attached*/
        {/*00*/ QP_NO_BUFFER,   MFX_ERR_NONE,                     MFX_ERR_UNDEFINED_BEHAVIOR, {{ EXT_ENCCTRL, &EncCtrl_PerCuQp, 1 }}
        },

        /* CodingOption3 EnableMBQP setup on Init*/
        {/*01*/ QP_ENABLE_MBQP, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_ON }}, 10
        },

        {/*02*/ QP_ENABLE_MBQP, MFX_ERR_NONE,                     MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_OFF }}, 10
        },

        {/*03*/ QP_ENABLE_MBQP, MFX_ERR_NONE,                     MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_UNKNOWN }}, 10
        },

        {/*04*/ QP_ENABLE_MBQP, MFX_ERR_NONE,                     MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_ADAPTIVE }}, 10
        },

        /* If PerCuQp is 1 and CodingOption3 EnableMBQP setup runtime */
        {/*05*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_ON },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 1 }}, 10
        },

        {/*06*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_OFF },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 1 }}, 10
        },

        {/*07*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_UNKNOWN },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 1 }}, 10
        },

        {/*08*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_ADAPTIVE },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 1 }}, 10
        },

        {/*09*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_ON },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 0 }}, 10
        },

        {/*10*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_OFF },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 0 }}, 10
        },

        {/*11*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_UNKNOWN },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 0 }}, 10
        },

        {/*12*/ QP_ENABLE_MBQP_RT, MFX_ERR_NONE, MFX_ERR_NONE, {{ EXT_COD3,    &mfx_EnableMBQP, MFX_CODINGOPTION_ADAPTIVE },
                                 { EXT_ENCCTRL, &EncCtrl_PerCuQp, 0 }}, 10
        },

        /* If PerCuQp is 1 and invalid value for QP buffer*/
        {/*13*/ QP_INVALID,        MFX_ERR_NONE, MFX_ERR_NONE, {{ EXT_ENCCTRL, &EncCtrl_PerCuQp, 1 }}, 52
        },
    };

    const mfxU32 TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const mfxU32 frameNumber = 1;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];
        m_mode              = tc.m_mode;
        m_qp_value          = tc.m_qp_value;
        m_encode_status     = tc.encode_status;
        m_init_status       = tc.init_status;

        SetDefaultsToCtrl(EncCtrlInvalidParam);

        SETPARS(&EncCtrlInvalidParam, EXT_ENCCTRL);

        ///////////////////////////////////////////////////////////////////////////
        MFXInit();

        if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
        {
            SetFrameAllocator(m_session, m_pVAHandle);
            m_pFrameAllocator = m_pVAHandle;
        }

        // ENCODE frames
        mfxExtFeiHevcEncFrameCtrl& rt_control = m_ctrl;
        SetDefaultsToCtrl(rt_control);

        if (m_mode != QP_NO_BUFFER)
        {
            mfxHDL hdl;
            mfxHandleType type;
            m_pVAHandle->get_hdl(type, hdl);

            m_hevcFeiAllocator.reset(new FeiBufferAllocator((VADisplay) hdl, m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height));

            mfxExtFeiHevcEncQP& hevcFeiEncQp = m_ctrl;
            m_hevcFeiAllocator->Free(&hevcFeiEncQp);
            m_hevcFeiAllocator->Alloc(&hevcFeiEncQp, m_request);
            AutoBufferLocker<mfxExtFeiHevcEncQP> auto_locker(*m_hevcFeiAllocator, hevcFeiEncQp);

            for (mfxU32 i = 0; i < m_cuqp_height; i++)
                for (mfxU32 j = 0; j < m_cuqp_width; j++)
                {
                    hevcFeiEncQp.Data[i * hevcFeiEncQp.Pitch + j] = m_qp_value;
                }

            if (m_mode == QP_ENABLE_MBQP)
            {
                // Set CodingOption3 parameters
                mfxExtCodingOption3& CO3 = m_par;
                SETPARS(&CO3, EXT_COD3);

                Load();

                g_tsStatus.expect(m_init_status);

                Init();
            }
            if (m_mode == QP_ENABLE_MBQP_RT || m_mode == QP_INVALID)
            {
                // Set CodingOption3 runtime
                mfxExtCodingOption3& CO3 = m_ctrl;
                SETPARS(&CO3, EXT_COD3);

                EncodeFrames(frameNumber);

                g_tsStatus.expect(m_encode_status);

                g_tsStatus.check(m_encode_status); TS_CHECK_MFX;

                g_tsStatus.expect(MFX_ERR_NONE);

            }
        }
        else
        {
            EncodeFrameAsync();

            g_tsStatus.expect(m_encode_status);

            g_tsStatus.check(m_encode_status); TS_CHECK_MFX;

            g_tsStatus.expect(MFX_ERR_NONE);

        }

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_ctuqp_invalid_params);
};