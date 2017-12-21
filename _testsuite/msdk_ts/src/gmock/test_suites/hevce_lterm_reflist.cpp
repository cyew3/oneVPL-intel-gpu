/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

//#define DUMP_BS

#define ARRAY_SIZE( A ) (sizeof(A)/sizeof(*(A)))

#define MAX_REF_LIST_COUNT  12
#define MAX_MFX_PARAM_COUNT 10

#define FRAME_WIDTH         720
#define FRAME_HEIGHT        576

namespace hevce_lterm_reflist
{
    using namespace BS_HEVC2;

    typedef struct {
        const tsStruct::Field* field;
        mfxU32 value;
    } ctrl;

    typedef struct
    {
        mfxU16 nFrames;
        ctrl mfx[MAX_MFX_PARAM_COUNT];
        struct _rl {
            mfxU16 poc;
            ctrl f[18];
        }rl[MAX_REF_LIST_COUNT];
    } tc_struct;

    typedef struct
    {
        mfxI32 POC;
        bool isLTR;
    } LtrStatus;

    class TestSuite : tsVideoEncoder,
        public tsBitstreamProcessor,
        public tsSurfaceProcessor,
        public tsParserHEVC2 {


    private:
        mfxEncodeCtrl& m_ctrl;
        tc_struct m_tc;
        mfxU32 m_i;
#ifdef DUMP_BS
        tsBitstreamWriter m_writer;
#endif

    public:
        TestSuite() :
            tsVideoEncoder(MFX_CODEC_HEVC)
#ifdef DUMP_BS
            , m_writer("debug_ltr.265")
#endif
            , tsParserHEVC2()
            , m_ctrl(*m_pCtrl)
            , m_tc()
            , m_i(0)
        {
            m_bs_processor = this;
            m_filler = this;
        }
        ~TestSuite() { }
        int RunTest(unsigned int id);
        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxExtAVCRefListCtrl m_RefList;
        mfxExtBuffer* m_pbuf;
        static const unsigned int n_cases;
        static const tc_struct test_case[];
    };


    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);
#ifdef DUMP_BS
        m_writer.ProcessBitstream(bs, nFrames);
#endif
        std::vector<LtrStatus> Frames;
        mfxU32 nLTR;

        //check is there poc for RejectedFrameList
        for (mfxU32 i = 0; i < ARRAY_SIZE(m_RefList.RejectedRefList); i++)
        {
            mfxI32 rjfo = m_RefList.RejectedRefList[i].FrameOrder;
            if (rjfo) //if there is a frame that must be rejected
            {
                Frames.push_back({ rjfo, false }); //push it with unmarker LTR flag
            }
        }
        //check is there poc for LongTermRefList
        for (mfxU32 i = 0; i < ARRAY_SIZE(m_RefList.LongTermRefList); i++)
        {
            mfxI32 ltfo = m_RefList.LongTermRefList[i].FrameOrder;
            if (ltfo) //if there is a LTR frame
            {
                auto isAlreadyRejected = find_if(Frames.begin(), Frames.end(), [&lt = ltfo](const LtrStatus& s) { return s.POC == lt && s.isLTR == false; });
                if (isAlreadyRejected == Frames.end()) //which is not rejected
                    Frames.push_back({ ltfo, true }); //push it with marker LTR flag
            }
        }

        while (nFrames--)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (!IsHEVCSlice(pNALU->nal_unit_type))
                    continue;

                const auto & sh = *pNALU->slice;

                nLTR = 0;

                for (LtrStatus Frame : Frames)
                {
                    if (sh.POC > Frame.POC && Frame.isLTR) //current frame is next to frame, marked as LTR
                    {
                        if (m_par.mfx.IdrInterval == 0 || 0 != sh.POC % m_par.mfx.GopPicSize) //check that this is not IDR
                        {
                            nLTR++;
                        }
                    }
                }

                if (nLTR)
                {
                    EXPECT_EQ(nLTR, sh.num_long_term_pics);
                }
                else
                {
                    EXPECT_EQ(0, sh.num_long_term_pics);
                }
            }
        }
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }


    mfxFrameSurface1* TestSuite::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {       
        if (ps->Data.FrameOrder == m_tc.rl[m_i].poc)
        {
            m_ctrl.NumExtParam = 1;
            m_pbuf = &m_RefList.Header;

            for (auto& f : m_tc.rl[m_i].f)
                if (f.field)
                    tsStruct::set(m_pbuf, *f.field, f.value);
            m_i++;
        }

        m_ctrl.ExtParam = &m_pbuf;
        ps->Data.FrameOrder++;
        return ps;
    }

#define MFX_PARS(ps,gps,grd,nrf,idr) {\
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, ps},  \
            {&tsStruct::mfxVideoParam.mfx.GopPicSize,          gps}, \
            {&tsStruct::mfxVideoParam.mfx.GopRefDist,          grd}, \
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame,         nrf}, \
            {&tsStruct::mfxVideoParam.mfx.IdrInterval,         idr}}

