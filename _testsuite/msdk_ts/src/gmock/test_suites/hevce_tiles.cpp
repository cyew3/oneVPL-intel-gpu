/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018-2019 Intel Corporation. All Rights Reserved.

File Name: hevce_tiles.cpp

*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"


namespace hevce_tiles {
#define TILES_PARS(type, rows, columns)                                 \
            {type, &tsStruct::mfxExtHEVCTiles.NumTileRows,     rows},   \
            {type, &tsStruct::mfxExtHEVCTiles.NumTileColumns,  columns}


#define CHECK_PARS(expected, actual) {                                                                           \
    EXPECT_EQ(expected.NumTileRows, actual.NumTileRows)                                                                        \
        << "ERROR: Expect rows = " << expected.NumTileRows << ", but real rows = " << actual.NumTileRows << "!\n";             \
    EXPECT_EQ(expected.NumTileColumns, actual.NumTileColumns)                                                                  \
        << "ERROR: Expect columns = " << expected.NumTileColumns << ", but real columns = " << actual.NumTileColumns << "!\n"; \
}

    enum {
        MFX_PAR = 1,
        TILES,
        TILES_EXPECTED,
        TILES_EXPECTED_UNSUP,
        TILES_EXPECTED_VDENC
    };

    enum TYPE {
        QUERY = 0x1,
        INIT = 0x2,
        ENCODE = 0x4
    };

    enum SUB_TYPE {
        NONE,
        TRIVIAL,
        UNSUP,
        GT_MAX_ROW,
        GT_MAX_COL,
        GT_MAX_COL_WRN,
        MIN_TILE_SIZE,
        NxN
    };

    struct tc_struct {
        struct status {
            mfxStatus query;
            mfxStatus init;
            mfxStatus encode;
        } sts;
        mfxU32 type;
        mfxU32 sub_type;
        struct tctrl {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite :tsVideoEncoder {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);
        int RunTest(tc_struct tc, unsigned int fourcc_id);

        static const unsigned int n_cases;
        void initParams();
        static const tc_struct test_case[];
    };

    void TestSuite::initParams() {
        m_par.mfx.CodecId = MFX_CODEC_HEVC;
        m_par.mfx.FrameInfo.Width = 640;
        m_par.mfx.FrameInfo.Height = 480;
        m_par.mfx.FrameInfo.CropW = 640;
        m_par.mfx.FrameInfo.CropH = 480;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, TRIVIAL, {
            TILES_PARS(TILES,          1, 1),
            TILES_PARS(TILES_EXPECTED, 1, 1)
        } },

        {/*01*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, UNSUP, {
            TILES_PARS(TILES,          2, 1),
            TILES_PARS(TILES_EXPECTED, 2, 1),
            TILES_PARS(TILES_EXPECTED_UNSUP, 1, 1)
        } },

        {/*02*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, NONE,{
            TILES_PARS(TILES,          1, 2),
            TILES_PARS(TILES_EXPECTED, 1, 2)
        } },

        {/*03*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, MIN_TILE_SIZE, {
            TILES_PARS(TILES,          8, 1),
            TILES_PARS(TILES_EXPECTED, 7, 1),
            TILES_PARS(TILES_EXPECTED_VDENC, 3, 1)
        } },

        {/*04*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, GT_MAX_COL_WRN, {
            TILES_PARS(TILES,          1, 3),
            TILES_PARS(TILES_EXPECTED, 1, 2)
        } },

        {/*05*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, GT_MAX_ROW, {
            TILES_PARS(TILES,          23, 1),
            TILES_PARS(TILES_EXPECTED, 22, 1)
        } },

        {/*06*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, GT_MAX_COL, {
            TILES_PARS(TILES,          1, 21),
            TILES_PARS(TILES_EXPECTED, 1, 20)
        } },

        {/*07*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, NxN, {
            TILES_PARS(TILES,          2, 2),
            TILES_PARS(TILES_EXPECTED, 2, 2)
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
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n" << " FAMILY = " << g_tsOSFamily << "\n";
            return 0;
        }

        if (g_tsImpl & MFX_IMPL_HARDWARE) {
            if (g_tsHWtype < MFX_HW_CNL || m_par.mfx.LowPower != MFX_CODINGOPTION_ON) { //this test is for VDEnc only
                g_tsLog << "SKIPPED for this platform or LowPower mode is off\n";
                return 0;
            }
        }

        bool unsupported_platform = (g_tsHWtype < MFX_HW_ICL);

        if (g_tsImpl == MFX_IMPL_HARDWARE) {
            if (unsupported_platform && tc.sub_type != TRIVIAL && tc.sub_type != UNSUP) {
                g_tsLog << "SKIPPED for this platform\n";
                return 0;
            }
        }

        bool lowpower = m_par.mfx.LowPower == MFX_CODINGOPTION_ON;

        mfxStatus sts = MFX_ERR_NONE;
        mfxExtBuffer* extHEVCTiles;
        tsExtBufType<mfxVideoParam> out_par;
        mfxExtHEVCTiles extHEVCTiles_expectation;

        MFXInit();
        m_session = tsSession::m_session;
        initParams();

        if (tc.sub_type == GT_MAX_COL || tc.sub_type == GT_MAX_ROW) {
            m_par.mfx.FrameInfo.Width = 7680;
            m_par.mfx.FrameInfo.Height = 4320;
            m_par.mfx.FrameInfo.CropW = 7680;
            m_par.mfx.FrameInfo.CropH = 4320;
        }

        m_par.AddExtBuffer(MFX_EXTBUFF_HEVC_TILES, sizeof(mfxExtHEVCTiles));
        extHEVCTiles = m_par.GetExtBuffer(MFX_EXTBUFF_HEVC_TILES);

        out_par = m_par;
        m_pParOut = &out_par;

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
            m_par.mfx.FrameInfo.Shift = 0;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        if (tc.type & QUERY) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(extHEVCTiles, TILES);

            if (unsupported_platform && tc.sub_type == UNSUP) {
                SETPARS(&extHEVCTiles_expectation, TILES_EXPECTED_UNSUP);
            }
            else if (lowpower && tc.sub_type == MIN_TILE_SIZE) {
                SETPARS(&extHEVCTiles_expectation, TILES_EXPECTED_VDENC);
            }
            else {
                SETPARS(&extHEVCTiles_expectation, TILES_EXPECTED);
            }

            if (tc.sub_type == GT_MAX_COL || tc.sub_type == GT_MAX_COL_WRN) {
                ENCODE_CAPS_HEVC caps = {};
                mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

                g_tsStatus.expect(MFX_ERR_NONE);
                g_tsStatus.check(GetCaps(&caps, &capSize));
                if (caps.NumScalablePipesMinus1 > 0) {
                    if (tc.sub_type == GT_MAX_COL) {
                        // we expect here the (max+1) number of Tile columns will be decreased to the (max) value
                        ((mfxExtHEVCTiles*)extHEVCTiles)->NumTileColumns = caps.NumScalablePipesMinus1 + 2;
                        extHEVCTiles_expectation.NumTileColumns = caps.NumScalablePipesMinus1 + 1;
                    }
                    else {
                        ((mfxExtHEVCTiles*)extHEVCTiles)->NumTileColumns = caps.NumScalablePipesMinus1 + 1;
                        extHEVCTiles_expectation.NumTileColumns = caps.NumScalablePipesMinus1 + 1;
                    }
                }
                else if (tc.sub_type == GT_MAX_COL_WRN) {
                    sts = tc.sts.query;
                }
            }

            if (unsupported_platform && tc.sub_type == UNSUP)
                sts = MFX_ERR_UNSUPPORTED;
            else if (tc.sub_type != GT_MAX_COL_WRN)
                sts = tc.sts.query;

            g_tsStatus.expect(sts);
            Query();

            extHEVCTiles = out_par.GetExtBuffer(MFX_EXTBUFF_HEVC_TILES);

            CHECK_PARS(extHEVCTiles_expectation, (*((mfxExtHEVCTiles*)extHEVCTiles)));
        }

        if (tc.type & INIT) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(extHEVCTiles, TILES);
            SETPARS(&extHEVCTiles_expectation, TILES);

            if (unsupported_platform && tc.sub_type == UNSUP)
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
            else if (tc.sub_type != GT_MAX_COL_WRN)
                sts = tc.sts.init;

            g_tsStatus.expect(sts);
            sts = Init();

            CHECK_PARS(extHEVCTiles_expectation, (*((mfxExtHEVCTiles*)extHEVCTiles)));
        }

        if (tc.type & ENCODE) {
            if (!m_initialized) //for test-cases which expects errors on Init()
            {
                //Initialize without incorrect Tiles ext buffer
                m_par.RemoveExtBuffer(MFX_EXTBUFF_HEVC_TILES);
                Init(); TS_CHECK_MFX;
            }
            m_ctrl.AddExtBuffer(MFX_EXTBUFF_HEVC_TILES, sizeof(mfxExtHEVCTiles));
            extHEVCTiles = m_ctrl.GetExtBuffer(MFX_EXTBUFF_HEVC_TILES);

            SETPARS(m_par, MFX_PAR);
            SETPARS(extHEVCTiles, TILES);
            SETPARS(&extHEVCTiles_expectation, TILES);
            g_tsStatus.expect(tc.sts.encode);

            sts = EncodeFrameAsync();

            if (sts == MFX_ERR_MORE_DATA)
                sts = MFX_ERR_NONE;

            CHECK_PARS(extHEVCTiles_expectation, (*((mfxExtHEVCTiles*)extHEVCTiles)));
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_tiles,  RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_tiles, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_tiles,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_tiles, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
}