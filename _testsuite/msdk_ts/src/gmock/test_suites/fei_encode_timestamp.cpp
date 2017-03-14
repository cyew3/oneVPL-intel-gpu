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
 * [MDP-31408] check mfxFrameData::TimeStamp processed correctly by FEI ENCODE and
 * reported in output bitstream.
 */

namespace fei_encode_timestamp
{

mfxU32 CeilLog2(mfxU32 val)
{
    mfxU32 res = 0;
    while (val)
    {
        val >>= 1;
        res++;
    }
    return res;
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
              , numReorderFrames(0)
              , dpbOutputDelay(0)
              , m_idrCnt(0)
              , m_frameOrder(0)
              , m_lastFrameOrder(0)
    {
        m_filler = this;
        m_bs_processor = this;
    }

        ~TestSuite() {}

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            double frN = m_par.mfx.FrameInfo.FrameRateExtN;
            mfxU32 frD = m_par.mfx.FrameInfo.FrameRateExtD;
            mfxU64 in_ts = (m_order++) * frD * (90000 / frN);

            s.Data.TimeStamp = in_ts;

            return MFX_ERR_NONE;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            double frN = m_par.mfx.FrameInfo.FrameRateExtN;
            mfxU32 frD = m_par.mfx.FrameInfo.FrameRateExtD;

            if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
                nFrames *= 2;
            SetBuffer(bs);

            while (nFrames--)
            {
                UnitType& hdr = ParseOrDie();

                for (UnitType::nalu_param* nalu = hdr.NALU; nalu < hdr.NALU + hdr.NumUnits; nalu ++)
                {
                    if(nalu->nal_unit_type != 1 && nalu->nal_unit_type != 5)
                        continue;

                    if (nalu->nal_unit_type == 5)
                        m_idrCnt ++;
                    if(m_idrCnt == 0)
                        continue;

                    slice_header *sh = nalu->slice_hdr;

                    mfxU32 m_encOrder = m_order - m_par.mfx.GopRefDist;
                    m_frameOrder = GetFrameOrder(sh->PicOrderCnt);
                    dpbOutputDelay = 2 * (m_frameOrder + numReorderFrames - m_encOrder);

                    mfxU64 expected_pts = m_frameOrder * frD * (90000 / frN);
                    mfxI64 expected_dts = CalcDTSFromPTS(expected_pts);

                    mfxI64 dts = bs.DecodeTimeStamp;
                    mfxU64 pts = bs.TimeStamp;

                    EXPECT_EQ(pts, expected_pts) << "ERROR: wrong PTS.\nFrame: " << m_frameOrder << ", expected: " << expected_pts << ", pts:" << pts;
                    EXPECT_EQ(dts, expected_dts) << "ERROR: wrong DTS.\nFrame: " << m_frameOrder << ", expected: " << expected_dts << ", dts:" << dts;
                }
            }
            bs.DataOffset =  bs.DataLength = 0;
            bs.DecodeTimeStamp = bs.TimeStamp = 0;

            return MFX_ERR_NONE;
        }

        struct tc_struct
        {
            mfxU16 NumRefFrame;
            mfxU16 GopRefDist;
            mfxU16 IdrInterval;
            mfxU16 FrameRateExtN;
            mfxU16 FrameRateExtD;
            mfxU16 PicStruct;
            mfxU16 BRefType;
        };

        static const tc_struct test_case[];
        static const unsigned int n_cases;
        int RunTest(unsigned int id);
        mfxI64 CalcDTSFromPTS(mfxU64 timeStamp);
        mfxU32 GetFrameOrder(mfxI32 POC);

