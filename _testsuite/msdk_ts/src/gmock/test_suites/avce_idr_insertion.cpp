/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

//set idr to every 50 frame
#define IDR_FRAME_NUMBER 50

namespace avce_idr_insertion
{
bool is_frame_idr(int time_stamp);
bool is_frame_last_before_idr(int time_stamp);

class TestSuite : public tsVideoEncoder, tsBitstreamProcessor, tsParserH264AU, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
                , m_fo(0)
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
    bool isSliceHeader(Bs8u nalu_type){
        return (nalu_type == 1 || nalu_type == 5 || nalu_type == 20);
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;
        // if frame should be idr set idr frame type
        if (is_frame_idr(s.Data.TimeStamp))
        {
            m_ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
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
                // check slice type for frame which should be idr
                if (is_frame_idr(bs.TimeStamp))
                {
                    int frame_type = (int)frame.slice_type;
                    EXPECT_EQ(7, frame_type) << "error, idr frame has type: " << frame_type << " \n";
                }
                // check slice type for last frame before idr frame
                if (is_frame_last_before_idr(bs.TimeStamp))
                {
                    int frame_type = (int)frame.slice_type;
                    EXPECT_EQ(5, frame_type) << "error, previous frame from idr has type: " << frame_type << " \n";
                }
            }
        }
        bs.DataLength = 0;
        bs.DataOffset = 0;

        return MFX_ERR_NONE;
    }
};

bool is_frame_idr(int time_stamp)
{
    // every IDR_FRAME_NUMBER (=50) frame should be idr
    if (time_stamp % IDR_FRAME_NUMBER == 0)
    {
        return true;
    }
    return false;
}

bool is_frame_last_before_idr(int time_stamp)
{
    return is_frame_idr(time_stamp + 1);
}

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
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_idr_insertion);
};
