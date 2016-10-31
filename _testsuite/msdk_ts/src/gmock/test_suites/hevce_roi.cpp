/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: hevce_roi.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS (256)
#define HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS (3)
#define HEVCE_ROI_MAXIMUM_ABS_QP_VALUE (51)
#define HEVCE_ROI_MAXIMUM_ABS_QP_PRIORITY (3)

namespace hevce_roi
{
    enum
    {
        NONE,
        MFX_PAR,
        CORRECT_ROI,
        WRONG_ROI,
        WRONG_ROI_OUT_OF_IMAGE,
        CHECK_QUERY,
        MAX_SUPPORTED_REGIONS
    };

    struct tc_struct
    {
        mfxStatus   sts;
        mfxU32      type;
        mfxU32      sub_type;
        mfxU16      roi_cnt;
        mfxU32      top;
        mfxU32      left;
        mfxU32      right;
        mfxU32      bottom;
        mfxI16      prt;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        {

        }
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(unsigned int test_num, unsigned int fourcc_id);

        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_p010;
        static const unsigned int n_cases_y410;

    private:
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        //one correct region [quantity, top, left, right, bottom, qp-alter]
        {/*00*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 1, 10, 10, 100, 100, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //one correct regin [quantity, top, left, right, bottom, qp]
        {/*01*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 1, 10, 10, 100, 100, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
            }
        },

        //one region with invalid size [quantity, top, left, right, bottom, qp-alter]
        {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //too many regions [quantity, top, left, right, bottom, qp]
        {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, CORRECT_ROI, NONE, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 1, 10, 10, 100, 100, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //region with invalid demensions [quantity, top, left, right, bottom, qp]
        {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 1, 100, 100, 10, 10, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //region with invalid demensions [quantity, top, left, right, bottom, qp]
        {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 1, 100, 100, 0, 0, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //3 correct regions [quantity, top, left, right, bottom, qp-alter]
        {/*06*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 3, 1, 1, 10, 10, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //25 regions / some out of the image [quantity, top, left, right, bottom, qp-alter]
        {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI_OUT_OF_IMAGE, NONE, 3/*25*/, 1, 1, 10, 10, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //one correct region with incorrect qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, 1, 10, 10, 100, 100, HEVCE_ROI_MAXIMUM_ABS_QP_VALUE + 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //one correct region with incorrect negative qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, 1, 10, 10, 100, 100, (-1)*HEVCE_ROI_MAXIMUM_ABS_QP_VALUE - 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //one correct region with CBR [quantity, top, left, right, bottom, qp-alter]
        {/*10*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 1, 10, 10, 100, 100, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        //one correct region with CBR, incorrect qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, 1, 10, 10, 100, 100, HEVCE_ROI_MAXIMUM_ABS_QP_PRIORITY + 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        //one correct region with VBR [quantity, top, left, right, bottom, qp-alter]
        {/*12*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 1, 10, 10, 100, 100, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        //one null-region [quantity, top, left, right, bottom, qp-alter]
        {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI, NONE, 1, 0, 0, 0, 0, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //checking Query with correct region [quantity, top, left, right, bottom, qp-alter]
        {/*14*/ MFX_ERR_NONE, CHECK_QUERY, NONE, 1, 10, 10, 100, 100, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //checking Query with correct region and incorrect qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*15*/ MFX_ERR_UNSUPPORTED, CHECK_QUERY, NONE, 1, 10, 10, 100, 100, 0xff,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        //request Query for maximum supported regions/ See man for NumROI desc [quantity, top, left, right, bottom, qp-alter]
        {/*16*/ MFX_ERR_NONE, CHECK_QUERY, MAX_SUPPORTED_REGIONS, MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS, 10, 10, 100, 100, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    const unsigned int TestSuite::n_cases_nv12 = n_cases;
    const unsigned int TestSuite::n_cases_p010 = n_cases;
    const unsigned int TestSuite::n_cases_y410 = n_cases;

    struct streamDesc
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
    };

    const streamDesc streams[] = {
        {352, 288,  "YUV/foreman_352x288_300.yuv"},
        {1280, 720, "YUV10bit/Suzie_ProRes_1280x720_50f.p010.yuv"},
        {352, 288, "YUV10bit444/Kimono1_352x288_24_y410.yuv"}
    };

    const streamDesc& getStreamDesc(const mfxU32& id)
    {
        switch(id)
        {
        case MFX_FOURCC_NV12: return streams[0];
        case MFX_FOURCC_P010: return streams[1];
        case MFX_FOURCC_Y410: return streams[2];
        default: assert(0); return streams[0];
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        return RunTest(id, fourcc);
    }

    int TestSuite::RunTest(unsigned int test_num, unsigned int fourcc_id)
    {
        TS_START;

        const tc_struct& tc = test_case[test_num];
        tsSurfaceProcessor *reader;
        mfxStatus sts;

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);

        if(fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = 8;
            m_par.mfx.FrameInfo.BitDepthChroma = 8;
        } 
        else if(fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = 10;
            m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if(fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = 10;
            m_par.mfx.FrameInfo.BitDepthChroma = 10;
        } else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter!\n";
            return 0;
        }

        const char* stream = g_tsStreamPool.Get(getStreamDesc(fourcc_id).name);
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = getStreamDesc(fourcc_id).w;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = getStreamDesc(fourcc_id).h;

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_CNL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less CNL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }

            //HEVCE_HW need aligned width and height for 32
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
        }

        g_tsStreamPool.Reg();

        mfxExtEncoderROI& roi = m_par;
        roi.NumROI            = tc.roi_cnt;
        const mfxU32 multiplier = tc.type == WRONG_ROI_OUT_OF_IMAGE ? m_par.mfx.FrameInfo.Width : 1;
        const mfxU32 end_count = tc.roi_cnt > MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS ? MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS : tc.roi_cnt;
        for(mfxU32 i = 0; i < end_count; ++i)
        {
            roi.ROI[i].Top        = tc.top + i*multiplier;
            roi.ROI[i].Left       = tc.left + i*multiplier;
            roi.ROI[i].Right      = tc.right + i*multiplier;
            roi.ROI[i].Bottom     = tc.bottom + i*multiplier;
            roi.ROI[i].Priority   = tc.prt;
        }

        InitAndSetAllocator();

        g_tsStatus.expect(tc.sts);

        if(tc.type == CHECK_QUERY)
        {
            //here only Query() is checked
            tsExtBufType<mfxVideoParam> pout;
            pout.mfx.CodecId = m_par.mfx.CodecId;
            mfxExtEncoderROI& roi = pout; 
            Query(m_session, &m_par, &pout);

            if(tc.sub_type == MAX_SUPPORTED_REGIONS)
            {
                EXPECT_EQ(HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS, roi.NumROI);
            }
            else if(MFX_ERR_NONE == tc.sts) //Check that buffer was copied
            {
                EXPECT_EQ(0,(memcmp(*m_pPar->ExtParam, &roi, sizeof(mfxExtEncoderROI))));
            }
        }
        else
        {
            //here Init() and EncodeFrameAsync() are checked
            TRACE_FUNC2(MFXVideoENCODE_Init, m_session, m_pPar);
            sts = MFXVideoENCODE_Init(m_session, m_pPar);
            g_tsStatus.check(sts);

            if(sts == MFX_ERR_NONE)
            {
                m_initialized = true;

                reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
                m_filler = reader;

                int encoded = 0;
                while (encoded < 1)
                {
                    if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                    {
                        continue;
                    }

                    g_tsStatus.check(); TS_CHECK_MFX;
                    SyncOperation(); TS_CHECK_MFX;
                    encoded++;
                }

                /*
                const int encoded_size = m_pBitstream->DataLength;
                FILE *fp = fopen("c:/TEMP/hevc_roi.hevc", "wb");
                fwrite(m_pBitstream->Data, encoded_size, 1, fp);
                fclose(fp);
                */
                sts = tc.sts;

                g_tsStatus.expect(MFX_ERR_NONE);
                Close();
            }
            else
            {
                // Initialization has failed - set appropriate status for close()
                g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
                Close();
            }
        }
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_roi,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_roi, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_roi, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
};
