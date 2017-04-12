/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_idr_insertion
{

class TestSuite : public tsVideoEncoder, tsBitstreamProcessor, tsParserH264AU, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
                , m_fo(0)
                , idr_timestamp(0) 
                , prev_timestamp(0)
                , pred_frame_type(-1)

    {
        m_bs_processor = this;
        set_trace_level(0);
        m_filler = this;
    }
    ~TestSuite() {}

    int RunTest(unsigned int id);

    enum
    {
        MFXPAR = 1,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        mfxU16 BRefType;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];
    static const unsigned int n_cases;

    mfxU32 m_fo;
    mfxU64 idr_timestamp;
    mfxU64 prev_timestamp;
    int pred_frame_type;
    bool isSliceHeader(Bs8u nalu_type){
        return (nalu_type == 1 || nalu_type == 5 || nalu_type == 20);
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;
        if (s.Data.TimeStamp % 50 == 0)
        {
            m_ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;  
            idr_timestamp = s.Data.TimeStamp;
            prev_timestamp = idr_timestamp - 1;
        }
        else 
        {
            m_ctrl.FrameType = MFX_FRAMETYPE_UNKNOWN;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);
        mfxU32 nf = 0;
        while (nf++ < nFrames)
        {
            UnitType& au = ParseOrDie();
            slice_header& frame1 = *au.NALU->slice_hdr;

            for (BS_H264_au::nalu_param* nalu = au.NALU; nalu < (au.NALU+au.NumUnits); nalu++)
            {
                if(!isSliceHeader(nalu->nal_unit_type)) continue;
                if(nalu->slice_hdr->first_mb_in_slice)  continue;

                slice_header& frame = *nalu->slice_hdr;
                if(bs.TimeStamp == prev_timestamp)
                {
                    pred_frame_type = (int)frame.slice_type;
                }
                if(pred_frame_type != -1)
                {
                    EXPECT_EQ(5, pred_frame_type) << "error last frame type: " << pred_frame_type << " \n";
                }
            }
        }
        bs.DataLength = 0;
        bs.DataOffset = 0;

        return MFX_ERR_NONE;
    }
};


const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, 2, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,  4}},
    {/*01*/ MFX_ERR_NONE, 0, 2, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,  7}},
    {/*02*/ MFX_ERR_NONE, 0, 2, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}},
    {/*03*/ MFX_ERR_NONE, 0, 2, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0}},
    {/*04*/ MFX_ERR_NONE, 0, 1, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0}},
    {/*05*/ MFX_ERR_NONE, 0, 1, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4}},
    {/*06*/ MFX_ERR_NONE, 0, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0}},
    {/*07*/ MFX_ERR_NONE, 0, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4}},
    {/*08*/ MFX_ERR_NONE, 0, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7}}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    m_par.mfx.TargetKbps = 5000;
    m_par.mfx.MaxKbps = 0;
    m_par.mfx.InitialDelayInKB = 0;
    m_par.mfx.FrameInfo.PicStruct = 1;
    m_par.mfx.GopPicSize = 12;
    SETPARS(m_pPar, MFXPAR);
    m_par.AsyncDepth = 1;
    mfxExtCodingOption2& cod2 = m_par;
    cod2.BRefType = tc.BRefType;

    m_par.mfx.FrameInfo.Width = 352;
    m_par.mfx.FrameInfo.Height = 288;
    m_par.mfx.FrameInfo.CropW = 352;
    m_par.mfx.FrameInfo.CropH = 288;

    g_tsStreamPool.Reg();
    MFXInit();

    Init();
    GetVideoParam();

    mfxU32 nframes = 60;
    EncodeFrames(nframes, true);
    EXPECT_EQ(true, pred_frame_type != -1) << "error last frame type: " << pred_frame_type << " \n";
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_idr_insertion);
};
