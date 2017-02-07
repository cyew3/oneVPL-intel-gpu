/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace vp9e_encode_frame_async
{
    class TestSuite : tsVideoEncoder, BS_VP9_parser
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9)
        {

        }
        ~TestSuite() {}

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);
        static const tc_struct test_case_nv12[]; //8b 420
        static const tc_struct test_case_p010[]; //10b 420

        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_p010;

    private:
        enum
        {
            MFX_PAR,
            MFX_BS,
            MFX_SURF,
            NULL_SESSION,
            NULL_BS,
            NULL_SURF,
            NOT_INIT,
            FAILED_INIT,
            CLOSED,
            CROP_XY,
            NULL_MEMID,
            NONE
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        //correct default params
        {/*00*/ MFX_ERR_NONE, NONE,
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY },
        },
        {/*01*/ MFX_ERR_NONE, NONE,
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
        },
        //check NULL params/ptrs
        {/*02*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*03*/ MFX_ERR_NULL_PTR, NULL_BS },
        {/*04*/ MFX_ERR_MORE_DATA, NULL_SURF },
        {/*05*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT },
        {/*06*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT },
        {/*07*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 10 }
        },
        {/*08*/ MFX_ERR_UNDEFINED_BEHAVIOR, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.DataOffset, 0xFFFFFFFF }
        },
        {/*09*/ MFX_ERR_NULL_PTR, NONE,

            { MFX_BS, &tsStruct::mfxBitstream.Data, 0 }
        },
        {/*10*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 0 },
        },
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, NULL_MEMID,
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
        },

        //check additional frame rate
        {/*12*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 }
            }
        },

        //check crop
        {/*13*/ MFX_ERR_NONE, CROP_XY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 100 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 100 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 5 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 5 },
            }
        },

        {/*14*/ MFX_ERR_UNDEFINED_BEHAVIOR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.PitchHigh, 0x8000 }
        },

        {/*15*/ MFX_ERR_NONE, NONE,
            { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1 },
        },
        {/*16*/ MFX_ERR_NONE, NONE,
            { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 5 },
        },
        {/*17*/ MFX_ERR_NONE, NONE,
            { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 10 },
        },
    };

    const TestSuite::tc_struct TestSuite::test_case_nv12[] =
    {
        {/*18*/ MFX_ERR_NULL_PTR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.Y, 0 }
        },
        {/*19*/ MFX_ERR_NULL_PTR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.U, 0 }
        },
        {/*20*/ MFX_ERR_NULL_PTR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.V, 0 }
        },
    };
    const unsigned int TestSuite::n_cases_nv12 = sizeof(TestSuite::test_case_nv12)/sizeof(TestSuite::tc_struct) + n_cases;

    const TestSuite::tc_struct TestSuite::test_case_p010[] =
    {
        {/*18*/ MFX_ERR_NULL_PTR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.Y16, 0 }
        },
        {/*19*/ MFX_ERR_NULL_PTR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.U16, 0 }
        },
        {/*20*/ MFX_ERR_NULL_PTR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.V16, 0 }
        },
    };
    const unsigned int TestSuite::n_cases_p010 = sizeof(TestSuite::test_case_p010)/sizeof(TestSuite::tc_struct) + n_cases;

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    struct streamDesc
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
    };

    const streamDesc streams[] = {
        {720, 480, "YUV/calendar_720x480_600_nv12.yuv"},
        {1280, 720, "YUV10bit/Suzie_ProRes_1280x720_50f.p010.yuv"},
    };

    const streamDesc& getStreamDesc(const mfxU32& id)
    {
        switch(id)
        {
        case MFX_FOURCC_NV12: return streams[0];
        case MFX_FOURCC_P010: return streams[1];
        default: assert(0); return streams[0];
        }
    }

    const TestSuite::tc_struct* getTestTable(const mfxU32& fourcc)
    {
        switch(fourcc)
        {
        case MFX_FOURCC_NV12: return TestSuite::test_case_nv12;
        case MFX_FOURCC_P010: return TestSuite::test_case_p010;
        default: assert(0); return 0;
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        //return RunTest(id, fourcc);
        const tc_struct* fourcc_table = getTestTable(fourcc);
        const unsigned int real_id = (id < n_cases) ? (id) : (id - n_cases);
        const tc_struct& tc = (real_id == id) ? test_case[real_id] : fourcc_table[real_id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        const char* stream = g_tsStreamPool.Get(getStreamDesc(fourcc_id).name);
        g_tsStreamPool.Reg();
        tsSurfaceProcessor *reader;
        mfxStatus sts;

        MFXInit();
        Load();

        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = getStreamDesc(fourcc_id).w;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = getStreamDesc(fourcc_id).h;
        m_par.mfx.QPI = m_par.mfx.QPP = 100;

        SETPARS(m_pPar, MFX_PAR);

        if(fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        } 
        else if(fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || (fourcc_id == MFX_FOURCC_P010 && g_tsHWtype < MFX_HW_ICL)) // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        } else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            Init();
            
            // set test param
            if(tc.sts < MFX_ERR_NONE)
            {
                AllocBitstream(); TS_CHECK_MFX;
                SETPARS(m_pBitstream, MFX_BS);
                AllocSurfaces(); TS_CHECK_MFX;
                m_pSurf = GetSurface(); TS_CHECK_MFX;
                if (m_filler)
                {
                    m_pSurf = m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
                }
            }

            SETPARS(m_pSurf, MFX_SURF);
        }

        if (tc.type == CLOSED)
        {
            Close();
        }

        if(tc.type == NULL_MEMID)
        {
            m_pSurf->Data.MemId = 0;
        }

        if (tc.sts >= MFX_ERR_NONE)
        {
            unsigned  encoded = 0;
            const unsigned encode_frames_count = m_pPar->AsyncDepth > 1 ? m_pPar->AsyncDepth + 2 : 1;
            while (encoded < encode_frames_count)
            {
                m_pBitstream->DataLength = m_pBitstream->DataOffset = 0;
                if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                {
                    continue;
                }

                g_tsStatus.check(); TS_CHECK_MFX;
                SyncOperation(); TS_CHECK_MFX;

                BSErr bserror = set_buffer(m_pBitstream->Data, m_pBitstream->DataLength);
                if(bserror != BS_ERR_NONE)
                {
                    ADD_FAILURE() << "ERROR: Set encoded buffer to stream parser failed!\n";
                    throw tsFAIL;
                }

                bserror = parse_next_unit();
                if(bserror != BS_ERR_NONE)
                {
                    ADD_FAILURE() << "ERROR: Parsing encoded frame header failed!\n";
                    throw tsFAIL;
                }

                void *ptr = get_header();
                if(ptr == nullptr) {
                    ADD_FAILURE() << "ERROR: Obtaining header from the encoded stream failed!";
                    throw tsFAIL;
                }

                BS_VP9::Frame* hdr = static_cast<BS_VP9::Frame*>(ptr);
                if(fourcc_id == MFX_FOURCC_NV12)
                {
                    if(hdr->uh.profile != 0)
                    {
                        ADD_FAILURE() << "ERROR: unc-header profile mismatch, expected 0, detected " << hdr->uh.profile;
                        throw tsFAIL;
                    }
                } 
                else if(fourcc_id == MFX_FOURCC_P010)
                {
                    if(hdr->uh.profile != 2)
                    {
                        ADD_FAILURE() << "ERROR: uncompressed header profile mismatch, expected 2, detected " << hdr->uh.profile;
                        throw tsFAIL;
                    }
                }
                /*
                const int encoded_size = m_pBitstream->DataLength;
                static FILE *fp_vp9 = fopen("vp9e_encoded.vp9", "wb");
                fwrite(m_pBitstream->Data, encoded_size, 1, fp_vp9);
                */
                encoded++;
            }
            sts = tc.sts;
        }
        else if (tc.type == NULL_MEMID)
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
        }
        else if (tc.type == NULL_SESSION)
        {
            sts = EncodeFrameAsync(NULL, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
        }
        else if (tc.type == NULL_BS)
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, NULL, m_pSyncPoint);
        }
        else if (tc.type == NULL_SURF)
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, NULL, m_pBitstream, m_pSyncPoint);
        }
        else
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
        }
        g_tsStatus.expect(tc.sts);
        g_tsStatus.check(sts);
        if (tc.sts != MFX_ERR_NOT_INITIALIZED)
            g_tsStatus.expect(MFX_ERR_NONE);
        else
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_encode_frame_async,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_encode_frame_async, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
};
