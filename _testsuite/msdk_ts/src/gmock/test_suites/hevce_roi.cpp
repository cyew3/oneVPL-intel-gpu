/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2018 Intel Corporation. All Rights Reserved.

File Name: hevce_roi.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS (256)

#if defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN) || defined (LINUX_TARGET_PLATFORM_CFL)
#define HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS (8)
#else
#define HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS (3)
#endif // defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN)

#define TBD_ON_RUNTIME (0xffff)
#define HEVCE_ROI_MAXIMUM_ABS_QP_VALUE (51)
#define HEVCE_ROI_MAXIMUM_ABS_QP_PRIORITY (3)

namespace hevce_roi
{
    enum
    {
        NONE,
        MFX_PAR,
        WRONG_ROI_OUT_OF_IMAGE,
        CHECK_QUERY,
        MAX_SUPPORTED_REGIONS,
        CAPS_NUM_ROI_EQ_MAX, // NumROI will be queried from caps in runtime for this type of test
        CAPS_NUM_ROI_GT_MAX, // NumROI will be queried from caps in runtime for this type of test
        NOT_ALIGNED_ROI
    };

    struct tc_struct
    {
        mfxStatus   sts;
        mfxU32      type;
        mfxU32      sub_type;
        mfxU16      roi_mode;
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

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        // one correct region [quantity, top, left, right, bottom, qp-alter]
        {/*00*/ MFX_ERR_NONE, NONE, NONE, 0, 1, 32, 32, 64, 64, 11 },

        // one correct region with not aligned coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, 0, 1, 10, 20, 120, 150, 11, },

        // one region with out of range coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 0, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 11 },

        // checking Query with unsupported ROI quantity [quantity, top, left, right, bottom, qp-alter]
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, CAPS_NUM_ROI_GT_MAX, 0, TBD_ON_RUNTIME, 32, 32, 64, 64, 1 },

        // unsupported ROI quantity [quantity, top, left, right, bottom, qp-alter]
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, CAPS_NUM_ROI_GT_MAX, 0, TBD_ON_RUNTIME, 32, 32, 64, 64, 1 },

        // checking maximum supported regions from Caps on Query [quantity, top, left, right, bottom, qp-alter]
        {/*05*/ MFX_ERR_NONE, CHECK_QUERY, CAPS_NUM_ROI_EQ_MAX, 0, TBD_ON_RUNTIME, 32, 32, 64, 64, 1 },

        // checking maximum supported regions from Caps on Encode [quantity, top, left, right, bottom, qp-alter]
        {/*06*/ MFX_ERR_NONE, NONE, CAPS_NUM_ROI_EQ_MAX, 0, TBD_ON_RUNTIME, 32, 32, 64, 64, 1 },

        // one region with invalid dimensions [quantity, top, left, right, bottom, qp-alter]
        {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 0, 1, 128, 128, 32, 32, 11 },

        // region with invalid unaligned coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 0, 1, 111, 13, 3, 1, 11},

        // three correct regions [quantity, top, left, right, bottom, qp-alter]
        {/*09*/ MFX_ERR_NONE, NONE, NONE, 0, 3, 32, 32, 64, 64, 11 },

        // three regions where some regions are out of the image [quantity, top, left, right, bottom, qp-alter]
        {/*10*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI_OUT_OF_IMAGE, NONE, 0, 3, 32, 32, 64, 64, 13 },

        // one correct region with incorrect positive qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 0, 1, 32, 32, 128, 128, HEVCE_ROI_MAXIMUM_ABS_QP_VALUE + 1 },

        // one correct region with incorrect negative qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 0, 1, 32, 32, 64, 64, (-1)*HEVCE_ROI_MAXIMUM_ABS_QP_VALUE - 1 },

        // one correct region with CBR [quantity, top, left, right, bottom, qp-alter]
        // CBR is no more supported - all new drivers are reporting caps.ROIBRCPriorityLevelSupport = 0.
        {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 0, 1, 32, 32, 128, 128, 1,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // one null-region [quantity, top, left, right, bottom, qp-alter]
        {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 0, 1, 0, 0, 0, 0, 11 },

        // checking Query with correct region [quantity, top, left, right, bottom, qp-alter]
        {/*15*/ MFX_ERR_NONE, CHECK_QUERY, NONE, 0, 1, 32, 32, 128, 128, 11 },

        // checking Query with correct region and incorrect qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*16*/ MFX_ERR_UNSUPPORTED, CHECK_QUERY, NONE, 0, 1, 32, 32, 128, 128, 0xff },

        // requesting Query for maximum supported regions [quantity, top, left, right, bottom, qp-alter]
        {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, MAX_SUPPORTED_REGIONS, 0, MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS, 32, 32, 64, 64, 1 },

        // checking Query with one region with not aligned coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*18*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, NOT_ALIGNED_ROI, 0, 1, 10, 20, 30, 40, 1 },

