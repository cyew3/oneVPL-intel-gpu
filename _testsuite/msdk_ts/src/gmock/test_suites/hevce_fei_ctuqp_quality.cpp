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
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"
#include "fei_buffer_allocator.h"

namespace hevce_fei_ctuqp_quality
{

    constexpr mfxF64 expected_bitrate_diff = 1.0;
    constexpr mfxF64 allowed_diff          = 0.003;

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        , m_mode(0)
        , m_block_size(32)
        , m_qp_value(0)
    {
        m_filler = this;
        m_bs_processor = this;

        m_pPar->mfx.FrameInfo.Width        = m_pPar->mfx.FrameInfo.CropW = 352;
        m_pPar->mfx.FrameInfo.Height       = m_pPar->mfx.FrameInfo.CropH = 288;
        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_pPar->AsyncDepth                 = 1; //limitation for FEI (from sample_fei)
        m_pPar->mfx.RateControlMethod      = MFX_RATECONTROL_CQP; //For now FEI work with CQP only
        m_pPar->IOPattern                  = MFX_IOPATTERN_IN_VIDEO_MEMORY;

        m_request.Width  = m_pPar->mfx.FrameInfo.Width;
        m_request.Height = m_pPar->mfx.FrameInfo.Height;
        m_cuqp_width     = (m_pPar->mfx.FrameInfo.Width  + m_block_size - 1) / m_block_size;
        m_cuqp_height    = (m_pPar->mfx.FrameInfo.Height + m_block_size - 1) / m_block_size;

        m_reader.reset(new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"),
                                   m_pPar->mfx.FrameInfo));
        g_tsStreamPool.Reg();

        m_Gen.seed(0);

    }
    ~TestSuite()
    {
        for (auto& map_entry : m_map_ctrl)
        {
            mfxExtFeiHevcEncQP& hevcFeiEncQp = map_entry.second;
            m_hevcFeiAllocator->Free(&hevcFeiEncQp);
        }
    }
    void SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl)
    {
        memset(&ctrl, 0, sizeof(ctrl));

        ctrl.Header.BufferId    = MFX_EXTBUFF_HEVCFEI_ENC_CTRL;
        ctrl.Header.BufferSz    = sizeof(mfxExtFeiHevcEncFrameCtrl);
        ctrl.SubPelMode         = 3; // quarter-pixel motion estimation
        ctrl.SearchWindow       = 5; // 48 SUs 48x40 window full search
        ctrl.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
        // enable internal L0/L1 predictors: 1 - spatial predictors
        ctrl.MultiPred[0]       = ctrl.MultiPred[1] = 1;
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        s.Data.TimeStamp = s.Data.FrameOrder;

        tsExtBufType<mfxEncodeCtrl>& ctrl  = m_map_ctrl[s.Data.FrameOrder];
        mfxExtFeiHevcEncFrameCtrl& feiCtrl = ctrl;
        SetDefaultsToCtrl(feiCtrl);

        //first launch with QP per frame
        if (!m_per_CTUQP)
        {
            feiCtrl.PerCuQp = 0;

            ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl));

            if (m_mode == QP_FRAME)
            {
                mfxU32 rand_value                    = GetRandomQP();
                m_qp_frame_vector[s.Data.FrameOrder] = rand_value;
                m_pCtrl->QP                          = rand_value;
            }
            else if (m_mode == QP_STREAM)
            {
                m_pCtrl->QP = m_qp_value;
            }
            return sts;
        }

        //second launch with attaching QP buffer
        feiCtrl.PerCuQp = 1;

        mfxExtFeiHevcEncQP& hevcFeiEncQp = ctrl;
        m_hevcFeiAllocator->Free(&hevcFeiEncQp);
        m_hevcFeiAllocator->Alloc(&hevcFeiEncQp, m_request);

        AutoBufferLocker<mfxExtFeiHevcEncQP> auto_locker(*m_hevcFeiAllocator, hevcFeiEncQp);

        if (m_mode == QP_FRAME) {

            SetQPforFrame(m_qp_frame_vector[s.Data.FrameOrder], ctrl, hevcFeiEncQp);

        }
        else if (m_mode == QP_STREAM)
        {

            SetQPforFrame(m_qp_value, ctrl, hevcFeiEncQp);

        }

        return sts;
    }

    void ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src)
    {
        dst = src;
    }

    mfxU32 GetRandomQP()
    {
        std::uniform_int_distribution<mfxU32> distr(0, 51);
        return distr(m_Gen);
    }

    void SetQPforFrame(mfxU32 qp, tsExtBufType<mfxEncodeCtrl>& ctrl, mfxExtFeiHevcEncQP& hevcFeiEncQp)
    {
        for (mfxU32 i = 0; i < m_cuqp_height; i++)
            for (mfxU32 j = 0; j < m_cuqp_width; j++)
            {
                hevcFeiEncQp.Data[i * hevcFeiEncQp.Pitch + j] = qp;
            }

        ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl));

        m_pCtrl->QP = qp;
    }

    enum
    {
        QP_FRAME  = 1,
        QP_STREAM = 2
    };

    struct tc_struct
    {
        mfxU32 m_mode;
        mfxU32 m_block_size;
        mfxU32 m_qp_value;
    };

    static const tc_struct test_case[];

    std::unique_ptr <tsRawReader> m_reader;
    std::vector<mfxU8> m_qp_frame_vector;
    mfxU32 m_mode                            = 0;

    mfxU32 m_block_size                      = 0;
    std::unique_ptr <FeiBufferAllocator> m_hevcFeiAllocator;
    mfxU32 m_qp_value                        = 0;
    mfxU32 m_cuqp_width                      = 0;
    mfxU32 m_cuqp_height                     = 0;

    std::mt19937 m_Gen;
    BufferAllocRequest m_request;
    std::map<mfxU32, tsExtBufType<mfxEncodeCtrl>> m_map_ctrl;
    bool m_per_CTUQP                         = false;
};

