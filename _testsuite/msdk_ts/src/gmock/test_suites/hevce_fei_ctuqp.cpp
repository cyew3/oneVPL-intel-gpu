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
#include "ts_parser.h"
#include "fei_buffer_allocator.h"

namespace hevce_fei_ctuqp
{

    constexpr mfxU16 const_frame_qp = 15;

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        , tsParserHEVCAU(BS_HEVC::INIT_MODE_CABAC)
        , m_mode(0)
        , m_block_size(32)
        , m_qp_value(0)
    {
        m_filler       = this;
        m_bs_processor = this;

        m_pPar->mfx.FrameInfo.Width        = m_pPar->mfx.FrameInfo.CropW = 352;
        m_pPar->mfx.FrameInfo.Height       = m_pPar->mfx.FrameInfo.CropH = 288;
        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_pPar->AsyncDepth                 = 1;                   //limitation for FEI
        m_pPar->mfx.RateControlMethod      = MFX_RATECONTROL_CQP; //For now FEI work with CQP only

        m_request.Width  = m_pPar->mfx.FrameInfo.Width;
        m_request.Height = m_pPar->mfx.FrameInfo.Height;

        m_cuqp_width  = (m_pPar->mfx.FrameInfo.Width  + m_block_size - 1) / m_block_size;
        m_cuqp_height = (m_pPar->mfx.FrameInfo.Height + m_block_size - 1) / m_block_size;

        m_reader.reset(new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"),
                                   m_pPar->mfx.FrameInfo));
        g_tsStreamPool.Reg();

