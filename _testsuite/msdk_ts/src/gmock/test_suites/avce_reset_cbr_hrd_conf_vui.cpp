/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

File Name: avce_reset_cbr_hrd_conf_vui.cpp
\* ****************************************************************************** */

/*!
\file avce_reset_cbr_hrd_conf_vui.cpp
\brief Gmock test avce_reset_cbr_hrd_conf_vui

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK session
- Set test suite and test case params
- Do encode and do Reset, check Reset's return status

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_reset_cbr_hrd_conf_vui {

    enum{
        MFX_PAR = 1
    };

    /*! \brief Structure of test suite parameters */
    struct tc_struct{
        //! Expected Reset() return status
        mfxStatus r_sts;
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair{

            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        } set_par[MAX_NPARS];

    };

    //!\brief Main test class
    class TestSuite : public tsVideoEncoder
    {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common mfor whole test suite
        tsExtBufType<mfxVideoParam> initParams();
        //! \brief Select stream depending on test case
        const char* select_stream(tsExtBufType<mfxVideoParam>& par);
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType<mfxVideoParam> par;
        par.mfx.CodecId = MFX_CODEC_AVC;
        par.mfx.TargetKbps = 3000;
        par.mfx.MaxKbps = 0;
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.FrameRateExtN = 60;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.Width = par.mfx.FrameInfo.CropW = 720;
        par.mfx.FrameInfo.Height = par.mfx.FrameInfo.CropH = 480;
        par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        par.mfx.GopPicSize = 10000;
        par.mfx.GopRefDist = 8;
        par.AsyncDepth = 1;
        par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION, sizeof (mfxExtCodingOption));
        return par;
    }

    const char* TestSuite::select_stream(tsExtBufType<mfxVideoParam>& par) {
        switch (par.mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_PROGRESSIVE:
            return "YUV/iceage_720x480_491.yuv";
        case MFX_PICSTRUCT_FIELD_TFF:
            return "YUV/iceage_720x480_491i.yuv";
        case MFX_PICSTRUCT_FIELD_BFF:
            return "YUV/group_720x576i_600.yuv";
        default:
            return "";
        }
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     480},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_PROGRESSIVE}}},

        {/*01*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     480},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_FIELD_TFF}}},

        {/*02*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     576},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_FIELD_BFF}}},

        {/*03*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     480},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_PROGRESSIVE}}},

        {/*04*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     480},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_FIELD_TFF}}},

        {/*05*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     576},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_FIELD_BFF}}},

        {/*06*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     480},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_PROGRESSIVE}}},

        {/*07*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     480},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_FIELD_TFF}}},

        {/*08*/ MFX_ERR_NONE,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,      720},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,     576},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_ON},
                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,  MFX_PICSTRUCT_FIELD_BFF}}},
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        auto& tc = test_case[id];
        mfxVideoParam new_par = {};

        m_impl = MFX_IMPL_HARDWARE;
        m_par = initParams();
        SETPARS(&m_par, MFX_PAR);
        new_par = m_par;

        MFXInit();

        Query();
        Init();

        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * 30);
        tsRawReader reader(g_tsStreamPool.Get(select_stream(m_par)), m_par.mfx.FrameInfo, 30 * 2);
        g_tsStreamPool.Reg();

        std::vector<mfxU32> fs_vec;
        std::vector<mfxU32> targetkbps_vec = { 5000, 3500, 1000 };

        g_tsStatus.expect(tc.r_sts);
        for (mfxU32 targetKbps : targetkbps_vec)
        {
            new_par.mfx.TargetKbps = targetKbps;
            Reset(m_session, &new_par);
            EncodeFrames(10, true);
            while (MFX_ERR_MORE_DATA != EncodeFrameAsync(m_session, NULL, NULL, m_pBitstream, m_pSyncPoint)) {};
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_reset_cbr_hrd_conf_vui);
}
