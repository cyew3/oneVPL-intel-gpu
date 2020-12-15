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

namespace vp9e_frame_order
{
    enum
    {
        DEFAULT = 0,
        RESET,
    };

    class TestSuite
        : public tsBitstreamProcessor
        , public tsSurfaceProcessor
        , public tsParserVP9
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
        };

        static const tc_struct test_case[];
        static const unsigned int n_cases;
        mfxU32 m_frameCnt;

        mfxU32 m_encoded;
        mfxU32 m_frameoffset;

        TestSuite()
            : tsParserVP9()
            , tsVideoEncoder(MFX_CODEC_VP9)
#ifdef DUMP_BS
            , m_writer("debug.vp9")
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
            if (en == fo) return MFX_ERR_NONE;

            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        mfxStatus EncodeFrames(mfxU32 n)
        {
            mfxU32 submitted = 0;
            mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
            mfxSyncPoint sp;

            async = TS_MIN(n, async - 1);

            mfxExtAVCEncodedFrameInfo& avcFrameInfo = m_bitstream;
            avcFrameInfo.FrameOrder = mfxU32(~0);

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

                if (avcFrameInfo.FrameOrder == mfxU32(~0))
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
        /*     type     */
        /*0*/{DEFAULT},
        /*1*/{RESET},
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

        m_par.mfx.FrameInfo.PicStruct = 1;
        m_par.mfx.GopOptFlag = 0;
        m_par.AsyncDepth = 1;
        m_par.mfx.GopPicSize = 3;

        m_bitstream.AddExtBuffer<mfxExtAVCEncodedFrameInfo>();

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

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_frame_order, RunTest, n_cases);
}