class BitstreamInfo : public tsBitstreamProcessor
{
public:
    std::vector<mfxU8>  m_buf;
    mfxU32 m_len;    //data length in m_buf
    BitstreamInfo(mfxU32 width, mfxU32 height)
        : m_len(0)
    {
        m_buf.resize(width * height * 4 * 30);
    }
    ~BitstreamInfo()
    {
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

mfxStatus BitstreamInfo::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    memcpy(&m_buf[m_len], bs.Data + bs.DataOffset, bs.DataLength - bs.DataOffset);
    m_len += bs.DataLength - bs.DataOffset;

    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ QP_STREAM, 32, 3},

    {/*01*/ QP_STREAM, 32, 10},

    {/*02*/ QP_STREAM, 32, 20},

    {/*03*/ QP_STREAM, 32, 30},

    {/*04*/ QP_STREAM, 32, 42},

    {/*05*/ QP_STREAM, 32, 51},

    {/*06*/ QP_FRAME,  32, 0xffffffff},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_HEVC_FEI_SUPPORT()

    const tc_struct& tc = test_case[id];
    m_mode              = tc.m_mode;
    m_qp_value          = tc.m_qp_value;

    ///////////////////////////////////////////////////////////////////////////

    MFXInit();

    SetFrameAllocator(m_session, m_pVAHandle);
    m_pFrameAllocator = m_pVAHandle;

    m_qp_frame_vector.resize(frameNumber);

    // 1. encode with frame level qp
    m_per_CTUQP = false;
    BitstreamInfo checker_perFrame(m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height);

    m_bs_processor = &checker_perFrame;


    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * frameNumber);
    EncodeFrames(frameNumber);
    Close();

    tsVideoDecoder dec_perFrame(MFX_CODEC_HEVC);

    mfxBitstream bs_perFrame;
    memset(&bs_perFrame, 0, sizeof(bs_perFrame));

    bs_perFrame.Data       = checker_perFrame.m_buf.data();
    bs_perFrame.DataLength = checker_perFrame.m_len;
    bs_perFrame.MaxLength  = checker_perFrame.m_buf.size();
    mfxI32 bitrate_frame   = checker_perFrame.m_len;

    tsBitstreamReader reader(bs_perFrame, checker_perFrame.m_buf.size());

    dec_perFrame.m_bs_processor = &reader;

    tsSurfaceCRC32 crc_proc_yuv;
    dec_perFrame.m_surf_processor = &crc_proc_yuv;

    dec_perFrame.Init();
    dec_perFrame.AllocSurfaces();
    dec_perFrame.DecodeFrames(frameNumber);

    mfxU32 crc = crc_proc_yuv.GetCRC();
    dec_perFrame.Close();

    // reset

    m_reader->ResetFile(true);

    memset(&m_bitstream, 0, sizeof(mfxBitstream));

    // 2. encode with LCU Qp setting
    m_per_CTUQP = true;
    BitstreamInfo checker_perLCU(m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height);
    m_bs_processor = &checker_perLCU;

    mfxHDL hdl;
    mfxHandleType type;
    m_pVAHandle->get_hdl(type, hdl);
    m_hevcFeiAllocator.reset(new FeiBufferAllocator((VADisplay) hdl, m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height));

    //to alloc more surfaces
    AllocSurfaces();

    EncodeFrames(frameNumber);

    tsVideoDecoder dec_perLCU(MFX_CODEC_HEVC);

    mfxBitstream bs_perLCU;
    memset(&bs_perLCU, 0, sizeof(bs_perLCU));

    bs_perLCU.Data       = checker_perLCU.m_buf.data();
    bs_perLCU.DataLength = checker_perLCU.m_len;
    bs_perLCU.MaxLength  = checker_perLCU.m_buf.size();
    mfxI32 bitrate_ctu   = checker_perLCU.m_len;

    tsBitstreamReader reader_perLCU(bs_perLCU, checker_perLCU.m_buf.size());
    dec_perLCU.m_bs_processor = &reader_perLCU;

    tsSurfaceCRC32 crc_cmp_yuv;
    dec_perLCU.m_surf_processor = &crc_cmp_yuv;

    dec_perLCU.Init();
    dec_perLCU.AllocSurfaces();
    dec_perLCU.DecodeFrames(frameNumber);

    mfxU32 cmp_crc = crc_cmp_yuv.GetCRC();
    EXPECT_EQ(crc, cmp_crc)<<  "ERROR: the 2 crc values should be the same";

    mfxF64 diff_bitrate = (mfxF64(abs(bitrate_ctu - bitrate_frame)) / mfxF64(std::max(bitrate_ctu, bitrate_frame) + 1)) * 100;
    g_tsLog << "First stream Rate = " << bitrate_frame << "\n";
    g_tsLog << "Second stream Rate = " << bitrate_ctu << "\n";
    if(diff_bitrate > expected_bitrate_diff)
    {
        g_tsLog << "ERROR: Bitrate difference more threshold\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }
    else if (diff_bitrate < allowed_diff)
    {
        g_tsLog << "ERROR: Bitrate equals\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_fei_ctuqp_quality);
};