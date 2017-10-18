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
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"
#include "ts_parser.h"
#include "vaapi_buffer_allocator.h"

// This test has to check quality and bitrate for Macroblock QP feature
// We have 3 main different cases: 
//a) compare stream QP (for whole stream) with stream that has QP value for each CTU
//b) compare stream with frame QP (for each frame) with stream that has QP value for each CTU
//c) The case when we have EnableMBQP is ON but we do not fill lcuqp buffer and do not send to MSDK
//here we expect different streams because int the stream with ctu qp has pps.cu_qp_delta_enable_flag equal 1
//
//1. In the all cases we check CRC YUV for frame qp stream and ctuqp stream. That CRC must be equal
//2. In the all cases we check bitrate of stream and calculate difference between them
//3. In the case when we do not send buffer, we check CRC yuv and bitrates between two streams. YUV must be equal, difference between bitrates must be less than EXPECTED_BITRATE_DIFFERENCE (One percent)
//4. We run the cases with differnt values of GopRefDist and SliceNum

namespace hevce_ctuqp_quality
{


#define DEBUG_STREAM 0//"debug.h265"
#define EXPECTED_BITRATE_DIFFERENCE 1.0 //Percent
class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVCAU(BS_HEVC::INIT_MODE_CABAC)
        , m_reader()
        , m_fo(0)
        , mode(0)
        , block_size(16)
        , qp_value(0)
        , m_writer(DEBUG_STREAM)

    {
        m_filler = this;
        m_bs_processor = this;
        qp_frame_vector.resize(30);
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
            delete m_reader;
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);
        m_qp.clear();

        int BlockSize = block_size;
        mfxU32 widthLCU  = (m_par.mfx.FrameInfo.Width  + BlockSize - 1)/BlockSize;
        mfxU32 heightLCU = (m_par.mfx.FrameInfo.Height + BlockSize - 1)/BlockSize;

        mfxU32 QPsize = heightLCU * widthLCU;

        m_qp.resize(QPsize);

        if (!m_mbqp_on)
        {
            if (mode & QP_FRAME)
            {
                mfxU32 rand_qp = 1 + rand() % 50;
                qp_frame_vector[m_fo] = rand_qp;
                m_pCtrl->QP           = rand_qp;
            }
            else if (mode & QP_STREAM)
            {
                m_pCtrl->QP = qp_value;  
            }
            m_fo++;
            return sts;
        }


        Ctrl& ctrl = m_mbqp_ctrl[m_fo];
        ctrl.buf.resize(QPsize);
        mfxExtMBQP& mbqp = ctrl.ctrl;
        mbqp.QP = &ctrl.buf[0];
        mbqp.NumQPAlloc = mfxU32(ctrl.buf.size());

        if (mode & QP_FRAME) {
            m_pCtrl->QP = qp_frame_vector[m_fo]; //it is needed if we have not buffer
            ctrl.ctrl.QP = qp_frame_vector[m_fo];
            for (int i = 0; i < (int)m_qp.size(); i++)
            {
                ctrl.buf[i] = qp_frame_vector[m_fo];
            }
        } else {
            m_pCtrl->QP = qp_value;
            ctrl.ctrl.QP = qp_value;
            for (int i = 0; i < (int)m_qp.size(); i++)
            {
                ctrl.buf[i] = qp_value;
            }
        }
        if (m_mbqp_on)
        {
            if(!(mode & QP_NO_BUFFER)){
                m_pCtrl = &ctrl.ctrl;
            }
            else
            {
                m_pCtrl->NumExtParam = 0;    
            }
        }
        else
        {
            m_pCtrl->NumExtParam = 0;
        }
        m_fo++;

        return sts;
    }

    struct LCU_QP
    {
        std::vector<mfxU8> buf;
    };
    enum
    {
        MFX_PAR = 1,
        EXT_COD2,
        EXT_COD3,
    };

    enum
    {
        QP_FRAME      = 1,
        QP_STREAM     = 2,
        QP_NO_BUFFER  = 4
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
        mfxU32 qp_value;
    };
    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
        std::vector<mfxU8>          buf;
    };
    std::map<mfxU32, Ctrl> m_mbqp_ctrl;
    std::vector<mfxU8> m_mbqp;
    bool m_mbqp_on;
    static const tc_struct test_case[];

    tsRawReader *m_reader;
    std::vector<mfxU8> m_qp;
    std::vector<mfxU8> qp_frame_vector;
    LCU_QP *lcu_buf;
    mfxU32 m_fo;
    mfxU32 mode;

    int block_size;
    vaapiBufferAllocator *m_hevcFeiAllocator;
    mfxU32 qp_value;

    #ifdef DEBUG_STREAM
        tsBitstreamWriter m_writer;
    #endif
};

class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVCAU
{
private:
    mfxI32 m_i;
    mfxU32 m_pic_struct;
    mfxVideoParam& m_par;
    const TestSuite::tc_struct & m_tc;
    mfxU32 m_IdrCnt;
#ifdef DEBUG_STREAM
    tsBitstreamWriter m_writer;
#endif
public:
    mfxU8  *m_buf;
    mfxU32 m_len;    //data length in m_buf
    mfxU32 m_buf_sz; //buf size

    BitstreamChecker(const TestSuite::tc_struct & tc, mfxVideoParam& par)
        : tsParserHEVCAU()
        , m_i(0)
        , m_par(par)
        , m_tc(tc)
        , m_IdrCnt(0)
#ifdef DEBUG_STREAM
        , m_writer("debug.265")
#endif
        , m_len(0)
    {
        m_buf_sz = m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 4 * 30;
        m_buf = new mfxU8[m_buf_sz];
    }

