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

#if !defined(OPEN_SOURCE)
namespace av1d_film_grain
{
    class TestSuite
        : public tsVideoDecoder
    {

    public:

        TestSuite() : tsVideoDecoder(MFX_CODEC_AV1) { }
        ~TestSuite() {}

        template <unsigned fourcc>
        int RunTest_fourcc(const unsigned int id);

        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU64 v;
            mfxU8  stage;
        };
        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 test_type;
            mfxU32 stream_type;
            mfxU32 n_frames;
            f_pair set_par[MAX_NPARS];
        };

        struct stream_desc
        {
            mfxU16      width;
            mfxU16      height;
            const char* name;
        };

        static const unsigned int n_cases;

    protected:

        static const tc_struct test_case[];      //base cases

        int RunTest(const tc_struct&, unsigned, char const*);
        tc_struct const* m_cur_tc;

        void CheckGetVideoParam(tsExtBufType<mfxVideoParam> const&);
        void CheckDrain();
        mfxU32 UpdateBitstream();
        void PassFirstSurface();
        void PassSecondSurface();
        void PassSingleSurface();
        void DecodeFrameWithFilmGrain(bool);
        void DecodeFrame();

        mfxFrameSurface1* GetSurface();

    protected:

        enum
        {
            CHECK_DECODE_HEADER = 1,
            CHECK_QUERY_MODE1,
            CHECK_QUERY_MODE2,
            CHECK_QUERY_IO_SURF,
            CHECK_INIT,
            CHECK_RESET,
            CHECK_GET_VIDEO_PARAM,
            REGULAR_FG,
            FG_FORCED_OFF,
            FG_FORCED_ON,
            EMPTY_BS,
            ZERO_SURF,
            DRAIN
        };

        enum
        {
            SET = 1,
            CHECK = 2,
            RESET = 3
        };

        enum
        {
            STREAM_WITH_FG = 1,
            STREAM_WITHOUT_FG = 2
        };

        enum
        {
            ZERO = 0,
            NONZERO = 1
        };
    };

    const mfxU16 ASYNC_DEPTH = 3;
    const mfxU16 AV1_DPB_SIZE = 8;
    const mfxU16 NUM_SURF_VIDEO = ASYNC_DEPTH + AV1_DPB_SIZE;
    const mfxU16 NUM_SURF_SYSTEM = ASYNC_DEPTH + 1;

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // check FilmGrain returned from DecodeHeader for stream with film grain params
        {/*00*/ MFX_ERR_NONE, CHECK_DECODE_HEADER, STREAM_WITH_FG, 0, {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, CHECK}},
        // check FilmGrain returned from DecodeHeader for stream without film grain params
        {/*01*/ MFX_ERR_NONE, CHECK_DECODE_HEADER, STREAM_WITHOUT_FG, 0, {&tsStruct::mfxVideoParam.mfx.FilmGrain, ZERO, CHECK}},

        // check that QueryIOSurf requests 2x surfaces for FilmGrain for video memory
        {/*02*/ MFX_ERR_NONE, CHECK_QUERY_IO_SURF, 0, 0, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, SET},
                                                           {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, SET},
                                                           {&tsStruct::mfxVideoParam.AsyncDepth, ASYNC_DEPTH, SET},
                                                           {&tsStruct::mfxFrameAllocRequest.NumFrameMin, 2 * NUM_SURF_VIDEO, CHECK},
                                                           {&tsStruct::mfxFrameAllocRequest.NumFrameSuggested, 2 * NUM_SURF_VIDEO, CHECK} } },
        // check that QueryIOSurf requests 1x surfaces for FilmGrain for system memory
        {/*03*/ MFX_ERR_NONE, CHECK_QUERY_IO_SURF, 0, 0, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, ZERO, SET},
                                                           {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, SET},
                                                           {&tsStruct::mfxVideoParam.AsyncDepth, ASYNC_DEPTH, SET},
                                                           {&tsStruct::mfxFrameAllocRequest.NumFrameMin, 1, CHECK},
                                                           {&tsStruct::mfxFrameAllocRequest.NumFrameSuggested, NUM_SURF_SYSTEM, CHECK} } },

        // check that Query mode 1 returns FilmGrain == 1
        {/*04*/ MFX_ERR_NONE, CHECK_QUERY_MODE1, 0, 0, {&tsStruct::mfxVideoParam.mfx.FilmGrain, 1, CHECK}},

        // check that Query mode 2 accepts nonzero FilmGrain
        {/*05*/ MFX_ERR_NONE, CHECK_QUERY_MODE2, 0, 0, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, SET},
                                                         {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, CHECK} } },

        // check that Init accepts nonzero FilmGrain
        {/*06*/ MFX_ERR_NONE, CHECK_INIT, 0, 0, {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, SET}},

        // check that GetVideoParam returns nonzero FilmGrain when decoder was initialized with nonzero FilmGrain
        {/*07*/ MFX_ERR_NONE, CHECK_GET_VIDEO_PARAM, 0, 0, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, SET},
                                                             {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, CHECK} } },
        // check that GetVideoParam returns zero FilmGrain when decoder was initialized with zero FilmGrain and started decoding stream with film grain params
        {/*08*/ MFX_ERR_NONE, CHECK_GET_VIDEO_PARAM, STREAM_WITH_FG, 3, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, ZERO, SET},
                                                                          {&tsStruct::mfxVideoParam.mfx.FilmGrain, ZERO, CHECK} } },

        // check that Reset returns proper error on attempt to change zero FilmGrain to nonzero in runtime
        {/*09*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, CHECK_RESET, STREAM_WITHOUT_FG, 3, {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, RESET}},

        // regular decoding pipeline with film grain for video memory
        {/*10*/ MFX_ERR_NONE, REGULAR_FG, STREAM_WITH_FG, 2, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, SET}},
        // regular decoding pipeline with film grain for system memory
        {/*11*/ MFX_ERR_NONE, REGULAR_FG, STREAM_WITH_FG, 2, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, SET}},

        // decoding of stream containing film grain with FilmGrain forced to 0 (video memory)
        {/*12*/ MFX_ERR_NONE, FG_FORCED_OFF, STREAM_WITH_FG, 2, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, ZERO, SET},
                                                                  {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, SET} } },
        // decoding of stream containing film grain with FilmGrain forced to 0 (system memory)
        {/*13*/ MFX_ERR_NONE, FG_FORCED_OFF, STREAM_WITH_FG, 2, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, ZERO, SET},
                                                                  {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, SET} } },

        // decoding of stream without film grain with FilmGrain forced to 1 (video memory)
        {/*14*/ MFX_ERR_NONE, FG_FORCED_ON, STREAM_WITHOUT_FG, 2, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, SET},
                                                                    {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, SET} } },
        // decoding of stream without film grain with FilmGrain forced to 1 (system memory)
        {/*15*/ MFX_ERR_NONE, FG_FORCED_ON, STREAM_WITHOUT_FG, 2, { {&tsStruct::mfxVideoParam.mfx.FilmGrain, NONZERO, SET},
                                                                    {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, SET} } },

        // check that pass of zero surface to DecodeFrameAsync is properly handled together with nonzero FilmGrain (video memory)
        {/*16*/ MFX_ERR_NONE, ZERO_SURF, STREAM_WITH_FG, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, SET}},
        // check that pass of zero surface to DecodeFrameAsync is properly handled together with nonzero FilmGrain (system memory)
        {/*17*/ MFX_ERR_NONE, ZERO_SURF, STREAM_WITH_FG, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, SET}},

        // check that pass of empty bitstream to DecodeFrameAsync is properly handled together with nonzero FilmGrain (video memory)
        {/*18*/ MFX_ERR_NONE, EMPTY_BS, STREAM_WITH_FG, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, SET}},
        // check that pass of empty bitstream to DecodeFrameAsync is properly handled together with nonzero FilmGrain (system memory)
        {/*19*/ MFX_ERR_NONE, EMPTY_BS, STREAM_WITH_FG, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, SET}},

        // check that pass of zero bitstream to DecodeFrameAsync at intermediate stage forces drain (video memory only)
        {/*20*/ MFX_ERR_NONE, DRAIN, STREAM_WITH_FG, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, SET}},

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

    inline void Check(mfxVideoParam const* par, mfxVideoParam const* ref)
    {
        if (par->mfx.FilmGrain != ref->mfx.FilmGrain)
        {
            ADD_FAILURE() << "ERROR: Reported FilmGrain is equal to " << par->mfx.FilmGrain << ", excpected is " << ref->mfx.FilmGrain;
            throw tsFAIL;
        }
    }

    inline void Check(mfxFrameAllocRequest const* req, mfxFrameAllocRequest const* ref)
    {
        if (req->NumFrameMin != ref->NumFrameMin)
        {
            ADD_FAILURE() << "ERROR: QueryIOSurf returned NumFrameMin " << req->NumFrameMin << ", excpected is " << ref->NumFrameMin;
            throw tsFAIL;
        }

        if (req->NumFrameSuggested != ref->NumFrameSuggested)
        {
            ADD_FAILURE() << "ERROR: QueryIOSurf returned NumFrameSuggested " << req->NumFrameSuggested << ", excpected is " << ref->NumFrameSuggested;
            throw tsFAIL;
        }
    }

    inline void CheckFirstCall(mfxBitstream const& bs, mfxU32 len, mfxFrameSurface1 const* surf_out, mfxSyncPoint syncp)
    {
        if (bs.DataLength >= len)
        {
            ADD_FAILURE() << "ERROR: MSDK must reduce mfxBitstream->DataLength at first call to DecodeFrameAsync. Returned DataLength before the call is " << len << ", DataLength after the call is " << bs.DataLength;
            throw tsFAIL;
        }

        if (surf_out)
        {
            ADD_FAILURE() << "ERROR: Pointer to output surface must be zero after first call to DecodeFrameAsync";
            throw tsFAIL;
        }

        if (syncp)
        {
            ADD_FAILURE() << "ERROR: Sync point must be zero after first call to DecodeFrameAsync";
            throw tsFAIL;
        }
    }

    inline void CheckSecondCall(mfxBitstream const& bs, mfxU32 len, mfxFrameSurface1 const* surf_out, mfxSyncPoint syncp)
    {
        if (bs.DataLength != len)
        {
            ADD_FAILURE() << "ERROR: MSDK should not touch mfxBitstream at second call to DecodeFrameAsync. Returned DataLength before the call is " << len << ", DataLength after the call is " << bs.DataLength;
            throw tsFAIL;
        }

        if (!surf_out)
        {
            ADD_FAILURE() << "ERROR: Pointer to output surface must be nonzero after second call to DecodeFrameAsync";
            throw tsFAIL;
        }

        if (!syncp)
        {
            ADD_FAILURE() << "ERROR: Sync point must be nonzero after second call to DecodeFrameAsync";
            throw tsFAIL;
        }
    }

    inline void CheckSingleCall(mfxBitstream const& bs, mfxU32 len, mfxFrameSurface1 const* surf_out, mfxSyncPoint syncp)
    {
        if (bs.DataLength >= len)
        {
            ADD_FAILURE() << "ERROR: MSDK must reduce mfxBitstream->DataLength at call to DecodeFrameAsync. Returned DataLength before the call is " << len << ", DataLength after the call is " << bs.DataLength;
            throw tsFAIL;
        }

        if (!surf_out)
        {
            ADD_FAILURE() << "ERROR: Pointer to output surface must be nonzero after call to DecodeFrameAsync";
            throw tsFAIL;
        }

        if (!syncp)
        {
            ADD_FAILURE() << "ERROR: Sync point must be nonzero after call to DecodeFrameAsync";
            throw tsFAIL;
        }
    }

    mfxU32 TestSuite::UpdateBitstream()
    {
        assert(m_pBitstream);

        if (!m_pBitstream->DataLength)
            m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

        assert(m_pBitstream);

        return m_pBitstream->DataLength;
    }

    inline void AttachExtBuffer(mfxFrameSurface1& surf)
    {
        mfxExtBuffer** pBuf = new mfxExtBuffer*;
        mfxExtAV1FilmGrainParam* pExtPar = new mfxExtAV1FilmGrainParam{};
        pExtPar->Header.BufferId = MFX_EXTBUFF_AV1_FILM_GRAIN_PARAM;
        pExtPar->Header.BufferSz = sizeof(mfxExtAV1FilmGrainParam);
        *pBuf = reinterpret_cast<mfxExtBuffer*>(pExtPar);

        surf.Data.ExtParam = pBuf;
        surf.Data.NumExtParam = 1;
    }

    inline void ReleaseExtBuffer(mfxFrameSurface1& surf)
    {
        if (surf.Data.NumExtParam && surf.Data.ExtParam)
        {
            assert(surf.Data.NumExtParam == 1);

            if (*surf.Data.ExtParam)
                delete *surf.Data.ExtParam;

            delete surf.Data.ExtParam;
            surf.Data.NumExtParam = 0;

        }
    }

    mfxFrameSurface1* TestSuite::GetSurface()
    {
        mfxFrameSurface1* const surf = tsVideoDecoder::GetSurface();

        if (m_cur_tc->test_type == FG_FORCED_OFF)
            AttachExtBuffer(*surf);

        return surf;
    }

    void TestSuite::PassFirstSurface()
    {
        // video memory: 2 calls per frame are required for stream with film grain
        // first call to DecodeFrameAsync: MSDK must take bitstream, return MFX_ERR_MORE_SURFACE and zero output surface and sync point
        const mfxU32 lengthBeforeCall = UpdateBitstream();
        g_tsStatus.expect(MFX_ERR_MORE_SURFACE);
        DecodeFrameAsync();
        g_tsStatus.check();
        CheckFirstCall(*m_pBitstream, lengthBeforeCall, m_pSurfOut, *m_pSyncPoint);
    }

    void TestSuite::PassSecondSurface()
    {
        // video memory: 2 calls per frame are required for stream with film grain
        // second call to DecodeFrameAsync: MSDK must ignore bitstream, return MFX_ERR_NONE and nonzero output surface and sync point
        const mfxU32 lengthBeforeCall = UpdateBitstream();
        g_tsStatus.expect(MFX_ERR_NONE);
        DecodeFrameAsync();
        g_tsStatus.check();
        CheckSecondCall(*m_pBitstream, lengthBeforeCall, m_pSurfOut, *m_pSyncPoint);
    }

    void TestSuite::PassSingleSurface()
    {
        // system memory: 1 call per frame is required for stream with film grain
        // MSDK must take bitstream, return MFX_ERR_NONE and nonzero output surface and sync point
        const mfxU32 lengthBeforeCall = UpdateBitstream();
        g_tsStatus.expect(MFX_ERR_NONE);
        DecodeFrameAsync();
        g_tsStatus.check();
        CheckSingleCall(*m_pBitstream, lengthBeforeCall, m_pSurfOut, *m_pSyncPoint);
    }

    void TestSuite::DecodeFrameWithFilmGrain(bool isSysMem)
    {
        if (isSysMem)
            PassSingleSurface();
        else
        {
            PassFirstSurface();
            PassSecondSurface();
        }

        g_tsStatus.expect(MFX_ERR_NONE);
        SyncOperation();
        g_tsStatus.check();
    }

    void TestSuite::DecodeFrame()
    {
        UpdateBitstream();
        g_tsStatus.expect(MFX_ERR_NONE);
        DecodeFrameAsync();
        g_tsStatus.check();
        SyncOperation();
        g_tsStatus.check();
    }

    inline void CheckDrainCall(mfxFrameSurface1 const* surf_out, mfxSyncPoint syncp)
    {
        if (surf_out)
        {
            ADD_FAILURE() << "ERROR: Pointer to output surface must be zero after drain call to DecodeFrameAsync";
            throw tsFAIL;
        }

        if (syncp)
        {
            ADD_FAILURE() << "ERROR: Sync point must be zero after drain call to DecodeFrameAsync";
            throw tsFAIL;
        }
    }

    void TestSuite::CheckDrain()
    {
        m_pSurf = GetSurface();
        g_tsStatus.expect(MFX_ERR_MORE_DATA);
        DecodeFrameAsync(m_session, nullptr, m_pSurf, &m_pSurfOut, m_pSyncPoint);
        g_tsStatus.check();
        CheckDrainCall(m_pSurfOut, *m_pSyncPoint);
    }

    void TestSuite::CheckGetVideoParam(tsExtBufType<mfxVideoParam> const& ref_par)
    {
        g_tsStatus.expect(m_cur_tc->sts);
        GetVideoParam();
        Check(m_pPar, &ref_par);
    }

    inline void CheckExtBuffer(mfxFrameSurface1 const& surf)
    {
        if (surf.Data.NumExtParam != 1)
        {
            ADD_FAILURE() << "ERROR: Wrong external buffer count attached to output surface";
            throw tsFAIL;
        }

        if (surf.Data.ExtParam == NULL)
        {
            ADD_FAILURE() << "ERROR: null pointer ExtParam in output surface";
            throw tsFAIL;
        }

        mfxExtBuffer const* pBuf = surf.Data.ExtParam[0];

        if (pBuf == NULL)
        {
            ADD_FAILURE() << "ERROR: null pointer ExtParam[0] in output surface";
            throw tsFAIL;
        }

        if (pBuf->BufferId != MFX_EXTBUFF_AV1_FILM_GRAIN_PARAM ||
            pBuf->BufferSz != sizeof(mfxExtAV1FilmGrainParam))
        {
            ADD_FAILURE() << "ERROR: BufferId or BufferSz does not match to mfxExtAV1FilmGrainParam";
            throw tsFAIL;
        }

        mfxExtAV1FilmGrainParam const* pFGPar = reinterpret_cast<mfxExtAV1FilmGrainParam const*>(pBuf);

        if ((pFGPar->Flags & MFX_FILM_GRAIN_APPLY) == 0)
        {
            ADD_FAILURE() << "ERROR: Incorrect film grain parameters reported by mfxExtAV1FilmGrainParam";
            throw tsFAIL;
        }
    }

    class SurfaceChecker : public tsSurfaceProcessor
    {
    public:
        SurfaceChecker(bool check) : m_check_ext_buffer(check) {}
        ~SurfaceChecker() {}

        mfxStatus ProcessSurface(mfxFrameSurface1& s);

    protected:
        bool m_check_ext_buffer;

    };

    mfxStatus SurfaceChecker::ProcessSurface(mfxFrameSurface1& surf)
    {
        if (m_check_ext_buffer)
        {
            CheckExtBuffer(surf);
            ReleaseExtBuffer(surf);
        }

        return MFX_ERR_NONE;
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned fourcc, char const* sname)
    {
        TS_START;

        m_cur_tc = &tc;

        // initializes session
        MFXInit();

        // fill mfxVideoParam with parameters defined by test case
        tsStruct::SetPars(m_par, tc, SET);

        // prepare external allocator and device
        const bool isSysMem = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
        UseDefaultAllocator(isSysMem);
        m_pFrameAllocator = GetAllocator();
        SetFrameAllocator(m_session, GetAllocator());

        if (!isSysMem && !m_is_handle_set)
        {
            mfxHDL hdl = 0;
            mfxHandleType type = static_cast<mfxHandleType>(0);
            GetAllocator()->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
        }

        // do all checks which don't require decoder initialization

        const bool filmGrainForced =
            (tc.test_type == FG_FORCED_OFF || tc.test_type == FG_FORCED_ON);


        tsExtBufType<mfxVideoParam> ref_par = {};
        tsStruct::SetPars(ref_par, tc, CHECK);

        std::unique_ptr<tsBitstreamReaderIVF> reader;
        const bool hasStream = (sname != nullptr);

        if (hasStream)
        {
            // prepare bitstream processor
            const Ipp32u size = 1024 * 1024;
            reader.reset(new tsBitstreamReaderIVF(sname, size));
            m_bs_processor = reader.get();
            if (!m_bs_processor)
            {
                ADD_FAILURE() << "ERROR: failed to create bitstream processor";
                throw tsFAIL;
            }

            // call and check DecodeHeader
            g_tsStatus.disable_next_check();
            DecodeHeader();

            if (tc.test_type == CHECK_DECODE_HEADER)
            {
                g_tsStatus.check(tc.sts);
                Check(m_pPar, &ref_par);
                return 0;
            }

            // force parameters to overwrite output of DecodeHeader
            if (filmGrainForced)
                tsStruct::SetPars(m_par, tc, SET);
        }
        else
        {
            m_par.mfx.FrameInfo.FourCC = fourcc;
            m_par_set = true;
        }

        g_tsStatus.disable_next_check();
        QueryIOSurf();

        if (tc.test_type == CHECK_QUERY_IO_SURF)
        {
            g_tsStatus.check(tc.sts);

            mfxFrameAllocRequest ref_req = {};
            tsStruct::SetPars(&ref_req, tc, CHECK);

            Check(m_pRequest, &ref_req);
            return 0;
        }

        const bool checkQuery =
            tc.test_type == CHECK_QUERY_MODE1 ||
            tc.test_type == CHECK_QUERY_MODE2;

        if (checkQuery)
        {
            g_tsStatus.expect(tc.sts);
            if (CHECK_QUERY_MODE1)
                m_pPar = nullptr;
            Query();
            Check(m_pParOut, &ref_par);
            return 0;
        }

        AllocSurfaces();

        g_tsStatus.disable_next_check();
        Init();

        if (tc.test_type == CHECK_INIT)
        {
            g_tsStatus.expect(tc.sts);
            g_tsStatus.check();
            return 0;
        }

        if (tc.test_type == CHECK_GET_VIDEO_PARAM && tc.n_frames == 0)
        {
            CheckGetVideoParam(ref_par);
            return 0;
        }

        // do checks which require real decoding

        SurfaceChecker checker(tc.test_type == FG_FORCED_OFF);
        m_surf_processor = &checker;

        if (hasStream)
        {
            const bool simpleDecoding = tc.test_type == CHECK_GET_VIDEO_PARAM
                || tc.test_type == CHECK_RESET;

            if (simpleDecoding)
            {
                DecodeFrames(tc.n_frames);

                if (tc.test_type == CHECK_GET_VIDEO_PARAM)
                    CheckGetVideoParam(ref_par);

                if (tc.test_type == CHECK_RESET)
                {
                    tsStruct::SetPars(m_par, tc, RESET);
                    g_tsStatus.expect(tc.sts);
                    Reset();
                }
            }
            else
            {
                if (tc.test_type == REGULAR_FG)
                {
                    for (unsigned i = 0; i < tc.n_frames; i++)
                        DecodeFrameWithFilmGrain(isSysMem);
                }

                if (filmGrainForced)
                {
                    for (unsigned i = 0; i < tc.n_frames; i++)
                        DecodeFrame();
                }

                if (tc.test_type == ZERO_SURF)
                {
                    // decode one frame with film grain
                    DecodeFrameWithFilmGrain(isSysMem);

                    // pass zero surface_work
                    g_tsStatus.expect(MFX_ERR_NULL_PTR);
                    DecodeFrameAsync(m_session, m_pBitstream, nullptr, &m_pSurfOut, m_pSyncPoint);
                    g_tsStatus.check();

                    if (isSysMem)
                        DecodeFrameWithFilmGrain(isSysMem);
                    else
                    {
                        PassFirstSurface();

                        // for video memory additionally check pass of zero surface_work at intermediate stage
                        g_tsStatus.expect(MFX_ERR_NULL_PTR);
                        DecodeFrameAsync(m_session, m_pBitstream, nullptr, &m_pSurfOut, m_pSyncPoint);
                        g_tsStatus.check();

                        // check that nothing is broken after it
                        PassSecondSurface();
                    }
                }

                if (tc.test_type == EMPTY_BS)
                {
                    // decode one frame with film grain
                    DecodeFrameWithFilmGrain(isSysMem);

                    // pass empty bitstream
                    mfxBitstream bs = {};
                    bs.Data = m_pBitstream->Data;
                    m_pSurf = GetSurface();
                    g_tsStatus.expect(MFX_ERR_MORE_DATA);
                    DecodeFrameAsync(m_session, &bs, m_pSurf, &m_pSurfOut, m_pSyncPoint);
                    g_tsStatus.check();
                    g_tsStatus.reset();

                    // check that nothing is broken after it
                    DecodeFrameWithFilmGrain(isSysMem);
                }

                if (tc.test_type == DRAIN)
                {
                    DecodeFrameWithFilmGrain(false);
                    PassFirstSurface();
                }

                CheckDrain();
            }
        }

        TS_END;
        return 0;
    }

    /* 8 bit 420*/
    char const* query_stream(std::integral_constant<unsigned, MFX_FOURCC_NV12>, bool hasFG)
    {
        if (hasFG)
            return "conformance/av1/DVK/MainProfile_8bit420/Syntax_AV1_520x520_film_grain_00_1.3.av1";
        else
            return "conformance/av1/DVK/MainProfile_8bit420/Syntax_AV1_512x512_ref_mv_basic05_1.3.av1";
    }

    /* 10 bit 420*/
    char const* query_stream(std::integral_constant<unsigned, MFX_FOURCC_P010>, bool hasFG)
    {
        ADD_FAILURE() << "ERROR: No 10 bit stream with film grain so far";
        throw tsFAIL;
    }

    template <unsigned fourcc>
    int TestSuite::RunTest_fourcc(const unsigned int id)
    {
        auto tc = TestSuite::test_case[id];
        char const* sname = nullptr;
        const bool hasStream = (tc.stream_type != 0);
        if (hasStream)
        {
            sname =
                g_tsStreamPool.Get(query_stream(std::integral_constant<unsigned, fourcc>(), tc.stream_type == STREAM_WITH_FG));
            g_tsStreamPool.Reg();
        }

        return RunTest(tc, fourcc, sname);
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(av1d_8b_420_nv12_film_grain,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(av1d_10b_420_p010_film_grain, RunTest_fourcc<MFX_FOURCC_P010>, n_cases);
}
#endif
