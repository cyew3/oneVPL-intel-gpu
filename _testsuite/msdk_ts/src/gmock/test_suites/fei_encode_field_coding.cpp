/******************************************************************************* *\

Copyright (C) 2016 Intel Corporation.  All rights reserved.

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
#include "ts_parser.h"
#include "ts_struct.h"

/*
 * This test is for MQ-90, to implement the "check diff" test for single-field coding:
 * to encode each field with different parameters, and then check each field separately.
 * Parameters to set for different fields include: mfxExtFeiEncQP/mfxExtFeiEncMBCtrl.
 */

namespace fei_encode_field_coding
{
typedef struct{
    mfxU16      NumExtParam;
    mfxExtBuffer **ExtParam;
} InBuf;

enum
{
    SET_FORCE_INTRA   = 0,
    SET_FORCE_SKIP    = 1,
    SET_FORCE_NONSKIP = 2
};

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
            if ((m_mb->mb_type == 0 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 25)
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
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true)
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

        //to free buffers
        const int numFields = 2;
        for (std::vector<InBuf*>::iterator it = m_InBufs.begin(); it != m_InBufs.end(); ++it)
        {
            if ((*it)->ExtParam)
            {
                for (int i = 0; i < (*it)->NumExtParam;)
                {
                    switch ((*it)->ExtParam[i]->BufferId) {
                    case MFX_EXTBUFF_FEI_ENC_CTRL:
                        {
                            mfxExtFeiEncFrameCtrl* feiEncCtrl = (mfxExtFeiEncFrameCtrl*)((*it)->ExtParam[i]);
                            delete[] feiEncCtrl;
                            feiEncCtrl = NULL;
                            i += numFields;
                        }
                        break;
                    case MFX_EXTBUFF_FEI_ENC_QP:
                        {
                            mfxExtFeiEncQP* feiEncMbQp = (mfxExtFeiEncQP*)((*it)->ExtParam[i]);
                            for (int fieldId = 0; fieldId < numFields; fieldId++) {
                                delete[] feiEncMbQp[fieldId].QP;
                            }
                            delete[] feiEncMbQp;
                            feiEncMbQp = NULL;
                            i += numFields;
                        }
                        break;
                    case MFX_EXTBUFF_FEI_ENC_MB:
                        {
                            mfxExtFeiEncMBCtrl* feiEncMbCtl = (mfxExtFeiEncMBCtrl*)((*it)->ExtParam[i]);
                            for (int fieldId = 0; fieldId < numFields; fieldId++) {
                                delete[] feiEncMbCtl[fieldId].MB;
                            }
                            delete[] feiEncMbCtl;
                            feiEncMbCtl = NULL;
                            i += numFields;
                        }
                        break;
                    default:
                        g_tsLog << "ERROR: not supported ext buffer\n";
                        g_tsStatus.check(MFX_ERR_ABORTED);
                        break;
                    }
                }
                delete[] ((*it)->ExtParam);
                (*it)->ExtParam = NULL;
            }
            delete (*it);
        }
        m_InBufs.clear();
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        InBuf *in_buffs = new InBuf;
        memset(in_buffs, 0, sizeof(InBuf));
        unsigned int numExtBuf = 0;

        //calculat numMB
        mfxU32 widthMB  = ((m_par.mfx.FrameInfo.Width  + 15) & ~15);
        mfxU32 heightMB = ((m_par.mfx.FrameInfo.Height + 15) & ~15);
        mfxU32 numMB = widthMB / 16 * (((heightMB / 16) + 1) / 2);
        //no progressive cases in this test
        mfxU32 numField = 2;

        mfxExtFeiEncFrameCtrl *feiEncCtrl = NULL;
        mfxExtFeiEncQP *feiEncMbQp = NULL;
        mfxExtFeiEncMBCtrl *feiEncMbCtl = NULL;
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
            feiEncCtrl[fieldId].NumMVPredictors[0] = 1;
            feiEncCtrl[fieldId].PerMBQp = 0; //non-zero value
            feiEncCtrl[fieldId].PerMBInput = 0;
            feiEncCtrl[fieldId].MBSizeCtrl = 0;
            feiEncCtrl[fieldId].RefHeight = 32;
            feiEncCtrl[fieldId].RefWidth = 32;
            feiEncCtrl[fieldId].SearchWindow = 5;

