#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

/*
 *
 *
 */

namespace fei_encode_field_coding
{
enum
{
    QP_INVALID = 255,
    QP_DO_NOT_CHECK = 254
};

class AUWrap : public H264AUWrapper
{
private:
    mfxU8 m_prevqp;
public:

    AUWrap(BS_H264_au& au)
        : H264AUWrapper(au)
        , m_prevqp(QP_INVALID)
    {
    }

    ~AUWrap()
    {
    }

    mfxU32 GetFrameType()
    {
        if (!m_sh)
            return 0;

        mfxU32 type = 0;
        switch (m_sh->slice_type % 5)
        {
        case 0:
            type = MFX_FRAMETYPE_P;
            break;
        case 1:
            type = MFX_FRAMETYPE_B;
            break;
        case 2:
            type = MFX_FRAMETYPE_I;
            break;
        default:
            break;
        }

        return type;
    }

    bool IsBottomField()
    {
        return m_sh && m_sh->bottom_field_flag;
    }

    mfxU8 NextQP()
    {
        if (!NextMB())
            return QP_INVALID;

        //assume bit-depth=8
        if (m_mb == m_sh->mb)
            m_prevqp = 26 + m_sh->pps_active->pic_init_qp_minus26 + m_sh->slice_qp_delta;

        m_prevqp = (m_prevqp + m_mb->mb_qp_delta + 52) % 52;

        if (m_sh->slice_type % 5 == 2)  // Intra
        {
            // 7-11
            if (m_mb->mb_type == 0 && m_mb->coded_block_pattern == 0 || m_mb->mb_type == 25)
                return QP_DO_NOT_CHECK;
        } else
        {
            if (m_mb->mb_skip_flag)
                return QP_DO_NOT_CHECK;

            if (m_sh->slice_type % 5 == 0)  // P
            {
                // 7-13 + 7-11
                if ((m_mb->mb_type <= 5 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 30)
                    return QP_DO_NOT_CHECK;
            } else
            {   // 7-14 + 7-11
                if ((m_mb->mb_type <= 23 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 48)
                    return QP_DO_NOT_CHECK;
            }
        }

        return m_prevqp;
    }
};


class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true)
        , tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        , m_reader()
        , m_fo(0)
        , m_mode(0)
        , m_writer("/tmp/debug.264")
    {
        srand(0);
        m_filler = this;
        m_bs_processor = this;

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
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        mfxU32 index = m_fo * 2; //index to m_qp;
        mfxU32 i = 0;
        for (i = 0; i < m_pCtrl->NumExtParam; i++) {
            //generate random QP value for 2 fields of the frame and assign mfxExtFeiEncQP
            if (m_pCtrl->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP) {
                m_qp[index] = 1 + rand() % 50;
                printf("m_qp[%d] = %d\n", index, m_qp[index]);
                mfxExtFeiEncQP *mbQP = (mfxExtFeiEncQP *)m_pCtrl->ExtParam[i];
                memset(mbQP->QP, m_qp[index], mbQP->NumQPAlloc);
                index++;
            }
        }
        //there must be 2 EXTBUFF_FEI_PREENC_QP in ExtParam.
        assert(index == (m_fo + 1) * 2);

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;
        return sts;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        assert(nFrames == 1);
        while (checked < nFrames)
        {
            UnitType& hdr = ParseOrDie();
            AUWrap au(hdr);
            mfxU8 qp = au.NextQP();
            mfxU32 i = 0;
            mfxU32 wMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16);
            mfxU32 hMB = ((m_par.mfx.FrameInfo.CropH + 15) / 16);
            bool bff = (bool)(m_par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF);
            //index: to calculate index to m_qp
            mfxU32 index = 2 * bs.TimeStamp + (mfxU32)(bff ^ au.IsBottomField());
            mfxU8 expected_qp = m_qp[index];

            g_tsLog << "Frame #"<< bs.TimeStamp <<" QPs:\n";

            while (qp != QP_INVALID)
            {

                if (qp != QP_DO_NOT_CHECK && expected_qp != qp)
                {
                    g_tsLog << "\nFAIL: Expected QP (" << mfxU16(expected_qp) << ") != real QP (" << mfxU16(qp) << ")\n";
                    g_tsLog << "\nERROR: Expected QP != real QP\n";
                    return MFX_ERR_ABORTED;
                }

                if (i++ % wMB == 0)
                    g_tsLog << "\n";

                g_tsLog << std::setw(3) << mfxU16(qp) << " ";

                qp = au.NextQP();
            }
            g_tsLog << "\n";
            checked++;
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
    std::map<mfxU32, mfxU8> m_qp; //contains qp for every "field"
    mfxU32 m_fo;
    mfxU32 m_mode;
    tsBitstreamWriter m_writer;
};


#define mfx_PicStruct tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_IdrInterval tsStruct::mfxVideoParam.mfx.IdrInterval
#define mfx_NumRefFrame tsStruct::mfxVideoParam.mfx.NumRefFrame
#define mfx_IOPattern tsStruct::mfxVideoParam.IOPattern
const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ MFX_ERR_NONE, 0,      {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    {/*02*/ MFX_ERR_NONE, 0,      {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    m_mode = tc.mode;

    //set parameters for mfxVideoParam
    SETPARS(m_pPar, MFX_PAR);
    m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)
    m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP; //For now FEI work with CQP only
    //TODO: if B-frame is enabled, test would fail, debugging
    m_pPar->mfx.GopRefDist = 1;

    //enable single field coding
    mfxExtFeiParam& extFeiPar = m_par;
    extFeiPar.SingleFieldProcessing = MFX_CODINGOPTION_ON;

    //calculat numMB
    mfxU32 widthMB  = ((m_par.mfx.FrameInfo.Width  + 15) & ~15);
    mfxU32 heightMB = ((m_par.mfx.FrameInfo.Height + 15) & ~15);
    mfxU32 numMB = widthMB / 16 * (((heightMB / 16) + 1) / 2);
    mfxU32 numField = 2;

    //set parameters for mfxEncodeCtrl
    SETPARS(m_pCtrl, MFX_FRM_CTRL);
    m_pCtrl->QP = 26; //default qp value.

    // Attach input structures for mfxEncodeCtrl
    std::vector<mfxExtBuffer*> in_buffs;

    mfxExtFeiEncFrameCtrl *feiEncCtrl = NULL;
    mfxU32 fieldId = 0;
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
        feiEncCtrl[fieldId].SearchWindow = 5;

        // set up parameters for mfxExtFeiEncFrameCtrl
        SETPARS(&feiEncCtrl[fieldId], EXT_FRM_CTRL);

        // put the buffer in in_buffs
        in_buffs.push_back((mfxExtBuffer *)&feiEncCtrl[fieldId]);
    }

    //mbqp to be tested, enable per-mb qp in mfxExtFeiEncFrameCtrl
    for (fieldId = 0; fieldId < numField; fieldId++) {
        feiEncCtrl[fieldId].PerMBQp = 1;
    }

    //assign ExtFeiEncQP
    mfxExtFeiEncQP *feiEncMbQp = NULL;
    feiEncMbQp = new mfxExtFeiEncQP[numField];
    memset(feiEncMbQp, 0, numField * sizeof(mfxExtFeiEncQP));
    for (fieldId = 0; fieldId < numField; fieldId++) {
        feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
        feiEncMbQp[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncQP);
        feiEncMbQp[fieldId].NumQPAlloc = numMB;
        feiEncMbQp[fieldId].QP = new mfxU8[numMB];

        //put the buffer in in_buffs
        in_buffs.push_back((mfxExtBuffer*)&feiEncMbQp[fieldId]);
    }

    m_pCtrl->ExtParam = &in_buffs[0];
    m_pCtrl->NumExtParam = (mfxU16)in_buffs.size();

    mfxU32 nf = 30;
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    MFXInit();
    if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        SetFrameAllocator(m_session, m_pVAHandle);
        m_pFrameAllocator = m_pVAHandle;
    }

    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * nf);
    EncodeFrames(nf);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_field_coding);
};
