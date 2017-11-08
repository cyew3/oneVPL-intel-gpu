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

/*
Test MBQP for HEVCe HW plugin.
*/


#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_mbqp
{

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVCAU(BS_HEVC::INIT_MODE_CABAC)
        , m_mode(RANDOM)
        , m_expected(0)
        , m_nFrame(0)
        , m_mbqp(0)
        , m_mbqp_on(true)
        , m_fo(0)
        , m_noiser()
        , m_reader()
        , m_writer("debug.hevc")
    {
        m_bs_processor = this;
        m_filler = this;
        m_mode = 0;
        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 352;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 288;
        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        m_reader = new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"),
                                 m_pPar->mfx.FrameInfo);
        g_tsStreamPool.Reg();
    }

    ~TestSuite()
    {
        if (m_reader)
        {
            delete m_reader;
        }
    }

    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
        std::vector<mfxU8>          buf;
    };
    std::map<mfxU32, Ctrl> m_ctrl;
    mfxU32 m_mode;
    mfxU16 m_expected;
    mfxU32 m_nFrame;
    mfxU32 m_numLCU; //LCU number in frame
    std::vector<mfxU8> m_mbqp;
    bool m_mbqp_on;
    tsExtBufType<mfxEncodeCtrl> m_mbqp_control;
    std::map<mfxU32, tsExtBufType<mfxEncodeCtrl> > m_mbqp_map_control;
    mfxU32 m_fo;
    mfxU32 m_framesToEncode;
    mfxU32 m_ts;
    tsNoiseFiller m_noiser;
    tsRawReader *m_reader;
    tsBitstreamWriter m_writer;
    mfxU32 m_cu;
    mfxU32 m_cuqp_width;
    mfxU32 m_cuqp_height;
    enum
    {
        MFX_PAR = 1,
        EXT_COD2,
        EXT_COD3,
    };

    enum
    {
        INIT    = 1 << 1,       //to set on initialization
        RESET   = 1 << 2,
    };

    enum
    {
        RANDOM = 1,
        LINEAR = 2,
        CONST_QP = 4, //For future use
        YUV_SOURCE = 8,
        INCORRECT_MBQP = 16,  
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
        char* skips;
    };

    static const tc_struct test_case[];
    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_mode & YUV_SOURCE)
        {
           m_reader->ProcessSurface(s);
        }
        else {
            m_noiser.ProcessSurface(s);
        }
        m_mbqp.clear();
        /* Granularity of MBQP buffer 16x16 blocks */
        int BlockSize = 16;
        mfxExtHEVCParam& hp = m_par;
        mfxU32 w = (hp.PicWidthInLumaSamples  + BlockSize - 1)/BlockSize;
        mfxU32 h = (hp.PicHeightInLumaSamples + BlockSize - 1)/BlockSize;
        mfxU32 numMB = w*h;
        if (m_mode & INCORRECT_MBQP)
        {
            numMB = 0;
        }
        m_mbqp.resize(numMB);
        if (m_mode & RANDOM)
        {
           for (size_t i = 0; i <  m_mbqp.size(); ++i)
           {
               m_mbqp[i] = 1 + rand() % 50;
           }
        }
        else if (m_mode & LINEAR)
        {
           for (size_t i = 0; i <  m_mbqp.size(); ++i)
           {
               m_mbqp[i] = i % 50;
           }
        }
        Ctrl& ctrl = m_ctrl[m_fo];
        ctrl.buf.resize(numMB);
        for (size_t i = 0; i <  m_mbqp.size(); ++i)
        {
            ctrl.buf[i] = m_mbqp[i];
        }
        mfxExtMBQP& mbqp = ctrl.ctrl;
        mbqp.QP = &ctrl.buf[0];
        mbqp.NumQPAlloc = mfxU32(ctrl.buf.size());
        if (m_mbqp_on)
        {
            m_pCtrl = &ctrl.ctrl;
        }
        else
        {
            m_pCtrl->NumExtParam = 0;
        }

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;
        return MFX_ERR_NONE;
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
        //return m_writer.ProcessBitstream(bs, nFrames);
        m_ts = bs.TimeStamp;
        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            auto& pic = *au.pic;
            for (mfxU32 i = 0; i < au.NumUnits; i++)
            {
                mfxU16 type = au.nalu[i]->nal_unit_type;
                //check only PPS header
                if (type == 34)
                {
                    auto& pps = *au.nalu[i]->pps;
                    mfxI32 cu_qp_delta_enabled_flag = pps.cu_qp_delta_enabled_flag;
                    printf(" cu_qp_delta_enabled_flag = %i \n", cu_qp_delta_enabled_flag);
                    if (m_mbqp_on)
                    {
                        EXPECT_EQ(cu_qp_delta_enabled_flag, 1) << " ERROR: Unexpected cu_qp_delta_enabled_flag, cu_qp_delta_enabled_flag must be 1 when we use mfxExtCodingOption3.EnableMBQP is ON";
                    }
                    else
                    {
                        EXPECT_EQ(cu_qp_delta_enabled_flag, 0) << " ERROR: Unexpected cu_qp_delta_enabled_flag, cu_qp_delta_enabled_flag must be 0 when we use mfxExtCodingOption3.EnableMBQP is OFF";
                    }

                }
                //check slice segment only
                if (type > 21 || ((type > 9) && (type < 16)))
                {
                    continue;
                }
                if (m_expected == 0) {
                    //not set, so expected value is m_numLCU
                    m_expected = m_numLCU;
                }
                if(m_mode & INCORRECT_MBQP)
                    continue;
                auto& s = *au.nalu[i]->slice;
                BS_HEVC::SPS& sps = *pic.slice[0]->slice->sps;
                mfxU32 Log2MinTrafoSize = sps.log2_min_transform_block_size_minus2 + 2;
                mfxU32 Log2MaxTrafoSize = Log2MinTrafoSize + sps.log2_diff_max_min_transform_block_size;
                mfxU32 Width = sps.pic_width_in_luma_samples;
                mfxU32 Height = sps.pic_height_in_luma_samples;
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
              mfxExtHEVCParam& hp = m_par;
              mfxU32 BlockSize = 16;
              mfxU32 pitch_MBQP = (hp.PicWidthInLumaSamples  + BlockSize - 1)/BlockSize;
              for (Bs32u y = 0; y < (Height >> Log2MinTrafoSize); y++)
              {
                 //16x32 only: driver limitation
                 m_cu =  ((y % k_lcu_w == 0) && (y != 0)) ? (m_cu_old + 2*pitch_MBQP) : m_cu_old;
                 if((y % k_lcu_w == 0) && (y != 0))
                     m_cu_old = m_cu;
                 for (mfxU32 x = 0; x < (Width >> Log2MinTrafoSize); x ++){
                       m_cu += ((x % k_lcu_w == 0) && x != 0) ? 2 : 0;
                       if(QP[y][x] != 255){
                           #if defined(LINUX32) || defined(LINUX64)
                           EXPECT_EQ(int(QP[y][x]), int(m_ctrl[m_ts].buf[m_cu]));
                           #endif
                       }
                 }
                  
               }
                
            }
            m_nFrame++;
        }
        
        m_ctrl.erase((mfxU32)bs.TimeStamp);
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    #if defined(LINUX32) || defined(LINUX64)
    /*00 - W&H Align 32 */
    {MFX_ERR_NONE, RANDOM, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480}}
    },
    /*01 - W&H Align 32, NumSlice = 2 */
    {MFX_ERR_NONE, RANDOM, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480}}
    },
    /*02 - Cropping values unaligned on 16 */
    {MFX_ERR_NONE, RANDOM, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720-8},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480-8}}
    },
    /*03*/
    {MFX_ERR_NONE, RANDOM, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352-16},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288}}
   },
    /*04 - W&H Align 32 */
    {MFX_ERR_NONE, LINEAR, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480}}
    },
    /*05 - W&H Align 32 */
    {MFX_ERR_NONE, LINEAR | YUV_SOURCE, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288}}
    },
    /*06 - W&H Align 32 */
    {MFX_ERR_NONE, RANDOM | YUV_SOURCE, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288}}
    },
    /*07*/
    {MFX_ERR_NONE, RANDOM | INCORRECT_MBQP, {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352-16},
                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288}}
    },
    #else
    /*00*/{MFX_ERR_UNSUPPORTED, 0, {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}},
    #endif
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];
    m_mode = tc.mode;
    srand(id);
    MFXInit();
    Load();
    m_reader->ResetFile();
    SETPARS(m_pPar, MFX_PAR);
    mfxExtCodingOption3& cod3 = m_par;
    SETPARS(&cod3, EXT_COD3);
    mfxExtHEVCParam& hp = m_par;
    m_par.AsyncDepth = 1;
    m_framesToEncode = 30;
    m_mbqp_on = true;

    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.GopRefDist = 2;
    Init();
    g_tsStatus.expect(tc.sts);
    EncodeFrames(m_framesToEncode);
    Close();
    TS_END;
    int err = 0;
    return err;
}

TS_REG_TEST_SUITE_CLASS(hevce_mbqp);
};
