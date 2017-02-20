//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2017 Intel Corporation. All Rights Reserved.
//

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace hevce_p_frame
{
    class TestSuite : tsVideoEncoder, tsParserHEVCAU, tsSurfaceProcessor, tsBitstreamProcessor
    {
    public:
        static const unsigned int n_cases;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , framesToEncode(1)
            , pframeCount(0)
        {
            srand(0);
            m_filler = this;
            m_bs_processor = this;
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:

        mfxU32 framesToEncode;
        mfxU32 pframeCount;

        enum
        {
            MFX_PAR = 1,
            EXT_COD3,
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 nframes;
            bool existPFrame;
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
                if (S.type == 1)
                    ++pframeCount;
            }

            bs.DataLength = 0;
            return MFX_ERR_NONE;
        }

    };


    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, 30, false, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15},
                                  {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
                                  {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB, 0x10}}},
        {/*02*/ MFX_ERR_NONE, 30, true, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15}, 
                                  {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
                                  {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB, 0x20}}},
        {/*03*/ MFX_ERR_NONE, 30, false, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 15},
                                  {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
                                  {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB, 0}}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        pframeCount = 0;

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);
        mfxExtCodingOption3& cod3 = m_par;
        SETPARS(&cod3, EXT_COD3);
        m_par.AsyncDepth = 1;
        framesToEncode = tc.nframes;
        m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
        m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
        g_tsStatus.expect(tc.sts);
        Init();
        EncodeFrames(framesToEncode);
        EXPECT_EQ(!!pframeCount, tc.existPFrame);
        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_p_frame);
};
