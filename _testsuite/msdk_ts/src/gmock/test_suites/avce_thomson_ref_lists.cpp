#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace avce_thomson_ref_lists
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
    static const mfxU16 max_rl = 12;

    struct ctrl{
        const tsStruct::Field* field;
        mfxU32 value;
    };

    struct tc_struct
    {
        mfxU16 nFrames;
        mfxU16 BRef;
        ctrl mfx[3];
        struct _rl{
            mfxU16 poc;
            ctrl f[18];

        }rl[max_rl];
    };

    static const tc_struct test_case[];

};

class SFiller : public tsSurfaceProcessor
{
private:
    mfxU32 m_c;
    mfxU32 m_i;
    mfxU32 m_pic_struct;
    mfxEncodeCtrl& m_ctrl;
    const TestSuite::tc_struct & m_tc;
    std::vector<mfxExtAVCRefLists> m_list;
    std::vector<mfxExtBuffer*> m_pbuf;

public:
    SFiller(const TestSuite::tc_struct & tc, mfxEncodeCtrl& ctrl) : m_c(0), m_i(0), m_tc(tc), m_ctrl(ctrl)
    {
        mfxExtAVCRefLists rlb = {{MFX_EXTBUFF_AVC_REFLISTS, sizeof(mfxExtAVCRefLists)}, };

        for (auto& x : rlb.RefPicList0)
            x.FrameOrder = MFX_FRAMEORDER_UNKNOWN;
        for (auto& x : rlb.RefPicList1)
            x.FrameOrder = MFX_FRAMEORDER_UNKNOWN;

        m_list.resize(TestSuite::max_rl+2, rlb);
        m_pbuf.resize(TestSuite::max_rl+2, 0  );

        
        for (auto& ctrl : tc.mfx)
        {
            if (ctrl.field == &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct)
            {
                m_pic_struct = ctrl.value;
                break;
            }
        }
    };
    mfxStatus ProcessSurface(mfxFrameSurface1& s);
};

 mfxStatus SFiller::ProcessSurface(mfxFrameSurface1& s)
 {
     bool bff = (m_pic_struct == MFX_PICSTRUCT_FIELD_BFF);

     s.Data.FrameOrder = m_c;
     memset(&m_ctrl, 0, sizeof(m_ctrl));

     m_pbuf[m_i+0] = &m_list[m_i+0].Header;
     m_pbuf[m_i+1] = &m_list[m_i+1].Header;

     m_ctrl.ExtParam = &m_pbuf[m_i];

     if (m_c * 2 == m_tc.rl[m_i].poc)
     {
         m_ctrl.NumExtParam = 1;

         for (auto& f : m_tc.rl[m_i].f)
             if (f.field)
                 tsStruct::set(m_pbuf[m_i], *f.field, f.value);
         m_i ++;
     }

     if (m_c * 2 + 1 == m_tc.rl[m_i].poc)
     {
         m_ctrl.NumExtParam = 2;

         for (auto& f : m_tc.rl[m_i].f)
             if (f.field)
                 tsStruct::set(m_pbuf[m_i], *f.field, f.value);

         if (bff)
             std::swap(m_ctrl.ExtParam[0], m_ctrl.ExtParam[1]);

         m_i ++;
     }

     m_c ++;
     
     return MFX_ERR_NONE;
 }

