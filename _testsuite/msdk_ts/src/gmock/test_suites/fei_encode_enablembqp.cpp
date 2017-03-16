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

/*
 * This test is for mfxExtCodingOption3: EnableMBQP and mfxExtMBQP when do FEI
 * ENCODE. FEI doesn't support mfxExtMBQP, so this is negative test.
 *
 */

namespace fei_encode_enablembqp
{

typedef std::vector<mfxExtBuffer*> InBuf;

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true)
        , m_reader()
        , m_fo(0)
        , m_bEnableMbQP(false)
        , mode(0)
    {
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
        g_tsStatus.expect(MFX_ERR_NONE);

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
                case MFX_EXTBUFF_FEI_ENC_QP:
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
                case MFX_EXTBUFF_MBQP:
                    {
                        mfxExtMBQP* EncMbQp = (mfxExtMBQP*)(*it_buf);

                        for (int fieldId = 0; fieldId < numFields; fieldId++) {
                            delete[] EncMbQp[fieldId].QP;
                        }
                        delete[] EncMbQp;
                        EncMbQp = NULL;
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
    typedef std::vector<mfxExtBuffer*> InBuf;

    static const unsigned int n_cases;

private:
    enum
    {
        MFX_PAR = 1,
        EXT_FRM_CTRL,
        MFX_FRM_CTRL,
        CO3PAR
    };

    enum
    {
        INIT    = 1,
        RUNTIME
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
    bool m_bEnableMbQP;
    mfxU32 mode;
    std::vector<InBuf> m_InBufs;
};

#define mfx_IOPattern tsStruct::mfxVideoParam.IOPattern

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, INIT,
            {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
            {CO3PAR, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_OFF},
            {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*01*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
            {CO3PAR, &tsStruct::mfxExtCodingOption3.EnableMBQP, MFX_CODINGOPTION_ON},
            {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*02*/ MFX_ERR_NONE, RUNTIME,
            {{MFX_FRM_CTRL, &tsStruct::mfxEncodeCtrl.QP, 42},
            {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}}},
};

const unsigned int TestSuite::n_cases =
                   sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    MFXInit();

    const tc_struct& tc = test_case[id];
    mode = tc.mode;

    //set parameters for mfxVideoParam
    SETPARS(m_pPar, MFX_PAR);
    m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)
    m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP; //For now FEI work with CQP only

    //set parameters for mfxEncodeCtrl
    SETPARS(m_pCtrl, MFX_FRM_CTRL);

    //set parameters for mfxExtCodingOption3
    if (mode & INIT) {
        mfxExtCodingOption3 & co3 = m_par;
        SETPARS(&co3, CO3PAR);
    }

    if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        SetFrameAllocator(m_session, m_pVAHandle);
    }

    mfxU32 nf = frameNumber;

    Query(m_session, m_pPar, m_pParOut);

    if (mode & INIT) {
        g_tsStatus.expect(tc.sts);
    }

    Init(m_session, m_pPar);

    if (mode & RUNTIME) {
        AllocSurfaces();
        AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * nf);

        SetFrameAllocator();

        /* attach buffers begin */
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
        mfxExtMBQP *EncMbQp = NULL;
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
            feiEncCtrl[fieldId].NumMVPredictors[0] = 0;
            feiEncCtrl[fieldId].PerMBQp = 0;
            feiEncCtrl[fieldId].PerMBInput = 0;
            feiEncCtrl[fieldId].MBSizeCtrl = 0;
            feiEncCtrl[fieldId].RefHeight = 32;
            feiEncCtrl[fieldId].RefWidth = 32;
            feiEncCtrl[fieldId].SearchWindow = 5;

            // put the buffer in in_buffs
            in_buffs.push_back((mfxExtBuffer *)&feiEncCtrl[fieldId]);
        }

        //save in_buffs into m_InBufs
        m_InBufs.push_back(in_buffs);

        m_pCtrl->QP = m_qp[m_fo];
        in_buffs = m_InBufs[m_fo];

        std::vector<mfxExtBuffer*>::iterator it;
        for (it = in_buffs.begin(); it != in_buffs.end(); ++it) {
            if ((*it)->BufferId == MFX_EXTBUFF_FEI_ENC_CTRL) {
                mfxExtFeiEncFrameCtrl *feiEncCtrl = (mfxExtFeiEncFrameCtrl *)(*it);
                //if mbqp to be tested, enable per-mb qp in mfxExtFeiEncFrameCtrl
                feiEncCtrl->PerMBQp = 1;
            }
        }

        for (fieldId = 0; fieldId < numField; fieldId++) {
            // assign ExtMBQP
            if (fieldId == 0) {
                EncMbQp = new mfxExtMBQP[numField];
                memset(EncMbQp, 0, numField * sizeof(mfxExtMBQP));
            }

            EncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_MBQP;
            EncMbQp[fieldId].Header.BufferSz = sizeof(mfxExtMBQP);
            EncMbQp[fieldId].NumQPAlloc = numMB;
            EncMbQp[fieldId].QP = new mfxU8[numMB];

            memset(EncMbQp[fieldId].QP,
                   m_qp[m_fo],
                   EncMbQp[fieldId].NumQPAlloc*sizeof(EncMbQp[fieldId].QP[0]));

            //put the buffer in in_buffs
            in_buffs.push_back((mfxExtBuffer*)&EncMbQp[fieldId]);
        }

        //save the updated in_buffs into m_InBufs
        m_InBufs[m_fo] = in_buffs;

        if (!in_buffs.empty()) {
            m_ctrl.ExtParam = &(m_InBufs[m_fo])[0];
            m_ctrl.NumExtParam = (mfxU16)in_buffs.size();
        }
        /* attach buffers end   */

        g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        mfxStatus st;

        m_pSurf = GetSurface();
        st = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);
        while (st == MFX_ERR_MORE_DATA)
        {
            m_pSurf = GetSurface();
            st = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);
        }

        g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        // skip sync if encoding wasn't started
        if (m_pSyncPoint && *m_pSyncPoint)
        {
            SyncOperation();
        }
    }

    TS_END;

    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_enablembqp);

};