    ~BitstreamChecker()
    {
        if(m_buf)
        {
            delete [] m_buf;
            m_buf = NULL;
        }
    }
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxU32 checked = 0;
    SetBuffer(bs);
    memcpy(&m_buf[m_len], bs.Data + bs.DataOffset, bs.DataLength - bs.DataOffset);
    m_len += bs.DataLength - bs.DataOffset;

#ifdef DEBUG
    m_writer.ProcessBitstream(bs, nFrames);
#endif

    while (checked++ < nFrames)
    {
        UnitType& hdr = ParseOrDie();
    }
    bs.DataLength = 0;

    return MFX_ERR_NONE;
}


const TestSuite::tc_struct TestSuite::test_case[] =
{ 
    #if defined(LINUX32) || defined(LINUX64)
    {/*00*/ MFX_ERR_NONE, QP_STREAM,      {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, 1},

    {/*01*/ MFX_ERR_NONE, QP_STREAM,      {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, 10},

    {/*02*/ MFX_ERR_NONE, QP_STREAM,      {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, 20},

    {/*03*/ MFX_ERR_NONE, QP_STREAM,      {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, 30},

    {/*04*/ MFX_ERR_NONE, QP_STREAM,      {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, 42},
    
    {/*05*/ MFX_ERR_NONE, QP_STREAM,      {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, 51},

    {/*06*/ MFX_ERR_NONE, QP_FRAME,       {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}},

    {/*07*/ MFX_ERR_NONE, QP_NO_BUFFER | QP_STREAM,      {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, 31},

    {/*08*/ MFX_ERR_NONE, QP_STREAM,      {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2}}, 20},

    {/*9*/ MFX_ERR_NONE, QP_STREAM,      {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4}}, 30},

    {/*10*/ MFX_ERR_NONE, QP_STREAM,      {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8}}, 41},

    {/*11*/ MFX_ERR_NONE, QP_STREAM,      {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3}}, 21},

    {/*12*/ MFX_ERR_NONE, QP_STREAM,      {{EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}}, 21},
    #else
    {/*00*/ MFX_ERR_UNSUPPORTED, QP_FRAME,       {EXT_COD3, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON}},
    #endif
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];
    mode                = tc.mode;
    qp_value            = tc.qp_value;

    //set parameters for mfxVideoParam
    m_pPar->mfx.GopRefDist = 1;
    m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    SETPARS(m_pPar, MFX_PAR);
    m_pPar->AsyncDepth = 1;
    mfxExtCodingOption3& cod3 = m_par;
    SETPARS(&cod3, EXT_COD3);
    cod3.EnableMBQP = MFX_CODINGOPTION_OFF;

    mfxU32 nf = frameNumber;
    g_tsStatus.expect(tc.sts);
    m_mbqp_on = false;
    MFXInit();
    Load();

    BitstreamChecker c(tc, *m_pPar);

    m_bs_processor = &c;

    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * nf);
    EncodeFrames(nf);
    Close();

    tsVideoDecoder dec(MFX_CODEC_HEVC);
    
    mfxBitstream bs;
    memset(&bs, 0, sizeof(bs));
    bs.Data = c.m_buf;
    bs.DataLength = c.m_len;
    bs.MaxLength = c.m_buf_sz;
    int bitrate_frame = c.m_len;
    tsBitstreamReader reader(bs, c.m_buf_sz);
    dec.m_bs_processor = &reader;
    tsSurfaceCRC32 crc_proc_yuv;
    dec.m_surf_processor = &crc_proc_yuv;

    dec.Init();
    dec.AllocSurfaces();
    dec.DecodeFrames(nf);
    
    Ipp32u crc = crc_proc_yuv.GetCRC();
    dec.Close();

    m_reader->ResetFile();
    
    memset(&m_bitstream, 0, sizeof(mfxBitstream));
    
    m_fo = 0;

    // 2. encode with PerMBQp setting
    BitstreamChecker c1(tc, *m_pPar);
    m_bs_processor = &c1;

    //to alloc more surfaces
    AllocSurfaces();
    m_mbqp_on = true;
    mfxExtHEVCParam& hp = m_par;    
    cod3.EnableMBQP = MFX_CODINGOPTION_ON;
    EncodeFrames(nf);
    tsVideoDecoder dec1(MFX_CODEC_HEVC);

    mfxBitstream bs1;
    memset(&bs1, 0, sizeof(bs1));
    bs1.Data = c1.m_buf;
    bs1.DataLength = c1.m_len;
    bs1.MaxLength = c1.m_buf_sz;
    int bitrate_ctu = c1.m_len;
    tsBitstreamReader reader1(bs1, c1.m_buf_sz);
    dec1.m_bs_processor = &reader1;

    tsSurfaceCRC32 crc_cmp_yuv;
    dec1.m_surf_processor = &crc_cmp_yuv;
    dec1.Init();
    dec1.AllocSurfaces();
    dec1.DecodeFrames(nf);

    Ipp32u cmp_crc = crc_cmp_yuv.GetCRC();
    g_tsLog << "crc = " << crc << "\n";
    g_tsLog << "cmp_crc = " << cmp_crc << "\n";
    if (crc != cmp_crc)
    {
        g_tsLog << "ERROR: the 2 crc values should be the same\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    } 
    double diff = (double(abs(bitrate_ctu - bitrate_frame)) / double(std::max(bitrate_ctu, bitrate_frame) + 1)) * 100; //Percents
    g_tsLog << "First stream Rate = " << bitrate_frame << "\n";
    g_tsLog << "Second stream Rate = " << bitrate_ctu << "\n";
    if(diff > EXPECTED_BITRATE_DIFFERENCE)
    {
        g_tsLog << "ERROR: Bitrate difference more threshold\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_ctuqp_quality);
};
