/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <memory>
#include "ts_decoder.h"
#include "ts_struct.h"

namespace av1d_decode_frame_async
{
    class TestSuite
        : public tsVideoDecoder
    {

    public:

        TestSuite() : tsVideoDecoder(MFX_CODEC_AV1) { }
        ~TestSuite() {}

        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU64 v;
            //bool   isInvalid;
            mfxU8  stage;
        };
        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 mode;
            mfxU32 n_calls;
            f_pair set_par[MAX_NPARS];
        };

        struct stream_desc
        {
            mfxU16      width;
            mfxU16      height;
            const char* name;
        };

        int RunTest(const tc_struct& tc, stream_desc const&);

    protected:

        static const tc_struct test_case[];      //base cases
        static const unsigned int n_cases;

    protected:

        enum
        {
            NULL_SESSION = 1,
            NULL_BS,
            NULL_SURF,
            NULL_SURF_OUT,
            NULL_SYNCP,
            NOT_INITIALIZED,
            FAILED_INIT,
            CLOSED_DECODER,
            NO_EXT_ALLOCATOR,
        };

        enum
        {
            INIT = 1,
            AFTER_INIT,
            RUNTIME_BS,
            RUNTIME_SURF
        };

        void AllocOpaque();
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, 0, 1},
        {/*01*/ MFX_ERR_NONE, NO_EXT_ALLOCATOR, 20},

        {/*02*/ MFX_ERR_NONE, 0, 20, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY, INIT} }, //opaque is not supported by decoder now
        {/*03*/ MFX_ERR_NONE, 0, 20, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, INIT} },
        {/*04*/ MFX_ERR_NONE, 0, 20, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, INIT} },

        {/*05*/ MFX_ERR_INVALID_HANDLE,  NULL_SESSION   , 1},
        {/*06*/ MFX_ERR_MORE_DATA,       NULL_BS        , 1},
        {/*07*/ MFX_ERR_NULL_PTR,        NULL_SURF      , 1},
        {/*08*/ MFX_ERR_NULL_PTR,        NULL_SURF_OUT  , 1},
        {/*09*/ MFX_ERR_NULL_PTR,        NULL_SYNCP     , 1},
        {/*10*/ MFX_ERR_NOT_INITIALIZED, NOT_INITIALIZED, 1},
        {/*11*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT    , 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 777, INIT}},
        {/*12*/ MFX_ERR_NOT_INITIALIZED, CLOSED_DECODER , 1},

        {/*13*/ MFX_ERR_NONE, 0, 1, {&tsStruct::mfxBitstream.DataFlag, 0, RUNTIME_BS} },
        {/*14*/ MFX_ERR_NONE, 0, 1, {&tsStruct::mfxBitstream.DataFlag, MFX_BITSTREAM_COMPLETE_FRAME, RUNTIME_BS} },
        {/*15*/ MFX_ERR_NULL_PTR,  0, 1, {&tsStruct::mfxBitstream.Data, 0, RUNTIME_BS} },
        {/*16*/ MFX_ERR_NULL_PTR,  0, 1, { {&tsStruct::mfxBitstream.Data, 0, RUNTIME_BS},
                                           {&tsStruct::mfxBitstream.DataLength, 0, RUNTIME_BS} ,
                                           {&tsStruct::mfxBitstream.MaxLength, 0,  RUNTIME_BS},
                                         } },
        {/*17*/ MFX_ERR_MORE_DATA, 0, 1, {&tsStruct::mfxBitstream.DataLength, 0, RUNTIME_BS} },
        {/*18*/ MFX_ERR_MORE_DATA, 0, 1, {&tsStruct::mfxBitstream.DataLength, 10, RUNTIME_BS} },
        {/*19*/ MFX_ERR_UNDEFINED_BEHAVIOR, 0, 1, { {&tsStruct::mfxBitstream.DataLength, 1024*1024, RUNTIME_BS}, {&tsStruct::mfxBitstream.MaxLength, 1024*1024-1, RUNTIME_BS}} },
        {/*20*/ MFX_ERR_UNDEFINED_BEHAVIOR, 0, 1, { {&tsStruct::mfxBitstream.DataOffset, 1024*1024+1, RUNTIME_BS}, {&tsStruct::mfxBitstream.MaxLength, 1024*1024, RUNTIME_BS}} },
        {/*21*/ MFX_ERR_UNDEFINED_BEHAVIOR, 0, 1, { {&tsStruct::mfxBitstream.MaxLength, 4096+4096, RUNTIME_BS},
                                                    {&tsStruct::mfxBitstream.DataLength, 4096+1, RUNTIME_BS},
                                                    {&tsStruct::mfxBitstream.DataOffset, 4096, RUNTIME_BS},
                                                  } },

        //Init resolution != stream resolution
        {/*22*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, 1, { {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920, INIT},
                                                          {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088, INIT},
                                                          {&tsStruct::mfxFrameSurface1.Info.Width, 1280, RUNTIME_SURF},
                                                          {&tsStruct::mfxFrameSurface1.Info.Height, 768, RUNTIME_SURF},
                                                        } },
        //Given frames resolution < init&stream resolution
        {/*23*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, 1, { {&tsStruct::mfxFrameSurface1.Info.Width, 160, RUNTIME_SURF},
                                                          {&tsStruct::mfxFrameSurface1.Info.Height, 96, RUNTIME_SURF},
                                                        } },

        {/*24*/ MFX_ERR_NONE, 0, 1, { {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4096, AFTER_INIT},
                                      {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096, AFTER_INIT},
                                    } },

        {/*25*/ MFX_ERR_NONE, 0, 20, { {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4096, AFTER_INIT},
                                       {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096, AFTER_INIT},
                                     } },

        {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, { &tsStruct::mfxFrameSurface1.Info.FourCC, 0xFFFF, RUNTIME_SURF} },
        {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, { &tsStruct::mfxFrameSurface1.Info.ChromaFormat, 0xFFFF, RUNTIME_SURF} },
        {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, { &tsStruct::mfxFrameSurface1.Info.Width, 0, RUNTIME_SURF} },
        {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, { &tsStruct::mfxFrameSurface1.Info.Height, 0, RUNTIME_SURF} },
        {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, { &tsStruct::mfxFrameSurface1.Info.BitDepthLuma, 100, RUNTIME_SURF} },

        {/*31*/ MFX_ERR_LOCK_MEMORY, 0, 1, { {&tsStruct::mfxFrameSurface1.Data.Y, 0, RUNTIME_SURF},
                                             {&tsStruct::mfxFrameSurface1.Data.U, 0, RUNTIME_SURF},
                                             {&tsStruct::mfxFrameSurface1.Data.A, 0, RUNTIME_SURF},
                                             {&tsStruct::mfxFrameSurface1.Data.MemId, 0, RUNTIME_SURF},
                                           } },
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(const tc_struct& tc, stream_desc const& sd)
    {
        TS_START;

        m_par.mfx.FrameInfo.Width  = sd.width;
        m_par.mfx.FrameInfo.Height = sd.height;

        const char* sname = g_tsStreamPool.Get(sd.name);
        g_tsStreamPool.Reg();

        tsBitstreamReaderIVF reader(sname, 1024*1024);
        m_bs_processor = &reader;

        MFXInit();
        if(m_uid)
            Load();

        tsStruct::SetPars(m_par, tc, INIT);

        if(NO_EXT_ALLOCATOR != tc.mode)
        {
            bool isSW = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
            UseDefaultAllocator(isSW);
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator(m_session, GetAllocator());

            if (!isSW && !m_is_handle_set)
            {
                mfxHDL hdl = 0;
                mfxHandleType type = static_cast<mfxHandleType>(0);
                GetAllocator()->get_hdl(type, hdl);
                SetHandle(m_session, type, hdl);
            }
        }

        if(FAILED_INIT == tc.mode)
            g_tsStatus.disable_next_check();
        if(NOT_INITIALIZED != tc.mode)
            Init();

        tsStruct::SetPars(m_par, tc, AFTER_INIT);
        AllocSurfaces();

        if(CLOSED_DECODER == tc.mode)
        {
            g_tsStatus.disable_next_check();
            Close();
        }

        //test section
        {
            m_pSurf = GetSurface();
            tsStruct::SetPars(m_bitstream, tc, RUNTIME_BS);
            tsStruct::SetPars(m_pSurf, tc, RUNTIME_SURF);

            auto ses     = (NULL_SESSION  == tc.mode) ? nullptr : m_session;
            auto bs      = (NULL_BS       == tc.mode) ? nullptr : m_pBitstream;
            auto surf    = (NULL_SURF     == tc.mode) ? nullptr : m_pSurf;
            auto surfOut = (NULL_SURF_OUT == tc.mode) ? nullptr : &m_pSurfOut;
            auto syncp   = (NULL_SYNCP    == tc.mode) ? nullptr : m_pSyncPoint;

            if(1 == tc.n_calls && tc.sts != MFX_ERR_NONE)
            {
                g_tsStatus.expect(tc.sts);
                DecodeFrameAsync(ses, bs, surf, surfOut, syncp);
                g_tsStatus.check();
            }
            else if(1 == tc.n_calls)
            {

                mfxStatus sts = MFX_ERR_MORE_DATA;
                size_t canary = 10000;
                while((canary-- != 0) &&
                      (MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts || MFX_WRN_VIDEO_PARAM_CHANGED == sts))
                {
                    sts = DecodeFrameAsync();
                }
                if (sts == MFX_ERR_NONE)
                {
                    SyncOperation(*m_pSyncPoint);
                }
                else
                {
                    g_tsStatus.expect(tc.sts);
                    g_tsStatus.m_status = sts;
                    g_tsStatus.check();
                }
            }
            else if(MFX_ERR_NONE != tc.sts)
            {
                for(size_t i(0); i<(tc.n_calls-1); ++i)
                {
                    g_tsStatus.disable_next_check();
                    DecodeFrameAsync();
                }
                g_tsStatus.expect(tc.sts);
                DecodeFrameAsync();
            }
            else
            {
                DecodeFrames(tc.n_calls);
            }
        }

        g_tsStatus.disable_next_check();
        Close();

        TS_END;
        return 0;
    }

    void TestSuite::AllocOpaque()
    {
        if(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        {
            //AllocSurfaces();
            m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
            mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

            mfxFrameAllocRequest request = {};
            g_tsStatus.disable_next_check();
            QueryIOSurf(m_session, m_pPar, &request);

            tsSurfacePool::AllocOpaque(request, *osa);
        }
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

    template <unsigned fourcc, unsigned profile>
    TestSuite::stream_desc const& query_stream(unsigned int, std::integral_constant<unsigned, fourcc>, std::integral_constant<unsigned, profile>);

    /* 8 bit */
    TestSuite::stream_desc const& query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_NV12>, std::integral_constant<unsigned, MFX_PROFILE_AV1_0>)
    {
        static const TestSuite::stream_desc sd{ 352, 288, "conformance/av1/self_coded/foreman_352x288_420.av1" };
        return sd;
    }

    /* 8b 420 */
    template <>
    TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::test_cases[] =
    {
        {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, { &tsStruct::mfxFrameSurface1.Info.BitDepthLuma, 10, RUNTIME_SURF} },
        {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, { {&tsStruct::mfxFrameSurface1.Info.FourCC, MFX_FOURCC_NV12, RUNTIME_SURF},
                                                     {&tsStruct::mfxFrameSurface1.Info.ChromaFormat, MFX_CHROMAFORMAT_YUV422, RUNTIME_SURF},
                                                   } },
        {/*34*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, 1, { {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_AYUV, AFTER_INIT},
                                                          {&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444, AFTER_INIT},
                                                        } },
        {/*35*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, 1, { {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P010, AFTER_INIT},
                                                          {&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420, AFTER_INIT},
                                                        } },
        {/*36*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, 1, { {&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y410, AFTER_INIT},
                                                          {&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444, AFTER_INIT},
                                                        } },
    };
    template <>
    unsigned int const TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::test_cases) / sizeof(TestSuite::tc_struct);

    template <unsigned fourcc, unsigned profile>
    int TestSuiteExt<fourcc, profile>::RunTest(unsigned int id)
    {
        auto tc =
            id >= TestSuite::n_cases ?
            test_cases[id - TestSuite::n_cases] : TestSuite::test_case[id];

        auto sd = query_stream(id, std::integral_constant<unsigned, fourcc>(), std::integral_constant<unsigned, profile>{});

        TestSuite suite;
        return suite.RunTest(tc, sd);
    }

TS_REG_TEST_SUITE(av1d_8b_420_decode_frame_async,  (TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::RunTest), (TestSuiteExt<MFX_FOURCC_NV12, MFX_PROFILE_AV1_0>::n_cases));

}
#undef TEST_NAME
