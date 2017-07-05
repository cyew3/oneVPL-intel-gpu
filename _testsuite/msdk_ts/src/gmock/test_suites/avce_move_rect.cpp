/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

File Name: avce_move_rect.cpp

*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_move_rect {

#define RECT_PARS(type, i, numrect,left,top,right,bottom, s_left, s_top)        \
            {type, &tsStruct::mfxExtMoveRect.NumRect,               numrect},   \
            {type, &tsStruct::mfxExtMoveRect.Rect[i].DestLeft,      left},      \
            {type, &tsStruct::mfxExtMoveRect.Rect[i].DestTop,       top},       \
            {type, &tsStruct::mfxExtMoveRect.Rect[i].DestRight,     right},     \
            {type, &tsStruct::mfxExtMoveRect.Rect[i].DestBottom,    bottom},    \
            {type, &tsStruct::mfxExtMoveRect.Rect[i].SourceLeft,    s_left},    \
            {type, &tsStruct::mfxExtMoveRect.Rect[i].SourceTop,     s_top}

#define CHECK_PARS(expected, actual) {                                                                            \
    EXPECT_EQ(expected.DestLeft, actual.DestLeft)                                                                 \
        << "ERROR: Expect rect = " << expected.DestLeft << ", but real rect = " << actual.DestLeft << "!\n";      \
    EXPECT_EQ(expected.DestTop, actual.DestTop)                                                                   \
        << "ERROR: Expect rect = " << expected.DestTop << ", but real rect = " << actual.DestTop << "!\n";        \
    EXPECT_EQ(expected.DestRight, actual.DestRight)                                                               \
        << "ERROR: Expect rect = " << expected.DestRight << ", but real rect = " << actual.DestRight << "!\n";    \
    EXPECT_EQ(expected.DestBottom, actual.DestBottom)                                                             \
        << "ERROR: Expect rect = " << expected.DestBottom << ", but real rect = " << actual.DestBottom << "!\n";  \
}