#if defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN) || defined(LINUX_TARGET_PLATFORM_CFL)
#if MFX_VERSION > 1021
        // one correct delta QP based region in CBR
        {/*19*/ MFX_ERR_NONE, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 64, 64, 20,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // one correct delta QP based region in VBR
        {/*20*/ MFX_ERR_NONE, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 64, 64, -20,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // one delta QP based region with not aligned coordinates in CBR
        {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 13, 30, 110, 111, 20,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // one delta QP based region with not aligned coordinates in VBR
        {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 10, 10, 20, 20, -20,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // one region with invalid size in CBR
        {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // one region with invalid size in VBR
        {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // unsupported number of regions in CBR
        {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 1, 32, 32, 64, 64, 15,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // unsupported number of regions in VBR
        {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 23, 32, 32, 64, 64, 15,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // one region with invalid dimensions in CBR
        {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 64, 64, 32, 32, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // region with invalid dimensions in VBR
        {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 128, 128, 32, 32, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // three correct regions in CBR
        {/*29*/ MFX_ERR_NONE, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 64, 64, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // three correct regions in VBR
        {/*30*/ MFX_ERR_NONE, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 64, 64, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // three regions where some regions are out of the image in CBR
        {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI_OUT_OF_IMAGE, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 128, 128, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // three regions where some regions are out of the image in VBR
        {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI_OUT_OF_IMAGE, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 64, 64, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // one correct region with incorrect positive QP delta in CBR
        {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 128, 128, HEVCE_ROI_MAXIMUM_ABS_QP_VALUE + 1,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // one correct region with incorrect negative QP delta in VBR
        {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 64, 64, (-1)*HEVCE_ROI_MAXIMUM_ABS_QP_VALUE - 1,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // one null-region in CBR
        {/*35*/ MFX_ERR_NONE, NONE, NONE, MFX_ROI_MODE_QP_DELTA, 1, 0, 0, 0, 0, 11,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // checking Query with correct region in CBR
        {/*35*/ MFX_ERR_NONE, CHECK_QUERY, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 128, 128, 21,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // checking Query with correct region and incorrect QP delta in VBR
        {/*36*/ MFX_ERR_UNSUPPORTED, CHECK_QUERY, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 64, 128, 0xff,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // requesting Query for maximum supported regions in CBR
        {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, MAX_SUPPORTED_REGIONS, MFX_ROI_MODE_QP_DELTA, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 5, 16, 16, 64, 64, 1,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // requesting Query for maximum supported regions in VBR
        {/*38*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, MAX_SUPPORTED_REGIONS, MFX_ROI_MODE_QP_DELTA, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 5, 16, 16, 64, 64, 1,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
        }
        },

        // checking Query with one region with not aligned coordinates in CBR
        {/*39*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, NOT_ALIGNED_ROI, MFX_ROI_MODE_QP_DELTA, 1, 11, 11, 33, 33, 21,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },

        // checking Query with one region with not aligned coordinates in VBR
        {/*40*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, NOT_ALIGNED_ROI, MFX_ROI_MODE_QP_DELTA, 1, 10, 20, 30, 40, 21,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        }
        },
#endif // #if  MFX_VERSION > 1021
#endif  // defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN)
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    struct streamDesc
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
    };

    const streamDesc streams[] = {
        {352, 288, "YUV/foreman_352x288_300.yuv"},
        {1280,720, "YUV10bit/Suzie_ProRes_1280x720_50f.p010.yuv"},
        {352, 288, "YUV8bit422/Kimono1_352x288_24_yuy2.yuv" },
        {352, 288, "YUV10bit422/Kimono1_352x288_24_y210.yuv" },
        {352, 288, "YUV10bit444/Kimono1_352x288_24_y410.yuv"},
        {352, 288, "YUV8bit444/Kimono1_352x288_24_ayuv.yuv" },
        {176,144, "YUV16bit420/FruitStall_176x144_240_p016.yuv"},
        {176,144, "YUV16bit422/FruitStall_176x144_240_y216.yuv"},
        {176,144, "YUV16bit444/FruitStall_176x144_240_y416.yuv"},
    };

    const streamDesc& getStreamDesc(const mfxU32& id)
    {
        switch(id)
        {
            case MFX_FOURCC_NV12: return streams[0];
            case MFX_FOURCC_P010: return streams[1];
            case MFX_FOURCC_YUY2: return streams[2];
            case MFX_FOURCC_Y210: return streams[3];
            case MFX_FOURCC_Y410: return streams[4];
            case MFX_FOURCC_AYUV: return streams[5];
            case GMOCK_FOURCC_P012: return streams[6];
            case GMOCK_FOURCC_Y212: return streams[7];
            case GMOCK_FOURCC_Y412: return streams[8];
            default: assert(0); return streams[0];
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        tsSurfaceProcessor *reader;
        mfxStatus sts;

        if ((0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
            if ((fourcc_id == MFX_FOURCC_P010)
                && (g_tsHWtype < MFX_HW_KBL)) {
                g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_Y210 || fourcc_id == MFX_FOURCC_YUY2 ||
                fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410)
                && (g_tsHWtype < MFX_HW_ICL))
            {
                g_tsLog << "\n\nWARNING: RExt formats are only supported starting from ICL!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == GMOCK_FOURCC_P012 || fourcc_id == GMOCK_FOURCC_Y212 || fourcc_id == GMOCK_FOURCC_Y412)
                && (g_tsHWtype < MFX_HW_TGL))
            {
                g_tsLog << "\n\nWARNING: 12b formats are only supported starting from TGL!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_YUY2 || fourcc_id == MFX_FOURCC_Y210 || fourcc_id == GMOCK_FOURCC_Y212)
                && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 4:2:2 formats are NOT supported on VDENC!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412)
                && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are only supported on VDENC!\n\n\n";
                throw tsSKIP;
            }
        }

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == GMOCK_FOURCC_P012)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y212)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y412)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        const char* stream = g_tsStreamPool.Get(getStreamDesc(fourcc_id).name);
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = getStreamDesc(fourcc_id).w;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = getStreamDesc(fourcc_id).h;

        g_tsStreamPool.Reg();

        ENCODE_CAPS_HEVC caps = {};

        if (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)
        {
            mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);
            g_tsStatus.check(GetCaps(&caps, &capSize));
        }
        else
        {
            caps.MaxNumOfROI = HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS;
        }

        mfxExtEncoderROI& roi = m_par;

        if (tc.sub_type == CAPS_NUM_ROI_GT_MAX)
        {
            EXPECT_EQ(tc.roi_cnt, TBD_ON_RUNTIME);
            roi.NumROI = caps.MaxNumOfROI + 1;
        }
        else if (tc.sub_type == CAPS_NUM_ROI_EQ_MAX)
        {
            EXPECT_EQ(tc.roi_cnt, TBD_ON_RUNTIME);
            roi.NumROI = caps.MaxNumOfROI;
        }
        else
        {
            EXPECT_NE(tc.roi_cnt, TBD_ON_RUNTIME);
            roi.NumROI = tc.roi_cnt;
        }

#if MFX_VERSION > 1021
        roi.ROIMode = tc.roi_mode;
#endif // MFX_VERSION > 1021
        const mfxU32 multiplier = tc.type == WRONG_ROI_OUT_OF_IMAGE ? m_par.mfx.FrameInfo.Width : 32;
        const mfxU32 end_count = roi.NumROI > MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS ? MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS : roi.NumROI;
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

            if (tc.sub_type == CAPS_NUM_ROI_GT_MAX || tc.sub_type == CAPS_NUM_ROI_EQ_MAX || tc.sub_type == MAX_SUPPORTED_REGIONS)
            {
                EXPECT_EQ(caps.MaxNumOfROI, roi.NumROI); //check that after Query() NumROI is equal to max supported value 
            }
            else if(tc.sub_type == NOT_ALIGNED_ROI)
            {
                EXPECT_NE(0,(memcmp(*m_pPar->ExtParam, &roi, sizeof(mfxExtEncoderROI))));
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

            if(sts == MFX_ERR_NONE || sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
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

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_roi, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_roi, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_roi, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_roi, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_roi, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_roi, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_roi, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_roi, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_roi, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
};
