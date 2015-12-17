#include "ts_encoder.h"
#include "ts_struct.h"

/*
 * This test is for HSD7432080 - in FEI, stream encoded with
 * same frameQP and perMBQP are not bit match
 */

namespace fei_encode_mbqp
{

typedef std::vector<mfxExtBuffer*> InBuf;

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true)
        , m_reader()
        , m_fo(0)
        , m_bPerMbQP(false)
        , mode(0)
    {
        srand(0);
        m_filler = this;

        m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 720;
        m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 480;
        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/iceage_720x480_491.yuv"),
                                   m_pPar->mfx.FrameInfo);
        g_tsStreamPool.Reg();

    }
    ~TestSuite()
    {
        delete m_reader;

        //to free buffers
        int numFields = 1;
        if ((m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
            (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)) {
            numFields = 2;
        }
        for (std::vector<InBuf>::iterator it = m_InBufs.begin(); it != m_InBufs.end(); ++it)
        {
            for (std::vector<mfxExtBuffer*>::iterator it_buf = (*it).begin(); it_buf != (*it).end();)
            {
                switch ((*it_buf)->BufferId) {
                case MFX_EXTBUFF_FEI_ENC_CTRL:
                    {
                        mfxExtFeiEncFrameCtrl* feiEncCtrl = (mfxExtFeiEncFrameCtrl*)(*it_buf);
                        delete[] feiEncCtrl;
                        feiEncCtrl = NULL;
                        it_buf += numFields;
                    }
                    break;
                case MFX_EXTBUFF_FEI_PREENC_QP:
                    {
                        mfxExtFeiEncQP* feiEncMbQp = (mfxExtFeiEncQP*)(*it_buf);
                        for (int fieldId = 0; fieldId < numFields; fieldId++) {
                            delete[] feiEncMbQp[fieldId].QP;
                        }
                        delete[] feiEncMbQp;
                        feiEncMbQp = NULL;
                        it_buf += numFields;
                    }
                    break;
                default:
                    g_tsLog << "ERROR: not supported ext buffer\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                    break;
                }
            }
            (*it).clear();
        }
        m_InBufs.clear();
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        //calculate numMB
        mfxU32 widthMB  = ((m_par.mfx.FrameInfo.Width  + 15) & ~15);
        mfxU32 heightMB = ((m_par.mfx.FrameInfo.Height + 15) & ~15);
        mfxU32 numMB = heightMB * widthMB / 256;
        mfxU32 numField = 1;
        if ((m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
            (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)) {
            numField = 2;
            numMB = widthMB / 16 * (((heightMB / 16) + 1) / 2);
        }

        InBuf in_buffs;
        mfxExtFeiEncFrameCtrl *feiEncCtrl = NULL;
        mfxExtFeiEncQP *feiEncMbQp = NULL;
        mfxU32 fieldId = 0;

        if (!m_bPerMbQP) {
            //1st stage, set per-frame qp only
            //in this stage, it may generate random qp value during runtime
            if (mode & RANDOM) {
                m_qp[m_fo] = 1 + rand() % 50;
                m_pCtrl->QP = m_qp[m_fo];
            } else {
                m_qp[m_fo] = m_pCtrl->QP;
            }

            //assign ExtFeiEncFrameCtrl
            for (fieldId = 0; fieldId < numField; fieldId++) {
                if (fieldId == 0) {
                    feiEncCtrl = new mfxExtFeiEncFrameCtrl[numField];
                    memset(feiEncCtrl, 0, numField * sizeof(mfxExtFeiEncFrameCtrl));
                }
                feiEncCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
                feiEncCtrl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
                feiEncCtrl[fieldId].SearchPath = 2;
                feiEncCtrl[fieldId].LenSP = 57;
                feiEncCtrl[fieldId].SubMBPartMask = 0;
                feiEncCtrl[fieldId].MultiPredL0 = 0;
                feiEncCtrl[fieldId].MultiPredL1 = 0;
                feiEncCtrl[fieldId].SubPelMode = 3;
                feiEncCtrl[fieldId].InterSAD = 2;
                feiEncCtrl[fieldId].IntraSAD = 2;
                feiEncCtrl[fieldId].DistortionType = 0;
                feiEncCtrl[fieldId].RepartitionCheckEnable = 0;
                feiEncCtrl[fieldId].AdaptiveSearch = 0;
                feiEncCtrl[fieldId].MVPredictor = 0;
                feiEncCtrl[fieldId].NumMVPredictors = 1;
                feiEncCtrl[fieldId].PerMBQp = 0; //non-zero value
                feiEncCtrl[fieldId].PerMBInput = 0;
                feiEncCtrl[fieldId].MBSizeCtrl = 0;
                feiEncCtrl[fieldId].RefHeight = 32;
                feiEncCtrl[fieldId].RefWidth = 32;
                feiEncCtrl[fieldId].SearchWindow = 0;

                // put the buffer in in_buffs
                in_buffs.push_back((mfxExtBuffer *)&feiEncCtrl[fieldId]);
            }

            //save in_buffs into m_InBufs
            m_InBufs.push_back(in_buffs);
        } else {
            //2nd stage, enable per-mb qp, set same value as the frame qp
            m_pCtrl->QP = m_qp[m_fo];
            in_buffs = m_InBufs[m_fo];

            //assert(in_buffs.size() == numField);
            std::vector<mfxExtBuffer*>::iterator it;
            for (it = in_buffs.begin(); it != in_buffs.end(); ++it) {
                if ((*it)->BufferId == MFX_EXTBUFF_FEI_ENC_CTRL) {
                    mfxExtFeiEncFrameCtrl *feiEncCtrl = (mfxExtFeiEncFrameCtrl *)(*it);
                    //if mbqp to be tested, enable per-mb qp in mfxExtFeiEncFrameCtrl
                    feiEncCtrl->PerMBQp = 1;
                }
            }
            //assert(it == in_buffs.end());

            for (fieldId = 0; fieldId < numField; fieldId++) {
                //assign ExtFeiEncQP
                if (fieldId == 0) {
                    feiEncMbQp = new mfxExtFeiEncQP[numField];
                    memset(feiEncMbQp, 0, numField * sizeof(mfxExtFeiEncQP));
                }
                feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                feiEncMbQp[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncQP);
                feiEncMbQp[fieldId].NumQPAlloc = numMB;
                feiEncMbQp[fieldId].QP = new mfxU8[numMB];

                memset(feiEncMbQp[fieldId].QP, m_qp[m_fo], feiEncMbQp[fieldId].NumQPAlloc*sizeof(feiEncMbQp[fieldId].QP[0]));

                //put the buffer in in_buffs
                in_buffs.push_back((mfxExtBuffer*)&feiEncMbQp[fieldId]);
            }

            //assert(in_buffs.size() == 2 * numField);

            //save the updated in_buffs into m_InBufs
            m_InBufs[m_fo] = in_buffs;
        }

        //m_pCtrl->ExtParam = &in_buffs[0];
        m_pCtrl->ExtParam = &(m_InBufs[m_fo])[0];
        m_pCtrl->NumExtParam = (mfxU16)in_buffs.size();
        printf("m_pCtrl->NumExtParam = %d\n", m_pCtrl->NumExtParam);
        printf("m_pCtrl->ExtParam = %d\n", m_pCtrl->ExtParam);

        m_fo++;
        return sts;
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
        RANDOM = 1 //random per-frame qp
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

    tsRawReader *m_reader;
    std::map<mfxU32, mfxU8> m_qp;
    mfxU32 m_fo;
    bool m_bPerMbQP;
    mfxU32 mode;
    std::vector<InBuf> m_InBufs;
};


#define mfx_PicStruct tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_IdrInterval tsStruct::mfxVideoParam.mfx.IdrInterval
#define mfx_NumRefFrame tsStruct::mfxVideoParam.mfx.NumRefFrame
#define mfx_IOPattern tsStruct::mfxVideoParam.IOPattern
const TestSuite::tc_struct TestSuite::test_case[] =
{
    // progressive
    {/*00*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*01*/ MFX_ERR_NONE, RANDOM,  {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
    // interlace frame tff
    {/*02*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*03*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    // interlace frame bff
    {/*04*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*05*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    // video memory
    {/*06*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},
    {/*07*/ MFX_ERR_NONE, RANDOM,  {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},
    {/*08*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},
    {/*09*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},
    {/*10*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},
    {/*11*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},
    // different gop patterns
    {/*12*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*13*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*14*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*15*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*16*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*17*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*18*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*19*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*20*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*21*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*22*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*23*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*24*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*25*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_GopPicSize, 1},
                                   {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*26*/ MFX_ERR_NONE, 0,      {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
                                   {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*27*/ MFX_ERR_NONE, RANDOM, {{MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
                                   {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                   {MFX_PAR, &mfx_GopPicSize, 15},
                                   {MFX_PAR, &mfx_GopRefDist, 3}}},
    //TODO: add more parameters suite
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mode = tc.mode;

    //set parameters for mfxVideoParam
    SETPARS(m_pPar, MFX_PAR);
    m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)
    m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP; //For now FEI work with CQP only

    //set parameters for mfxEncodeCtrl
    SETPARS(m_pCtrl, MFX_FRM_CTRL);

    mfxU32 nf = frameNumber;
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

// 1. encode with frame level qp
    tsBitstreamCRC32 bs_crc;
    m_bs_processor = &bs_crc;

    MFXInit();
    if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        SetFrameAllocator(m_session, m_pVAHandle);
        m_pFrameAllocator = m_pVAHandle;
    }
    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * nf);
    EncodeFrames(nf);
    Ipp32u crc = bs_crc.GetCRC();

// reset
    Close();
    m_reader->ResetFile();
    memset(&m_bitstream, 0, sizeof(mfxBitstream));
    m_fo = 0;
    m_bPerMbQP = true; //indicate the 2nd stage, per-mb qp is enabled

// 2. encode with PerMBQp setting
    tsBitstreamCRC32 bs_cmp_crc;
    m_bs_processor = &bs_cmp_crc;

    //to alloc more surfaces
    AllocSurfaces();
    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * nf);
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

TS_REG_TEST_SUITE_CLASS(fei_encode_mbqp);
};