#define CHECK(expected, actual, numrect) {                                                                              \
    if (numrect <= 256)                                                                                                 \
        for (int i = 0; i < numrect; i++) {                                                                             \
            CHECK_PARS(expected.Rect[i], actual->Rect[i]);                                                              \
        }                                                                                                               \
    else                                                                                                                \
        EXPECT_EQ(expected.NumRect, actual->NumRect)                                                                    \
            << "ERROR: Expect num_rect = " << expected.NumRect << ", but real num_rect = " << actual->NumRect << "!\n"; \
}

    enum {
        MFX_PAR = 1,
        MOVERECT,
        MOVERECT_EXPECTED_QUERY
    };

    enum TYPE {
        QUERY = 0x1,
        INIT = 0x2,
        ENCODE = 0x4
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct {
        struct status {
            mfxStatus query;
            mfxStatus init;
            mfxStatus encode;
        } sts;
        mfxU32 type;
        struct tctrl {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    //!\brief Main test class
    class TestSuite :tsVideoEncoder {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {
            m_par.mfx.CodecId = MFX_CODEC_AVC;
            m_par.mfx.TargetKbps = 9000;
            m_par.mfx.MaxKbps = 12000;
            m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.FrameRateExtN = 30;
            m_par.mfx.FrameInfo.FrameRateExtD = 1;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            m_par.mfx.FrameInfo.Width = 1920;
            m_par.mfx.FrameInfo.Height = 1088;
            m_par.mfx.FrameInfo.CropW = 1920;
            m_par.mfx.FrameInfo.CropH = 1088;
        }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common mfor whole test suite
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        // For now driver doesn't support moving rect, that's why we don't check expected parameters and wait for
        // MFX_ERR_UNSUPPORTED status from Query() and MFX_ERR_INVALID_VIDEO_PARAM status from Init()
        // When driver support for move rect will be available, need to fix expected status by the correct one (in the comments)

        {/*00*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(MOVERECT,                0, 1,   0, 0, 0, 0,   16, 16),
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 0, 0,   16, 16)
        } },

        // { MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE }
        {/*01*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(MOVERECT,                0, 1,   16, 16, 48, 48,   16, 16),
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   16, 16, 48, 48,   16, 16)
        } },

        {/*02*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(MOVERECT,                0, 1,   0, 0, 0, 16,   16, 16), // Right == Left
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 0, 16,   16, 16)
        } },

        {/*03*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(MOVERECT,                0, 1,   1920, 0, 0, 16,   16, 16), // Left == Width
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,      0, 0, 0, 16,   16, 16)
        } },

        {/*04*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(MOVERECT,                0, 1,   0, 0, 1950, 16,   16, 16), // Right > Width, out of bounds
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0,    0, 16,   16, 16)
        } },

        {/*05*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(MOVERECT,                0, 1,   0, 0, 16, 0,   16, 16), // Top == Bottom
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 16, 0,   16, 16)
        } },

        {/*06*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(MOVERECT,                0, 1,   0, 1088, 16, 0,   16, 16), // Top == Height
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0,    0, 16, 0,   16, 16)
        } },

        {/*07*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(MOVERECT,                0, 1,   0, 0, 16, 1105,   16, 16), // Bottom > Height, out of bounds
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 16,    0,   16, 16)
        } },

        // { MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE }
        {/*08*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(MOVERECT,                0, 1,   1, 1, 33, 33,   16, 16),
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 32, 32,   16, 16)
        } },

        {/*09*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(MOVERECT,                0, 1,   0, 0, 32, 32,   1920, 16), // SourceLeft >= Width, out of bounds
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 32, 32,      0, 16)
        } },

        {/*10*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(MOVERECT,                0, 1,   0, 0, 32, 32,   16, 1088), // SourceTop >= Height, out of bounds
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 32, 32,   16,    0)
        } },

        // { MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE }
        {/*11*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(MOVERECT,                0, 2,   0, 0, 16, 16,   16, 16), //Rect[0]
            RECT_PARS(MOVERECT,                1, 2,   0, 0, 32, 32,   16, 16), //Rect[1]

            RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 2,   0, 0, 16, 16,   16, 16),
            RECT_PARS(MOVERECT_EXPECTED_QUERY, 1, 2,   0, 0, 32, 32,   16, 16),
        } },

        // { MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE }
        {/*12*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            { MOVERECT, &tsStruct::mfxExtMoveRect.NumRect, 300 }, // MAX NumRect is 256
            { MOVERECT_EXPECTED_QUERY, &tsStruct::mfxExtMoveRect.NumRect, 256 }
        } },

        //{/*13*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        //    ENCODE,{
        //    RECT_PARS(MOVERECT,                0, 1,   0, 0, 32, 32,   0, 0),
        //    RECT_PARS(MOVERECT_EXPECTED_QUERY, 0, 1,   0, 0, 32, 32,   0, 0)
        //} },
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        auto& tc = test_case[id];

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }

        if (g_tsImpl == MFX_IMPL_HARDWARE) {
            if (g_tsHWtype < MFX_HW_SKL) {
                g_tsLog << "SKIPPED for this platform\n";
                return 0;
            }
        }

        mfxStatus sts = MFX_ERR_NONE;
        mfxExtMoveRect* extMoveRect;
        tsExtBufType<mfxVideoParam> out_par;
        mfxExtMoveRect extMoveRect_expectation;

        MFXInit();
        m_session = tsSession::m_session;

        m_par.AddExtBuffer<mfxExtMoveRect>();
        extMoveRect = m_par;

        out_par = m_par;
        m_pParOut = &out_par;

        if (tc.type & QUERY) {
            SETPARS(extMoveRect, MOVERECT);
            int num_rect = extMoveRect->NumRect;
            if (num_rect == 300)
                for (int i = 0; i < 256; i++) {
                    extMoveRect->Rect[i] = { 0, 0, 16, 16 };
                }

            g_tsStatus.expect(tc.sts.query);
            SETPARS(&extMoveRect_expectation, MOVERECT_EXPECTED_QUERY);
            sts = Query();

            extMoveRect = out_par;

            if (sts != MFX_ERR_UNSUPPORTED) // Need to delete this condition if driver support will be avaliable
                CHECK(extMoveRect_expectation, extMoveRect, num_rect);
        }

        if (tc.type & INIT) {
            SETPARS(extMoveRect, MOVERECT);
            int num_rect = extMoveRect->NumRect;
            if (num_rect == 300)
                for (int i = 0; i < 256; i++) {
                    extMoveRect->Rect[i] = { 0, 0, 16, 16 };
                }

            g_tsStatus.expect(tc.sts.init);
            SETPARS(&extMoveRect_expectation, MOVERECT);
            sts = Init();

            if (sts != MFX_ERR_INVALID_VIDEO_PARAM) // Need to delete this condition if driver support will be avaliable
                CHECK(extMoveRect_expectation, extMoveRect, num_rect);

            if (sts != MFX_ERR_INVALID_VIDEO_PARAM) {
                SETPARS(&extMoveRect_expectation, MOVERECT_EXPECTED_QUERY);
                GetVideoParam(m_session, &out_par);
                extMoveRect = out_par;

                CHECK(extMoveRect_expectation, extMoveRect, num_rect);
            }
        }

        if ((sts != MFX_ERR_UNSUPPORTED) && (sts != MFX_ERR_INVALID_VIDEO_PARAM) && (tc.type & ENCODE)) {
            m_ctrl.AddExtBuffer<mfxExtMoveRect>();
            extMoveRect = m_ctrl;

            SETPARS(extMoveRect, MOVERECT);
            int num_rect = extMoveRect->NumRect;

            sts = EncodeFrameAsync();

            if (sts == MFX_ERR_MORE_DATA)
                sts = MFX_ERR_NONE;
            g_tsStatus.expect(tc.sts.encode);
            g_tsStatus.check(sts);

            SETPARS(&extMoveRect_expectation, MOVERECT_EXPECTED_QUERY);
            CHECK(extMoveRect_expectation, extMoveRect, num_rect);
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_move_rect);
}