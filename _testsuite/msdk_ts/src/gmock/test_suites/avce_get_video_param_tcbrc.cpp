/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

File Name: avce_get_video_param_tcbrc.cpp

\* ****************************************************************************** */

/*!
\file avce_get_video_param_tcbrc.cpp
\brief Gmock test avce_get_video_param_tcbrc

Description:
This suite checks the correctness of the TargetKbps from GetVideoParam in case when TCBRC (LowDelayBRC with 'fast' bitrate reset) is on (AVC encoder, KBL) \n

Algorithm:
- Initialize MSDK session
- Set initial TargetKbps (4000 in test case)
  -> Encode 3 frames
  -> Get TargetKbps from GetVideoParam
  -> Compare initial and returned value of TargetKbps (they must be equal)
- Then do reset every 3 frames with generating new random TargetKbps (in the range of 50% to 100% of the initial TargetKbps)
  -> Do encode
  -> On each reset get TargetKbps from GetVideoParam
  -> Compare generated and returned value of targetKbps (they must be equal)

*/

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include <fstream>
#include <random>

//#define DUMP_BS(bs);  std::fstream fout("out.bin", std::fstream::binary | std::fstream::out);fout.write((char*)bs.Data, bs.DataLength); fout.close();
namespace avce_get_video_param_tcbrc
{
    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        tsExtBufType<mfxVideoParam> initParams();
    private:
        enum
        {
            MFX_PAR = 1
        };

        struct tc_struct
        {
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType<mfxVideoParam> par;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 720;
        par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
        par.mfx.GopPicSize = 10000;
        par.mfx.GopRefDist = 1;
        par.mfx.IdrInterval = 2;
        par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        par.mfx.LowPower = MFX_CODINGOPTION_ON;
        par.mfx.TargetKbps = 4000;
        par.mfx.MaxKbps = 4000;
        par.AsyncDepth = 1;
        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION, sizeof(mfxExtCodingOption));
        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));
        mfxExtCodingOption& cod = par;
        mfxExtCodingOption3& cod3 = par;
        cod.NalHrdConformance = MFX_CODINGOPTION_OFF;
        cod3.LowDelayBRC = MFX_CODINGOPTION_ON;
        return par;
    }

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR } },
        {/*01*/ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_QVBR } },
        {/*02*/ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VCM } },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        mfxVideoParam new_par = {};
        mfxU32 frames_to_encode = 60;

        MFXInit();

        m_par = initParams();
        SETPARS(&m_par, MFX_PAR);

        // init
        Query();
        Init();
        // prepare to encode
        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * frames_to_encode);
        tsRawReader reader(g_tsStreamPool.Get("YUV/720x480i29_jetcar_CC60f.nv12"), m_par.mfx.FrameInfo, frames_to_encode * 2);
        g_tsStreamPool.Reg();
        m_filler = &reader;
        // encode 3 frames
        EncodeFrames(3, true);
        // check TargetKbps from GetVideoParam
        GetVideoParam(m_session, &new_par);
        EXPECT_EQ(0, memcmp(&(new_par.mfx.TargetKbps), &(m_par.mfx.TargetKbps), sizeof(mfxU32)))
            << "ERROR: " << "reset_num: 0"
            << ",  Initial TargetKbps not equal TargetKbps from GetVideoParam ("
            << m_par.mfx.TargetKbps << " != " << new_par.mfx.TargetKbps << ")";

        mfxU32 num_resets = frames_to_encode / 3 - 1;
        mfxU32 target_kbps_random = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        // new target bitrate generates in the range of 50% to 100% of the initial TargetKbps
        std::uniform_int_distribution<mfxU16> dis(m_par.mfx.TargetKbps / 2, m_par.mfx.TargetKbps);
        for (mfxU32 i = 0; i < num_resets; ++i)
        {
            // generate new TargetKbps
            target_kbps_random = dis(gen);
            // change TargetKbps, do reset
            new_par.mfx.TargetKbps = target_kbps_random;
            Reset(m_session, &new_par);
            // encode 3 frames
            EncodeFrames(3, true);
            // check TargetKbps from GetVideoParam
            GetVideoParam(m_session, &new_par);
            EXPECT_EQ(true, new_par.mfx.TargetKbps == target_kbps_random)
                << "ERROR: " << "reset_num: " << i+1
                << ",  Reset TargetKbps not equal TargetKbps from GetVideoParam ("
                << target_kbps_random << " != " << new_par.mfx.TargetKbps << ")";
        }

        //DUMP_BS(m_bitstream);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_get_video_param_tcbrc);
};