#define LTR(idx,apl,fo,ps,lti) {\
            { &tsStruct::mfxExtAVCRefListCtrl.ApplyLongTermIdx, apl },\
            { &tsStruct::mfxExtAVCRefListCtrl.LongTermRefList[idx].FrameOrder, fo },\
            { &tsStruct::mfxExtAVCRefListCtrl.LongTermRefList[idx].PicStruct, ps },\
            { &tsStruct::mfxExtAVCRefListCtrl.LongTermRefList[idx].LongTermIdx, lti } }

#define RRL(idx,fo,ps,lti) {\
            { &tsStruct::mfxExtAVCRefListCtrl.RejectedRefList[idx].FrameOrder, fo },\
            { &tsStruct::mfxExtAVCRefListCtrl.RejectedRefList[idx].PicStruct, ps },\
            { &tsStruct::mfxExtAVCRefListCtrl.RejectedRefList[idx].LongTermIdx, lti } }

#define PROGR MFX_PICSTRUCT_PROGRESSIVE

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/
            15,
            MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 10, /*GopRefDist*/ 1, /*NumRefFrame*/ 3, /*IdrInterval*/ 1),
            {/*POC*/ 4, LTR(/*LTRLidx*/ 0, /*ApplyLTidx*/ 1, /*FrameOrder*/ 4, /*PicStruct*/ PROGR, /*LTidx*/ 0) }
        },

        {/*01*/
            15,
            MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 10, /*GopRefDist*/ 1, /*NumRefFrame*/ 3, /*IdrInterval*/ 0),
            {
                {/*POC*/ 3, LTR(/*LTRLidx*/ 0, /*ApplyLTidx*/ 1, /*FrameOrder*/ 3, /*PicStruct*/ PROGR, /*LTidx*/ 0) },
                {/*POC*/ 7, RRL(/*RJFLidx*/ 0, /*FrameOrder*/ 3, /*PicStruct*/ PROGR, /*LTidx*/ 0) }
            }
        },

        {/*02*/
            15,
            MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 10, /*GopRefDist*/ 1, /*NumRefFrame*/ 3, /*IdrInterval*/ 1),
            {
                {/*POC*/ 4, LTR(/*LTRLidx*/ 0, /*ApplyLTidx*/ 1, /*FrameOrder*/ 4, /*PicStruct*/ PROGR, /*LTidx*/ 0) },
                {/*POC*/ 7, LTR(/*LTRLidx*/ 1, /*ApplyLTidx*/ 1, /*FrameOrder*/ 7, /*PicStruct*/ PROGR, /*LTidx*/ 0) }
            }
        },

        {/*03*/
            15,
            MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 10, /*GopRefDist*/ 1, /*NumRefFrame*/ 3, /*IdrInterval*/ 0),
            {
                {/*POC*/ 3, LTR(/*LTRLidx*/ 0, /*ApplyLTidx*/ 1, /*FrameOrder*/ 3, /*PicStruct*/ PROGR, /*LTidx*/ 0) },
                {/*POC*/ 5, LTR(/*LTRLidx*/ 1, /*ApplyLTidx*/ 1, /*FrameOrder*/ 5, /*PicStruct*/ PROGR, /*LTidx*/ 0) },
                {/*POC*/ 7, RRL(/*RJFLidx*/ 0, /*FrameOrder*/ 3, /*PicStruct*/ PROGR, /*LTidx*/ 0) } //on 7th frame remove LTR for 3rd frame
            }
        },

        {/*04*/
            15,
            MFX_PARS(/*PicStruct*/ PROGR, /*GopPicSize*/ 10, /*GopRefDist*/ 1, /*NumRefFrame*/ 3, /*IdrInterval*/ 0),
            {
                {/*POC*/ 3, LTR(/*LTRLidx*/ 0, /*ApplyLTidx*/ 1, /*FrameOrder*/ 3, /*PicStruct*/ PROGR, /*LTidx*/ 0) },
                {/*POC*/ 5, LTR(/*LTRLidx*/ 1, /*ApplyLTidx*/ 1, /*FrameOrder*/ 5, /*PicStruct*/ PROGR, /*LTidx*/ 0) },
                {/*POC*/ 7, RRL(/*RJFLidx*/ 0, /*FrameOrder*/ 5, /*PicStruct*/ PROGR, /*LTidx*/ 0) } //on 7th frame remove LTR for 5th frame
            }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        auto& tc = test_case[id];

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }

        if (g_tsImpl == MFX_IMPL_HARDWARE) {
            if (g_tsHWtype < MFX_HW_CNL) {
                g_tsLog << "SKIPPED for this platform\n";
                return 0;
            }
        }

        mfxStatus sts = MFX_ERR_NONE;

        mfxExtAVCRefListCtrl& rlc = m_par;
        m_RefList = rlc;

        MFXInit();
        set_brc_params(&m_par);
        Load();

        m_par.AsyncDepth   = 1;
        m_par.mfx.NumSlice = 1;
        m_par.mfx.FrameInfo.Width =  m_par.mfx.FrameInfo.CropW = FRAME_WIDTH;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = FRAME_HEIGHT;

        for (auto& ctrl : tc.mfx)
            if (ctrl.field)
                tsStruct::set(m_pPar, *ctrl.field, ctrl.value);

        m_tc = tc;

        EncodeFrames(tc.nFrames);

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_lterm_reflist);
}