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

/* Check the ext buffer of weighted prediction table and related paramter settings in
 * pps/slice header for AVC FEI ENCODE.
 * [MDP-33148] Added support for P-frame.
 * [MDP-36509] Added support for B-frame
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
                if ((m_frameType & MFX_FRAMETYPE_P) || (m_frameType & MFX_FRAMETYPE_B) || ((m_frameType & MFX_FRAMETYPE_IDR) && numField == 2)) {
                    //assign PredWeightTable
                    if (fieldId == 0) {
                        feiEncPWT = new mfxExtPredWeightTable[numField];
                        memset(feiEncPWT, 0, numField * sizeof(mfxExtPredWeightTable));
                    }
                    feiEncPWT[fieldId].Header.BufferId = MFX_EXTBUFF_PRED_WEIGHT_TABLE;
                    feiEncPWT[fieldId].Header.BufferSz = sizeof(mfxExtPredWeightTable);

                    feiEncPWT[fieldId].LumaLog2WeightDenom = GetRandomNumber(0, 7);
                    feiEncPWT[fieldId].ChromaLog2WeightDenom = GetRandomNumber(0, 7);

                    mfxU16 hasBframe = !!(m_frameType & MFX_FRAMETYPE_B);
                    for (mfxU32 lx = 0; lx <= hasBframe; lx++) {
                        for (mfxU32 ref = 0; ref < 32; ref++) {
                            if (ref < m_par.mfx.NumRefFrame) {
                                feiEncPWT[fieldId].LumaWeightFlag[lx][ref] = GetRandomNumber(0, 1);
                                if (feiEncPWT[fieldId].LumaWeightFlag[lx][ref] == 1) {
                                    feiEncPWT[fieldId].Weights[lx][ref][iY][iWeight] = GetRandomNumber(-128, 127);
                                    feiEncPWT[fieldId].Weights[lx][ref][iY][iOffset] = GetRandomNumber(-128, 127);

                                }
                            } else {
                                feiEncPWT[fieldId].LumaWeightFlag[lx][ref] = 0;
                                feiEncPWT[fieldId].Weights[lx][ref][iY][iWeight] = 0;
                                feiEncPWT[fieldId].Weights[lx][ref][iY][iOffset] = 0;
                            }
                        }
                    }

                    for (mfxU32 lx = 0; lx <= hasBframe; lx++) {
                        for (mfxU32 ref = 0; ref < 32; ref++) {
                            if (ref < m_par.mfx.NumRefFrame) {
                                feiEncPWT[fieldId].ChromaWeightFlag[lx][ref] = GetRandomNumber(0, 1);
                                if (feiEncPWT[fieldId].ChromaWeightFlag[lx][ref] == 1) {
                                    feiEncPWT[fieldId].Weights[lx][ref][iCb][iWeight] = GetRandomNumber(-128, 127);
                                    feiEncPWT[fieldId].Weights[lx][ref][iCb][iOffset] = GetRandomNumber(-128, 127);
                                    feiEncPWT[fieldId].Weights[lx][ref][iCr][iWeight] = GetRandomNumber(-128, 127);
                                    feiEncPWT[fieldId].Weights[lx][ref][iCr][iOffset] = GetRandomNumber(-128, 127);
                                }
                            } else {
                                feiEncPWT[fieldId].ChromaWeightFlag[lx][ref] = 0;
                                feiEncPWT[fieldId].Weights[lx][ref][iCb][iWeight] = 0;
                                feiEncPWT[fieldId].Weights[lx][ref][iCb][iOffset] = 0;
                                feiEncPWT[fieldId].Weights[lx][ref][iCr][iWeight] = 0;
                                feiEncPWT[fieldId].Weights[lx][ref][iCr][iOffset] = 0;
                            }
                        }
                    }

                    if (m_frameType & MFX_FRAMETYPE_B) {
                        for (mfxU32 l0 = 0; l0 < 32; l0++) {
                            for (mfxU32 l1 = 0; l1 < 32; l1++) {
                                //CHECK Y
                                if (feiEncPWT[fieldId].LumaWeightFlag[0][l0] && feiEncPWT[fieldId].LumaWeightFlag[1][l1]) {
                                    mfxI16 weightYL0 = feiEncPWT[fieldId].Weights[0][l0][iY][iWeight];
                                    mfxI16 weightYL1 = feiEncPWT[fieldId].Weights[1][l1][iY][iWeight];
                                    if (weightYL0 + weightYL1 < -128) {
                                        feiEncPWT[fieldId].Weights[0][l0][iY][iWeight] = weightYL0 < -64 ? -64 : weightYL0;
                                        feiEncPWT[fieldId].Weights[1][l1][iY][iWeight] = weightYL1 < -64 ? -64 : weightYL1;
                                    }
                                    mfxU16 lwd = (feiEncPWT[fieldId].LumaLog2WeightDenom == 7) ? 127 : 128;
                                    if (weightYL0 + weightYL1 > lwd) {
                                        feiEncPWT[fieldId].Weights[0][l0][iY][iWeight] = weightYL0 > lwd/2 ? lwd/2 : weightYL0;
                                        feiEncPWT[fieldId].Weights[1][l1][iY][iWeight] = weightYL1 > lwd/2 ? lwd/2 : weightYL1;
                                    }
                                }
                                //CHECK UV
                                if (feiEncPWT[fieldId].ChromaWeightFlag[0][l0] && feiEncPWT[fieldId].ChromaWeightFlag[1][l1]) {
                                    mfxI16 weightCbL0 = feiEncPWT[fieldId].Weights[0][l0][iCb][iWeight];
                                    mfxI16 weightCbL1 = feiEncPWT[fieldId].Weights[1][l1][iCb][iWeight];
                                    mfxI16 weightCrL0 = feiEncPWT[fieldId].Weights[0][l0][iCr][iWeight];
                                    mfxI16 weightCrL1 = feiEncPWT[fieldId].Weights[1][l1][iCr][iWeight];
                                    if (weightCbL0 + weightCbL1 < -128) {
                                        feiEncPWT[fieldId].Weights[0][l0][iCb][iWeight] = weightCbL0 < -64 ? -64 : weightCbL0;
                                        feiEncPWT[fieldId].Weights[1][l1][iCb][iWeight] = weightCbL1 < -64 ? -64 : weightCbL1;
                                    }
                                    if (weightCrL0 + weightCrL1 < -128) {
                                        feiEncPWT[fieldId].Weights[0][l0][iCr][iWeight] = weightCrL0 < -64 ? -64 : weightCrL0;
                                        feiEncPWT[fieldId].Weights[1][l1][iCr][iWeight] = weightCrL1 < -64 ? -64 : weightCrL1;
                                    }
                                    mfxU16 lwd = (feiEncPWT[fieldId].ChromaLog2WeightDenom == 7) ? 127 : 128;
                                    if (weightCbL0 + weightCbL1 > lwd) {
                                        feiEncPWT[fieldId].Weights[0][l0][iCb][iWeight] = weightCbL0 > lwd/2 ? lwd/2 : weightCbL0;
                                        feiEncPWT[fieldId].Weights[1][l1][iCb][iWeight] = weightCbL1 > lwd/2 ? lwd/2 : weightCbL1;
                                    }
                                    if (weightCrL0 + weightCrL1 > lwd) {
                                        feiEncPWT[fieldId].Weights[0][l0][iCr][iWeight] = weightCrL0 > lwd/2 ? lwd/2 : weightCrL0;
                                        feiEncPWT[fieldId].Weights[1][l1][iCr][iWeight] = weightCrL1 > lwd/2 ? lwd/2 : weightCrL1;
                                    }
                                }
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
                    mfxU8 weighted_bipred_idc = mfxU8(CO3.WeightedBiPred ? mfxU8(CO3.WeightedBiPred - 1) : 0);

                    EXPECT_EQ(pps->weighted_pred_flag, weighted_pred_flag) << "--ERROR: wrong weighted_pred_flag in PPS.\n";
                    EXPECT_EQ(pps->weighted_bipred_idc, weighted_bipred_idc) << "--ERROR: wrong weighted_bipred_idc in PPS.\n";

                    if ((weighted_pred_flag && (slice->slice_type % 5 == SLICE_P))
                         || ((weighted_bipred_idc == 1) && (slice->slice_type % 5 == SLICE_B))) {
                        // buffers are attached by order, i.e. first field buffer goes first
                        switch (m_par.mfx.FrameInfo.PicStruct)
                        {
                        case MFX_PICSTRUCT_FIELD_TFF:
                            fieldId = slice->bottom_field_flag;
                            break;
                        case MFX_PICSTRUCT_FIELD_BFF:
                            fieldId = !slice->bottom_field_flag;
                            break;
                        default:
                            fieldId = 0;
                            break;
                        }

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
        mfxU16 WeightedBiPredMode;
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
    /*01*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},
    /*02*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},
    /*03*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},

    /*04*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*05*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*06*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},

    /*07*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_UNKNOWN,  MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},
    /*08*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_UNKNOWN,  MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},
    /*09*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_UNKNOWN,  MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},

    /*10*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_UNKNOWN,  MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*11*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_UNKNOWN,  MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*12*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_UNKNOWN,  MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},

    /*13*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},
    /*14*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},
    /*15*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, NO_PWT_BUFF},

    /*16*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*17*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*18*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},

    /*19*/ {4, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*20*/ {4, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*21*/ {4, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},

    /*22*/ {4, 4, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*23*/ {4, 4, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*24*/ {4, 4, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT, MFX_WEIGHTED_PRED_DEFAULT, WITH_PWT_BUFF},
    /*test case for B-frame*/
    //IP mode, explicit for B, w/ wpt
    /*25*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*26*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*27*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    //IP mode, explicit for B, w/o wpt
    /*28*/ {1, 1, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*29*/ {2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*30*/ {2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,  MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    //IPB mode, explicit for B, w/ wpt
    /*31*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*32*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*33*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    //IPB mode, explicit for B, w/o wpt
    /*34*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*35*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*36*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    //IPB mode, implicit for B, w/ wpt
    /*37*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_IMPLICIT, WITH_PWT_BUFF},
    /*38*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_IMPLICIT, WITH_PWT_BUFF},
    /*39*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_IMPLICIT, WITH_PWT_BUFF},
    //IPB mode, implicit for B, w/o wpt
    /*40*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_IMPLICIT, NO_PWT_BUFF},
    /*41*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_IMPLICIT, NO_PWT_BUFF},
    /*42*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_DEFAULT,   MFX_WEIGHTED_PRED_IMPLICIT, NO_PWT_BUFF},
    //IPB mode, explicit for PB, w/ wpt
    /*43*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*44*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*45*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*46*/ {4, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*47*/ {4, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*48*/ {4, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*49*/ {4, 4, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*50*/ {4, 4, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    /*51*/ {4, 4, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, WITH_PWT_BUFF},
    //IPB mode, explicit for PB, w/o wpt
    /*52*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*53*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    /*54*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_EXPLICIT, NO_PWT_BUFF},
    //IPB mode, explicit for P, implicit for B, w/ wpt
    /*55*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_IMPLICIT, WITH_PWT_BUFF},
    /*56*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_IMPLICIT, WITH_PWT_BUFF},
    /*57*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_IMPLICIT, WITH_PWT_BUFF},
    //IPB mode, explicit for P, implicit for B, w/o wpt
    /*58*/ {1, 2, MFX_PICSTRUCT_PROGRESSIVE, MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_IMPLICIT, NO_PWT_BUFF},
    /*59*/ {2, 2, MFX_PICSTRUCT_FIELD_TFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_IMPLICIT, NO_PWT_BUFF},
    /*60*/ {2, 2, MFX_PICSTRUCT_FIELD_BFF,   MFX_WEIGHTED_PRED_EXPLICIT,  MFX_WEIGHTED_PRED_IMPLICIT, NO_PWT_BUFF},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();
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
    CO3.WeightedBiPred             = tc.WeightedBiPredMode;
    m_hasPWT                       = tc.WeightedPredTable;

    g_tsStatus.check(Init());
    GetVideoParam();

    EncodeFrames(FRAME_TO_ENCODE);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_weighted_prediction);
}
