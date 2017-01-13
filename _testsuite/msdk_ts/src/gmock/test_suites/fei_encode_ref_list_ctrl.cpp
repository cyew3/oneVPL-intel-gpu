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
#include "ts_parser.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"

/*
 * [MDP-31409] test for mfxExtAVCRefListCtrl::NumRefIdxL0Active/NumRefIdxL1Active,
 *     set them per-frame and then check the num in encoded stream.
 */

namespace fei_encode_ref_list_ctrl
{

typedef struct{
    mfxU16      NumExtParam;
    mfxExtBuffer **ExtParam;
} InBuf;

mfxStatus resetAVCRefListCtrl(mfxExtAVCRefListCtrl* const refListCtl)
{
    memset(refListCtl, 0, sizeof(mfxExtAVCRefListCtrl));

    int i = 0;
    for (i = 0; i < 32; i++) {
        refListCtl->PreferredRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
    }
    for (i = 0; i < 16; i++) {
        refListCtl->RejectedRefList[i].FrameOrder  = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        refListCtl->LongTermRefList[i].FrameOrder  = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

class TestSuite : public tsVideoEncoder,
                  public tsSurfaceProcessor,
                  public tsBitstreamProcessor,
                  public tsParserH264AU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true)
        , m_order(0)
        //, m_writer("/tmp/debug.264")
    {
        srand(0);
        m_filler = this;
        m_bs_processor = this;
    }

    ~TestSuite()
    {
        //to free ext buffers
        mfxU32 numFields = (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) ? 2 : 1 ;
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
                    case MFX_EXTBUFF_AVC_REFLIST_CTRL:
                        {
                            mfxExtAVCRefListCtrl* refListCtl = (mfxExtAVCRefListCtrl*)((*it)->ExtParam[i]);
                            delete refListCtl;
                            refListCtl = NULL;
                            i++;
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
        mfxU32 numField = (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) ? 2 : 1 ;

        //specify the max num of reference frames, should be less or equal to the NumRefFrame
        //parameter from encoding initialization.
        m_ctl[m_order].NumRefIdxL0Active = rand() % (m_NumRef * numField) + 1;
        m_ctl[m_order].NumRefIdxL1Active = rand() % (m_NumRef * numField) + 1;

        InBuf *in_buffs = new InBuf;
        memset(in_buffs, 0, sizeof(InBuf));
        unsigned int numExtBuf = 0;

        mfxExtFeiEncFrameCtrl *feiEncCtrl = NULL;
        mfxExtAVCRefListCtrl  *refListCtl = NULL;
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
            feiEncCtrl[fieldId].NumMVPredictors[1] = 1;
            feiEncCtrl[fieldId].PerMBQp = 0;
            feiEncCtrl[fieldId].PerMBInput = 0;
            feiEncCtrl[fieldId].MBSizeCtrl = 0;
            feiEncCtrl[fieldId].RefHeight = 32;
            feiEncCtrl[fieldId].RefWidth = 32;
            feiEncCtrl[fieldId].SearchWindow = 5;

            numExtBuf++;
        }

        //assign mfxExtAVCRefListCtrl
        refListCtl = new mfxExtAVCRefListCtrl;
        resetAVCRefListCtrl(refListCtl);
        refListCtl->Header.BufferId = MFX_EXTBUFF_AVC_REFLIST_CTRL;
        refListCtl->Header.BufferSz = sizeof(mfxExtAVCRefListCtrl);
        refListCtl->NumRefIdxL0Active = m_ctl[m_order].NumRefIdxL0Active;
        refListCtl->NumRefIdxL1Active = m_ctl[m_order].NumRefIdxL1Active;

        in_buffs->ExtParam = new mfxExtBuffer*[numExtBuf];
        for (fieldId = 0; fieldId < numField; fieldId++) {
            in_buffs->ExtParam[in_buffs->NumExtParam++] = (mfxExtBuffer *)(feiEncCtrl + fieldId);
        }
        in_buffs->ExtParam[in_buffs->NumExtParam++] = (mfxExtBuffer *)refListCtl;

        //assign the ext buffers to m_pCtrl
        m_pCtrl->ExtParam = in_buffs->ExtParam;
        m_pCtrl->NumExtParam = in_buffs->NumExtParam;

        //save in_buffs into m_InBufs
        m_InBufs.push_back(in_buffs);

        //store frame order
        s.Data.TimeStamp = s.Data.FrameOrder = m_order++;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
            nFrames *= 2;
        SetBuffer(bs);

        while (nFrames--) {
            H264AUWrapper au(ParseOrDie());
            slice_header *sh;

            while ((sh = au.NextSlice())) {
                if (sh->first_mb_in_slice)
                    continue;

                if (sh->slice_type % 5 == 0) {
                    //P slice
                    EXPECT_GE(m_ctl[bs.TimeStamp].NumRefIdxL0Active, sh->num_ref_idx_l0_active_minus1);
                } else if (sh->slice_type % 5 == 1) {
                    //B slice
                    EXPECT_GE(m_ctl[bs.TimeStamp].NumRefIdxL0Active, sh->num_ref_idx_l0_active_minus1);
                    EXPECT_GE(m_ctl[bs.TimeStamp].NumRefIdxL1Active, sh->num_ref_idx_l1_active_minus1);
                }
            }
        }

        bs.DataOffset =  bs.DataLength = 0;
        return MFX_ERR_NONE;
    }

    struct ctrl{
        mfxU16 NumRefIdxL0Active;
        mfxU16 NumRefIdxL1Active;
    };

    struct tc_struct
    {
        mfxU16 NumRefFrame;
        mfxU16 GopRefDist;
        mfxU16 IdrInterval;
        mfxU16 PicStruct;
        mfxU16 PRefType;
        mfxU16 BRefType;
    };

    static const tc_struct test_case[];

    mfxU16 m_NumRef;
    std::map<mfxU32, ctrl> m_ctl; //keep the info for every frame
    mfxU32 m_order;
    //tsBitstreamWriter m_writer;
    std::vector<InBuf*> m_InBufs;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*01*/ {6, 5, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*02*/ {6, 4, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*03*/ {6, 3, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*04*/ {6, 2, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*05*/ {6, 1, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*06*/ {0, 5, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*07*/ {0, 4, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*08*/ {0, 3, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*09*/ {0, 2, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*10*/ {0, 1, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*11*/ {6, 5, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_OFF},
    /*12*/ {6, 4, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*13*/ {6, 3, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*14*/ {6, 2, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*15*/ {6, 1, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*16*/ {0, 5, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*17*/ {0, 4, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*18*/ {0, 3, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*19*/ {0, 2, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*20*/ {0, 1, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*21*/ {6, 5, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*22*/ {6, 4, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*23*/ {6, 3, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*24*/ {6, 2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*25*/ {6, 1, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];

    m_par.AsyncDepth = 1;
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;

    //CodingOption3/2
    mfxExtCodingOption3& CO3 = m_par;
    mfxExtCodingOption2& CO2 = m_par;

    m_par.mfx.GopPicSize = 30;

    //assign the specified values
    m_par.mfx.FrameInfo.PicStruct = tc.PicStruct;
    m_par.mfx.NumRefFrame         = tc.NumRefFrame;
    m_par.mfx.GopRefDist          = tc.GopRefDist;
    m_par.mfx.IdrInterval         = tc.IdrInterval;
    CO3.PRefType                  = tc.PRefType;
    CO2.BRefType                  = tc.BRefType;

    Init();
    GetVideoParam();
    m_NumRef = m_par.mfx.NumRefFrame;

    EncodeFrames(70);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_ref_list_ctrl);
}
