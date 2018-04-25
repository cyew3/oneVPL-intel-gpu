/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define NONZERO_MAX_SLICE_SIZE 100
#define NUMBER_OF_ENCODE_FRAMES 4
#define TEST_WIDTH 720
#define TEST_HEIGHT 480

/* Behavior test to check video parameter mfxExtCodingOption2.MaxSliceSize for library reaction on setting it. */

namespace hevce_max_slice_size
{
    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() {}

        struct tc_struct
        {
            struct status {
                mfxStatus query;
                mfxStatus init;
                mfxStatus encode;
                mfxStatus reset;
            } sts;
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

        int RunTest(tc_struct tc, unsigned int fourcc_id);

        static const unsigned int n_cases;

        static const tc_struct test_case[];

        enum CTRL_TYPE
        {
            QUERY  = 1,
            INIT   = 1 << 1,
            ENCODE = 1 << 2,
            RESET  = 1 << 3,

            LOWPOWER_ON  = MFX_CODINGOPTION_ON,
            LOWPOWER_OFF = MFX_CODINGOPTION_OFF
        };

        enum
        {
            MFX_PAR = 1,
            EXT_CO2,
            EXT_HEVCP,
            EXT_CO2_RESET
        };
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // Enabled MaxSliceSize with VBR
        {/*00*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET | LOWPOWER_ON, {
            { EXT_CO2, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            { EXT_CO2_RESET, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE * 5 } }
        },

        // Enabled MaxSliceSize with CQP
        {/*01*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET | LOWPOWER_ON, {
            { EXT_CO2, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { EXT_CO2_RESET, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE * 5 } }
        },

        // Enabled MaxSliceSize with CBR
        {/*02*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET | LOWPOWER_ON, {
            { EXT_CO2, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { EXT_CO2_RESET, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE * 10 } }
        },

        // Disabled MaxSliceSize without LowPower mode
        {/*03*/{ MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | LOWPOWER_ON | LOWPOWER_OFF, {
            { EXT_CO2, &tsStruct::mfxExtCodingOption2.MaxSliceSize, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } }
        },

        // Enabled MaxSliceSize with enabled SAO
        {/*04*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET | LOWPOWER_ON, {
            { EXT_CO2, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE },
            { EXT_HEVCP, &tsStruct::mfxExtHEVCParam.SampleAdaptiveOffset, MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { EXT_CO2_RESET, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE * 10 } }
        },

        // Enabled MaxSliceSize with disable SAO
        {/*05*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET | LOWPOWER_ON, {
            { EXT_CO2, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE },
            { EXT_HEVCP, &tsStruct::mfxExtHEVCParam.SampleAdaptiveOffset, MFX_SAO_DISABLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { EXT_CO2_RESET, &tsStruct::mfxExtCodingOption2.MaxSliceSize, NONZERO_MAX_SLICE_SIZE * 50 } }
        },

        // Enabled different MaxSliceSize - FrameSize
        {/*06*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET | LOWPOWER_ON, {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, TEST_WIDTH },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, TEST_HEIGHT },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, TEST_WIDTH },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, TEST_HEIGHT },
            { EXT_CO2, &tsStruct::mfxExtCodingOption2.MaxSliceSize, TEST_WIDTH * TEST_HEIGHT },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { EXT_CO2_RESET, &tsStruct::mfxExtCodingOption2.MaxSliceSize, TEST_WIDTH * TEST_HEIGHT + 1 } }
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        // Set params --------------------------------------------------------
        SETPARS(m_par, MFX_PAR);

        mfxExtCodingOption2& ext_params_CO2 = m_par;
        SETPARS(&ext_params_CO2, EXT_CO2);

        mfxExtHEVCParam& ext_params_HEVCP = m_par;
        SETPARS(&ext_params_HEVCP, EXT_HEVCP);

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
            if ((g_tsHWtype < MFX_HW_CNL)
                && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: Platform less CNL are NOT supported on VDENC!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412)
                && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are only supported on VDENC!\n\n\n";
                throw tsSKIP;
            }
            if ((g_tsHWtype == MFX_HW_CNL) && (ext_params_HEVCP.SampleAdaptiveOffset == (MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA)))
            {
                tc.sts.query = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
        }
        else {
            g_tsLog << "WARNING: only HEVCe HW plugin is tested\n";
            return 0;
        }

        if (!(tc.type & g_tsConfig.lowpower)) {
            tc.sts.query  = MFX_ERR_UNSUPPORTED;
            tc.sts.init   = MFX_ERR_INVALID_VIDEO_PARAM;

            tc.type &= ~ENCODE;
            tc.type &= ~RESET;
        }

        MFXInit();

        if (!m_loaded)
            Load();

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
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        mfxExtCodingOption2 ext_params_CO2_copy = ext_params_CO2;

        // Query -------------------------------------------------------------
        if (tc.type & QUERY) {
            g_tsStatus.expect(tc.sts.query);
            Query();
        }

        ext_params_CO2 = ext_params_CO2_copy;

        // Init --------------------------------------------------------------
        if (tc.type & INIT) {
            g_tsStatus.expect(tc.sts.init);
            Init();
        }

        // EncodeFrames ------------------------------------------------------
        if (tc.type & ENCODE) {
            g_tsStatus.expect(tc.sts.encode);
            EncodeFrames(NUMBER_OF_ENCODE_FRAMES);
        }

        // Reset -------------------------------------------------------------
        if (tc.type & RESET) {

            SETPARS(&ext_params_CO2, EXT_CO2_RESET);

            g_tsStatus.expect(tc.sts.reset);
            Reset();

            if (tc.type & ENCODE) {
                g_tsStatus.expect(tc.sts.encode);
                GetVideoParam();
                EncodeFrames(NUMBER_OF_ENCODE_FRAMES);
            }
        }

        if (m_initialized)
            Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_max_slice_size, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_max_slice_size, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p012_max_slice_size, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_max_slice_size, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_max_slice_size, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y212_max_slice_size, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_max_slice_size, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_max_slice_size, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y412_max_slice_size, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
}