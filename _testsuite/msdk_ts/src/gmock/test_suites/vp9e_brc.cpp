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

    private:
        enum
        {
            MFX_PAR,
            MFX_BS,
            MULTIPLIER_CHECK,
            NONE
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, NONE, NONE},
        {/*01*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
        {/*02 bitrate unspecified for CBR*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
        {/*03 max-bitrate unspecified for CBR*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
        {/*04 too small bitrate*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
        {/*05 too small 'InitialDelayInKB'*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 10 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
        {/*06 too small 'BufferSizeInKB'*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 10 },
            }
        },
        {/*07 'BufferSizeInKB' smaller than 'InitialDelayInKB'*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 20 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 10 },
            }
        },
        {/*08 check Multiplier for normal values*/ MFX_ERR_NONE, MULTIPLIER_CHECK,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 2 },
            }
        },
        {/*09 check Multiplier for out-of-range values*/ MFX_ERR_NONE, MULTIPLIER_CHECK,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 30000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 30000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 4 },
            }
        },
        {/*10*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE * 2 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
        {/*11 'MaxKbps' is smaller than 'TargetKbps'*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
        {/*12*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 150 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 150 },
            }
        },
        {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 0 },
            }
        },
        {/*14 QP out of range*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, VP9E_MAX_QP_VALUE + 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, VP9E_MAX_QP_VALUE },
            }
        },
        {/*15 AVBR maps to VBR with a warning*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE * 2 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },

        // Check all unsupported brc methods
        {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM } },
        {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
        {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR } },
        {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VME } },
        {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },

        {/*28 default brc-type(=unspecified brc type)*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },
/*
ICQ-based brc causes hanging of the encoding, so it is disabled until the issue solve

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

    const unsigned int TestSuite::n_cases_nv12 = n_cases;

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    struct streamDesc
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
    };

    const streamDesc streams[] = {
        {720, 480, "YUV/calendar_720x480_600_nv12.yuv"},
    };

    const streamDesc& getStreamDesc(const mfxU32& id)
    {
        switch(id)
        {
        case MFX_FOURCC_NV12: return streams[0];
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

        SETPARS(m_pPar, MFX_PAR);

        if(fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            if (fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL) // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL
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
        g_tsStatus.expect(tc.sts);
        sts = Init();

        if (tc.type == MULTIPLIER_CHECK)
        {
            mfxVideoParam new_par = {};
            GetVideoParam(m_session, &new_par); TS_CHECK_MFX

            EXPECT_EQ(new_par.mfx.TargetKbps * new_par.mfx.BRCParamMultiplier, m_pPar->mfx.TargetKbps * m_pPar->mfx.BRCParamMultiplier) << "ERROR: Multiplying did not apply to: TargetKbps";
            EXPECT_EQ(new_par.mfx.MaxKbps * new_par.mfx.BRCParamMultiplier, m_pPar->mfx.MaxKbps * m_pPar->mfx.BRCParamMultiplier) << "ERROR: Multiplying did not apply to: MaxKbps";
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

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_brc, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
};
