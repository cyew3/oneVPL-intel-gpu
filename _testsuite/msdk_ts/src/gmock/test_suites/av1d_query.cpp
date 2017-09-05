/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace av1d_query
{

class TestSuite : public tsVideoDecoder
{
    static const mfxU32 max_num_ctrl     = 5;
    static const mfxU32 max_num_ctrl_par = 4;

protected:

    struct tc_struct
    {
        mfxStatus sts;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
            HWType platform; //on which platform [type == VARIED] matters
        } ctrl[max_num_ctrl];
    };

public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AV1){}
    ~TestSuite() { }

    int RunTest(tc_struct const&);
    static const unsigned int n_cases;

protected:

    mfxSession m_session;

    static const tc_struct test_case[];

    enum CTRL_TYPE
    {
          CHECK   = 0xFF000000
        , VALID   = 0x01000000
        , INVALID = 0x02000000
        , WRN     = 0x04000000
        , VARIED  = VALID | INVALID
        , SESSION = 1
        , MFX_IN
        , MFX_OUT
        , NATIVE
    };

    void apply_par(const tc_struct& p)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;


            switch(c.type & ~CHECK)
            {
            case SESSION   : base = (void**)&m_session;   break;
            case MFX_IN    : base = (void**)&m_pPar;      break;
            case MFX_OUT   : base = (void**)&m_pParOut;   break;
            case NATIVE    : m_uid = 0; break;
            default: break;
            }

            if(base)
            {
                if(c.field)
                    tsStruct::set(*base, *c.field, c.par[0]);
                else
                    //no way to have persistent pointers here, the only valid value is NULL
                    *base = NULL;
            }
        }
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, },
    {/* 1*/ MFX_ERR_NONE, {MFX_IN}},
    {/* 2*/ MFX_ERR_INVALID_HANDLE, {SESSION}},
    {/* 3*/ MFX_ERR_NULL_PTR, {MFX_OUT}},
    {/* 4*/ MFX_ERR_NONE, {MFX_IN, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/* 5*/ MFX_ERR_NONE, {MFX_IN, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}},
    {/* 6*/ MFX_ERR_NONE, {MFX_IN|VALID, &tsStruct::mfxVideoParam.AsyncDepth, {10}}},
    {/* 7*/ MFX_ERR_NONE, {MFX_IN|VALID, &tsStruct::mfxVideoParam.AsyncDepth, {1}}},
    {/* 8*/ MFX_ERR_NONE, {MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.NumThread, {4}}},
    {/* 9*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YV12}}},
    {/*10*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {0x11111111}}},
    {/*11*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, {721}}},
    {/*12*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {577}}},
    {/*13*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {255}}},
    {/*14*/ MFX_ERR_UNSUPPORTED,
       {{MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  {4106}},
        {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {4106}}}
    },
    {/*15*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.DecodedOrder, {2}}},
    {/*16*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, {MFX_PICSTRUCT_FIELD_TFF}}},
    {/*17*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, {MFX_PICSTRUCT_FIELD_BFF}}},
    {/*18*/ MFX_ERR_NONE, {MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, {MFX_PICSTRUCT_FIELD_SINGLE}}},
    {/*19*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.ExtendedPicStruct, {2}}},
    {/*20*/ MFX_ERR_UNSUPPORTED, {MFX_IN|INVALID, &tsStruct::mfxVideoParam.Protected, {16}}},
    {/*21*/ MFX_ERR_UNSUPPORTED, {NATIVE}},

    {/* 22*/ MFX_ERR_UNSUPPORTED,
        { {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, {1}},
          {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, {1}} }
    },

    {/* 23*/ MFX_ERR_UNSUPPORTED,
        { {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, {42}},
          {MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, {42}} }
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(tc_struct const& tc)
{
    TS_START;

    g_tsStreamPool.Reg();
    tsExtBufType<mfxVideoParam> out_par;
    out_par.mfx.CodecId = m_par.mfx.CodecId;
    m_pParOut = &out_par;

    MFXInit();
    m_session = tsSession::m_session;

    apply_par(tc);

    if(m_uid)
    {
        Load();
    }

    g_tsStatus.expect(tc.sts);

    Query(m_session, m_pPar, m_pParOut);

    for(mfxU32 i = 0; i < max_num_ctrl; i ++)
    {
        auto c = tc.ctrl[i];
        if (c.type == (MFX_IN | VARIED))
            c.type = g_tsHWtype < c.platform ? (MFX_IN | INVALID) : (MFX_IN | VALID);

        if (c.type == (MFX_IN|INVALID) && c.field)
        {
            tsStruct::check_eq(m_pParOut, *c.field, 0);
        }

        if (c.type == (MFX_IN|VALID) && c.field)
        {
            tsStruct::check_eq(m_pParOut, *c.field, tsStruct::get(m_pPar, *c.field));
        }

        if (c.type == (MFX_IN|WRN) && c.field)
        {
            tsStruct::check_ne(m_pParOut, *c.field, tsStruct::get(m_pPar, *c.field));
        }
    }

    if(tc.sts == MFX_ERR_NONE && !m_pPar)
    {
        EXPECT_NE(out_par.AsyncDepth, 0);
        EXPECT_NE(out_par.IOPattern, 0);
        EXPECT_NE(out_par.mfx.CodecId, (mfxU32)0);
        EXPECT_NE(out_par.mfx.CodecProfile, 0);
        EXPECT_NE(out_par.mfx.CodecLevel, 0);
        EXPECT_NE(out_par.mfx.ExtendedPicStruct, 0);
        EXPECT_NE(out_par.mfx.FrameInfo.FourCC, (mfxU32)0);
        EXPECT_NE(out_par.mfx.FrameInfo.Width, 0);
        EXPECT_NE(out_par.mfx.FrameInfo.Height, 0);
        EXPECT_NE(out_par.mfx.FrameInfo.FrameRateExtN, (mfxU32)0);
        EXPECT_NE(out_par.mfx.FrameInfo.FrameRateExtD, (mfxU32)0);
        EXPECT_NE(out_par.mfx.FrameInfo.AspectRatioW, 0);
        EXPECT_NE(out_par.mfx.FrameInfo.AspectRatioH, 0);
        EXPECT_NE(out_par.mfx.FrameInfo.PicStruct, 0);
        EXPECT_NE(out_par.mfx.FrameInfo.ChromaFormat, 0);
    }

    TS_END;
    return 0;
}

template <unsigned fourcc, unsigned profile>
struct TestSuiteExt
    : public TestSuite
{
    static
    int RunTest(unsigned int id);

    static tc_struct const test_cases[];
    static unsigned int const n_cases;
};

/* 8b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::test_cases[] =
{
    {/*24*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV420} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_NV12} } }
    },

    {/*25*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV420} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_NV12} } }
    },

    {/*26*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV420} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, {8} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, {8} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_NV12} } }
    },

    {/*27*/ MFX_ERR_UNSUPPORTED,
        { { MFX_IN|VARIED, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, { MFX_CHROMAFORMAT_YUV422 } },
          { MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC,{ MFX_FOURCC_NV12 } } },
    },

    {/*28*/ MFX_ERR_UNSUPPORTED,
        { { MFX_IN|VARIED, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, { MFX_CHROMAFORMAT_YUV444 } },
          { MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC,{ MFX_FOURCC_NV12 } } },
    },
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::test_cases) / sizeof(TestSuite::tc_struct);

