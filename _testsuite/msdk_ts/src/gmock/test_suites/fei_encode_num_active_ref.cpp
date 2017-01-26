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
 * [mdp-30735] beh test for mfxExtCodingOption3::NumRefActiveP/NumRefActiveBL0/NumRefActiveBL1
 * Test the options in both initialization and reset stage.
 */

namespace fei_encode_num_active_ref
{

#define MaxNumActiveRefP      4
#define MaxNumActiveRefBL0    4
#define MaxNumActiveRefBL1    1
#define MaxNumActiveRefBL1_i  2

#define nEncodedFrames         70

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true)
        , m_writer("/tmp/debug.264")
        , m_order(0)
    {
        m_filler = this;
        m_bs_processor = this;
    }
    ~TestSuite(){}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        s.Data.TimeStamp = s.Data.FrameOrder = m_order++;
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
#if 1
        SetBuffer(bs);

        bool bField = m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF) ? true : false;
        if (bField)
            nFrames *= 2;

        struct num_active_ref act_limit;

        if (bs.TimeStamp < nEncodedFrames) {
            act_limit = m_init;
        } else {
            act_limit = m_reset;
        }

        while (nFrames--) {
            H264AUWrapper au(ParseOrDie());
            slice_header *sh;

            int iSlice = 0;
            while ((sh = au.NextSlice())) {
                if (sh->first_mb_in_slice)
                    continue;

                //to calculate the limit ref num in list0/1
                mfxU32 numActiveRefL0 = 0;
                mfxU32 numActiveRefL1 = 0;

                if (sh->slice_type % 5 == 0) {
                    numActiveRefL0 = act_limit.NumRefActiveP;
                    numActiveRefL0 = (0 == numActiveRefL0) ? MaxNumActiveRefP : numActiveRefL0;
                    numActiveRefL0 = (numActiveRefL0 > MaxNumActiveRefP) ? MaxNumActiveRefP : numActiveRefL0;
                } else if (sh->slice_type % 5 == 1) {
                    numActiveRefL0 = act_limit.NumRefActiveBL0;
                    numActiveRefL0 = (0 == numActiveRefL0) ? MaxNumActiveRefBL0 : numActiveRefL0;
                    numActiveRefL0 = (numActiveRefL0 > MaxNumActiveRefBL0) ? MaxNumActiveRefBL0 : numActiveRefL0;

                    numActiveRefL1 = act_limit.NumRefActiveBL1;
                    numActiveRefL1 = (0 == numActiveRefL1) ? (bField ? MaxNumActiveRefBL1_i : MaxNumActiveRefBL1) : numActiveRefL1;
                    numActiveRefL1 = (numActiveRefL1 > (bField ? MaxNumActiveRefBL1_i : MaxNumActiveRefBL1)) ?
                                       (bField ? MaxNumActiveRefBL1_i : MaxNumActiveRefBL1) : numActiveRefL1;
                }

                //if it's P slice
                if (sh->slice_type % 5 == 0) {
                    EXPECT_GE(numActiveRefL0, (size_t)sh->num_ref_idx_l0_active_minus1 + 1);
                } else if (sh->slice_type % 5 == 1) {
                //if it's B slice
                    EXPECT_GE(numActiveRefL0, (size_t)sh->num_ref_idx_l0_active_minus1 + 1);
                    EXPECT_GE(numActiveRefL1, (size_t)sh->num_ref_idx_l1_active_minus1 + 1);
                }
            }
        }

        bs.DataOffset = bs.DataLength = 0;

        return MFX_ERR_NONE;
#else
        return m_writer.ProcessBitstream(bs, nFrames);
#endif
    }