class BitstreamChecker : public tsBitstreamProcessor, public tsParserH264AU
{
private:
    mfxI32 m_i;
    mfxU32 m_pic_struct;
    const TestSuite::tc_struct & m_tc;
public:
    BitstreamChecker(const TestSuite::tc_struct & tc)
        : tsParserH264AU() 
        , m_tc(tc)
        , m_i(0)
    {
        set_trace_level(BS_H264_TRACE_LEVEL_REF_LIST);
        
        for (auto& ctrl : tc.mfx)
        {
            if (ctrl.field == &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct)
            {
                m_pic_struct = ctrl.value;
                break;
            }
        }
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

    mfxU32 GetPOC(mfxExtAVCRefLists::mfxRefPic rp)
    {
        if (rp.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
            return (2 * rp.FrameOrder);
        return (2 * rp.FrameOrder + (rp.PicStruct != m_pic_struct));
    }
};

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxU32 checked = 0;
    SetBuffer(bs);

    nFrames *= 1 + (m_pic_struct != MFX_PICSTRUCT_PROGRESSIVE);

    while (checked++ < nFrames)
    {
        UnitType& hdr = ParseOrDie();
        
        for (UnitType::nalu_param* nalu = hdr.NALU; nalu < hdr.NALU + hdr.NumUnits; nalu ++)
        {
            if(nalu->nal_unit_type != 1 && nalu->nal_unit_type != 5)
                continue;

            slice_header& s = *nalu->slice_hdr;
            m_i = -1;


            for (auto& l : m_tc.rl)
            {
                if (l.poc == s.PicOrderCnt && s.PicOrderCnt)
                {
                    m_i = &l - &m_tc.rl[0];
                    break;
                }
            }

            if (m_i >= 0)
            {
                mfxExtAVCRefLists rl = {};
                for (auto& f : m_tc.rl[m_i].f)
                {
                    if (f.field)
                        tsStruct::set(&rl, *f.field, f.value);
                }

                if (!s.num_ref_idx_active_override_flag)
                {
                    s.num_ref_idx_l0_active_minus1 = s.pps_active->num_ref_idx_l0_default_active_minus1;
                    if (s.slice_type % 5 == 1)
                        s.num_ref_idx_l1_active_minus1 = s.pps_active->num_ref_idx_l1_default_active_minus1;
                }

                if (rl.NumRefIdxL0Active)
                {
                    EXPECT_EQ(rl.NumRefIdxL0Active, s.num_ref_idx_l0_active_minus1+1);

                    for (mfxU16 i = 0; i <= s.num_ref_idx_l0_active_minus1; i ++)
                    {
                        EXPECT_EQ(GetPOC(rl.RefPicList0[i]), s.RefPicList[0][i]->PicOrderCnt);
                    }
                }

                if (rl.NumRefIdxL1Active)
                {
                    EXPECT_EQ(rl.NumRefIdxL1Active, s.num_ref_idx_l1_active_minus1+1);

                    for (mfxU16 i = 0; i <= s.num_ref_idx_l1_active_minus1; i ++)
                    {
                        EXPECT_EQ(GetPOC(rl.RefPicList1[i]), s.RefPicList[1][i]->PicOrderCnt);
                    }
                }

                m_i ++;
            }
        }
    }
    bs.DataLength = 0;

    return MFX_ERR_NONE;
}


const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*0*/ 
        6, 1,
        {
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
            {&tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame, 4},
        },
        {
            {
                4,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                }
                
            },
            {
                6,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 3},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 0},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].FrameOrder, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[2].FrameOrder, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[2].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                }
            },
            {
                10,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].FrameOrder, 4},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                }
            },
        }
    },
    {/*1*/ 
        10, 2,
        {
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
            {&tsStruct::mfxVideoParam.mfx.GopRefDist, 4},
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame, 4},
        },
        {
            {
                2,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 0},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].FrameOrder, 4},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                }
            },
            {
                10,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].FrameOrder, 8},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                }
                
            },
        }
    },
    {/*2*/ 
        4, 1,
        {
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
            {&tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame, 4},
        },
        {
            {
                4,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 3},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 0},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].FrameOrder, 0},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[2].FrameOrder, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[2].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                }
            },
            {
                5,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].FrameOrder, 0},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                }
            },
            {
                6,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 3},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 0},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].FrameOrder, 0},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[2].FrameOrder, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[2].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                }
            },
            {
                7,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].FrameOrder, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                }
            },
        }
    },
    {/*3*/ 
        10, 2,
        {
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
            {&tsStruct::mfxVideoParam.mfx.GopRefDist, 4},
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame, 4},
        },
        {
            {
                10,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 4},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].FrameOrder, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[1].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].FrameOrder, 8},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                }
            },
            {
                11,
                {
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 1},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].FrameOrder, 4},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList0[0].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                    {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active, 2},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].FrameOrder, 6},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[0].PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[1].FrameOrder, 8},
                    {&tsStruct::mfxExtAVCRefLists.RefPicList1[1].PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                }
            },
        }
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    SFiller sf(tc, *m_pCtrl); 
    BitstreamChecker c(tc);

    m_filler = &sf;
    m_bs_processor = &c;
    m_par.AsyncDepth   = 1;
    m_par.mfx.NumSlice = 1;

    for (auto& ctrl : tc.mfx)
        if (ctrl.field)
            tsStruct::set(m_pPar, *ctrl.field, ctrl.value);
    
    ((mfxExtCodingOption2&)m_par).BRefType = tc.BRef;
    
    EncodeFrames(tc.nFrames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_thomson_ref_lists);
}