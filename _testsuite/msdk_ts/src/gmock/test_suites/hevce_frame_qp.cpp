//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace hevce_frame_qp
{
    class TestSuite : tsVideoEncoder, tsParserHEVCAU, tsSurfaceProcessor, tsBitstreamProcessor
    {
    public:
        static const unsigned int n_cases;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , frames(0)
            , encoded(0)
            , framesToEncode(1)
        {
            srand(0);
            m_filler = this;
            m_bs_processor = this;
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:

        mfxU32 frames;
        mfxU32 encoded;
        mfxU32 framesToEncode;

        std::vector<mfxU16> qp;

        enum
        {
            MFX_PAR = 1,
            RESET,
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

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            //if (frames < framesToEncode)
            {
                m_pCtrl->QP = 1 + rand() % 50;
                qp.push_back(m_pCtrl->QP);

                s.Data.TimeStamp = s.Data.FrameOrder = frames++;
            }
            return MFX_ERR_NONE;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            mfxU32 checked = 0;

            if (bs.Data)
                set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

            while (checked++ < nFrames)
            {
                auto& AU = ParseOrDie();
                auto& S = AU.pic->slice[0]->slice[0];

                if (bs.TimeStamp < framesToEncode)
                {
                    mfxI16 QP  = S.slice_qp_delta + S.pps->init_qp_minus26 + 26;
                    EXPECT_EQ(qp[bs.TimeStamp], QP) << "ERROR: Frame's QP is not equal to mfxEncodeCtrl's per frame QP\n";
                    encoded++;
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

    };


    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_PAR, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  1}},
        {/*02*/ MFX_ERR_NONE, MFX_PAR, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15}},
        {/*03*/ MFX_ERR_NONE, RESET,   {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);

        m_par.AsyncDepth = 1;
        framesToEncode = 5;

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }

            //HEVCE_HW need aligned width and height for 32
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
        }

        Init();

        EncodeFrames(framesToEncode);

        if (tc.mode == RESET)
        {
            Reset();
            qp.clear();
            encoded = 0;
            frames = 0;

            EncodeFrames(framesToEncode);
        }

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_frame_qp);
};
