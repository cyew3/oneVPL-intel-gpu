/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

File Name: hevce_dirty_rect.cpp

*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_dirty_rect {

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

    mfxU32 BlkSize = 32; //for MFX_HW_CNL

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

    class TestSuite :tsVideoEncoder {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) { }
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;
        void initParams();
        static const tc_struct test_case[];
    };

    void TestSuite::initParams() {
        m_par.mfx.CodecId = MFX_CODEC_HEVC;
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
        {/*00*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 0, 0),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, 0, 0)
        } },

        {/*01*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   BlkSize, BlkSize, BlkSize * 2, BlkSize * 2),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   BlkSize, BlkSize, BlkSize * 2, BlkSize * 2)
        } },

        {/*02*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 0, BlkSize), // Right == Left
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, 0, BlkSize)
        } },

        {/*03*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   1920, 0, 0, BlkSize), // Left == Width
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   1920, 0, 0, BlkSize)
        } },

        {/*04*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, 1950, BlkSize), // Right > Width, out of bounds
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0,    0, BlkSize)
        } },

        {/*05*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, BlkSize, BlkSize * 2, BlkSize), // Top == Bottom
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, BlkSize, BlkSize * 2, BlkSize)
        } },

        {/*06*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 1088, BlkSize, 0), // Top == Height
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 1088, BlkSize, 0)
        } },

        {/*07*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, BlkSize, 1105), // Bottom > Height, out of bounds
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, BlkSize,    0)
        } },

        {/*08*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   BlkSize * 2, 0, BlkSize, BlkSize), // Left > Rigth
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   BlkSize * 2, 0, BlkSize, BlkSize)
        } },

        {/*09*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, BlkSize * 2, BlkSize, BlkSize), // Top > Bottom
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, BlkSize * 2, BlkSize, BlkSize)
        } },

        {/*10*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  0 },
            RECT_PARS(DIRTYRECT,                0, 1,   BlkSize * 2, 0, BlkSize, BlkSize), // Left > Rigth, Width = 0
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   BlkSize * 2, 0, BlkSize, BlkSize)
        } },

        {/*11*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  0 },
            RECT_PARS(DIRTYRECT,                0, 1,   0, BlkSize * 2, BlkSize, BlkSize), // Top > Bottom, Height = 0
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, BlkSize * 2, BlkSize, BlkSize)
        } },

        {/*12*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   BlkSize - (BlkSize - 1), BlkSize - (BlkSize - 1), BlkSize + 1, BlkSize + 1),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,                         0,                       0, BlkSize * 2, BlkSize * 2)
        } },

        {/*13*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   BlkSize - 1, BlkSize - 1, BlkSize + 1, BlkSize + 1),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,             0,           0, BlkSize * 2, BlkSize * 2)
        } },

        {/*14*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 2,   0, 0,     BlkSize,        BlkSize), //Rect[0]
            RECT_PARS(DIRTYRECT,                1, 2,   0, 0, BlkSize * 2,    BlkSize * 2), //Rect[1]

            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 2,   0, 0,     BlkSize,     BlkSize),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 1, 2,   0, 0, BlkSize * 2, BlkSize * 2),
        } },

        {/*15*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        QUERY | INIT,{
            { DIRTYRECT, &tsStruct::mfxExtDirtyRect.NumRect, 100 }, // MAX NumRect is 64
            { DIRTYRECT_EXPECTED_QUERY, &tsStruct::mfxExtDirtyRect.NumRect, 64 }
        } },

        {/*16*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        ENCODE,{
            RECT_PARS(DIRTYRECT,                0, 1,   0, 0, BlkSize * 2, BlkSize * 2),
            RECT_PARS(DIRTYRECT_EXPECTED_QUERY, 0, 1,   0, 0, BlkSize * 2, BlkSize * 2)
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
            if (g_tsHWtype < MFX_HW_CNL || m_par.mfx.LowPower != MFX_CODINGOPTION_ON) { //this test is for VDEnc only
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

        Load();

        if (tc.type & QUERY) {
            SETPARS(m_par, MFX_PAR);
            SETPARS(extDirtyRect, DIRTYRECT);
            int num_rect = ((mfxExtDirtyRect*)extDirtyRect)->NumRect;
            if (num_rect == 100)
                for (int i = 0; i < 64; i++) {
                    ((mfxExtDirtyRect*)extDirtyRect)->Rect[i] = { 0, 0, 16, 16 };
                }

            g_tsStatus.expect(tc.sts.query);
            SETPARS(&extDirtyRect_expectation, DIRTYRECT_EXPECTED_QUERY);
            Query();

            extDirtyRect = out_par.GetExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES);

            if (num_rect <= 64)
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
            if (num_rect == 100)
                for (int i = 0; i < 64; i++) {
                    ((mfxExtDirtyRect*)extDirtyRect)->Rect[i] = { 0, 0, 16, 16 };
                }

            g_tsStatus.expect(tc.sts.init);
            SETPARS(&extDirtyRect_expectation, DIRTYRECT);
            sts = Init();

            if (num_rect <= 64)
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

                if (num_rect <= 64)
                    for (int i = 0; i < num_rect; i++) {
                        CHECK_PARS(extDirtyRect_expectation.Rect[i], ((mfxExtDirtyRect*)extDirtyRect)->Rect[i]);
                    }
                else
                    EXPECT_EQ(extDirtyRect_expectation.NumRect, ((mfxExtDirtyRect*)extDirtyRect)->NumRect)
                    << "ERROR: Expect num_rect = " << extDirtyRect_expectation.NumRect << ", but real num_rect = " << ((mfxExtDirtyRect*)extDirtyRect)->NumRect << "!\n";
            }
        }

        if (tc.type & ENCODE) {
            if (!m_initialized) //for test-cases which expects errors on Init()
            {
                //Initialize without incorrect DirtyRect ext buffer
                m_par.RemoveExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES);
                Init(); TS_CHECK_MFX;
                //then set incorrect values back
                m_par.AddExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES, sizeof(mfxExtDirtyRect));
            }
            m_ctrl.AddExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES, sizeof(mfxExtDirtyRect));
            extDirtyRect = m_ctrl.GetExtBuffer(MFX_EXTBUFF_DIRTY_RECTANGLES);

            SETPARS(m_par, MFX_PAR);
            SETPARS(extDirtyRect, DIRTYRECT);
            int num_rect = ((mfxExtDirtyRect*)extDirtyRect)->NumRect;

            g_tsStatus.expect(tc.sts.encode);
            sts = EncodeFrameAsync();

            if (sts == MFX_ERR_MORE_DATA)
                sts = MFX_ERR_NONE;

            SETPARS(&extDirtyRect_expectation, DIRTYRECT_EXPECTED_QUERY);
            if (num_rect <= 64)
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

    TS_REG_TEST_SUITE_CLASS(hevce_dirty_rect);
}