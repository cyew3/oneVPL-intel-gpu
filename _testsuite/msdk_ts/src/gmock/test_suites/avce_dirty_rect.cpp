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

    enum {
        MFX_PAR = 1,
    };

    /*!\brief Structure of test suite parameters
    */

    static const mfxU32 num_rectangles = 1;
    struct tc_struct
    {
        mfxStatus sts;
        struct tctrl {
            mfxU32 par[4];
        } ctrl[num_rectangles];



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
        {/*00*/ MFX_ERR_NONE,{ 0, 0, 0, 0 } },
        {/*01*/ MFX_ERR_UNSUPPORTED,{ 0, 1, 0, 0 } },
        {/*02*/ MFX_ERR_UNSUPPORTED,{ 0, 0, 1, 0 } },
        {/*03*/ MFX_ERR_UNSUPPORTED,{ 0, 0, 0, 1 } }

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        auto& tc = test_case[id];

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }  

        if (g_tsImpl == MFX_IMPL_HARDWARE)
        {
            if (g_tsHWtype < MFX_HW_SKL)
            {
                g_tsLog << "SKIPPED for this platform\n";
                return 0;
            }
        }

        MFXInit();

        initParams();
        Query();
        Init();

        std::vector<mfxExtBuffer*> in_buffs;
        mfxExtDirtyRect db;
        memset(&db, 0, sizeof(db));
        db.Header.BufferId = MFX_EXTBUFF_DIRTY_RECTANGLES;
        db.Header.BufferSz = sizeof(db);

        db.NumRect = num_rectangles;

        for (mfxU32 i = 0; i < num_rectangles; i++)
        {
            db.Rect[i].Top = tc.ctrl[i].par[0];
            db.Rect[i].Bottom = tc.ctrl[i].par[1];
            db.Rect[i].Right = tc.ctrl[i].par[2];
            db.Rect[i].Left = tc.ctrl[i].par[3];
        }
        in_buffs.push_back((mfxExtBuffer*)&db);
        m_pCtrl->NumExtParam = 1;
        m_pCtrl->ExtParam = &in_buffs[0];


        mfxStatus sts = EncodeFrameAsync();
        if (sts == MFX_ERR_MORE_DATA)
            sts = MFX_ERR_NONE;
        g_tsStatus.expect(tc.sts);
        g_tsStatus.check(sts);

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_dirty_rect);
}