            numExtBuf++;
        }

        if (m_mode & CMP_MBQP) {
            for (fieldId = 0; fieldId < numField; fieldId++) {
                //if mbqp to be tested, enable per-mb qp in mfxExtFeiEncFrameCtrl
                feiEncCtrl[fieldId].PerMBQp = 1;
                //assign ExtFeiEncQP
                if (fieldId == 0) {
                    feiEncMbQp = new mfxExtFeiEncQP[numField];
                    memset(feiEncMbQp, 0, numField * sizeof(mfxExtFeiEncQP));
                }
                feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_QP;
                feiEncMbQp[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncQP);
                feiEncMbQp[fieldId].NumQPAlloc = numMB;
                feiEncMbQp[fieldId].QP = new mfxU8[numMB];

                //generate random qp value and assign
                m_qp[m_fo * 2 + fieldId] = 1 + rand() % 50;
                memset(feiEncMbQp[fieldId].QP, m_qp[m_fo * 2 + fieldId],
                       feiEncMbQp[fieldId].NumQPAlloc * sizeof(feiEncMbQp[fieldId].QP[0]));

                numExtBuf++;
            }
        }

        if (m_mode & CMP_MBCTL) {
            for (fieldId = 0; fieldId < numField; fieldId++) {
                feiEncCtrl[fieldId].PerMBInput = 1;
                //assign mfxExtFeiEncMBCtrl
                if (fieldId == 0) {
                    feiEncMbCtl = new mfxExtFeiEncMBCtrl[numField];
                    memset(feiEncMbCtl, 0, numField * sizeof(mfxExtFeiEncMBCtrl));
                }
                feiEncMbCtl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
                feiEncMbCtl[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncMBCtrl);
                feiEncMbCtl[fieldId].NumMBAlloc = numMB;
                feiEncMbCtl[fieldId].MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[numMB];
                memset(feiEncMbCtl[fieldId].MB, 0, numMB * sizeof(mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB));

                //generate random value for mbctrl
                unsigned int index = m_fo * 2 + fieldId;
                //According to Sergey, only one of the three parameters can be set at one time.
                m_ctl[index] = (mfxU8)(rand() % 3);//possible values: 0, 1, 2;
                printf("m_ctl[%d] = %d\n", index, m_ctl[index]);
                for (mfxU32 i = 0; i < numMB; i++) {
                    switch (m_ctl[index]) {
                    case SET_FORCE_INTRA:
                        feiEncMbCtl[fieldId].MB[i].ForceToIntra    = 1;
                        break;
                    case SET_FORCE_SKIP:
                        feiEncMbCtl[fieldId].MB[i].ForceToSkip     = 1;
                        break;
                    case SET_FORCE_NONSKIP:
                        feiEncMbCtl[fieldId].MB[i].ForceToNoneSkip = 1;
                        break;
                    default:
                        break;
                    }
                }

                numExtBuf++;
            }
        }

        in_buffs->ExtParam = new mfxExtBuffer*[numExtBuf];
        for (fieldId = 0; fieldId < numField; fieldId++) {
            in_buffs->ExtParam[in_buffs->NumExtParam++] = (mfxExtBuffer *)(feiEncCtrl + fieldId);
        }

        if (m_mode & CMP_MBQP) {
            for (fieldId = 0; fieldId < numField; fieldId++) {
                in_buffs->ExtParam[in_buffs->NumExtParam++] = (mfxExtBuffer *)(feiEncMbQp + fieldId);
            }
        }

        if (m_mode & CMP_MBCTL) {
            for (fieldId = 0; fieldId < numField; fieldId++) {
                in_buffs->ExtParam[in_buffs->NumExtParam++] = (mfxExtBuffer *)(feiEncMbCtl + fieldId);
            }
        }

        //assign the ext buffers to m_pCtrl
        m_pCtrl->ExtParam = in_buffs->ExtParam;
        m_pCtrl->NumExtParam = in_buffs->NumExtParam;

        //save in_buffs into m_InBufs
        m_InBufs.push_back(in_buffs);

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;
        return sts;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
