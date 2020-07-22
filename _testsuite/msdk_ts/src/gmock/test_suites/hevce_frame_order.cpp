/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright (c) 2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

//#define DUMP_BS
#define FRAME_TO_ENCODE 40

namespace hevce_frame_order
{
    enum
    {
        DEFAULT = 0,
        RESET,
    };

    class TestSuite
        : public tsBitstreamProcessor
        , public tsSurfaceProcessor
        , public tsParserHEVC
        , public tsVideoEncoder
    {
    private:
#ifdef DUMP_BS
        tsBitstreamWriter m_writer;
#endif
    public:
        struct tc_struct
        {
            mfxU16 type;
            mfxU16 GopRefDist;
            mfxU16 BRefType;
        };

        static const tc_struct test_case[];
        static const unsigned int n_cases;
        mfxU32 m_frameCnt;

        mfxU32 m_encoded;
        mfxU32 m_frameoffset;

        TestSuite()
            : tsParserHEVC()
            , tsVideoEncoder(MFX_CODEC_HEVC)
#ifdef DUMP_BS
            , m_writer("debug.265")
#endif
            , m_frameCnt(0)
            , m_encoded(0)
            , m_frameoffset(0)
        {
            m_bs_processor = this;
            m_filler = this;
        }

        ~TestSuite() {}

        mfxI32 RunTest(mfxU32 id);

        mfxStatus ProcessSurface(mfxFrameSurface1& s);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

        void Init()
        {
            GetVideoParam();
        }

        mfxStatus checkFrameOrder(mfxU32 fo, mfxU32 en)
        {
            mfxExtCodingOption3& co3 = m_par;
            mfxExtCodingOption2& co2 = m_par;
            if (en == m_frameoffset) return MFX_ERR_NONE;
            else if (m_par.mfx.GopRefDist == 3)
            {
                //pattern: {0,3,1,2,6,4,5,...}
                if (co2.BRefType == 1)
                    switch ((en - m_frameoffset) % 3)
                    {
                    case 1: en += 2; break;
                    case 2: en--; break;
                    case 0:
                        m_frameoffset = en;
                        en--; break;
                    }
                //pattern: {0,3,2,1,6,5,4,...}
                else
                {
                    switch ((en - m_frameoffset) % 3)
                    {
                    case 1: en += 2; break;
                    case 2: break;
                    case 0: m_frameoffset = en; en -= 2; break;
                    }
                }
            }
            EXPECT_EQ(en, fo);
            return MFX_ERR_NONE;
        }

        mfxStatus EncodeFrames(mfxU32 n)
        {
            mfxU32 submitted = 0;
            mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
            mfxSyncPoint sp;

            async = TS_MIN(n, async - 1);

            mfxExtAVCEncodedFrameInfo& avcFrameInfo = m_bitstream;
            avcFrameInfo.FrameOrder = mfxU32(~0);
            avcFrameInfo.SecondFieldOffset = mfxU32(~0);

            while (m_encoded < n)
            {
                mfxStatus sts = EncodeFrameAsync();

                if (sts == MFX_ERR_MORE_DATA)
                {
                    if (!m_pSurf)
                    {
                        if (submitted)
                        {
                            SyncOperation(sp);
                            m_encoded += submitted;
                        }
                        break;
                    }

                    continue;
                }

                g_tsStatus.check(); TS_CHECK_MFX;
                sp = m_syncpoint;

                if (++submitted >= async)
                {
                    SyncOperation(); TS_CHECK_MFX;

                    m_encoded += submitted;
                    submitted = 0;
                    async = TS_MIN(async, (n - m_encoded));
                }

                if (avcFrameInfo.FrameOrder == mfxU32(~0) && avcFrameInfo.SecondFieldOffset == mfxU32(~0))
                {
                    g_tsLog << "ERROR >> FrameInfo buffer data haven't changed\n";
                    throw tsFAIL;
                }
                g_tsLog << "Received Frame order: " << avcFrameInfo.FrameOrder << "\n";
                checkFrameOrder(avcFrameInfo.FrameOrder, m_encoded - 1);
            }

            g_tsLog << m_encoded << " FRAMES ENCODED\n";

            if (m_encoded != n)
                return MFX_ERR_UNKNOWN;

            return g_tsStatus.get();
        }
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /*check if frame order is reported correctly*/
        /*     type       GopRefDist   BRefType*/
        /*0*/{DEFAULT,          1,          1},
        /*1*/{DEFAULT,          3,          1},
        /*2*/{DEFAULT,          3,          2},
        /*3*/{RESET,            1,          1},
        /*4*/{RESET,            3,          1},
        /*5*/{RESET,            3,          2},

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);


    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
    {
        s.Data.FrameOrder = m_frameCnt++;
        return MFX_ERR_NONE;
    }

    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer0(bs);

#ifdef DUMP_BS
        m_writer.ProcessBitstream(bs, nFrames);
#endif

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    mfxI32 TestSuite::RunTest(mfxU32 id)
    {
        mfxStatus sts = MFX_ERR_NONE;
        const tc_struct& tc = test_case[id];

        TS_START;

        if (g_tsOSFamily == MFX_OS_FAMILY_LINUX)
        {
            g_tsLog << "SKIPPED: this test is for Windows only.\n";
            throw tsSKIP;
        }

        MFXInit();

        mfxExtCodingOption2& co2 = m_par;
        co2.BRefType = tc.BRefType;

        mfxExtCodingOption3& co3 = m_par;
        co3.PRefType = 0;
        co3.GPB = MFX_CODINGOPTION_OFF;
        co3.EnableNalUnitType = MFX_CODINGOPTION_OFF;

        m_par.mfx.FrameInfo.PicStruct = 1;
        m_par.mfx.GopRefDist = tc.GopRefDist;
        m_par.mfx.GopOptFlag = 0;
        m_par.AsyncDepth = 1;
        m_par.mfx.GopPicSize = 15;

        m_bitstream.AddExtBuffer<mfxExtAVCEncodedFrameInfo>(0);

        Init();
        GetVideoParam();
        AllocBitstream();
        AllocSurfaces();

        EncodeFrames(FRAME_TO_ENCODE);
        if (tc.type == RESET)
        {
            //frame order shouldn't be set to zero
            m_par.mfx.GopPicSize = 9;
            Reset();
            m_encoded = m_frameoffset = m_frameCnt;
            EncodeFrames(FRAME_TO_ENCODE * 2);
        }
        Close();
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_frame_order, RunTest, n_cases);
}