        mfxU32 m_order;
        mfxU32 numReorderFrames;
        mfxU32 dpbOutputDelay;
        mfxU32 m_idrCnt;
        mfxU32 m_frameOrder;
        mfxU32 m_lastFrameOrder;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*01*/ {1, 1, 0, 30, 1,       MFX_PICSTRUCT_PROGRESSIVE, MFX_B_REF_OFF},
    /*02*/ {4, 2, 0, 30, 1,       MFX_PICSTRUCT_PROGRESSIVE, MFX_B_REF_OFF},
    /*03*/ {4, 4, 1, 30, 1,       MFX_PICSTRUCT_PROGRESSIVE, MFX_B_REF_OFF},
    /*04*/ {4, 4, 1, 30000, 1001, MFX_PICSTRUCT_PROGRESSIVE, MFX_B_REF_OFF},
    /*05*/ {4, 4, 2, 25000, 1001, MFX_PICSTRUCT_PROGRESSIVE, MFX_B_REF_OFF},
    /*06*/ {2, 4, 1, 30, 1,       MFX_PICSTRUCT_PROGRESSIVE, MFX_B_REF_PYRAMID},
    /*07*/ {4, 4, 2, 30, 1,       MFX_PICSTRUCT_FIELD_BFF,   MFX_B_REF_PYRAMID},
    /*08*/ {2, 1, 0, 30, 1,       MFX_PICSTRUCT_FIELD_TFF,   MFX_B_REF_OFF},
    /*09*/ {2, 4, 1, 30, 1,       MFX_PICSTRUCT_FIELD_TFF,   MFX_B_REF_OFF},
    /*10*/ {2, 1, 0, 30, 1,       MFX_PICSTRUCT_FIELD_BFF,   MFX_B_REF_OFF},
    /*11*/ {2, 6, 1, 30, 1,       MFX_PICSTRUCT_FIELD_BFF,   MFX_B_REF_OFF},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

mfxU32 TestSuite::GetFrameOrder(mfxI32 POC)
{
    return (m_idrCnt - 1) * m_par.mfx.GopPicSize * (m_par.mfx.IdrInterval + 1) + POC / 2;
}

mfxI64 TestSuite::CalcDTSFromPTS(mfxU64 timeStamp)
{
    if (timeStamp != static_cast<mfxU64>(MFX_TIMESTAMP_UNKNOWN))
    {
        mfxF64 tcDuration90KHz = (mfxF64)m_par.mfx.FrameInfo.FrameRateExtD / (m_par.mfx.FrameInfo.FrameRateExtN * 2) * 90000;
        return mfxI64(timeStamp - tcDuration90KHz * dpbOutputDelay);
    }

    return MFX_TIMESTAMP_UNKNOWN;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];
    mfxExtCodingOption2& CO2 = m_par;

    m_par.AsyncDepth = 1;
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.GopPicSize = 30;

    //assign the specified values
    m_par.mfx.NumRefFrame               = tc.NumRefFrame;
    m_par.mfx.GopRefDist                = tc.GopRefDist;
    m_par.mfx.IdrInterval               = tc.IdrInterval;
    m_par.mfx.FrameInfo.FrameRateExtN   = tc.FrameRateExtN;
    m_par.mfx.FrameInfo.FrameRateExtD   = tc.FrameRateExtD;
    m_par.mfx.FrameInfo.PicStruct       = tc.PicStruct;
    CO2.BRefType                        = tc.BRefType;

    mfxU32 numField = 1;
    if ((m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
        (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)) {
            numField = 2;
    }

    mfxExtBuffer* bufFrCtrl[2];

    mfxExtFeiEncFrameCtrl feiEncCtrl[2];

    mfxU32 fieldId = 0;

    //assign ExtFeiEncFrameCtrl(mfxEncodeCtrl/runtime)
    for (fieldId = 0; fieldId < numField; fieldId++) {

        memset(&feiEncCtrl[fieldId], 0, sizeof(mfxExtFeiEncFrameCtrl));

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
        feiEncCtrl[fieldId].PerMBQp = 0;
        feiEncCtrl[fieldId].PerMBInput = 0;
        feiEncCtrl[fieldId].MBSizeCtrl = 0;
        feiEncCtrl[fieldId].RefHeight = 32;
        feiEncCtrl[fieldId].RefWidth = 32;
        feiEncCtrl[fieldId].SearchWindow = 5;

        bufFrCtrl[fieldId] = (mfxExtBuffer*)&feiEncCtrl[fieldId];
    }

    m_ctrl.NumExtParam = numField;
    m_ctrl.ExtParam = bufFrCtrl;

    numReorderFrames = m_par.mfx.GopRefDist > 1 ? 1 : 0;
    if (m_par.mfx.GopRefDist > 2 && tc.BRefType == MFX_B_REF_PYRAMID)
    {
        numReorderFrames = (mfxU8)CeilLog2(m_par.mfx.GopRefDist - 1);
    }

    Init();
    GetVideoParam();

    EncodeFrames(70);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_timestamp);
}
