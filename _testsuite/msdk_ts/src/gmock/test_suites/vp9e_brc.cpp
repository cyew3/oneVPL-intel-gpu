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

namespace vp9e_brc
{
#define VP9E_DEFAULT_BITRATE (2000)
#define VP9E_MAX_ICQ_QUALITY_INDEX (255)
#define VP9E_MAX_QP_VALUE (255)

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

        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_p010;
        static const unsigned int n_cases_ayuv;
        static const unsigned int n_cases_y410;

    private:
        enum
        {
            MFX_PAR,
            MFX_BS,
            MULTIPLIER_CHECK,
            CHECK_BITRATE_DYNAMIC_CHANGE,
            NONE
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        {/*01 bitrate unspecified for CBR*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },
            }
        },

        {/*02 MaxKbps is less than TargetKbps CBR*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE / 2 },
            }
        },

        {/*03 too small 'InitialDelayInKB'*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 10 },
            }
        },

        {/*04 too small 'BufferSizeInKB'*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 10 },
            }
        },

        {/*05 'BufferSizeInKB' smaller than 'InitialDelayInKB'*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 20 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 10 },
            }
        },

        {/*06 check Multiplier for normal values*/ MFX_ERR_NONE, MULTIPLIER_CHECK,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 2 },
            }
        },

        {/*07 check Multiplier for out-of-range values*/ MFX_ERR_NONE, MULTIPLIER_CHECK,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 30000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 4 },
            }
        },

        {/*08*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE * 2 },
            }
        },

        {/*09 'MaxKbps' is smaller than 'TargetKbps'*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE / 2 },
            }
        },

        {/*10*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 150 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 150 },
            }
        },

        {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 0 },
            }
        },

        {/*12 QP out of range*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, VP9E_MAX_QP_VALUE + 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, VP9E_MAX_QP_VALUE },
            }
        },

        // Check all unsupported brc methods
        {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM } },
        {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
        {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR } },
        {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VME } },
        {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },

        {/*26 unspecified both brc-type and kbps)*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 0 },
            }
        },

        {/*27 unspecified brc type, but correct kbps => map to CBR)*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        {/*28*/ MFX_ERR_NONE, CHECK_BITRATE_DYNAMIC_CHANGE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        {/*29*/ MFX_ERR_NONE, CHECK_BITRATE_DYNAMIC_CHANGE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE * 2 },
            }
        },

/*
// This case is working on the first post-Si CNL-hardware, but there are issues on CNL-Simics (switched off until post-Si validations)
        {too small bitrate MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
*/