        m_Gen.seed(0);

    }

    ~TestSuite()
    {
        for (auto& map_entry : m_map_ctrl)
        {
            mfxExtFeiHevcEncQP& hevcFeiEncQp = map_entry.second.ctrl;
            m_hevcFeiAllocator->Free(&hevcFeiEncQp);
        }
    }

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
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        s.Data.TimeStamp = s.Data.FrameOrder;

        Ctrl& ctrl = m_map_ctrl[s.Data.FrameOrder];
        ctrl.buf.resize(m_cuqp_width * m_cuqp_height);

        mfxExtFeiHevcEncFrameCtrl& feiCtrl = ctrl.ctrl;
        SetDefaultsToCtrl(feiCtrl);
        feiCtrl.PerCuQp = 1;

        mfxExtFeiHevcEncQP& hevcFeiEncQp = ctrl.ctrl;
        m_hevcFeiAllocator->Free(&hevcFeiEncQp);
        m_hevcFeiAllocator->Alloc(&hevcFeiEncQp, m_request);

        AutoBufferLocker<mfxExtFeiHevcEncQP> auto_locker(*m_hevcFeiAllocator, hevcFeiEncQp);

        mfxU32 random_value;

        if (m_mode == QP_RANDOM)
        {
            ctrl.QPflag = 1;

            for (mfxU32 i = 0; i < m_cuqp_height; i++)
                for (mfxU32 j = 0; j < m_cuqp_width; j++)
                {
                    random_value = GetRandomQP(const_frame_qp);
                    hevcFeiEncQp.Data[i * hevcFeiEncQp.Pitch + j] = random_value;
                    ctrl.buf[i * m_cuqp_width + j]                = random_value;
                }

        }
        else if (m_mode == QP_FRAME)
        {
            random_value = GetRandomQP(const_frame_qp);
            ctrl.QPflag = 1;

            for (mfxU32 i = 0; i < m_cuqp_height; i++)
                for (mfxU32 j = 0; j < m_cuqp_width; j++)
                {
                    hevcFeiEncQp.Data[i * hevcFeiEncQp.Pitch + j] = random_value;
                    ctrl.buf[i * m_cuqp_width + j]                = random_value;
                }

        }
        else if (m_mode == QP_STREAM)
        {
            ctrl.QPflag = 1;

            for (mfxU32 i = 0; i < m_cuqp_height; i++)
                for (mfxU32 j = 0; j < m_cuqp_width; j++)
                {
                    hevcFeiEncQp.Data[i * hevcFeiEncQp.Pitch + j] = m_qp_value;
                    ctrl.buf[i * m_cuqp_width + j]                = m_qp_value;
                }
        }
        else if (m_mode == QP_DYNAMIC)
        {
            if (GetRandomBit())
            {
                feiCtrl.PerCuQp = 0;
                ctrl.QPflag     = 0;
                std::fill(ctrl.buf.begin(), ctrl.buf.end(), const_frame_qp);
            }
            else
            {
                feiCtrl.PerCuQp = 1;
                ctrl.QPflag     = 1;
                random_value    = GetRandomQP(const_frame_qp);

                for (mfxU32 i = 0; i < m_cuqp_height; i++)
                    for (mfxU32 j = 0; j < m_cuqp_width; j++)
                    {
                        hevcFeiEncQp.Data[i * hevcFeiEncQp.Pitch + j] = random_value;
                        ctrl.buf[i * m_cuqp_width + j]                = random_value;
                    }
            }
        }

        ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl.ctrl));
        m_pCtrl->QP = const_frame_qp;

        return sts;
    }

    void ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src)
    {
        dst = src;
    }

    mfxU32 GetRandomQP(mfxU32 qp_to_avoid = 0xffffffff)
    {
        std::uniform_int_distribution<mfxU32> distr(0, 51);
        mfxU32 qp_gen;
        while ((qp_gen = distr(m_Gen)) == qp_to_avoid);
        return qp_gen;
    }

    bool GetRandomBit()
    {
        std::bernoulli_distribution distr(0.33);
        return distr(m_Gen);
    }

    void SetQP(std::vector< std::vector<mfxU16> >& QP, BS_HEVC::TransTree const & tu, mfxU8 skip, mfxU32 Log2MaxTrafoSize, mfxU32 Log2MinTrafoSize)
    {
        if (tu.split_transform_flag)
        {
            SetQP(QP, tu.split[0], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, tu.split[1], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, tu.split[2], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, tu.split[3], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            return;
        }

        mfxU32 x0 = tu.x >> Log2MinTrafoSize;
        mfxU32 y0 = tu.y >> Log2MinTrafoSize;
        mfxU32 Log2Sz = Log2MaxTrafoSize - tu.trafoDepth - Log2MinTrafoSize + 1;
        mfxU8 noQp = skip || ((tu.cbf_cb | tu.cbf_cr | tu.cbf_luma) == 0);
        for (mfxU32 y = 0; y < mfxU32(1 << Log2Sz); y++){
            for (mfxU32 x = 0; x < mfxU32(1 << Log2Sz); x++){
                QP[y0 + y][x0 + x] = noQp ? 255 : tu.QpY;
            }
        }
    }

    void SetQP(std::vector< std::vector<mfxU16> >& QP, BS_HEVC::CQT const & cqt, mfxU32 Log2MaxTrafoSize, mfxU32 Log2MinTrafoSize)
    {
        if (cqt.split_cu_flag)
        {
            SetQP(QP, cqt.split[0], Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, cqt.split[1], Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, cqt.split[2], Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, cqt.split[3], Log2MaxTrafoSize, Log2MinTrafoSize);
            return;
        }
        if (!cqt.tu)
            return;
        SetQP(QP, *cqt.tu, cqt.cu_skip_flag, Log2MaxTrafoSize, Log2MinTrafoSize);
    }


    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            auto& pic = *au.pic;
            for (mfxU32 i = 0; i < au.NumUnits; i++)
            {
                mfxU16 type = au.nalu[i]->nal_unit_type;
                if (type == 34)
                {
                    auto& pps = *au.nalu[i]->pps;

                    if (m_map_ctrl[bs.TimeStamp].QPflag == 1)
                    {
                        EXPECT_EQ(pps.cu_qp_delta_enabled_flag, 1) << " ERROR: Unexpected cu_qp_delta_enabled_flag, cu_qp_delta_enabled_flag must be 1 when we use PerCuQp is ON";
                    }
                    else
                    {
                        EXPECT_EQ(pps.cu_qp_delta_enabled_flag, 0) << " ERROR: Unexpected cu_qp_delta_enabled_flag, cu_qp_delta_enabled_flag must be 0 when we use PerCuQp is OFF";
                    }
                }

                if (type > 21 || ((type > 9) && (type < 16)))
                {
                    continue;
                }

                auto& s = *au.nalu[i]->slice;
                BS_HEVC::SPS& sps = *pic.slice[0]->slice->sps;
                mfxU32 Log2MinTrafoSize = sps.log2_min_transform_block_size_minus2 + 2;
                mfxU32 Log2MaxTrafoSize = Log2MinTrafoSize + sps.log2_diff_max_min_transform_block_size;
                mfxU32 Width = sps.pic_width_in_luma_samples;
                mfxU32 Height = sps.pic_height_in_luma_samples;
                std::vector< std::vector<mfxU16> > QP(
                           (Height + 64) >> Log2MinTrafoSize,
                           std::vector<mfxU16>((Width + 64) >> Log2MinTrafoSize, 0));
                for (BS_HEVC::CTU* cu = pic.CuInRs; cu < pic.CuInRs + pic.NumCU; cu++)
                {
                    SetQP(QP, cu->cqt, Log2MaxTrafoSize, Log2MinTrafoSize);
                }

                mfxU32 k_lcu_w = 32 / (1 << Log2MinTrafoSize);
                m_cu = 0;
                mfxU32 m_cu_old = 0;

                mfxU32 pitch = (m_par.mfx.FrameInfo.Width  + m_block_size - 1)/m_block_size;
                for (mfxU32 y = 0; y < (Height >> Log2MinTrafoSize); y++)
                {
                    m_cu = ((y % k_lcu_w == 0) && (y != 0)) ? (m_cu_old + pitch) : m_cu_old;
                    if((y % k_lcu_w == 0) && (y != 0))
                        m_cu_old = m_cu;
                    for (mfxU32 x = 0; x < (Width >> Log2MinTrafoSize); x ++){
                          m_cu += ((x % k_lcu_w == 0) && x != 0) ? 1 : 0;
                          if(QP[y][x] != 255){
                                EXPECT_EQ(int(QP[y][x]), int(m_map_ctrl[bs.TimeStamp].buf[m_cu]));
                          }
                    }
                }
            }
        }
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
        std::vector<mfxU8>          buf;
        mfxI32                      QPflag;
    };

    enum
    {
        MFX_PAR = 1,
    };

    enum
    {
        QP_RANDOM   = 1,
        QP_FRAME    = 2,
        QP_STREAM   = 3,
        QP_DYNAMIC  = 4
    };

    struct tc_struct
    {
        mfxU32 m_mode;
        mfxU32 m_block_size;
        mfxU32 m_qp_value;
    };

    static const tc_struct test_case[];

    std::unique_ptr <tsRawReader>        m_reader;
    std::unique_ptr <FeiBufferAllocator> m_hevcFeiAllocator;

    mfxU32 m_mode        = 0;
    mfxU32 m_block_size  = 0;
    mfxU32 m_cu          = 0;
    mfxU32 m_qp_value    = 0;
    mfxU32 m_cuqp_width  = 0;
    mfxU32 m_cuqp_height = 0;

    std::mt19937           m_Gen;
    std::map<mfxU32, Ctrl> m_map_ctrl;
    BufferAllocRequest     m_request;

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ QP_STREAM,  32, 0},

    {/*01*/ QP_STREAM,  32, 11},

    {/*02*/ QP_STREAM,  32, 23},

    {/*03*/ QP_STREAM,  32, 31},

    {/*04*/ QP_STREAM,  32, 42},

    {/*05*/ QP_STREAM,  32, 51},

    {/*06*/ QP_RANDOM,  32, 0xffffffff},

    {/*07*/ QP_FRAME,   32, 10},

    {/*08*/ QP_DYNAMIC, 32, 14},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_HEVC_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];
    m_mode              = tc.m_mode;
    m_qp_value          = tc.m_qp_value;
    m_block_size        = tc.m_block_size;

    ///////////////////////////////////////////////////////////////////////////

    MFXInit();

    if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        SetFrameAllocator(m_session, m_pVAHandle);
        m_pFrameAllocator = m_pVAHandle;
    }

    // Create Buffer Allocator for libva buffers
    mfxHDL hdl;
    mfxHandleType type;
    m_pVAHandle->get_hdl(type, hdl);

    m_hevcFeiAllocator.reset(new FeiBufferAllocator((VADisplay) hdl, m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height));

    EncodeFrames(frameNumber);

    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_fei_ctuqp);
};