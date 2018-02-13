/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

File Name: avce_extbrcadaptiveltr.cpp
\* ****************************************************************************** */

/*!
\file avce_extbrcadaptiveltr.cpp
\brief Gmock test avce_extbrcadaptiveltr

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK lib
- Set test suite params
- Set test case params
- Call Query() function
- Check returned status
- If returned status is not MFX_ERR_NONE check extbrcadaptiveltr is correct
- Call Init() function
- Check returned status

*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"


/*! \brief Main test name space */
namespace avce_extbrcadaptiveltr{

    enum{
        MFX_PAR = 1
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! Expected Query() return status
        mfxStatus q_sts;
        //! Expected Init() return status
        mfxStatus i_sts;
        //! Expected default value
        mfxU16    i_val;

        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;
        } set_par[MAX_NPARS];
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
        void initParams();
        //! Set of test cases
        static const tc_struct test_case[];


    };


    void TestSuite::initParams()
    {
        m_par.mfx.CodecId                 = MFX_CODEC_AVC;
        m_par.mfx.CodecProfile            = MFX_PROFILE_AVC_HIGH;
        m_par.mfx.CodecLevel              = MFX_LEVEL_AVC_41;
        m_par.mfx.TargetKbps              = 3000;
        m_par.mfx.MaxKbps                 = m_par.mfx.TargetKbps;
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.Width         = 352;
        m_par.mfx.FrameInfo.Height        = 288;
        m_par.mfx.FrameInfo.CropW         = 352;
        m_par.mfx.FrameInfo.CropH         = 288;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_UNKNOWN},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // LD Auto
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_OFF},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // LD OFF
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // LD ON
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ADAPTIVE},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // Invalid
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_OFF},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // Need ExtBRC
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_UNKNOWN},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_VBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // LD Auto VBR
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_VBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // Not Interlace
        {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         1},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_4}}}, // Need 1+MinRef
        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON },
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON },
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR },
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         2 },
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1 },
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0 },
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_5}}}, // Has 1+MinRef, Last 2 Active Ref TU
        {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         2},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_6}}}, // Need >1 Active Ref P (For Correct Err status refactor ActiveRefP option)
        {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         2},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          1},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_7}}}, // Need >1 Active Ref P (For Correct Err status refactor ActiveRefP option)
        {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         2},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          3},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_5}}}, // IPBB Need 1+MinRef, Last 2 Active Ref TU
        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_UNKNOWN},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         3},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          3},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          0},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_5}}}, // IPBB Auto
        {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         3},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          4},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          2},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_5}}}, // 4 Pyramid Need 1+MinRef
        {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_UNKNOWN},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         4},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          4},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          2},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_5}}}, // 4 Pyramid Auto
        {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_CODINGOPTION_OFF,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         4},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          8},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          2},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_5}}}, // 8 Pyramid Need 1+MinRef
        {/*16*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_CODINGOPTION_ON,
                                            {{MFX_PAR, &tsStruct::mfxExtCodingOption3.ExtBrcAdaptiveLTR, MFX_CODINGOPTION_UNKNOWN},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC,            MFX_CODINGOPTION_ON},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CBR},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame,         5},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,          8},
                                             {MFX_PAR, &tsStruct::mfxExtCodingOption2.BRefType,          2},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage,         MFX_TARGETUSAGE_5}}}, // 8 Pyramid Auto
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];

        if (g_tsImpl == MFX_IMPL_SOFTWARE || g_tsHWtype == MFX_HW_IVB || g_tsHWtype == MFX_HW_VLV)
        {
            g_tsLog << "SKIPPED for this platform\n";
            throw tsSKIP;
        }

        MFXInit();
        initParams();

        SETPARS(&m_par, MFX_PAR);

        mfxStatus qexp = tc.q_sts;
        mfxStatus iexp = tc.i_sts;
        mfxU16    ival = tc.i_val;
        mfxExtCodingOption3 *m_co3 = reinterpret_cast <mfxExtCodingOption3*>(m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3));
        mfxU32 ExtBrcAdaptiveLTRbefore = m_co3->ExtBrcAdaptiveLTR;

        /*if ((g_tsHWtype == MFX_HW_IVB || g_tsHWtype == MFX_HW_VLV)
            && (ExtBrcAdaptiveLTRbefore == MFX_CODINGOPTION_ON
                || (ExtBrcAdaptiveLTRbefore != MFX_CODINGOPTION_OFF && ExtBrcAdaptiveLTRbefore != MFX_CODINGOPTION_UNKNOWN)))
        {
            qexp = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            iexp = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            ival = MFX_CODINGOPTION_OFF;
        }*/

        g_tsStatus.expect(qexp);
        Query();

        if (qexp > MFX_ERR_NONE)
        {
            if (ExtBrcAdaptiveLTRbefore == MFX_CODINGOPTION_ON)
            {
                EXPECT_EQ(32, m_co3->ExtBrcAdaptiveLTR) << "ExtBrcAdaptiveLTR wasn't turned off";
            }
            else if (ExtBrcAdaptiveLTRbefore != MFX_CODINGOPTION_OFF && ExtBrcAdaptiveLTRbefore != MFX_CODINGOPTION_UNKNOWN)
            {
                EXPECT_NE(ExtBrcAdaptiveLTRbefore, m_co3->ExtBrcAdaptiveLTR) << "ExtBrcAdaptiveLTR value wasn't changed";
            }
            SETPARS(&m_par, MFX_PAR);
        }

        g_tsStatus.expect(iexp);
        Init();
        GetVideoParam();

        if (iexp == MFX_ERR_NONE && ExtBrcAdaptiveLTRbefore == MFX_CODINGOPTION_UNKNOWN)
        {
            EXPECT_EQ(ival, m_co3->ExtBrcAdaptiveLTR) << "ExtBrcAdaptiveLTR default value unexpected";
        }
        TS_END;
        return 0;
    }
     //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_extbrcadaptiveltr);
}