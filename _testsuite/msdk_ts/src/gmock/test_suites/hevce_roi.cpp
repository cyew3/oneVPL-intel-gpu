/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: hevce_roi.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS (256)

#if defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN)
#define HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS (8)
#else
#define HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS (3)
#endif // defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN)

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
        MAX_SUPPORTED_REGIONS,
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
        // one correct region [quantity, top, left, right, bottom, qp-alter]
        {/*00*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 0, 1, 32, 32, 64, 64, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // one correct region with not aligned coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, 0, 1, 10, 20, 120, 150, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // one region with out of range coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, 0, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // unsupported ROI quantity [quantity, top, left, right, bottom, qp-alter]
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, 0, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 1, 32, 32, 64, 64, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // one region with invalid dimensions [quantity, top, left, right, bottom, qp-alter]
        {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 0, 1, 128, 128, 32, 32, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // region with invalid unaligned coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 0, 1, 111, 13, 3, 1, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // three correct regions [quantity, top, left, right, bottom, qp-alter]
        {/*06*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 0, 3, 32, 32, 64, 64, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // three regions where some regions are out of the image [quantity, top, left, right, bottom, qp-alter]
        {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI_OUT_OF_IMAGE, NONE, 0, 3, 32, 32, 64, 64, 13,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // one correct region with incorrect positive qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 0, 1, 32, 32, 128, 128, HEVCE_ROI_MAXIMUM_ABS_QP_VALUE + 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // one correct region with incorrect negative qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 0, 1, 32, 32, 64, 64, (-1)*HEVCE_ROI_MAXIMUM_ABS_QP_VALUE - 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // one correct region with CBR [quantity, top, left, right, bottom, qp-alter]
        {/*10*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 0, 1, 32, 32, 128, 128, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // one correct region with CBR, incorrect qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, 0, 1, 32, 32, 64, 64, HEVCE_ROI_MAXIMUM_ABS_QP_PRIORITY + 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // one correct region with VBR [quantity, top, left, right, bottom, qp-alter]
        {/*12*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 0, 1, 32, 32, 64, 64, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // one null-region [quantity, top, left, right, bottom, qp-alter]
        {/*13*/ MFX_ERR_NONE, CORRECT_ROI, NONE, 0, 1, 0, 0, 0, 0, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // checking Query with correct region [quantity, top, left, right, bottom, qp-alter]
        {/*14*/ MFX_ERR_NONE, CHECK_QUERY, NONE, 0, 1, 32, 32, 128, 128, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // checking Query with correct region and incorrect qp-alter [quantity, top, left, right, bottom, qp-alter]
        {/*15*/ MFX_ERR_UNSUPPORTED, CHECK_QUERY, NONE, 0, 1, 32, 32, 128, 128, 0xff,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // requesting Query for maximum supported regions [quantity, top, left, right, bottom, qp-alter]
        {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, MAX_SUPPORTED_REGIONS, 0, MEDIASDK_API_MAXIMUM_SUPPORTED_REGIONS, 32, 32, 64, 64, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

        // checking Query with one region with not aligned coordinates [quantity, top, left, right, bottom, qp-alter]
        {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY, NOT_ALIGNED_ROI, 0, 1, 10, 20, 30, 40, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            }
        },

#if defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN)
#if MFX_VERSION > 1021
        // one correct delta QP based region in CBR
        {/*18*/ MFX_ERR_NONE, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 64, 64, 20,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // one correct delta QP based region in VBR
        {/*19*/ MFX_ERR_NONE, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 64, 64, -20,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // one delta QP based region with not aligned coordinates in CBR
        {/*20*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 13, 30, 110, 111, 20,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // one delta QP based region with not aligned coordinates in VBR
        {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 10, 10, 20, 20, -20,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // one region with invalid size in CBR
        {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // one region with invalid size in VBR
        {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // unsupported number of regions in CBR
        {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 1, 32, 32, 64, 64, 15,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // unsupported number of regions in VBR
        {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, HEVCE_ROI_MAXIMUM_SUPPORTED_REGIONS + 23, 32, 32, 64, 64, 15,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // one region with invalid dimensions in CBR
        {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 64, 64, 32, 32, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // region with invalid dimensions in VBR
        {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 128, 128, 32, 32, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // three correct regions in CBR
        {/*28*/ MFX_ERR_NONE, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 64, 64, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // three correct regions in VBR
        {/*29*/ MFX_ERR_NONE, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 64, 64, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // three regions where some regions are out of the image in CBR
        {/*30*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI_OUT_OF_IMAGE, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 128, 128, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // three regions where some regions are out of the image in VBR
        {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, WRONG_ROI_OUT_OF_IMAGE, NONE, MFX_ROI_MODE_QP_DELTA, 3, 32, 32, 64, 64, 11,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // one correct region with incorrect positive QP delta in CBR
        {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 128, 128, HEVCE_ROI_MAXIMUM_ABS_QP_VALUE + 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            }
        },

        // one correct region with incorrect negative QP delta in VBR
        {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, WRONG_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 32, 32, 64, 64, (-1)*HEVCE_ROI_MAXIMUM_ABS_QP_VALUE - 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            }
        },

        // one null-region in CBR
        {/*34*/ MFX_ERR_NONE, CORRECT_ROI, NONE, MFX_ROI_MODE_QP_DELTA, 1, 0, 0, 0, 0, 11,
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
            if (g_tsHWtype < MFX_HW_CNL && g_tsHWtype != MFX_HW_APL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less CNL and not APL
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
#if MFX_VERSION > 1021
        roi.ROIMode           = tc.roi_mode;
#endif // MFX_VERSION > 1021
        const mfxU32 multiplier = tc.type == WRONG_ROI_OUT_OF_IMAGE ? m_par.mfx.FrameInfo.Width : 32;
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

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_roi,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_roi, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_roi, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
};