private:
    struct num_active_ref
    {
        mfxU16 NumRefActiveP;   //currently in AVC encoder, no layers specified
        mfxU16 NumRefActiveBL0;
        mfxU16 NumRefActiveBL1;
    };

    struct tc_struct
    {
        mfxU16      NumRefFrame;
        mfxU16      GopRefDist;
        mfxU16      IdrInterval;
        mfxU16      PicStruct;
        mfxU16      PRefType; //P-Pyramid
        mfxU16      BRefType;
    };

    static const tc_struct test_case[];

    tsBitstreamWriter m_writer;
    struct num_active_ref m_init;
    struct num_active_ref m_reset;
    mfxU32 m_order;
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
    /*11*/ {0, 5, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_DEFAULT, MFX_B_REF_OFF},
    /*12*/ {0, 4, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_DEFAULT, MFX_B_REF_OFF},
    /*13*/ {0, 3, 0, MFX_PICSTRUCT_PROGRESSIVE, MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*14*/ {6, 5, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*15*/ {6, 4, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*16*/ {6, 3, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*17*/ {6, 2, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*18*/ {6, 1, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*19*/ {0, 5, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*20*/ {0, 4, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*21*/ {0, 3, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*22*/ {0, 2, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*23*/ {0, 1, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*24*/ {0, 5, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_OFF},
    /*25*/ {0, 4, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_OFF},
    /*26*/ {0, 3, 0, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*27*/ {0, 2, 0, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*28*/ {6, 5, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*29*/ {6, 4, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*30*/ {6, 3, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*31*/ {6, 2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*32*/ {6, 1, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*33*/ {0, 5, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*34*/ {0, 4, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_PYRAMID},
    /*35*/ {0, 3, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_PYRAMID},
    /*36*/ {0, 2, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*37*/ {0, 1, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*38*/ {0, 5, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_DEFAULT, MFX_B_REF_OFF},
    /*39*/ {0, 4, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_DEFAULT, MFX_B_REF_OFF},
    /*40*/ {0, 3, 1, MFX_PICSTRUCT_FIELD_BFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
    /*41*/ {0, 2, 1, MFX_PICSTRUCT_FIELD_TFF,   MFX_P_REF_PYRAMID, MFX_B_REF_OFF},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];

    //set parameters for mfxVideoParam
    m_par.AsyncDepth = 1; //limitation for FEI (from sample_fei)
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP; //For now FEI work with CQP only

    mfxExtCodingOption3& CO3 = m_par;
    mfxExtCodingOption2& CO2 = m_par;

    m_par.mfx.GopPicSize = 30;

    //assign the specified GOP
    m_par.mfx.FrameInfo.PicStruct = tc.PicStruct;
    m_par.mfx.NumRefFrame         = tc.NumRefFrame;
    m_par.mfx.GopRefDist          = tc.GopRefDist;
    m_par.mfx.IdrInterval         = tc.IdrInterval;
    CO3.PRefType                  = tc.PRefType; //Actually PRefType is not working for AVC encoder
    CO2.BRefType                  = tc.BRefType;

    srand(rand() % 1000);
    bool bField = m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF) ? true : false;
    m_init.NumRefActiveP    = rand() % (MaxNumActiveRefP + 1);
    m_init.NumRefActiveBL0  = rand() % (MaxNumActiveRefBL0 + 1);
    m_init.NumRefActiveBL1  = rand() % ((bField ? MaxNumActiveRefBL1_i : MaxNumActiveRefBL1) + 1);
    m_reset.NumRefActiveP   = rand() % (MaxNumActiveRefP + 1);
    m_reset.NumRefActiveBL0 = rand() % (MaxNumActiveRefBL0 + 1);
    m_reset.NumRefActiveBL1 = rand() % ((bField ? MaxNumActiveRefBL1_i : MaxNumActiveRefBL1) + 1);

    //only layer#0 is supported in AVC encoder
    CO3.NumRefActiveP[0]   = m_init.NumRefActiveP;
    CO3.NumRefActiveBL0[0] = m_init.NumRefActiveBL0;
    CO3.NumRefActiveBL1[0] = m_init.NumRefActiveBL1;

    //set mfxEncodeCtrl ext buffers
    //There is no per-frame adjustment of m_ctrl, so setting it here directly.
    //Or else, it needs to set m_ctrl in ProcessSurface();
    std::vector<mfxExtBuffer*> in_buffs;

    // Interlaced case requires two control buffers to be passed
    mfxExtFeiEncFrameCtrl in_efc[2];
    memset(in_efc, 0, 2 * sizeof(in_efc[0]));

    for (int i = 0; i < 2; ++i)
    {
        in_efc[i].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        in_efc[i].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
        in_efc[i].SearchPath             = 2;
        in_efc[i].LenSP                  = 57;
        in_efc[i].SubMBPartMask          = 0;
        in_efc[i].MultiPredL0            = 0;
        in_efc[i].MultiPredL1            = 0;
        in_efc[i].SubPelMode             = 3;
        in_efc[i].InterSAD               = 2;
        in_efc[i].IntraSAD               = 2;
        in_efc[i].DistortionType         = 0;
        in_efc[i].RepartitionCheckEnable = 0;
        in_efc[i].AdaptiveSearch         = 0;
        in_efc[i].MVPredictor            = 0;
        in_efc[i].NumMVPredictors[0]     = 0;
        in_efc[i].NumMVPredictors[1]     = 0;
        in_efc[i].PerMBQp                = 0;
        in_efc[i].PerMBInput             = 0;
        in_efc[i].MBSizeCtrl             = 0;
        in_efc[i].RefHeight              = 32;
        in_efc[i].RefWidth               = 32;
        in_efc[i].SearchWindow           = 5;
        in_buffs.push_back((mfxExtBuffer*)(&in_efc[i]));
    }

    if (!in_buffs.empty())
    {
        m_ctrl.ExtParam    = &in_buffs[0];
        m_ctrl.NumExtParam = mfxU16(1 + (m_par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE));
    }

    Init();
    m_max = nEncodedFrames;
    m_cur = 0;
    EncodeFrames(nEncodedFrames);

    //reset the value of 3 options
    CO3.NumRefActiveP[0]   = m_reset.NumRefActiveP;
    CO3.NumRefActiveBL0[0] = m_reset.NumRefActiveBL0;
    CO3.NumRefActiveBL1[0] = m_reset.NumRefActiveBL1;
    Reset();
    m_max = nEncodedFrames;
    m_cur = 0;
    EncodeFrames(nEncodedFrames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_num_active_ref);
};
