#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_b_pyramid
{

class TestSuite : public tsVideoEncoder, tsBitstreamProcessor, tsParserH264AU
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
                , last_frame_poc(0)
                , dpb_reorder()
    {
        m_bs_processor = this;
        set_trace_level(0);
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
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];
    static const unsigned int n_cases;

    mfxU32 last_frame_poc;
    std::list<mfxI32> dpb_reorder;
    bool isSliceHeader(Bs8u nalu_type){
        return (nalu_type == 1 || nalu_type == 5 || nalu_type == 20);
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        mfxU32 nf = 0;

        while (nf++ < nFrames)
        {
            UnitType& au = ParseOrDie();

            for (BS_H264_au::nalu_param* nalu = au.NALU; nalu < (au.NALU+au.NumUnits); nalu++)
            {
                if(!isSliceHeader(nalu->nal_unit_type)) continue;
                if(nalu->slice_hdr->first_mb_in_slice)  continue;

                slice_header& frame = *nalu->slice_hdr;
                bool num_reorder_frames_set = false;

                dpb_reorder.push_back(frame.PicOrderCnt);
                dpb_reorder.sort(std::less<mfxI32>());
                while(dpb_reorder.size() && (dpb_reorder.front() == 0 || (dpb_reorder.front() - last_frame_poc) < 3)){
                    last_frame_poc = dpb_reorder.front();
                    dpb_reorder.pop_front();
                }

                if(frame.sps_active->vui_parameters_present_flag){
                    if(frame.sps_active->vui->bitstream_restriction_flag){
                        num_reorder_frames_set = true;
                        if (frame.sps_active->vui->num_reorder_frames < dpb_reorder.size())
                        {
                            g_tsLog << "ERROR: num_reorder_frames value is wrong (vui->num_reorder_frames = "
                                    << frame.sps_active->vui->num_reorder_frames << ";"
                                    << "dpb_reorder buffer size = " << dpb_reorder.size()
                                    << "\n";
                            g_tsStatus.check(MFX_ERR_ABORTED);
                        }
                    }
                }

                if (!num_reorder_frames_set)
                {
                    g_tsLog << "ERROR: num_reorder_frames is not set\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }

                if(nalu->slice_hdr->slice_type%5 != 1)  continue;
                if(nalu->slice_hdr->isReferencePicture) continue;

                Bs32u l0_idx = 1 + ( frame.num_ref_idx_active_override_flag ? frame.num_ref_idx_l0_active_minus1
                                                                            : frame.pps_active->num_ref_idx_l0_default_active_minus1 );
                Bs32u l1_idx = 1 + ( frame.num_ref_idx_active_override_flag ? frame.num_ref_idx_l1_active_minus1
                                                                            : frame.pps_active->num_ref_idx_l1_default_active_minus1 );
                bool b_pyramid = false;

                while(l0_idx--){
                    if (!frame.RefPicList[0][l0_idx])
                    {
                        g_tsLog << "ERROR: incomplete RefPicList0\n";
                        g_tsStatus.check(MFX_ERR_ABORTED);
                    }
                    if(    frame.prev_ref_pic == frame.RefPicList[0][l0_idx]
                        && frame.prev_ref_pic->slice_type%5 == 1
                        && frame.PicOrderCnt > frame.prev_ref_pic->PicOrderCnt)
                            b_pyramid = true;
                }
                while(l1_idx--){
                    if (!frame.RefPicList[1][l1_idx])
                    {
                        g_tsLog << "ERROR: incomplete RefPicList1\n";
                        g_tsStatus.check(MFX_ERR_ABORTED);
                    }
                    if(    frame.prev_ref_pic == frame.RefPicList[1][l1_idx]
                        && frame.prev_ref_pic->slice_type%5 == 1
                        && frame.PicOrderCnt < frame.prev_ref_pic->PicOrderCnt)
                            b_pyramid = true;
                }

                if (!b_pyramid)
                {
                    g_tsLog << "ERROR: B-pyramyd expected\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
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
    {/*00*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,  4}},
    {/*01*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,  7}},
    {/*02*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}},
    {/*03*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 13}},
    {/*04*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 16}}
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
    SETPARS(m_pPar, MFXPAR);

    mfxExtCodingOption2& cod2 = m_par;
    cod2.BRefType = 2;

    m_par.mfx.FrameInfo.Width = 352;
    m_par.mfx.FrameInfo.Height = 288;
    m_par.mfx.FrameInfo.CropW = 352;
    m_par.mfx.FrameInfo.CropH = 288;

    tsRawReader reader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_filler = &reader;

    MFXInit();

    Init();
    GetVideoParam();

    mfxU32 nframes = 50;
    EncodeFrames(nframes, true);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_b_pyramid);
};