/*
ICQ-based brc is not supported by the driver now, cases are left for the future using
        { MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, MFX_RATECONTROL_ICQ },
            }
        },
        { MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 0 },
            }
        },
        { MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, VP9E_MAX_ICQ_QUALITY_INDEX + 1 },
            }
        }
*/
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const unsigned int TestSuite::n_cases_nv12 = n_cases;
    const unsigned int TestSuite::n_cases_p010 = n_cases;
    const unsigned int TestSuite::n_cases_ayuv = n_cases;
    const unsigned int TestSuite::n_cases_y410 = n_cases;

    struct streamDesc
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
    };

    const streamDesc streams[] = {
        { 720, 480, "YUV/calendar_720x480_600_nv12.yuv"},
        { 352, 288, "YUV10bit420_ms/Kimono1_352x288_24_p010_shifted.yuv" },
        { 352, 288, "YUV8bit444/Kimono1_352x288_24_ayuv.yuv" },
        { 352, 288, "YUV10bit444/Kimono1_352x288_24_y410.yuv" },
    };

    const streamDesc& getStreamDesc(const mfxU32& id)
    {
        switch(id)
        {
            case MFX_FOURCC_NV12: return streams[0];
            case MFX_FOURCC_P010: return streams[1];
            case MFX_FOURCC_AYUV: return streams[2];
            case MFX_FOURCC_Y410: return streams[3];
            default: assert(0); return streams[0];
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
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
        m_par.mfx.InitialDelayInKB = m_par.mfx.BufferSizeInKB = 0;

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

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL (ICL for REXTs)
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
        else
        {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        //init
        g_tsStatus.expect(tc.sts);
        sts = Init();

        if (tc.type == MULTIPLIER_CHECK)
        {
            mfxVideoParam new_par = {};
            GetVideoParam(m_session, &new_par); TS_CHECK_MFX

            EXPECT_EQ(new_par.mfx.TargetKbps * new_par.mfx.BRCParamMultiplier, m_pPar->mfx.TargetKbps * m_pPar->mfx.BRCParamMultiplier) << "ERROR: Multiplying did not apply to: TargetKbps";
            if (m_pPar->mfx.MaxKbps)
            {
                EXPECT_EQ(new_par.mfx.MaxKbps * new_par.mfx.BRCParamMultiplier, m_pPar->mfx.MaxKbps * m_pPar->mfx.BRCParamMultiplier) << "ERROR: Multiplying did not apply to: MaxKbps";
            }
            EXPECT_EQ(new_par.mfx.InitialDelayInKB * new_par.mfx.BRCParamMultiplier, m_pPar->mfx.InitialDelayInKB * m_pPar->mfx.BRCParamMultiplier) << "ERROR: Multiplying did not apply to: InitialDelayInKB";
            EXPECT_EQ(new_par.mfx.BufferSizeInKB * new_par.mfx.BRCParamMultiplier, m_pPar->mfx.BufferSizeInKB * m_pPar->mfx.BRCParamMultiplier) << "ERROR: Multiplying did not apply to: BufferSizeInKB";
        }

        if (sts == MFX_ERR_NONE || sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            unsigned  encoded = 0;
            const unsigned encode_frames_count = 1;
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
                if (bserror != BS_ERR_NONE)
                {
                    ADD_FAILURE() << "ERROR: Set encoded buffer to stream parser failed!\n";
                    throw tsFAIL;
                }

                bserror = parse_next_unit();
                if (bserror != BS_ERR_NONE)
                {
                    ADD_FAILURE() << "ERROR: Parsing encoded frame header failed!\n";
                    throw tsFAIL;
                }

                void *ptr = get_header();
                if (ptr == nullptr) {
                    ADD_FAILURE() << "ERROR: Obtaining header from the encoded stream failed!";
                    throw tsFAIL;
                }

                BS_VP9::Frame* hdr = static_cast<BS_VP9::Frame*>(ptr);
                if (fourcc_id == MFX_FOURCC_NV12)
                {
                    if (hdr->uh.profile != 0)
                    {
                        ADD_FAILURE() << "ERROR: unc-header profile mismatch, expected 0, detected " << hdr->uh.profile;
                        throw tsFAIL;
                    }
                }
                else if (fourcc_id == MFX_FOURCC_P010)
                {
                    if (hdr->uh.profile != 2)
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

            g_tsStatus.expect(MFX_ERR_NONE);
        } else {
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        }

        if (tc.type == CHECK_BITRATE_DYNAMIC_CHANGE)
        {
            DrainEncodedBitstream();

            for (mfxU16 i = 2; i <= 3; i++)
            {
                // now increasing bitrate and encode one more frame
                m_par.mfx.TargetKbps = m_par.mfx.TargetKbps * i;
                m_par.mfx.MaxKbps = m_par.mfx.MaxKbps ? (m_par.mfx.MaxKbps * i) : 0;

                mfxStatus reset_status = Reset();
                if (reset_status == MFX_ERR_NONE)
                {
                    mfxVideoParam new_par = {};
                    GetVideoParam(m_session, &new_par); TS_CHECK_MFX

                    if (new_par.mfx.TargetKbps != m_par.mfx.TargetKbps)
                    {
                        ADD_FAILURE() << "ERROR: TargetKbps before Reset() is " << m_par.mfx.TargetKbps << " but GetVideoParam() after Reset() has " << new_par.mfx.TargetKbps;
                        throw tsFAIL;
                    }

                    if (m_par.mfx.MaxKbps && new_par.mfx.MaxKbps != m_par.mfx.MaxKbps)
                    {
                        ADD_FAILURE() << "ERROR: MaxKbpsKbps before Reset() is " << m_par.mfx.MaxKbps << " but GetVideoParam() after Reset() has " << new_par.mfx.MaxKbps;
                        throw tsFAIL;
                    }

                    m_pBitstream->DataLength = m_pBitstream->DataOffset = 0;
                    mfxStatus second_encode_status = EncodeFrames(2);
                    if (second_encode_status == MFX_ERR_NONE)
                    {
                        DrainEncodedBitstream();
                    }
                    else
                    {
                        ADD_FAILURE() << "ERROR: EncodeFrameAsync() after changing bitrate returned " << reset_status;
                        throw tsFAIL;
                    }
                }
                else
                {
                    ADD_FAILURE() << "ERROR: Reset() with chaning bitrate returned " << reset_status;
                    throw tsFAIL;
                }
            }

        }

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_brc,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_brc, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_brc,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases_ayuv);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_brc, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
};
