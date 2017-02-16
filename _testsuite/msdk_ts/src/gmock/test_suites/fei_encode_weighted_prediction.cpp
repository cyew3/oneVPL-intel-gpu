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

#include <random>
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"

/*
 * [MDP-33148] check the weighted prediction setting in PPS/SliceHeader for FEI ENCODE.
 * only support P-frame checking currently.
 */

namespace fei_encode_weighted_prediction
{

#define FRAME_TO_ENCODE 70

typedef std::vector<mfxExtBuffer*> InBuf;

mfxU8 GetFrameType(
    mfxVideoParam const & video,
    mfxU32                frameOrder,
    bool                  isBPyramid)
{
    mfxU32 gopOptFlag = video.mfx.GopOptFlag;
    mfxU32 gopPicSize = video.mfx.GopPicSize;
    mfxU32 gopRefDist = video.mfx.GopRefDist;
    mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval + 1);

    if (gopPicSize == 0xffff) //infinite GOP
        idrPicDist = gopPicSize = 0xffffffff;

    if (idrPicDist && frameOrder % idrPicDist == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (!idrPicDist && frameOrder == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (frameOrder % gopPicSize == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

    if (frameOrder % gopPicSize % gopRefDist == 0)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

    if ( (gopOptFlag & MFX_GOP_STRICT) == 0 && (frameOrder + 1) % gopPicSize == 0)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    if (isBPyramid && (frameOrder % gopPicSize % gopRefDist - 1) % 2 == 1)
        return (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF);

    return (MFX_FRAMETYPE_B);
}

class TestSuite : public tsVideoEncoder,
                  public tsSurfaceProcessor,
                  public tsBitstreamProcessor,
                  public tsParserH264AU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true)
        , m_frameCnt(0)
        , m_frameType(0)
        , m_hasPWT(0)
    {
        m_filler = this;
        m_bs_processor = this;
        m_max = FRAME_TO_ENCODE;
    }

    ~TestSuite()
    {
        mfxU32 numFields = 1;
        if ((m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
            (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)) {
            numFields = 2;
        }

        for (std::vector<InBuf>::iterator it = m_InBufs.begin(); it != m_InBufs.end(); ++it) {
            for (std::vector<mfxExtBuffer*>::iterator it_buf = (*it).begin(); it_buf != (*it).end();) {
                switch ((*it_buf)->BufferId) {
                case MFX_EXTBUFF_FEI_ENC_CTRL:
                    {
                        mfxExtFeiEncFrameCtrl* feiFrmCtl = (mfxExtFeiEncFrameCtrl*)(*it_buf);
                        delete[] feiFrmCtl;
                        feiFrmCtl = NULL;
                        it_buf += numFields;
                    }
                    break;
                case MFX_EXTBUFF_PRED_WEIGHT_TABLE:
                    {
                        mfxExtPredWeightTable* feiEncPWT = (mfxExtPredWeightTable*)(*it_buf);
                        delete[] feiEncPWT;
                        feiEncPWT = NULL;
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

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        InBuf in_buffs;
        mfxU32 numField = 1;
        mfxExtPredWeightTable *feiEncPWT = NULL;
        mfxExtFeiEncFrameCtrl *feiFrmCtl = NULL;
        const mfxU32 iWeight = 0, iOffset = 1, iY = 0, iCb = 1, iCr = 2;
        //mfxU16 hasBframe = (m_par.mfx.GopRefDist == 1) ? 0 : 1;

        s.Data.TimeStamp = m_frameCnt;
        m_frameType = GetFrameType(m_par, s.Data.TimeStamp, 0);

        if ((m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
            (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)) {
            numField = 2;
        }

        for (mfxU32 fieldId = 0; fieldId < numField; fieldId++) {
            // assign mfxExtFeiEncFrameCtrl
            if (fieldId == 0) {
                feiFrmCtl = new mfxExtFeiEncFrameCtrl[numField];
                memset(feiFrmCtl, 0, numField * sizeof(mfxExtFeiEncFrameCtrl));
            }
            feiFrmCtl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
            feiFrmCtl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
            feiFrmCtl[fieldId].SearchPath = 0;
            feiFrmCtl[fieldId].LenSP = 57;
            feiFrmCtl[fieldId].SubMBPartMask = 0;
            feiFrmCtl[fieldId].MultiPredL0 = 0;
            feiFrmCtl[fieldId].MultiPredL1 = 0;
            feiFrmCtl[fieldId].SubPelMode = 3;
            feiFrmCtl[fieldId].InterSAD = 2;
            feiFrmCtl[fieldId].IntraSAD = 2;
            feiFrmCtl[fieldId].DistortionType = 0;
            feiFrmCtl[fieldId].RepartitionCheckEnable = 0;
            feiFrmCtl[fieldId].AdaptiveSearch = 0;
            feiFrmCtl[fieldId].MVPredictor = 0;
            feiFrmCtl[fieldId].NumMVPredictors[0] = 4;
            feiFrmCtl[fieldId].NumMVPredictors[1] = 4;
            feiFrmCtl[fieldId].PerMBQp = 0;
            feiFrmCtl[fieldId].PerMBInput = 0;
            feiFrmCtl[fieldId].MBSizeCtrl = 0;
            feiFrmCtl[fieldId].RefHeight = 32;
            feiFrmCtl[fieldId].RefWidth = 32;
            feiFrmCtl[fieldId].SearchWindow = 5;

            in_buffs.push_back((mfxExtBuffer*)&feiFrmCtl[fieldId]);
        }

        if(m_hasPWT) {
            for (mfxU32 fieldId = 0; fieldId < numField; fieldId++) {
                if ((m_frameType & MFX_FRAMETYPE_P) || ((m_frameType & MFX_FRAMETYPE_IDR) && numField == 2)) {
                    //assign PredWeightTable
                    if (fieldId == 0) {
                        feiEncPWT = new mfxExtPredWeightTable[numField];
                        memset(feiEncPWT, 0, numField * sizeof(mfxExtPredWeightTable));
                    }
                    feiEncPWT[fieldId].Header.BufferId = MFX_EXTBUFF_PRED_WEIGHT_TABLE;
                    feiEncPWT[fieldId].Header.BufferSz = sizeof(mfxExtPredWeightTable);

                    feiEncPWT[fieldId].LumaLog2WeightDenom = GetRandomNumber(0, 7);
                    feiEncPWT[fieldId].ChromaLog2WeightDenom = GetRandomNumber(0, 7);

                    for (mfxU32 lx = 0; lx <= 0 /*hasBframe*/; lx++) {
                        for (mfxU32 ref = 0; ref < m_par.mfx.NumRefFrame; ref++) {
                            feiEncPWT[fieldId].LumaWeightFlag[lx][ref] = GetRandomNumber(0, 1);
                            if (feiEncPWT[fieldId].LumaWeightFlag[lx][ref] == 1) {
                                feiEncPWT[fieldId].Weights[lx][ref][iY][iWeight] = GetRandomNumber(-128, 127);
                                feiEncPWT[fieldId].Weights[lx][ref][iY][iOffset] = GetRandomNumber(-128, 127);
                            }
                        }
                    }

                    for (mfxU32 lx = 0; lx <= 0 /*hasBframe*/; lx++) {
                        for (mfxU32 ref = 0; ref < m_par.mfx.NumRefFrame; ref++) {
                            feiEncPWT[fieldId].ChromaWeightFlag[lx][ref] = GetRandomNumber(0, 1);
                            if (feiEncPWT[fieldId].ChromaWeightFlag[lx][ref] == 1) {
                                feiEncPWT[fieldId].Weights[lx][ref][iCb][iWeight] = GetRandomNumber(-128, 127);
                                feiEncPWT[fieldId].Weights[lx][ref][iCb][iOffset] = GetRandomNumber(-128, 127);
                                feiEncPWT[fieldId].Weights[lx][ref][iCr][iWeight] = GetRandomNumber(-128, 127);
                                feiEncPWT[fieldId].Weights[lx][ref][iCr][iOffset] = GetRandomNumber(-128, 127);
                            }
                        }
                    }

                    in_buffs.push_back((mfxExtBuffer*)&feiEncPWT[fieldId]);
                }
            }
        }

        m_InBufs.push_back(in_buffs);
        if(!in_buffs.empty()) {
            m_pCtrl->ExtParam = &(m_InBufs[m_frameCnt])[0];
            m_pCtrl->NumExtParam = (mfxU16)in_buffs.size();
        }

        m_frameCnt++;
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxExtCodingOption3& CO3 = m_par;
        mfxU32 fieldId = 0;
        mfxU32 numField = 1;

        if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) {
            nFrames *= 2;
            numField = 2;
        }
        SetBuffer(bs);

        while (nFrames--) {
            UnitType& hdr = ParseOrDie();

            for (UnitType::nalu_param* nalu = hdr.NALU; nalu < hdr.NALU + hdr.NumUnits; nalu ++) {
                if (nalu->nal_unit_type == 1) {
                    slice_header  *slice = nalu->slice_hdr;
                    pic_param_set *pps = slice->pps_active;
                    mfxU8 weighted_pred_flag = mfxU8(CO3.WeightedPred == MFX_WEIGHTED_PRED_EXPLICIT);

                    EXPECT_EQ(pps->weighted_pred_flag, weighted_pred_flag) << "--ERROR: wrong weighted_pred_flag in PPS.\n";

                    if (weighted_pred_flag && slice->slice_type % 5 == SLICE_P) {
                        if((int)slice->bottom_field_flag == 0)
                            fieldId = 0;
                        else
                            fieldId = 1 - fieldId;

                        if(m_hasPWT) {
                            //g_tsLog <<"check: the external PWT\n";
                            InBuf in_buffs = m_InBufs[bs.TimeStamp];
                            assert(in_buffs[numField + fieldId]->BufferId == MFX_EXTBUFF_PRED_WEIGHT_TABLE);

                            ComparePWT((mfxExtPredWeightTable *)(in_buffs[numField+fieldId]), slice);
                        } else {
                            //g_tsLog <<"check: the default PWT\n";
                            mfxExtPredWeightTable feiEncPWT;

                            memset(&feiEncPWT, 0, sizeof(mfxExtPredWeightTable));
                            feiEncPWT.LumaLog2WeightDenom = 6;
                            feiEncPWT.ChromaLog2WeightDenom = 0;

                            ComparePWT(&feiEncPWT, slice);
                        }
                    }
                }
            }
        }

        bs.DataOffset = bs.DataLength = 0;
        return MFX_ERR_NONE;
    }

    enum
    {
        SLICE_P  = 0,
        SLICE_B  = 1,
        SLICE_I  = 2,
    };

    enum
    {
        NO_PWT_BUFF    = 0,
        WITH_PWT_BUFF  = 1,
    };

    struct tc_struct
    {
        mfxU16 NumRefFrame;
        mfxU16 GopRefDist;
        mfxU16 PicStruct;
        mfxU16 WeightedPredMode;
        mfxU16 WeightedPredTable;
    };

    mfxI32 GetRandomNumber(mfxI32 min, mfxI32 max);
    void ComparePWT(mfxExtPredWeightTable *feiEncPWT, slice_header *slice);
    static const tc_struct test_case[];
    static const unsigned int n_cases;
    int RunTest(unsigned int id);

    mfxU32              m_frameCnt;
    mfxU32              m_frameType;
    mfxU16              m_hasPWT;
    std::vector<InBuf>  m_InBufs;
};

mfxI32 TestSuite::GetRandomNumber(mfxI32 min, mfxI32 max)
{
    mfxU16 rnd = 0;
    rnd = 1 + rand()%m_par.mfx.GopPicSize;

    std::mt19937 generator;
    generator.seed(m_frameCnt + rnd);

    std::uniform_int_distribution<mfxI32> distr(min, max);
    return distr(generator);
}

void TestSuite::ComparePWT(mfxExtPredWeightTable *feiEncPWT, slice_header *slice)
{
    ASSERT_NE(feiEncPWT, (void *)NULL);
    ASSERT_NE(slice, (void *)NULL);
    const mfxU32 iWeight = 0, iOffset = 1, iY = 0, iCb = 1, iCr = 2;

    //g_tsLog <<"check: compare the PWT\n";
    mfxU32 listSize[2] = {slice->num_ref_idx_l0_active_minus1 + 1, slice->num_ref_idx_l1_active_minus1 + 1};
    EXPECT_EQ(feiEncPWT->LumaLog2WeightDenom, slice->pred_weight_table->luma_log2_weight_denom);
    EXPECT_EQ(feiEncPWT->ChromaLog2WeightDenom, slice->pred_weight_table->chroma_log2_weight_denom);

    for (mfxU32 lx = 0; lx <= (slice->slice_type % 5 == SLICE_B); lx++) {
        for (mfxU32 ref = 0; ref < listSize[lx]; ref++) {
            EXPECT_EQ(feiEncPWT->LumaWeightFlag[lx][ref], (int)slice->pred_weight_table->listX[lx][ref].luma_weight_lx_flag);
            if (feiEncPWT->LumaWeightFlag[lx][ref] == 1) {
                EXPECT_EQ(feiEncPWT->Weights[lx][ref][iY][iWeight], slice->pred_weight_table->listX[lx][ref].luma_weight_lx);
                EXPECT_EQ(feiEncPWT->Weights[lx][ref][iY][iOffset], slice->pred_weight_table->listX[lx][ref].luma_offset_lx);
            }
        }
    }

    for (mfxU32 lx = 0; lx <= (slice->slice_type % 5 == SLICE_B); lx++) {
        for (mfxU32 ref = 0; ref < listSize[lx]; ref++) {
            EXPECT_EQ(feiEncPWT->ChromaWeightFlag[lx][ref], (int)slice->pred_weight_table->listX[lx][ref].chroma_weight_lx_flag);
            if (feiEncPWT->ChromaWeightFlag[lx][ref] == 1) {
                EXPECT_EQ(feiEncPWT->Weights[lx][ref][iCb][iWeight], slice->pred_weight_table->listX[lx][ref].chroma_weight_lx[0]);
                EXPECT_EQ(feiEncPWT->Weights[lx][ref][iCb][iOffset], slice->pred_weight_table->listX[lx][ref].chroma_offset_lx[0]);
                EXPECT_EQ(feiEncPWT->Weights[lx][ref][iCr][iWeight], slice->pred_weight_table->listX[lx][ref].chroma_weight_lx[1]);
                EXPECT_EQ(feiEncPWT->Weights[lx][ref][iCr][iOffset], slice->pred_weight_table->listX[lx][ref].chroma_offset_lx[1]);
            }
        }
    }
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*01*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,  NO_PWT_BUFF},
    /*02*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,  NO_PWT_BUFF},
    /*03*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,  NO_PWT_BUFF},

    /*04*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,  WITH_PWT_BUFF},
    /*05*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,  WITH_PWT_BUFF},
    /*06*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,  WITH_PWT_BUFF},

    /*07*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_UNKNOWN,  NO_PWT_BUFF},
    /*08*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_UNKNOWN,  NO_PWT_BUFF},
    /*09*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_UNKNOWN,  NO_PWT_BUFF},

    /*10*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_UNKNOWN,  WITH_PWT_BUFF},
    /*11*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_UNKNOWN,  WITH_PWT_BUFF},
    /*12*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_UNKNOWN,  WITH_PWT_BUFF},

    /*13*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*14*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*15*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},

    /*16*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*17*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*18*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},

    /*19*/ {4, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*20*/ {4, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*21*/ {4, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},

    /*22*/ {4, 4, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*23*/ {4, 4, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*24*/ {4, 4, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();
    mfxStatus sts;
    const tc_struct& tc = test_case[id];
    mfxExtCodingOption3& CO3 = m_par;

    m_par.AsyncDepth               = 1;
    m_par.mfx.GopPicSize           = 30;
    m_par.mfx.GopOptFlag           = MFX_GOP_STRICT;
    m_par.mfx.IdrInterval          = 0;
    m_par.mfx.RateControlMethod    = MFX_RATECONTROL_CQP;
    //assign the specified values
    m_par.mfx.NumRefFrame          = tc.NumRefFrame;
    m_par.mfx.GopRefDist           = tc.GopRefDist;
    m_par.mfx.FrameInfo.PicStruct  = tc.PicStruct;
    CO3.WeightedPred               = tc.WeightedPredMode;
    m_hasPWT                       = tc.WeightedPredTable;

    g_tsStatus.check(Init());
    GetVideoParam();

    EncodeFrames(FRAME_TO_ENCODE);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_weighted_prediction);
}
