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
#include "ts_parser.h"
#include "fei_buffer_allocator.h"

namespace hevc_fei_lcuqp
{

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVCAU(BS_HEVC::INIT_MODE_CABAC)
        , m_reader(NULL)
        , m_fo(0)
        , mode(0)
        , test_type(0)
        , block_size(32)
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
        if (m_reader)
            delete m_reader;
        delete m_hevcFeiAllocator;
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);
        m_qp.clear();

        //calculate size of LCU
        int BlockSize = block_size;
        mfxU32 widthLCU  = (m_par.mfx.FrameInfo.Width  + BlockSize - 1)/BlockSize;
        mfxU32 heightLCU = (m_par.mfx.FrameInfo.Height + BlockSize - 1)/BlockSize;
        mfxU32 size = heightLCU * widthLCU * 4;

        m_qp.resize(size);

        mfxExtFeiHevcEncFrameCtrl& hevcFeiEncCtrl = m_ctrl;
        if (!hevcFeiEncCtrl.PerCuQp)
        {
            if (mode & QP_FRAME)
            {
                mfxU32 rand_qp = rand() % 50;
                qp_frame_vector.resize(size);
                qp_frame_vector[m_fo] = rand_qp;
                m_pCtrl->QP           = rand_qp;
            }
            return sts;
        }

        mfxExtFeiHevcEncQP& hevcFeiEncQp = m_ctrl;

        AutoBufferLocker<mfxExtFeiHevcEncQP> auto_locker(*m_hevcFeiAllocator, hevcFeiEncQp);

        if (mode & QP_RANDOM)  {
            for (size_t i = 0; i < m_qp.size(); i++)
            {
                m_qp[i]              = rand() % 50;
                hevcFeiEncQp.Data[i] = m_qp[i];
            }
        } else if (mode & QP_FRAME) {
            if (test_type & LCU_CHECK)
            {
                mfxU32 rand_qp = rand() % 50;
                for (size_t i = 0; i < m_qp.size(); i++)
                {
                    m_qp[i]              = rand_qp;
                    hevcFeiEncQp.Data[i] = rand_qp;
                }
            }
            else if (test_type & QUALITY_CHECK)
            {
                for (size_t i = 0; i < m_qp.size(); i++)
                {
                    hevcFeiEncQp.Data[i] = qp_frame_vector[m_fo];
                }
            }
        } else {
            for (size_t i = 0; i < m_qp.size(); i++)
            {
                m_qp[i]              = m_pCtrl->QP;
                hevcFeiEncQp.Data[i] = m_pCtrl->QP;
            }
        }

        m_fo++;

        return sts;
    }

    void SetQP(std::vector< std::vector<Bs16u> >& QP, BS_HEVC::TransTree const & tu, Bs8u skip, Bs32u Log2MaxTrafoSize, Bs32u Log2MinTrafoSize)
    {
        if (tu.split_transform_flag)
        {
            SetQP(QP, tu.split[0], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, tu.split[1], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, tu.split[2], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            SetQP(QP, tu.split[3], skip, Log2MaxTrafoSize, Log2MinTrafoSize);
            return;
        }

        Bs32u x0 = tu.x >> Log2MinTrafoSize;
        Bs32u y0 = tu.y >> Log2MinTrafoSize;
        Bs32u Log2Sz = Log2MaxTrafoSize - tu.trafoDepth - Log2MinTrafoSize + 1;
        Bs8u noQp = skip || ((tu.cbf_cb | tu.cbf_cr | tu.cbf_luma) == 0);
        for (Bs32u y = 0; y < Bs32u(1 << Log2Sz); y++){
            for (Bs32u x = 0; x < Bs32u(1 << Log2Sz); x++){
                QP[y0 + y][x0 + x] = noQp ? 255 : tu.QpY;
            }
        }
    }

    void SetQP(std::vector< std::vector<Bs16u> >& QP, BS_HEVC::CQT const & cqt, Bs32u Log2MaxTrafoSize, Bs32u Log2MinTrafoSize)
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
        m_ts = bs.TimeStamp;
        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            auto& pic = *au.pic;
            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                auto& s = *au.nalu[i]->slice;
                BS_HEVC::SPS& sps = *pic.slice[0]->slice->sps;
                Bs32u Log2MinTrafoSize = sps.log2_min_transform_block_size_minus2 + 2;
                Bs32u Log2MaxTrafoSize = Log2MinTrafoSize + sps.log2_diff_max_min_transform_block_size;
                Bs32u Width = sps.pic_width_in_luma_samples;
                Bs32u Height = sps.pic_height_in_luma_samples;
                std::vector< std::vector<Bs16u> > QP(
                           (Height + 64) >> Log2MinTrafoSize,
                           std::vector<Bs16u>((Width + 64) >> Log2MinTrafoSize, 0));
              for (BS_HEVC::CTU* cu = pic.CuInRs; cu < pic.CuInRs + pic.NumCU; cu++)
              {
                  SetQP(QP, cu->cqt, Log2MaxTrafoSize, Log2MinTrafoSize);
              }

              mfxU32 k_lcu_w = 32 / (1 << Log2MinTrafoSize);
              m_cu = 0;
              mfxU32 m_cu_old = 0;

              mfxU32 pitch_LCUQP = (m_par.mfx.FrameInfo.Width  + block_size - 1)/block_size;
              for (Bs32u y = 0; y < (Height >> Log2MinTrafoSize); y++)
              {
                 //16x32 only: driver limitation
                 m_cu = (!(y % k_lcu_w) && (y != 0)) ? (m_cu_old + 2*pitch_LCUQP) : m_cu_old;
                 if((y % k_lcu_w == 0) && (y != 0))
                     m_cu_old = m_cu;
                 for (Bs32u x = 0; x < (Width >> Log2MinTrafoSize); x ++){
                       m_cu += ((x % k_lcu_w == 0) && x != 0) ? 2 : 0;
                       if(QP[y][x] != 255){
                           EXPECT_EQ(int(QP[y][x]), int(m_qp[m_cu]));
                       }
                 }
               }
            }
        }
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

