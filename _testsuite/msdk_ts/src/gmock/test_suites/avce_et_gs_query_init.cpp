/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2020 Intel Corporation. All Rights Reserved.

File Name: avce_et_gs_query_init_lowpower.cpp
\* ****************************************************************************** */

/*!
\file avce_et_gs_query_init_lowpower.cpp
\brief Gmock test avce_et_gs_query_init_lowpower

Description:
This suite tests AVC encoder

Algorithm:
- Call Query() of the MSDK session w/ EncTools
- Call Init() of the MSDK session w/ EncTools

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "mfxenctools.h"

/*! \brief Main test name space */
namespace avce_et_gs_query_init_lowpower
{

    enum
    {
        MFX_PAR  = 1,
        ETC_PAR  = 2,
        CDO2_PAR = 3,
    };

    /*!\brief Structure of test suite parameters */
    struct tc_struct
    {
        mfxStatus sts_query;
        mfxStatus sts_init;
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;
        }set_par[MAX_NPARS];
    };

    class VideoEncoder :public tsVideoEncoder
    {
    public:
        //! \brief A constructor (AVC encoder)
        VideoEncoder() :tsVideoEncoder(MFX_CODEC_AVC) {}
        //! \brief A destructor
        ~VideoEncoder() {}
        //! \brief Initialize params common for whole test suite
        void initParams();
        //! \brief Initializes FrameAllocator
        /*! \details This method is required because of some troubles on Windows platform
        befoe Init() calling frameAllocator should be defined*/
        void allocatorInit();
        //! \brief Sets Handle
        /*! \details This method is required because of some troubles on Windows platform (working with d3d)
        before Init() calling Handle should be defined*/
        void setHandle();
    };

    //!\brief Main test class
    class TestSuite
    {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        const bool IsMDSCrossMode(const mfxExtEncToolsConfig *enc_tools_config);
        const bool IsMDSFieldsZero(const mfxExtEncToolsConfig *enc_tools_config);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
        //! Encoder
        VideoEncoder encoder;
    };

    void VideoEncoder::initParams()
    {
        //Default codec was set in encoder constructor
        m_par.mfx.LowPower = MFX_CODINGOPTION_ON;
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        {
            m_par.mfx.MaxKbps = 0;
        }
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        m_par.mfx.FrameInfo.Width  = 1920;
        m_par.mfx.FrameInfo.Height = 1088;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_par.mfx.GopRefDist = 1;//TODO: Add B-frames as valid cases on new platforms (DG2+)

        m_par.AddExtBuffer(MFX_EXTBUFF_ENCTOOLS_CONFIG, sizeof(mfxExtEncToolsConfig));

        //Set ScenarioInfo to GS mode
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));
        mfxExtCodingOption3 *coding_option3 = reinterpret_cast<mfxExtCodingOption3*>(m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3));
        EXPECT_NE(coding_option3, nullptr) << "ERROR: Extension buffer wasn't found\n";
        coding_option3->ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;

    }

    void VideoEncoder::allocatorInit() {
        if (!m_pFrameAllocator
            && ((m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            if (!GetAllocator())
            {
                if (m_pVAHandle)
                    SetAllocator(m_pVAHandle, true);
                else
                    UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
    }

    void VideoEncoder::setHandle()
    {
        mfxHDL hdl;
        mfxHandleType type;
        m_pFrameAllocator->get_hdl(type, hdl);
        SetHandle(m_session, type, hdl);


    }

    const tc_struct TestSuite::test_case[] =
    {
        //GS
        //CBR + valid video params + all EncTools options ON
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        //CBR + valid video params + all EncTools options OFF
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //CBR + valid video params + all EncTools options UNKNOWN
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        //VBR + valid video params + all EncTools options ON
        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options OFF
        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //VBR + valid video params + all EncTools options UNKNOWN
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        //CBR + pairwise EncTools + valid video params
        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*15*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*16*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*17*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //VBR + pairwise EncTools + valid video params
        {/*18*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*19*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*20*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        {/*21*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        {/*22*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*23*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        {/*24*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*25*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*26*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*27*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        {/*28*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        {/*29*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //CBR + invalid codec + all EncTools options ON
        {/*30*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_HEVC },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        //CBR + invalid codec + all EncTools options OFF
        {/*31*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_HEVC },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //CBR + invalid codec + all EncTools options UNKNOWN
        {/*32*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_HEVC },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        //VBR + invalid codec + all EncTools options ON
        {/*33*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_HEVC },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        //VBR + invalid codec + all EncTools options OFF
        {/*34*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_HEVC },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //VBR + invalid codec + all EncTools options UNKNOWN
        {/*35*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_HEVC },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        //CBR + ExtBRC + all EncTools options ON
        {/*36*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        //VBR + ExtBRC + all EncTools options ON
        {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON } } },

        //CBR + ExtBRC + all EncTools options OFF
        {/*38*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //VBR + ExtBRC + all EncTools options OFF
        {/*39*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_OFF } } },

        //CBR + ExtBRC + all EncTools options UNKNOWN
        {/*40*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        //VBR + ExtBRC + all EncTools options UNKNOWN
        {/*41*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.ExtBRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_UNKNOWN } } },

        //GS + AdaptiveB
        //CBR + valid video params + all EncTools options ON
        {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options ON
        {/*43*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON } } },

        //GS + AdaptiveRefB
        //CBR + valid video params + all EncTools options ON
        {/*44*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options ON
        {/*45*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON } } },

        //GS + AdaptivePyramidQuantB
        //CBR + valid video params + all EncTools options ON
        {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options ON
        {/*47*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //GS + AdaptiveLTR
        //CBR + valid video params + all EncTools options ON
        {/*48*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options ON
        {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.SceneChange, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveQuantMatrices, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRCBufferHints, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON } } },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    const bool TestSuite::IsMDSCrossMode(const mfxExtEncToolsConfig *enc_tools_config)
    {
        if (enc_tools_config->AdaptiveB || enc_tools_config->AdaptiveRefB
            || enc_tools_config ->AdaptivePyramidQuantB || enc_tools_config->AdaptiveLTR)
        {
            return true;
        }
        return false;
    }

    const bool TestSuite::IsMDSFieldsZero(const mfxExtEncToolsConfig *enc_tools_config)
    {
        if ((enc_tools_config->AdaptiveB == 0 || enc_tools_config->AdaptiveB == MFX_CODINGOPTION_OFF)
            && (enc_tools_config->AdaptiveRefB == 0 || enc_tools_config->AdaptiveRefB == MFX_CODINGOPTION_OFF)
            && (enc_tools_config->AdaptivePyramidQuantB == 0 || enc_tools_config->AdaptivePyramidQuantB == MFX_CODINGOPTION_OFF)
            && (enc_tools_config->AdaptiveLTR == 0 || enc_tools_config->AdaptiveLTR == MFX_CODINGOPTION_OFF))
        {
            return true;
        }
        return false;
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        bool IsExtBRCCase = false;
        bool isMDSCrossMode = false;
        mfxStatus sts = MFX_ERR_NONE;
        mfxExtEncToolsConfig *enc_tools_config = nullptr;
        mfxExtCodingOption2 *coding_option2 = nullptr;//for ExtBRC

        //HW implementation only
        encoder.m_impl = MFX_IMPL_HARDWARE;
        encoder.MFXInit();


        if (!(encoder.m_impl & MFX_IMPL_HARDWARE)) {
            g_tsLog << "\n\nWARNING: EncTools is only supported by HW library\n\n\n";
            throw tsSKIP;
        }

        //Query stage

        //Set MFX params
        SETPARS(&encoder.m_par, MFX_PAR);

        //Set basic parameters and attach mfxExtEncToolsConfig buffer to video parameters
        encoder.initParams();

        enc_tools_config = reinterpret_cast<mfxExtEncToolsConfig*>(encoder.m_par.GetExtBuffer(MFX_EXTBUFF_ENCTOOLS_CONFIG));
        EXPECT_NE(enc_tools_config, nullptr) << "ERROR: Extension buffer wasn't found\n";
        SETPARS(enc_tools_config, ETC_PAR);
        //TODO: remove temporal zeroing of the SceneChange
        enc_tools_config->SceneChange = 0;

        isMDSCrossMode = IsMDSCrossMode(enc_tools_config);

        //ExtBRC block filling
        encoder.m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        coding_option2 = reinterpret_cast<mfxExtCodingOption2*>(encoder.m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));
        EXPECT_NE(coding_option2, nullptr) << "ERROR: Extension buffer wasn't found\n";
        SETPARS(coding_option2, CDO2_PAR);
        if (coding_option2->ExtBRC != MFX_CODINGOPTION_ON)
        {
            encoder.m_par.RemoveExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        }
        else
        {
            IsExtBRCCase = true;
        }
        //End of ExtBRC block filling

        encoder.allocatorInit();
        TS_CHECK_MFX;

#if defined(_WIN32) || defined(_WIN64)
        encoder.setHandle();
#endif

        g_tsStatus.expect(tc.sts_query);
        encoder.Query();

        if (IsExtBRCCase && tc.sts_query != MFX_ERR_NONE)
        {
            EXPECT_NE(coding_option2->ExtBRC, MFX_CODINGOPTION_ON) << "ERROR: ExtBRC is enabled after Query()\n";
        }


        if (isMDSCrossMode)
        {
            EXPECT_TRUE(IsMDSFieldsZero(enc_tools_config)) << "ERROR: enc_tools_config has no zero values for MDS after Query()\n";
        }

        //Init stage

        //Rewriting of extension buffers
        SETPARS(enc_tools_config, ETC_PAR);
        //TODO: remove temporal zeroing of the SceneChange
        enc_tools_config->SceneChange = 0;
        if (IsExtBRCCase)
        {
            SETPARS(coding_option2, CDO2_PAR);
        }

        g_tsStatus.expect(tc.sts_init);
        encoder.Init();

        //Need to check params after Init()
        if (tc.sts_init != MFX_ERR_INVALID_VIDEO_PARAM)
        {
            encoder.GetVideoParam();
        }

        if (IsExtBRCCase && tc.sts_init != MFX_ERR_NONE)
        {
            EXPECT_NE(coding_option2->ExtBRC, MFX_CODINGOPTION_ON) << "ERROR: ExtBRC is enabled after Init()\n";
        }

        if (isMDSCrossMode)
        {
            EXPECT_TRUE(IsMDSFieldsZero(enc_tools_config)) << "ERROR: enc_tools_config has no zero values for MDS after Init()\n";
        }

        //Close stage

        if (tc.sts_init == MFX_ERR_INVALID_VIDEO_PARAM)
        {
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        }

        encoder.Close();


        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_et_gs_query_init_lowpower);
}
