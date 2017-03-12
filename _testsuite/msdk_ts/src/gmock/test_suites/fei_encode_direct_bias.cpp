/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define BS_LEN 1024*1024*20

namespace fei_encode_direct_bias
/*
 Enabling direct bias will produce fewer direct mode MBs in B frame.
 1. With the specific stream, global motion scenario;
 2. In per-MB level in FEI, enable direct bias adjustment and disable;
 3. Compare the number of direct MB in Encoded stream;
 4. The number should be decreased at least by the threshold.
 * */
{

class Parser : public tsBitstreamProcessor, public tsParserH264AU
{
    tsBitstreamWriter *mp_writer;
public:
    mfxU32 m_num_direct;
    Parser(const char* fname = NULL)
        : tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        , m_num_direct(0)
    {
        if(fname) {
            mp_writer = new tsBitstreamWriter(fname);
        }
    }
    ~Parser()
    {
        if(mp_writer) {
            delete mp_writer;
        }
    }

    mfxStatus ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
    {
        if(mp_writer) {
            mfxU32 tmp;
            tmp = bs.DataLength;
            mp_writer -> ProcessBitstream(bs, nFrames);
            bs.DataLength = tmp;
        }

        SetBuffer(bs);
        BS_H264_au au;
        au = ParseOrDie();
        if(m_sts == BS_ERR_MORE_DATA) { //end of bs buffer
            bs.DataLength = 0;
            return MFX_ERR_NONE;
        }

        for(Bs32u au_ind = 0; au_ind < au.NumUnits; au_ind++) {
            auto nalu = &au.NALU[au_ind];

            if(nalu->nal_unit_type == 0x01) {
                slice_header& s = *nalu->slice_hdr;
                if((s.slice_type % 5) == 1) {
                    for(Bs32u num_ind = 0; num_ind < s.NumMb; num_ind++) {
                        if(s.mb[num_ind].mb_skip_flag == 0 && s.mb[num_ind].mb_type == 0) {
                            m_num_direct++;
                        }
                    }
                }
            }
        }

        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }
};

class TestSuite:tsSession
{
public:
    static const unsigned int n_cases;

    TestSuite()
        : m_nframes(100) {}
    ~TestSuite() {}

    int RunTest(unsigned int id);

private:
    mfxU32 m_nframes;

    enum {
        MFX_PAR = 1,
        EXT_COD,
        CODPAR,
        COD3PAR
    };

    struct tc_struct
    {
        mfxStatus sts;
        const char* stream;
        mfxU32 threshold;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ MFX_ERR_NONE, "YUV/cust/Fairytale_1280x720p.yuv", 40,
             {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 300},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, MSDK_ALIGN16(1280)},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, MSDK_ALIGN16(720)},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, MSDK_ALIGN16(1280)},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, MSDK_ALIGN16(720)},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 40},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 40},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 40}},
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    srand(0);
    const tc_struct& tc = test_case[id];

    tsVideoEncoder enc_0(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true);
    tsVideoEncoder enc_1(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true);
    Parser parser0;
    Parser parser1;
    enc_0.m_bs_processor = &parser0;
    enc_1.m_bs_processor = &parser1;

    SETPARS(enc_0.m_pPar, MFX_PAR);
    SETPARS(enc_1.m_pPar, MFX_PAR);

    tsRawReader stream0(g_tsStreamPool.Get(tc.stream), enc_0.m_pPar->mfx.FrameInfo);
    tsRawReader stream1(g_tsStreamPool.Get(tc.stream), enc_1.m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    enc_0.m_filler = &stream0;
    enc_1.m_filler = &stream1;

    mfxExtFeiEncFrameCtrl in_efc[2] = {};
    mfxExtFeiEncMBCtrl in_mbc[2] = {};
    std::vector<mfxExtBuffer*> in_buffs0, in_buffs1;

    mfxU32 mbWidth  = enc_0.m_pPar->mfx.FrameInfo.Width/16;
    mfxU32 mbHeight = enc_0.m_pPar->mfx.FrameInfo.Height/16;
    mfxU32 numMB    = mbWidth * mbHeight;

    for(int i = 0; i < 2; i++) {
        in_efc[i].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        in_efc[i].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
        in_efc[i].SearchPath = 57;
        in_efc[i].LenSP = 57;
        in_efc[i].MultiPredL0 = 0;
        in_efc[i].MultiPredL1 = 0;
        in_efc[i].SubPelMode = 3;
        in_efc[i].InterSAD = 2;
        in_efc[i].IntraSAD = 2;
        in_efc[i].DistortionType = 2;
        in_efc[i].RepartitionCheckEnable = 0;
        in_efc[i].AdaptiveSearch = 1;
        in_efc[i].MVPredictor = 0;
        in_efc[i].NumMVPredictors[0] = 0;
        in_efc[i].PerMBQp = 0;
        in_efc[i].PerMBInput = 1;
        in_efc[i].MBSizeCtrl = 0;
        in_efc[i].SearchWindow = 2;

        in_mbc[i].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
        in_mbc[i].Header.BufferSz = sizeof(mfxExtFeiEncMBCtrl);
        in_mbc[i].MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[numMB];
        memset(in_mbc[i].MB, 0, numMB * sizeof(mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB));
        in_mbc[i].NumMBAlloc = numMB;
    }

    for(mfxU32 row = 0; row < mbHeight; row++) {
        for(mfxU32 col = 0; col < mbWidth; col++) {
            in_mbc[0].MB[row * mbWidth + col].DirectBiasAdjustment = 0;
            in_mbc[1].MB[row * mbWidth + col].DirectBiasAdjustment = 1;
        }
    }

    in_buffs0.push_back((mfxExtBuffer*)&in_efc[0]);
    in_buffs0.push_back((mfxExtBuffer*)&in_mbc[0]);
    in_buffs1.push_back((mfxExtBuffer*)&in_efc[1]);
    in_buffs1.push_back((mfxExtBuffer*)&in_mbc[1]);

    enc_0.m_ctrl.ExtParam = &in_buffs0[0];
    enc_0.m_ctrl.NumExtParam = (mfxU16)in_buffs0.size();
    enc_1.m_ctrl.ExtParam = &in_buffs1[0];
    enc_1.m_ctrl.NumExtParam = (mfxU16)in_buffs1.size();

    enc_0.Init();
    enc_1.Init();

    g_tsStatus.expect(tc.sts);
    enc_0.EncodeFrames(m_nframes);
    enc_1.EncodeFrames(m_nframes);

    g_tsLog << "Num direct MB for parser1 " << parser0.m_num_direct << " Num direct MB for parser2 "  << parser1.m_num_direct << "\n";
    EXPECT_GT(parser0.m_num_direct * (100 - tc.threshold) / 100, parser1.m_num_direct)
        << "ERROR: Direct bias adjustment should decrease the number of B direct MB, with threshold: " << tc.threshold << "\n";

    enc_0.Close();
    enc_1.Close();

    for(int i = 0; i < 2; i++) {
        if(in_mbc[i].MB) {
            delete[] in_mbc[i].MB;
        }
    }

    TS_END;

    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_direct_bias);
}
