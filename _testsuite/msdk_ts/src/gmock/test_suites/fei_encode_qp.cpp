#include "ts_encoder.h"
#include "ts_struct.h"

/*
 * This test is for HSD7432080 - in FEI, stream encoded with
 * same frameQP and perMBQP are not bit match
 */

namespace fei_encode_qp
{

class TestSuite : public tsVideoEncoder
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true)
    {
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;


private:
    enum
    {
        MFXPAR = 1,
        EXT_FRM_CTRL,
        MFX_FRM_CTRL
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
    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] =
{
    // qp
    {/*00*/ MFX_ERR_NONE, 0, {MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42}},
    {/*01*/ MFX_ERR_NONE, 0, {MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 50}},
    // LenSP/MaxLenSP
    {/*03*/ MFX_ERR_NONE, 0, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 1},
                              {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 14},
                              {MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42}}
    },
    {/*04*/ MFX_ERR_NONE, 0, {{EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.LenSP, 63},
                              {EXT_FRM_CTRL, &tsStruct::mfxExtFeiEncFrameCtrl.SearchPath, 63},
                              {MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42}}
    },
    //TODO: add more parameters suite
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 720;
    m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 480;
    m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    tsRawReader stream(g_tsStreamPool.Get("YUV/iceage_720x480_491.yuv"), m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_filler = &stream;

    m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)

    // Attach input structures
    std::vector<mfxExtBuffer*> in_buffs;

    //assign ExtFeiEncFrameCtrl
    mfxExtFeiEncFrameCtrl in_efc = {0};
    in_efc.Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
    in_efc.Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
    in_efc.SearchPath = 2;
    in_efc.LenSP = 57;
    in_efc.SubMBPartMask = 0;
    in_efc.MultiPredL0 = 0;
    in_efc.MultiPredL1 = 0;
    in_efc.SubPelMode = 3;
    in_efc.InterSAD = 2;
    in_efc.IntraSAD = 2;
    in_efc.DistortionType = 0;
    in_efc.RepartitionCheckEnable = 0;
    in_efc.AdaptiveSearch = 0;
    in_efc.MVPredictor = 0;
    in_efc.NumMVPredictors = 1;
    in_efc.PerMBQp = 0; //non-zero value
    in_efc.PerMBInput = 0;
    in_efc.MBSizeCtrl = 0;
    in_efc.RefHeight = 32;
    in_efc.RefWidth = 32;
    in_efc.SearchWindow = 5;
    in_buffs.push_back((mfxExtBuffer*)&in_efc);

    // set up parameters for case
    SETPARS(m_pPar, MFXPAR);
    SETPARS(&in_efc, EXT_FRM_CTRL);
    SETPARS(m_pCtrl, MFX_FRM_CTRL);

    m_pCtrl->ExtParam = &in_buffs[0];
    m_pCtrl->NumExtParam = (mfxU16)in_buffs.size();

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    mfxU32 nf = 5;

// 1. encode with frame level qp
    tsBitstreamCRC32 bs_crc;
    m_bs_processor = &bs_crc;

    Init();
    AllocSurfaces();
    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * 10);
    EncodeFrames(nf);
    Ipp32u crc = bs_crc.GetCRC();
    g_tsLog << "crc = " << crc << "\n";

// reset /
    Close();
    stream.ResetFile();
    memset(&m_bitstream, 0, sizeof(mfxBitstream));

// 2. encode with PerMBQp setting
    mfxU32 num_mb = mfxU32(m_par.mfx.FrameInfo.Width / 16) * mfxU32(m_par.mfx.FrameInfo.Height / 16);

    //enable permbqp
    in_efc.PerMBQp = 1;

    //assign ExtFeiEncQP
    mfxExtFeiEncQP feiEncMbQp = {0};
    feiEncMbQp.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
    feiEncMbQp.Header.BufferSz = sizeof (mfxExtFeiEncQP);
    feiEncMbQp.NumQPAlloc = num_mb;
    feiEncMbQp.QP = new mfxU8[num_mb];
    memset(feiEncMbQp.QP, m_pCtrl->QP, num_mb);
    in_buffs.push_back((mfxExtBuffer*)&feiEncMbQp);

    m_pCtrl->ExtParam = &in_buffs[0];
    m_pCtrl->NumExtParam = (mfxU16)in_buffs.size();

    tsBitstreamCRC32 bs_cmp_crc;
    m_bs_processor = &bs_cmp_crc;

    Init();
    AllocSurfaces();
    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * 10);
    EncodeFrames(nf);
    Ipp32u cmp_crc = bs_cmp_crc.GetCRC();
    g_tsLog << "crc = " << crc << "\n";
    g_tsLog << "cmp_crc = " << cmp_crc << "\n";
    if (crc != cmp_crc)
    {
        g_tsLog << "ERROR: the 2 crc values should be the same\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_qp);
};
