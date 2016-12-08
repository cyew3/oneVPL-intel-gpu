/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_trellis_cavlc.cpp
\* ****************************************************************************** */


/*!
\file avce_trellis_cavlc.cpp
\brief Gmock test avce_trellis_cavlc

Description:
This suite tests AVC encoder\n
4 test cases with different trellis parameter\n


Algorithm:
- Initializing Media SDK lib
- Initializing suite and case params
- Initializing Frame Allocator
- (Windows only) Initializing handle
- Run Query
- Check query status
- Init encoder
- Check init status

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
/*! \brief Main test name space */
namespace avce_trellis_cavlc
{

    enum
    {
        MFX_PAR = 1,
    };


    /*!\brief Structure of test suite parameters
    */
    struct tc_struct
    {
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {

            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

     //!\brief Main test class
    class TestSuite : tsVideoEncoder
    {
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
        //! \brief Initialize params common for whole test suite
        void initParams();
        //! \brief Initializes FrameAllocator
        /*! \details This method is required because return status (MFX_ERR_NONE) 
        if this method isn't called before Init(), then during Init() it will return two statuses (MFX_ERR_NONE and MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)*/
        void allocatorInit();
        //! \brief Sets Handle
        /*! \details This method is required because of some troubles on Windows platform (working with d3d)
        before Init() calling Handle should be defined*/
        void setHandle();

        //! Set of test cases
        static const tc_struct test_case[];

    };



    void TestSuite::initParams()
    {
        m_par.mfx.CodecId                 = MFX_CODEC_AVC;
        m_par.mfx.CodecProfile            = MFX_PROFILE_AVC_HIGH;
        m_par.mfx.TargetKbps              = 5000;
        m_par.mfx.MaxKbps                 = m_par.mfx.TargetKbps;
        m_par.mfx.RateControlMethod       = MFX_RATECONTROL_CBR;
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Width         = 720;
        m_par.mfx.FrameInfo.Height        = 480;
        m_par.mfx.FrameInfo.CropW         = 720;
        m_par.mfx.FrameInfo.CropH         = 480;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_VIDEO_MEMORY;

        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof (mfxExtCodingOption2));
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION, sizeof (mfxExtCodingOption));
    }

    void TestSuite::allocatorInit(){
        if(     !m_pFrameAllocator
            && (   (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            if(!GetAllocator())
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

    void TestSuite::setHandle()
    {
        mfxHDL hdl;
        mfxHandleType type;
        m_pFrameAllocator->get_hdl(type,hdl);
        SetHandle(m_session, type, hdl);


    }
    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_OFF},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 1}}},
        {/*01*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_OFF},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 2}}},
        {/*02*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_OFF},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 4}}},
        {/*03*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_OFF},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 8}}},
        {/*04*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_I},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 1}}},
        {/*05*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_I},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 2}}},
        {/*06*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_I},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 4}}},
        {/*07*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_I},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 8}}},
        {/*08*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_P},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 1}}},
        {/*09*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_P},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 2}}},
        {/*10*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_P},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 4}}},
        {/*11*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_P},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 8}}},
        {/*12*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_B},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 1}}},
        {/*13*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_B},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 2}}},
        {/*14*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_B},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 4}}},
        {/*15*/ {{ MFX_PAR, &tsStruct::mfxExtCodingOption2.Trellis, MFX_TRELLIS_B},
                    { MFX_PAR, &tsStruct::mfxExtCodingOption.CAVLC, 8}}},


    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);


    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        MFXInit();

        initParams();
        SETPARS(&m_par, MFX_PAR);

        allocatorInit();
        TS_CHECK_MFX;

#if defined(_WIN32) || defined(_WIN64)
        setHandle();
#endif

        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        Query();

        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        SETPARS(&m_par, MFX_PAR);
        Init();

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_trellis_cavlc);
}
