/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_multislice
{
    enum
    {
        MFX_PAR = 1,
        EXT_COD2
    };

    enum TYPE {
        QUERY = 0x1,
        INIT = 0x2,
        ENCODE = 0x4,
        RESET = 0x8
    };

    struct tc_struct {
        struct status {
            mfxStatus query;
            mfxStatus init;
            mfxStatus encode;
            mfxStatus reset;
        } sts;
        mfxU32 type;
        struct tctrl {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, public tsParserHEVC {
    public:
        TestSuite() :
            tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC()
        {
        }
        ~TestSuite() { }

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);
        int RunTest(tc_struct tc, unsigned int fourcc_id);

        static const unsigned int n_cases;
        static const tc_struct test_case[];
    };

#define mfx_NumSlice        tsStruct::mfxVideoParam.mfx.NumSlice
#define mfx_NumMbPerSlice   tsStruct::mfxExtCodingOption2.NumMbPerSlice
#define mfx_Width           tsStruct::mfxVideoParam.mfx.FrameInfo.Width
#define mfx_Height          tsStruct::mfxVideoParam.mfx.FrameInfo.Height
#define mfx_CropW           tsStruct::mfxVideoParam.mfx.FrameInfo.CropW
#define mfx_CropH           tsStruct::mfxVideoParam.mfx.FrameInfo.CropH

#define SET_RESOLUTION(W, H) { MFX_PAR, &mfx_Width,  W}, \
                             { MFX_PAR, &mfx_Height, H}, \
                             { MFX_PAR, &mfx_CropW,  W}, \
                             { MFX_PAR, &mfx_CropH,  H}

#define SET_RESOLUTION_736x480   SET_RESOLUTION(736,480)
#define SET_RESOLUTION_1280x720  SET_RESOLUTION(1280,720)
#define SET_RESOLUTION_1920x1088 SET_RESOLUTION(1920,1088)

    const tc_struct TestSuite::test_case[] =
    {
        // 1. Resolution 736x480

        // 1.1. Positive tests

        {/*00*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { MFX_PAR, &mfx_NumSlice, 1 },
        } },

        {/*01*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { MFX_PAR, &mfx_NumSlice, 2 },
        } },

        {/*02*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { MFX_PAR, &mfx_NumSlice, 3 },
        } },

        {/*03*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { MFX_PAR, &mfx_NumSlice, 4 },
        } },

        {/*04*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { MFX_PAR, &mfx_NumSlice, 8 },
        } },

        {/*05*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { EXT_COD2, &mfx_NumMbPerSlice, 12 },
        } },

        {/*06*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { EXT_COD2, &mfx_NumMbPerSlice, 24 },
        } },

        {/*07*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { EXT_COD2, &mfx_NumMbPerSlice, 36 },
        } },

        {/*08*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { EXT_COD2, &mfx_NumMbPerSlice, 48 },
        } },

        {/*09*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { EXT_COD2, &mfx_NumMbPerSlice, CEIL_DIV(736,64) * CEIL_DIV(480,64) }, // check max NumMbPerSlice
        } },

        // 1.2. Negative tests

        {/*10*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { MFX_PAR, &mfx_NumSlice, 5 },
        } },

        {/*11*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { MFX_PAR, &mfx_NumSlice, 10 },
        } },

        {/*12*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_736x480,
            { EXT_COD2, &mfx_NumMbPerSlice, CEIL_DIV(736,64) * CEIL_DIV(480,64) + 1 }, // check max NumMbPerSlice + 1
        } },

        // 2. Resolution 1280x720

        // 2.1. Positive tests

        {/*13*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { MFX_PAR, &mfx_NumSlice, 1 },
        } },

        {/*14*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { MFX_PAR, &mfx_NumSlice, 2 },
        } },

        {/*15*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { MFX_PAR, &mfx_NumSlice, 3 },
        } },

        {/*16*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { MFX_PAR, &mfx_NumSlice, 4 },
        } },

        {/*17*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { MFX_PAR, &mfx_NumSlice, 6 },
        } },

        {/*18*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { MFX_PAR, &mfx_NumSlice, 12 },
        } },

        {/*19*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { EXT_COD2, &mfx_NumMbPerSlice, 20 },
        } },

        {/*20*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { EXT_COD2, &mfx_NumMbPerSlice, 40 },
        } },

        {/*21*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { EXT_COD2, &mfx_NumMbPerSlice, 60 },
        } },

        {/*22*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { EXT_COD2, &mfx_NumMbPerSlice, 80 },
        } },

        {/*23*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { EXT_COD2, &mfx_NumMbPerSlice, CEIL_DIV(1280,64) * CEIL_DIV(720,64) }, // check max NumMbPerSlice
        } },

        // 2.2. Negative tests

        {/*24*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { MFX_PAR, &mfx_NumSlice, 15 },
        } },

        {/*25*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { EXT_COD2, &mfx_NumMbPerSlice, 25 },
        } },

        {/*26*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1280x720,
            { EXT_COD2, &mfx_NumMbPerSlice, CEIL_DIV(1280,64) * CEIL_DIV(720,64) + 1 }, // check max NumMbPerSlice + 1
        } },

        // 3. Resolution 1920x1088

        // 3.1. Positive tests

        {/*27*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 1 },
        } },

        {/*28*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 2 },
        } },

        {/*29*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 3 },
        } },

        {/*30*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 4 },
        } },

        {/*31*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 5 },
        } },

        {/*32*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 6 },
        } },

        {/*33*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 9 },
        } },

        {/*34*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 17 },
        } },

        {/*35*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { EXT_COD2, &mfx_NumMbPerSlice, 30 },
        } },

        {/*36*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { EXT_COD2, &mfx_NumMbPerSlice, 60 },
        } },

        {/*37*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { EXT_COD2, &mfx_NumMbPerSlice, 90 },
        } },

        {/*38*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { EXT_COD2, &mfx_NumMbPerSlice, CEIL_DIV(1920,64) * CEIL_DIV(1088,64) }, // check max NumMbPerSlice
        } },

        // 3.2. Negative tests

        {/*39*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { MFX_PAR, &mfx_NumSlice, 20 },
        } },

        {/*40*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { EXT_COD2, &mfx_NumMbPerSlice, 35 },
        } },

        {/*41*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
        QUERY | INIT | ENCODE | RESET,{
            SET_RESOLUTION_1920x1088,
            { EXT_COD2, &mfx_NumMbPerSlice, CEIL_DIV(1920,64) * CEIL_DIV(1088,64) + 1 }, // check max NumMbPerSlice + 1
        } },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id) {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id) {
        TS_START;

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }

        if (g_tsImpl == MFX_IMPL_HARDWARE) {
            if (g_tsHWtype < MFX_HW_CNL) {
                g_tsLog << "SKIPPED for this platform\n";
                return 0;
            }
        }

        mfxStatus sts = MFX_ERR_NONE;

        MFXInit();
        set_brc_params(&m_par);

        mfxExtCodingOption2& co2 = m_par;
        SETPARS(&co2, EXT_COD2)

        Load();

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        if (tc.type & QUERY) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(&co2, EXT_COD2);
            g_tsStatus.expect(tc.sts.query);
            Query();
        }

        if (tc.type & INIT) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(&co2, EXT_COD2);
            g_tsStatus.expect(tc.sts.init);
            Init();
        }

        if (tc.type & ENCODE) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(&co2, EXT_COD2);
            g_tsStatus.expect(tc.sts.encode);
            EncodeFrameAsync();
        }

        if (tc.type & RESET) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(&co2, EXT_COD2);
            g_tsStatus.expect(tc.sts.reset);
            Reset();
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_420_nv12_multislice,  RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_multislice, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_multislice,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_multislice, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_multislice,  RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_multislice, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
}