/* 8b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_YUY2, MFX_PROFILE_AV1_0>::test_cases[] =
{
    {/*24*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV422} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YUY2} } }
    },

    {/*25*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV422} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YUY2} } }
    },

    {/*26*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV422} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, {8} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, {8} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YUY2} } }
    },

    {/*28*/ MFX_ERR_UNSUPPORTED,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV420} },
          { MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YUY2} } }
    },

    {/*29*/ MFX_ERR_UNSUPPORTED,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV444} },
          { MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YUY2} } }
    },
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_YUY2, MFX_PROFILE_AV1_0>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_YUY2, MFX_PROFILE_AV1_0>::test_cases) / sizeof(TestSuite::tc_struct);

/* 8b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_AYUV, MFX_PROFILE_AV1_0>::test_cases[] =
{
    {/*24*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV444} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_AYUV} } }
    },

    {/*25*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV444} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_AYUV} } }
    },

    {/*26*/ MFX_ERR_NONE,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV444} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, {8} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, {8} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_AYUV} } }
    },

    {/*27*/ MFX_ERR_UNSUPPORTED,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV420} },
          { MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_AYUV} } }
    },

    {/*28*/ MFX_ERR_UNSUPPORTED,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV422} },
          { MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_AYUV} } }
    },

    {/*29*/ MFX_ERR_UNSUPPORTED,
        { { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.CodecProfile, {MFX_PROFILE_AV1_0} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV444} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, {10} },
          { MFX_IN|VALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, {10} },
          { MFX_IN|INVALID, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_AYUV} } }
    },
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_AYUV, MFX_PROFILE_AV1_0>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_AYUV, MFX_PROFILE_AV1_0>::test_cases) / sizeof(TestSuite::tc_struct);

template <unsigned fourcc, unsigned profile>
int TestSuiteExt<fourcc, profile>::RunTest(unsigned int id)
{
    auto tc =
        id >= TestSuite::n_cases ?
        test_cases[id - TestSuite::n_cases] : TestSuite::test_case[id];

    TestSuite suite;
    return suite.RunTest(tc);
}

TS_REG_TEST_SUITE(av1d_420_nv12_query,   (TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::RunTest), (TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::n_cases));
TS_REG_TEST_SUITE(av1d_422_yuy2_query,   (TestSuiteExt<MFX_FOURCC_YUY2, MFX_PROFILE_AV1_0>::RunTest), (TestSuiteExt<MFX_FOURCC_YUY2, MFX_PROFILE_AV1_0>::n_cases));
TS_REG_TEST_SUITE(av1d_444_ayuv_query,   (TestSuiteExt<MFX_FOURCC_AYUV, MFX_PROFILE_AV1_0>::RunTest), (TestSuiteExt<MFX_FOURCC_AYUV, MFX_PROFILE_AV1_0>::n_cases));

}
