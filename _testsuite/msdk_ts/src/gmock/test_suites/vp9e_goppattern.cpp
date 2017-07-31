/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include <memory>

namespace vp9e_goppattern
{
    enum
    {
        NONE,
        MFX_PAR,
        INTRA_REQUEST
    };

    class TestSuite : tsVideoEncoder, BS_VP9_parser
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9)
        {

        }
        ~TestSuite() {}
        int RunTest(unsigned int id);
        const char *FrameTypeByMFXEnum(mfxU16 enum_frame_type)
        {
            switch (enum_frame_type)
            {
                case MFX_FRAMETYPE_I : return "I";
                case MFX_FRAMETYPE_P : return "P";
                default : return "UNKNOWN";
            }
        }

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            mfxU16 num_frames;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);

        static const tc_struct test_case_nv12[];
        static const tc_struct test_case_rext[];

        static const unsigned int n_cases;
        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_p010;
        static const unsigned int n_cases_ayuv;
        static const unsigned int n_cases_y410;

    private:
        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // Default case
        {/*00*/ MFX_ERR_NONE, NONE, 18, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 }
                                 }
        },

        // Big async depth
        {/*01*/ MFX_ERR_NONE, NONE, 18, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 5 },
                                 }
        },

        // BRC=CBR
        {/*02*/ MFX_ERR_NONE, NONE, 18, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                                 }
        },

        // Multiref is ON
        {/*03*/ MFX_ERR_NONE, NONE, 18,{
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
                                 }
        },

        // Corner-case: not enoupgh frames for a full GOP
        {/*04*/ MFX_ERR_NONE, NONE, 5, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 }
                                 }
        },

        // Corner-case: 1 frame in the end for a new GOP
        {/*05*/ MFX_ERR_NONE, NONE, 7, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 }
                                  }
        },

        // 1-frame GOP (only I frames)
        {/*06*/ MFX_ERR_NONE, NONE, 3, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 }
                                  }
        },

        // Default GOP
        {/*07*/ MFX_ERR_NONE, NONE, 3, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0 }
                                  }
        },

        // Request Intra-frame
        {/*08*/ MFX_ERR_NONE, INTRA_REQUEST, 10, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 5 }
                                  }
        },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const TestSuite::tc_struct TestSuite::test_case_nv12[] =
    {
        // Very long GOP
        {/*09*/ MFX_ERR_NONE, NONE, 260,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 257 }
            }
        }
    };
    const unsigned int TestSuite::n_cases_nv12 = sizeof(TestSuite::test_case_nv12) / sizeof(TestSuite::tc_struct) + n_cases;

    const TestSuite::tc_struct TestSuite::test_case_rext[] =
    {
        // Very long GOP
        {/*09*/ MFX_ERR_NONE, NONE, 240,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 237 }
            }
        }
    };
    const unsigned int TestSuite::n_cases_p010 = sizeof(TestSuite::test_case_rext) / sizeof(TestSuite::tc_struct) + n_cases;
    const unsigned int TestSuite::n_cases_ayuv = sizeof(TestSuite::test_case_rext) / sizeof(TestSuite::tc_struct) + n_cases;
    const unsigned int TestSuite::n_cases_y410 = sizeof(TestSuite::test_case_rext) / sizeof(TestSuite::tc_struct) + n_cases;

    struct streamDesc
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
    };

    const streamDesc streams[] = {
        { 176, 144, "YUV/salesman_176x144_449.yuv" },
        { 176, 144, "YUV10bit420_ms/Kimono1_176x144_24_p010_shifted.yuv" },
        { 176, 144, "YUV8bit444/Kimono1_176x144_24_ayuv.yuv" },
        { 176, 144, "YUV10bit444/Kimono1_176x144_24_y410.yuv" },
    };

    const streamDesc& getStreamDesc(const mfxU32& id)
    {
        switch (id)
        {
        case MFX_FOURCC_NV12: return streams[0];
        case MFX_FOURCC_P010: return streams[1];
        case MFX_FOURCC_AYUV: return streams[2];
        case MFX_FOURCC_Y410: return streams[3];
        default: assert(0); return streams[0];
        }
    }

    const TestSuite::tc_struct* getTestTable(const mfxU32& fourcc)
    {
        switch (fourcc)
        {
        case MFX_FOURCC_NV12:
            return TestSuite::test_case_nv12;
        case MFX_FOURCC_P010:
        case MFX_FOURCC_AYUV:
        case MFX_FOURCC_Y410:
            return TestSuite::test_case_rext;
        default:
            assert(0); return 0;
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
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

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        //set default params
        m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = getStreamDesc(fourcc_id).w;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = getStreamDesc(fourcc_id).h;
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            m_par.mfx.QPI = m_par.mfx.QPP = 100;
        }
        else if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        {
            m_par.mfx.TargetKbps = 500;
        }

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        //set reader
        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        const mfxU32 default_frames_number = 10;
        const mfxU32 default_intra_frame_request_position = 3;

        const mfxU32 frames_count = (tc.num_frames == 0 ? default_frames_number : tc.num_frames);

        std::unique_ptr<mfxU32[]> frame_types_expected (new mfxU32[frames_count]);
        std::unique_ptr<mfxU32[]> frame_types_encoded (new mfxU32[frames_count]);
        std::unique_ptr<mfxU32[]> frame_sizes_encoded(new mfxU32[frames_count]);

        //create expected sequence of I and P frames for the later check with the actual one
        mfxU32 gop_frame_count = 0;
        for(mfxU32 i = 0; i < frames_count; i++)
        {
            if (tc.type == INTRA_REQUEST && i == default_intra_frame_request_position)
            {
                frame_types_expected[i] = MFX_FRAMETYPE_I;
            }
            else
            {
                //specifying expected frame type
                if (gop_frame_count == 0)
                {
                    frame_types_expected[i] = MFX_FRAMETYPE_I;
                }
                else
                {
                    frame_types_expected[i] = MFX_FRAMETYPE_P;
                }
            }

            gop_frame_count ++;

            //start new GOP
            if (gop_frame_count == m_pPar->mfx.GopPicSize)
            {
                gop_frame_count = 0;
            }

            frame_types_encoded[i] = MFX_FRAMETYPE_UNKNOWN;
        }

        g_tsStatus.expect(tc.sts);

        Init(m_session, m_pPar);

        if (tc.sts >= MFX_ERR_NONE)
        {
            mfxU32 encoded = 0;
            mfxU32 submitted = 0;
            mfxSyncPoint sp;

            mfxU32 i = 0;
            while(encoded < frames_count)
            {
                if (tc.type == INTRA_REQUEST && i == default_intra_frame_request_position)
                {
                    m_pCtrl->FrameType = MFX_FRAMETYPE_I;
                }
                else if (m_pCtrl)
                {
                    m_pCtrl->FrameType = 0;
                }
                i++;

                //submit source frames to the encoder
                m_pBitstream->DataLength = m_pBitstream->DataOffset = 0;
                if (submitted < frames_count)
                {
                    g_tsLog << "INFO: Submitting frame #" << submitted << "\n";
                    if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                    {
                        submitted++;
                        continue;
                    }
                    else
                    {
                        submitted++;
                    }
                }
                else if (m_pPar->AsyncDepth > 1)
                {
                    g_tsLog << "INFO: Draining encoded frames\n";
                    mfxStatus mfxRes_drain = EncodeFrameAsync(m_session, 0, NULL, m_pBitstream, m_pSyncPoint);
                    if (MFX_ERR_MORE_DATA == mfxRes_drain)
                    {
                        break;
                    }
                    else if (mfxRes_drain != MFX_ERR_NONE)
                    {
                        ADD_FAILURE() << "ERROR: Unexpected state on drain: " << mfxRes_drain << "\n";
                        throw tsFAIL;
                    }
                }

                g_tsStatus.check();TS_CHECK_MFX;
                sp = m_syncpoint;

                SyncOperation();TS_CHECK_MFX;
                encoded++;

                BSErr bserror = set_buffer(m_pBitstream->Data, m_pBitstream->DataLength);
                EXPECT_EQ(bserror, BS_ERR_NONE) << "ERROR: Set buffer to stream parser error!";

                bserror = parse_next_unit();
                EXPECT_EQ(bserror, BS_ERR_NONE) << "ERROR: Parsing encoded frame's header error!";

                void *ptr = get_header();
                if(ptr == nullptr) {
                    ADD_FAILURE() << "ERROR: Obtaining headers from the encoded stream error!";
                    throw tsFAIL;
                }

                BS_VP9::Frame* hdr = static_cast<BS_VP9::Frame*>(ptr);

                g_tsLog << "INFO: got encoded frame #" << hdr->FrameOrder << " of size=" << m_pBitstream->DataLength << " type=" << hdr->uh.frame_is_intra << "\n";

                if(hdr->FrameOrder > frames_count)
                {
                    ADD_FAILURE() << "ERROR: BS_VP9::Frame->FrameOrder=" << hdr->FrameOrder << " exceeds frames_count=" << frames_count;
                    throw tsFAIL;
                }

                if( (hdr->uh.frame_is_intra && m_pBitstream->FrameType != MFX_FRAMETYPE_I ) || (!hdr->uh.frame_is_intra && m_pBitstream->FrameType != MFX_FRAMETYPE_P ) )
                {
                    ADD_FAILURE() << "ERROR: m_pBitstream->FrameType (" << m_pBitstream->FrameType
                        << ") does not match to BS_VP9::Frame->uh.frame_is_intra (" << hdr->uh.frame_is_intra << ")";
                    throw tsFAIL;
                }

                frame_types_encoded[hdr->FrameOrder] = hdr->uh.frame_is_intra ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P;
                frame_sizes_encoded[hdr->FrameOrder] = m_pBitstream->DataLength;

                if(frame_types_expected[hdr->FrameOrder] != frame_types_encoded[hdr->FrameOrder])
                {
                    ADD_FAILURE() << "ERROR: GOP-pattern in the encoded frame does not match to the expected: frame["
                        << hdr->FrameOrder << "].type=" << FrameTypeByMFXEnum(frame_types_encoded[hdr->FrameOrder])
                        << ", expected " << FrameTypeByMFXEnum(frame_types_expected[hdr->FrameOrder]);
                    throw tsFAIL;
                }

            }
        }

        //print results of the encoding
        g_tsLog << "\nTEST RESULTS:\n";
        for(mfxU32 i = 0; i < frames_count; i++)
        {
            g_tsLog << "frame[" << i << "] type=" << FrameTypeByMFXEnum(frame_types_encoded[i]) << " expected_type="
                << FrameTypeByMFXEnum(frame_types_expected[i]) << " size=" << frame_sizes_encoded[i] << "\n";
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_goppattern,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_goppattern, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_goppattern,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases_ayuv);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_goppattern, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
};