private:
    enum
    {
        MFX_PAR = 1,
        EXT_FRM_CTRL,
        MFX_FRM_CTRL
    };

    enum
    {
        QP_RANDOM     = 1,
        LCU_CHECK     = 2,
        QP_FRAME      = 3,
        QP_STREAM     = 4,
        QUALITY_CHECK = 5
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        mfxU32 test_type;
        int block_size;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

    tsRawReader *m_reader;
    std::vector<mfxU8> m_qp;
    std::vector<mfxU8> qp_frame_vector;
    mfxU32 m_ts;
    mfxU32 m_fo;
    mfxU32 mode;
    mfxU32 test_type;
    int  block_size;
    FeiBufferAllocator *m_hevcFeiAllocator;
    mfxU32 m_cu;

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, QP_STREAM, QUALITY_CHECK, 32,     {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 0},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*01*/ MFX_ERR_NONE, QP_STREAM, LCU_CHECK, 32,         {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 0},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*02*/ MFX_ERR_NONE, QP_STREAM, QUALITY_CHECK, 32,     {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 10},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*03*/ MFX_ERR_NONE, QP_STREAM, LCU_CHECK, 32,         {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 10},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*04*/ MFX_ERR_NONE, QP_STREAM, QUALITY_CHECK, 32,     {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 20},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*05*/ MFX_ERR_NONE, QP_STREAM, LCU_CHECK, 32,         {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 20},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*06*/ MFX_ERR_NONE, QP_STREAM, QUALITY_CHECK, 32,     {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 30},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*07*/ MFX_ERR_NONE, QP_STREAM, LCU_CHECK, 32,         {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 30},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*08*/ MFX_ERR_NONE, QP_STREAM, QUALITY_CHECK, 32,     {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*09*/ MFX_ERR_NONE, QP_STREAM, LCU_CHECK, 32,         {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*10*/ MFX_ERR_NONE, QP_STREAM, QUALITY_CHECK, 32,     {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 51},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*11*/ MFX_ERR_NONE, QP_STREAM, LCU_CHECK, 32,         {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 51},
                                                            {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},

    {/*12*/ MFX_ERR_NONE, QP_RANDOM, LCU_CHECK, 32,         {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},

    {/*13*/ MFX_ERR_NONE, QP_FRAME, QUALITY_CHECK, 32,      {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},


    {/*14*/ MFX_ERR_NONE, QP_FRAME, LCU_CHECK, 32,          {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];
    mode                = tc.mode;
    test_type           = tc.test_type;
    block_size          = tc.block_size;

    //set parameters for mfxVideoParam
    SETPARS(m_pPar, MFX_PAR);
    m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)
    m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP; //For now FEI work with CQP only

    //set parameters for mfxEncodeCtrl
    SETPARS(m_pCtrl, MFX_FRM_CTRL);

    mfxU32 nf = frameNumber;
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    MFXInit();

    g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, MFX_PLUGINID_HEVC_FEI_ENCODE);
    m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
    m_loaded = false;
    Load();

    mfxExtFeiHevcEncFrameCtrl& feiCtrl =  m_ctrl;

    feiCtrl.SearchPath         = 2;
    feiCtrl.LenSP              = 57;
    feiCtrl.MultiPred[0]       = 0;
    feiCtrl.MultiPred[1]       = 0;
    feiCtrl.SubPelMode         = 3;
    feiCtrl.AdaptiveSearch     = 0;
    feiCtrl.MVPredictor        = 0;
    feiCtrl.NumMvPredictors[0] = 0;
    feiCtrl.NumMvPredictors[1] = 0;
    feiCtrl.PerCuQp            = 0;
    feiCtrl.RefHeight          = 32;
    feiCtrl.RefWidth           = 32;
    feiCtrl.SearchWindow       = 5;
    feiCtrl.NumFramePartitions = 4;

    mfxU32 widthLCU  = (m_par.mfx.FrameInfo.Width  + block_size - 1)/block_size;
    mfxU32 heightLCU = (m_par.mfx.FrameInfo.Height + block_size - 1)/block_size;
    mfxU32 size = heightLCU * widthLCU * 4;


    if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        SetFrameAllocator(m_session, m_pVAHandle);
        m_pFrameAllocator = m_pVAHandle;
    }
    mfxHDL hdl;
    mfxHandleType type;
    m_pVAHandle->get_hdl(type, hdl);
    m_hevcFeiAllocator = new FeiBufferAllocator((VADisplay) hdl, m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height);

    if (test_type & LCU_CHECK)
    {
        feiCtrl.PerCuQp = 1;
        mfxExtFeiHevcEncQP& hevcFeiEncQp = m_ctrl;
        BufferAllocRequest buf_request;
        buf_request.Width = m_par.mfx.FrameInfo.CropW;
        buf_request.Height = m_par.mfx.FrameInfo.CropH;
        m_hevcFeiAllocator->Alloc(&hevcFeiEncQp, buf_request);
        EncodeFrames(nf);
        Close();
    }
    else if (test_type & QUALITY_CHECK)
    {
        // 1. encode with frame level qp
        tsBitstreamCRC32 bs_crc;
        m_bs_processor = &bs_crc;
        feiCtrl.PerCuQp = 0;

        AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * nf);
        EncodeFrames(nf);

        Ipp32u crc = bs_crc.GetCRC();

        // reset
        Close();
        m_reader->ResetFile();
        memset(&m_bitstream, 0, sizeof(mfxBitstream));
        m_fo = 0;

        // 2. encode with PerLCUQp setting
        tsBitstreamCRC32 bs_cmp_crc;
        m_bs_processor = &bs_cmp_crc;

        //to alloc more surfaces
        AllocSurfaces();
        feiCtrl.PerCuQp = 1;
        EncodeFrames(nf);
        Ipp32u cmp_crc = bs_cmp_crc.GetCRC();
        g_tsLog << "crc = " << crc << "\n";
        g_tsLog << "cmp_crc = " << cmp_crc << "\n";
        if (crc != cmp_crc)
        {
            g_tsLog << "ERROR: the 2 crc values should be the same\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    }

    mfxExtFeiHevcEncQP& feiQP = m_ctrl;
    m_hevcFeiAllocator->Free(&feiQP);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevc_fei_lcuqp);
};
