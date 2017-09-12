/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

File Name: avce_dirty_rect.cpp

*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_dirty_rect {

#define RECT_PARS(type, i, numrect,left,top,right,bottom)                    \
            {type, &tsStruct::mfxExtDirtyRect.NumRect,           numrect},   \
            {type, &tsStruct::mfxExtDirtyRect.Rect[i].Left,      left},      \
            {type, &tsStruct::mfxExtDirtyRect.Rect[i].Top,       top},       \
            {type, &tsStruct::mfxExtDirtyRect.Rect[i].Right,     right},     \
            {type, &tsStruct::mfxExtDirtyRect.Rect[i].Bottom,    bottom}


#define CHECK_PARS(expected, actual) {                                                                    \
    EXPECT_EQ(expected.Left, actual.Left)                                                                 \
        << "ERROR: Expect rect = " << expected.Left << ", but real rect = " << actual.Left << "!\n";      \
    EXPECT_EQ(expected.Top, actual.Top)                                                                   \
        << "ERROR: Expect rect = " << expected.Top << ", but real rect = " << actual.Top << "!\n";        \
    EXPECT_EQ(expected.Right, actual.Right)                                                               \
        << "ERROR: Expect rect = " << expected.Right << ", but real rect = " << actual.Right << "!\n";    \
    EXPECT_EQ(expected.Bottom, actual.Bottom)                                                             \
        << "ERROR: Expect rect = " << expected.Bottom << ", but real rect = " << actual.Bottom << "!\n";  \
}

    enum {
        MFX_PAR = 1,
        DIRTYRECT,
        DIRTYRECT_EXPECTED_QUERY
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
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) { }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common mfor whole test suite
        void initParams();
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    void TestSuite::initParams() {
        m_par.mfx.CodecId = MFX_CODEC_AVC;
        m_par.mfx.TargetKbps = 9000;
        m_par.mfx.MaxKbps = 12000;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.ChromaFormat = 1;
        m_par.mfx.FrameInfo.PicStruct = 1;
        m_par.IOPattern = 2;
        m_par.mfx.FrameInfo.Width = 1920;
        m_par.mfx.FrameInfo.Height = 1088;
        m_par.mfx.FrameInfo.CropW = 1920;
        m_par.mfx.FrameInfo.CropH = 1088;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE, {
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 0, 0),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, 0, 0)
        } },

        {/*01*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   16, 16, 48, 48),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   16, 16, 48, 48)
        } },

        {/*02*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 0, 16), // Right == Left
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, 0, 16)
        } },

        {/*03*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   1920, 0, 0, 16), // Left == Width
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,      0, 0, 0, 16)
        } },

        {/*04*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 1950, 16), // Right > Width, out of bounds
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0,    0, 16)
        } },

        {/*05*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 16, 32, 16), // Top == Bottom
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 16, 32,  0)
        } },

        {/*06*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 1088, 16, 0), // Top == Height
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0,    0, 16, 0)
        } },

        {/*07*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 16, 1105), // Bottom > Height, out of bounds
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, 16,    0)
        } },

        {/*08*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   32, 0, 16, 16), // Left > Rigth
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   32, 0, 0,  16)
        } },

        {/*09*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 32, 16, 16), // Top > Bottom
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 32, 16,  0)
        } },

        {/*10*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  0 },
            RECT_PARS(DIRTYRECT,                0, 1,   32, 0, 16, 16), // Left > Rigth, Width = 0
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   32, 0, 0,  16)
        } },

        {/*11*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  0 },
            RECT_PARS(DIRTYRECT,                0, 1,   0, 32, 16, 16), // Top > Bottom, Height = 0
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 32, 16,  0)
        } },

        {/*12*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   1, 1, 33, 33),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, 48, 48)
        } },

        {/*13*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 2,   0, 0, 16, 16), //Rect[0]
            RECT_PARS(DIRTYRECT,                1, 2,   0, 0, 32, 32), //Rect[1]

            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 2,   0, 0, 16, 16),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 1, 2,   0, 0, 32, 32),
        } },

        {/*14*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            { DIRTYRECT, &tsStruct::mfxExtDirtyRect.NumRect, 300 }, // MAX NumRect is 256
            { DIRTYRECT_EXPECTED_QUERY, &tsStruct::mfxExtDirtyRect.NumRect, 256 }
        } },

        {/*15*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 32, 32),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, 32, 32)
        } },
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
        mfxExtBuffer* extDirtyRect;
        tsExtBufType<mfxVideoParam> out_par;
        mfxExtDirtyRect extDirtyRect_expectation;

        MFXInit();
        m_session = tsSession::m_session;
        initParams();

        m_par.AddExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES, sizeof(mfxExtDirtyRect));
        extDirtyRect = m_par.GetExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES);

        out_par = m_par;
        m_pParOut = &out_par;

        if (tc.type & QUERY) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(extDirtyRect, DIRTYRECT);
            int num_rect = ((mfxExtDirtyRect*)extDirtyRect)->NumRect;
            if (num_rect == 300)
                for (int i = 0; i < 256; i++) {
                    ((mfxExtDirtyRect*)extDirtyRect)->Rect[i] = { 0, 0, 16, 16 };
                }

            g_tsStatus.expect(tc.sts.query);
            SETPARS(&extDirtyRect_expectation, DIRTYRECT_EXPECTED_QUERY);
            Query();

            extDirtyRect = out_par.GetExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES);

            if (num_rect <= 256)
                for (int i = 0; i < num_rect; i++) {
                    CHECK_PARS(extDirtyRect_expectation.Rect[i], ((mfxExtDirtyRect*)extDirtyRect)->Rect[i]);
                }
            else
                EXPECT_EQ(extDirtyRect_expectation.NumRect, ((mfxExtDirtyRect*)extDirtyRect)->NumRect)
                << "ERROR: Expect num_rect = " << extDirtyRect_expectation.NumRect << ", but real num_rect = " << ((mfxExtDirtyRect*)extDirtyRect)->NumRect << "!\n";
        }

        if (tc.type & INIT) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(extDirtyRect, DIRTYRECT);
            int num_rect = ((mfxExtDirtyRect*)extDirtyRect)->NumRect;
            if (num_rect == 300)
                for (int i = 0; i < 256; i++) {
                    ((mfxExtDirtyRect*)extDirtyRect)->Rect[i] = { 0, 0, 16, 16 };
                }

            g_tsStatus.expect(tc.sts.init);
            SETPARS(&extDirtyRect_expectation, DIRTYRECT);
            sts = Init();

            if (num_rect <= 256)
                for (int i = 0; i < num_rect; i++) {
                    CHECK_PARS(extDirtyRect_expectation.Rect[i], ((mfxExtDirtyRect*)extDirtyRect)->Rect[i]);
                }
            else
                EXPECT_EQ(extDirtyRect_expectation.NumRect, ((mfxExtDirtyRect*)extDirtyRect)->NumRect)
                << "ERROR: Expect num_rect = " << extDirtyRect_expectation.NumRect << ", but real num_rect = " << ((mfxExtDirtyRect*)extDirtyRect)->NumRect << "!\n";

            if (sts != MFX_ERR_INVALID_VIDEO_PARAM) {
                SETPARS(&extDirtyRect_expectation, DIRTYRECT_EXPECTED_QUERY);
                GetVideoParam(m_session, &out_par);
                extDirtyRect = out_par.GetExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES);

                if (num_rect <= 256)
                    for (int i = 0; i < num_rect; i++) {
                        CHECK_PARS(extDirtyRect_expectation.Rect[i], ((mfxExtDirtyRect*)extDirtyRect)->Rect[i]);
                    }
                else
                    EXPECT_EQ(extDirtyRect_expectation.NumRect, ((mfxExtDirtyRect*)extDirtyRect)->NumRect)
                    << "ERROR: Expect num_rect = " << extDirtyRect_expectation.NumRect << ", but real num_rect = " << ((mfxExtDirtyRect*)extDirtyRect)->NumRect << "!\n";
            }
        }

        if (tc.type & ENCODE) {
            m_ctrl.AddExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES, sizeof(mfxExtDirtyRect));
            extDirtyRect = m_ctrl.GetExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES);

            SETPARS(m_par, MFX_PAR);
            SETPARS(extDirtyRect, DIRTYRECT);
            int num_rect = ((mfxExtDirtyRect*)extDirtyRect)->NumRect;

            sts = EncodeFrameAsync();

            if (sts == MFX_ERR_MORE_DATA)
                sts = MFX_ERR_NONE;
            g_tsStatus.expect(tc.sts.encode);
            g_tsStatus.check(sts);

            SETPARS(&extDirtyRect_expectation, DIRTYRECT_EXPECTED_QUERY);
            if (num_rect <= 256)
                for (int i = 0; i < num_rect; i++) {
                    CHECK_PARS(extDirtyRect_expectation.Rect[i], ((mfxExtDirtyRect*)extDirtyRect)->Rect[i]);
                }
            else
                EXPECT_EQ(extDirtyRect_expectation.NumRect, ((mfxExtDirtyRect*)extDirtyRect)->NumRect)
                << "ERROR: Expect num_rect = " << extDirtyRect_expectation.NumRect << ", but real num_rect = " << ((mfxExtDirtyRect*)extDirtyRect)->NumRect << "!\n";
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_dirty_rect);
}