#if 1
        mfxU32 checked = 0;

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        mfxU32 wMB = ((m_par.mfx.FrameInfo.Width + 15) / 16);
        //no progressive case in this test
        mfxU32 hMB = ((m_par.mfx.FrameInfo.Height / 2 + 15) / 16);
        mfxU32 numMB = wMB * hMB;
        assert(nFrames == 1);
        while (checked < nFrames)
        {
            UnitType& hdr = ParseOrDie();
            if (m_mode & CMP_MBQP) {
                AUWrap au(hdr);
                mfxU8 qp = au.NextQP();
                mfxU32 i = 0;
                bool bff = (bool)(m_par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF);
                //to calculate index to m_qp
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
            }
            if (m_mode & CMP_MBCTL) {
                AUWrap au(hdr);
                bool bff = (bool)(m_par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF);
                //to calculate index to m_qp
                slice_header *sh = au.NextSlice();
                macro_block *mb = NULL;
                mfxU32 nCountIntra = 0;
                mfxU32 nCountSkip = 0;
                bool b_IField = false;
                mfxU32 index = 2 * bs.TimeStamp + (mfxU32)(bff ^ au.IsBottomField());
                mfxU8 ex_ctl = m_ctl[index];
                while (sh) {
                    switch (sh->slice_type % 5) {
                    case 0: //p slice
                        while ((mb = au.NextMB())) {
                            if (mb->mb_type > 4) nCountIntra++;
                            if (mb->mb_skip_flag) nCountSkip++;
                        }
                        break;
                    case 1: //b slice
                        while ((mb = au.NextMB())) {
                            if (mb->mb_type > 22) nCountIntra++;
                            if (mb->mb_skip_flag) nCountSkip++;
                        }
                        break;
                    case 2: //i slice
                        b_IField = true;
                        break;
                    default:
                        g_tsLog << "SI/SP is not considered yet\n";
                        break;
                    }
                    sh = au.NextSlice();
                }
                if (!b_IField) {
                    switch (ex_ctl) {
                    case SET_FORCE_INTRA:
                        EXPECT_EQ(nCountIntra, numMB);
                        break;
                    case SET_FORCE_SKIP:
                        EXPECT_EQ(nCountSkip, numMB);
                        break;
                    case SET_FORCE_NONSKIP:
                        EXPECT_EQ(nCountSkip, (mfxU32)0);
                        break;
                    }
                }
            }
            checked++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
#else
        return m_writer.ProcessBitstream(bs, nFrames);
#endif
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

    //define different ext parameters to set for fields
    enum
    {
        CMP_MBQP               = 1,
        CMP_MBCTL              = 1 << 1,
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
    /*
     * m_ctl contains mbctrl for every "field". Only three possible values:
     * SET_FORCE_INTRA     - force MB to intra.
     * SET_FORCE_SKIP      - force MB to skip
     * SET_FORCE_NONESKIP  - force MB to non-skip
     */
    std::map<mfxU32, mfxU8> m_ctl;
    mfxU32 m_fo;
    mfxU32 m_mode;
    tsBitstreamWriter m_writer;
    std::vector<InBuf*> m_InBufs;
};


#define mfx_PicStruct tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist tsStruct::mfxVideoParam.mfx.GopRefDist
const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ MFX_ERR_NONE, CMP_MBQP, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                     {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*02*/ MFX_ERR_NONE, CMP_MBQP, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                     {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*03*/ MFX_ERR_NONE, CMP_MBQP, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                     {MFX_PAR, &mfx_GopPicSize, 30},
                                     {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*04*/ MFX_ERR_NONE, CMP_MBQP, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                     {MFX_PAR, &mfx_GopPicSize, 30},
                                     {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*05*/ MFX_ERR_NONE, CMP_MBCTL, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                      {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*06*/ MFX_ERR_NONE, CMP_MBCTL, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                      {MFX_PAR, &mfx_GopRefDist, 1}}},
    {/*07*/ MFX_ERR_NONE, CMP_MBCTL, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                      {MFX_PAR, &mfx_GopPicSize, 30},
                                      {MFX_PAR, &mfx_GopRefDist, 3}}},
    {/*08*/ MFX_ERR_NONE, CMP_MBCTL, {{MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                      {MFX_PAR, &mfx_GopPicSize, 30},
                                      {MFX_PAR, &mfx_GopRefDist, 3}}},
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

    //enable single field coding
    mfxExtFeiParam& extFeiPar = m_par;
    extFeiPar.SingleFieldProcessing = MFX_CODINGOPTION_ON;


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
