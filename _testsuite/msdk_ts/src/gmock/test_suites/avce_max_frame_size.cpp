/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_max_frame_size.cpp
\* ****************************************************************************** */

/*!
\file avce_max_frame_size.cpp
\brief Gmock test avce_max_frame_size

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK lib
- Set test suite params
- Set tes case params
- Set expected Init status and Expected Max Frame Size
- Call Init() and GetVideoParam()
- Check Max Frame Size value
- Set expected EncodeFrameAsync() sts
- Call EncodeFrameAsync() and GetVideoParam()
- Check Max Frame Size value
- Set expected Reset() sts
- Call Reset() and GetVideoParam()
- Check Max Frame Size value

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_max_frame_size{

    enum{
        MFX_PAR = 1,
        CDO2_PAR = 2
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct{
        //! Expected Init() status
        mfxStatus init_sts;
        //! Expected EncodeFarmeAsync() status
        mfxStatus runtime_sts;
        //! Expected Reset() status
        mfxStatus reset_sts;

        //! Array with multiples to get mfs value from average frame size (avg_fs)
        double multiplies[6];

        /*! \brief Structure contains params for some fields of encoder */

        struct f_pair{

            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        }set_par[MAX_NPARS];

    };

    //!\brief Main test class
    class TestSuite:tsVideoEncoder{
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
        tsExtBufType<mfxVideoParam> initParams();
        //! \brief Set of test cases
        static const tc_struct test_case[];
        //! Average frame size
        mfxU32 avg_fs;
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {

        tsExtBufType<mfxVideoParam> par;
        par.mfx.CodecId                 = MFX_CODEC_AVC;
        par.mfx.RateControlMethod       = MFX_RATECONTROL_VBR;
        par.mfx.TargetKbps              = 3000;
        par.mfx.MaxKbps                 = 5000;
        par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.Width         = 352;
        par.mfx.FrameInfo.Height        = 288;
        par.mfx.FrameInfo.CropW         = 352;
        par.mfx.FrameInfo.CropH         = 288;
        par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        par.mfx.GopRefDist              = 1;
        par.AsyncDepth                  = 1;

        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof (mfxExtCodingOption2));

        avg_fs = par.mfx.TargetKbps * 1000 * par.mfx.FrameInfo.FrameRateExtD / par.mfx.FrameInfo.FrameRateExtN /8;
        return par;
    }

    const tc_struct TestSuite::test_case[] =
    {
        /*      Init exp sts                        Encode exp sts                              Reset exp sts */
        {/*00*/ MFX_ERR_NONE,                       MFX_ERR_NONE,                               MFX_ERR_NONE,
        {1.4, 1.4, 1.2,   1.4, 1.3, 1.3},
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0}}},
        /*      Init exp sts                        Encode exp sts                                Reset exp sts */
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,   MFX_ERR_NONE,                                 MFX_ERR_NONE,
        {0.9, 1, 0, 1, 1.3, 1.3},
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0}}},

        /*      Init exp sts                        Encode exp sts                                Reset exp sts */
        {/*02*/ MFX_ERR_NONE,                       MFX_ERR_NONE,                                 MFX_ERR_NONE,
        {1.4, 1.4, 0.9, 1.4, 1.3, 1.3},
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0}}},

        /*      Init exp sts                        Encode exp sts                                Reset exp sts */
        {/*03*/ MFX_ERR_NONE,                       MFX_ERR_NONE,                                 MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {1.4, 1.4, 1.2, 1.4, 0.9, 1},
        {{CDO2_PAR, &tsStruct::mfxExtCodingOption2.MaxFrameSize, 0}}},

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];
        MFXInit();
        m_par = initParams();
        SETPARS(&m_par, CDO2_PAR);
        avg_fs = m_par.mfx.TargetKbps * 1000 * m_par.mfx.FrameInfo.FrameRateExtD / m_par.mfx.FrameInfo.FrameRateExtN / 8;

        mfxU64 init_mfs = avg_fs * tc.multiplies[0];
        mfxU64 expected_init_mfs = avg_fs * tc.multiplies[1];

        mfxU64 runtime_mfs = avg_fs * tc.multiplies[2];
        mfxU64 expected_runtime_mfs = avg_fs * tc.multiplies[3];

        mfxU64 reset_mfs = avg_fs * tc.multiplies[4];
        mfxU64 expected_reset_mfs = avg_fs * tc.multiplies[5];

        mfxExtCodingOption2* co2 = (mfxExtCodingOption2*) m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        co2->MaxFrameSize = init_mfs;


        g_tsStatus.expect(tc.init_sts);
        Init();
        GetVideoParam();
        EXPECT_EQ(expected_init_mfs, co2->MaxFrameSize) << "MaxFrameSize value is wrong, expected $expected_init_mfs";

        QueryIOSurf();
        AllocSurfaces();
        AllocBitstream();

        tsRawReader reader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo, m_par.mfx.FrameInfo.FrameRateExtN); // ?????
        g_tsStreamPool.Reg();
        m_filler = &reader;

        co2->MaxFrameSize = runtime_mfs;
        g_tsStatus.expect(tc.runtime_sts);
        EncodeFrameAsync();
        GetVideoParam();
        EXPECT_EQ(expected_runtime_mfs, co2->MaxFrameSize) << "MaxFrameSize value is wrong";

        co2->MaxFrameSize = reset_mfs;
        g_tsStatus.expect(tc.reset_sts);
        Reset();
        GetVideoParam();
        EXPECT_EQ(expected_reset_mfs, co2->MaxFrameSize) << "MaxFrameSize value is wrong";

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_max_frame_size);
}