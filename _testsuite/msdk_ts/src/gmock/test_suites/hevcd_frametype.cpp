/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_frametype
{

class TestSuite : tsVideoDecoder
{
    mfxU16                     frame;
    tsExtBufType<mfxFrameData> frame_type;
    std::list <mfxU16>         expected;

public:

    static const mfxU32 max_frames = 20;

    struct tc_struct
    {
        mfxStatus sts;
        std::string stream;

        mfxU16 n_frames;
        mfxU16 frame_type[max_frames];

    };

public:

    TestSuite()
        : tsVideoDecoder(MFX_CODEC_HEVC)
        , frame(0)
    {
        frame_type.AddExtBuffer(MFX_EXTBUFF_DECODED_FRAME_INFO, sizeof(mfxExtDecodedFrameInfo));
    }

    int RunTest(tc_struct const&);
    static const unsigned int n_cases;

    mfxStatus DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        surface_work->Data.NumExtParam = frame_type.NumExtParam;
        surface_work->Data.ExtParam    = frame_type.ExtParam;

        mfxStatus sts =
            tsVideoDecoder::DecodeFrameAsync(session, bs, surface_work, surface_out, syncp);

        if (sts == MFX_ERR_NONE && *surface_out)
        {
            sts = verify(**surface_out);
            g_tsStatus.check(sts);
        }

        return sts;
    }

private:

    mfxStatus verify(mfxFrameSurface1 const& s)
    {
        if (s.Data.NumExtParam != 1)
        {
            g_tsLog << "ERROR: Wrong external buffer count\n";
            return MFX_ERR_NOT_INITIALIZED;
        }

        if (s.Data.ExtParam[0] == NULL)
        {
            g_tsLog << "ERROR: null pointer ExtParam\n";
            return MFX_ERR_NULL_PTR;
        }

        if (s.Data.ExtParam[0]->BufferId != MFX_EXTBUFF_DECODED_FRAME_INFO ||
            s.Data.ExtParam[0]->BufferSz == 0)
        {
            g_tsLog << "ERROR: Wrong external buffer or zero size buffer\n";
            return MFX_ERR_NOT_INITIALIZED;
        }

        mfxU16 const frame_type =
            reinterpret_cast<mfxExtDecodedFrameInfo*>(s.Data.ExtParam[0])->FrameType;

        if (expected.empty())
            return MFX_ERR_NONE;

        if (frame_type != expected.front())
        {
            g_tsLog << "ERROR: Frame#" << frame << " actual type = " << frame_type  << " is not equal expected = " << expected.front() << "\n";

            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        expected.pop_front();
        frame++;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(tc_struct const& tc)
{
    TS_START;

    expected.assign(tc.frame_type, tc.frame_type + tc.n_frames);

    MFXInit();

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    Init();
    AllocSurfaces();

    DecodeFrames(tc.n_frames);

    g_tsStatus.expect(tc.sts);

    TS_END;
    return 0;
}

template <unsigned fourcc>
TestSuite::tc_struct const& query_case(unsigned int, std::integral_constant<unsigned, fourcc>);

/* 8 bit */
TestSuite::tc_struct const& query_case(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
{
    static TestSuite::tc_struct const cases[] =
    {
        /*01*/ MFX_ERR_NONE, "conformance/hevc/self_coded/stockholm_1280x720_604.x265",
        11,
        {
            /*  0*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  1*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /*  2*/ MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  3*/ MFX_FRAMETYPE_B,
            /*  4*/ MFX_FRAMETYPE_B,
            /*  5*/ MFX_FRAMETYPE_B,
            /*  6*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  7*/ MFX_FRAMETYPE_B,
            /*  8*/ MFX_FRAMETYPE_B,
            /*  9*/ MFX_FRAMETYPE_B,
            /* 10*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF
       },
    };

    assert(!(id > 1));
    return cases[id];
}

TestSuite::tc_struct const& query_case(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_YUY2>)
{
    static TestSuite::tc_struct const cases[] =
    {
        MFX_ERR_NONE, "forBehaviorTest/picturetype/inter422_8b_1920x1080.h265",
        20,
        {
            /*  0*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  1*/ MFX_FRAMETYPE_B,
            /*  2*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  3*/ MFX_FRAMETYPE_B,
            /*  4*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  5*/ MFX_FRAMETYPE_B,
            /*  6*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  7*/ MFX_FRAMETYPE_B,
            /*  8*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  9*/ MFX_FRAMETYPE_I,
            /* 10*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /* 11*/ MFX_FRAMETYPE_B,
            /* 12*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 13*/ MFX_FRAMETYPE_B,
            /* 14*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 15*/ MFX_FRAMETYPE_B,
            /* 16*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 17*/ MFX_FRAMETYPE_B,
            /* 18*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 19*/ MFX_FRAMETYPE_I,
        }
    };

    assert(!(id > 1));
    return cases[id];
}

TestSuite::tc_struct const& query_case(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_AYUV>)
{
    static TestSuite::tc_struct const cases[] =
    {
        MFX_ERR_NONE, "forBehaviorTest/picturetype/CCP_8bit_RExt_QCOM_2g.h265",
        18,
        {
            /*  0*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  1*/ MFX_FRAMETYPE_B,
            /*  2*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  3*/ MFX_FRAMETYPE_B,
            /*  4*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  5*/ MFX_FRAMETYPE_B,
            /*  6*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  7*/ MFX_FRAMETYPE_B,
            /*  8*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  9*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /* 10*/ MFX_FRAMETYPE_B,
            /* 11*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 12*/ MFX_FRAMETYPE_B,
            /* 13*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 14*/ MFX_FRAMETYPE_B,
            /* 15*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 16*/ MFX_FRAMETYPE_B,
            /* 17*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
        }
    };

    assert(!(id > 1));
    return cases[id];
}

/* 10 bit */
TestSuite::tc_struct const& query_case(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_P010>)
{
    static TestSuite::tc_struct const cases[] =
    {
        MFX_ERR_NONE, "forBehaviorTest/picturetype/sbe_main10_448x256.h265 ",
        14,
        {
            /*  0*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  1*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /*  2*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /*  3*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  4*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  5*/ MFX_FRAMETYPE_B,
            /*  6*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /*  7*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  8*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /*  9*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /* 10*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 11*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /* 12*/ MFX_FRAMETYPE_B,
            /* 13*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF
        }
    };

    assert(!(id > 1));
    return cases[id];
}

TestSuite::tc_struct const& query_case(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_Y216>)
{
    static TestSuite::tc_struct const cases[] =
    {
        MFX_ERR_NONE, "forBehaviorTest/picturetype/sbe_rext_422_10b_448x256.h265",
        10,
        {
            /*  0*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  1*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  2*/ MFX_FRAMETYPE_B,
            /*  3*/ MFX_FRAMETYPE_B,
            /*  4*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /*  5*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  6*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  7*/ MFX_FRAMETYPE_B,
            /*  8*/ MFX_FRAMETYPE_B,
            /*  9*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
        }
    };

    assert(!(id > 1));
    return cases[id];
}

TestSuite::tc_struct const& query_case(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_Y410>)
{
    static TestSuite::tc_struct const cases[] =
    {
        MFX_ERR_NONE, "forBehaviorTest/picturetype/sbe_rext_444_10b_448x256.h265",
        10,
        {
            /*  0*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  1*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  2*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  3*/ MFX_FRAMETYPE_B,
            /*  4*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
            /*  5*/ MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF,
            /*  6*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  7*/ MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF,
            /*  8*/ MFX_FRAMETYPE_B,
            /*  9*/ MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF,
        }
    };

    assert(!(id > 1));
    return cases[id];
}

template <unsigned fourcc>
struct TestSuiteEx
    : public TestSuite
{
    static
    int RunTest(unsigned int id)
    {
        auto tc = query_case(id, std::integral_constant<unsigned, fourcc>());

        TestSuite suite;
        return suite.RunTest(tc);
    }
};

TS_REG_TEST_SUITE(hevcd_frametype,     TestSuiteEx<MFX_FOURCC_NV12>::RunTest, 1);
TS_REG_TEST_SUITE(hevcd_422_frametype, TestSuiteEx<MFX_FOURCC_YUY2>::RunTest, 1);
TS_REG_TEST_SUITE(hevcd_444_frametype, TestSuiteEx<MFX_FOURCC_AYUV>::RunTest, 1);

TS_REG_TEST_SUITE(hevc10d_frametype,     TestSuiteEx<MFX_FOURCC_P010>::RunTest, 1);
TS_REG_TEST_SUITE(hevc10d_422_frametype, TestSuiteEx<MFX_FOURCC_Y216>::RunTest, 1);
TS_REG_TEST_SUITE(hevc10d_444_frametype, TestSuiteEx<MFX_FOURCC_Y410>::RunTest, 1);

}
