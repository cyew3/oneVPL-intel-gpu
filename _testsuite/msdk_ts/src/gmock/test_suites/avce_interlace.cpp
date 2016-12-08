/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_interlace.cpp
\* ****************************************************************************** */

/*!
\file avce_interlace.cpp
\brief Gmock test avce_interlace

Description:
This suite tests AVC encoder\n

Algorithm:
- Initializing MSDK lib
- Set suite params (AVC encoder params)
- Set case params
- Define allocator
- Set Handle if platform is Windows
- Call Query() function
- Check returned status (Expected in test case params)
- Set suite and case params again
- Call Init() function
- Check returned status (Expected in test case params)
- Call GetVideoParam() function
- Check returned function (Expected MFX_ERR_NONE)

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_interlace{

    enum{
        MFX_PAR = 1
    };

    /*!\brief Structure of test suite parameters
    */
    struct tc_struct
    {
        //! Expected reurn status of Query() and Init()
        mfxStatus expected_sts;
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case
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
        //! \brief Sets system handles that the SDK implementation might need (Only windows platform required)
        void setHandle();
    };

    void TestSuite::setHandle()
    {
        mfxHDL hdl;
        mfxHandleType type;
        m_pFrameAllocator->get_hdl(type,hdl);
        SetHandle(m_session, type, hdl);
    }

    void TestSuite::initParams(){

        m_par.mfx.RateControlMethod       = MFX_RATECONTROL_CBR;
        m_par.mfx.TargetKbps              = 12000;
        m_par.mfx.MaxKbps                 = m_par.mfx.TargetKbps;
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_FIELD_TFF;
        m_par.mfx.FrameInfo.Width         = 1920;
        m_par.mfx.FrameInfo.Height        = 1088;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE,},
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 31}}},
        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2304},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 30000}}},
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 5}}},
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 7000},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 10}}},
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 10000}}}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];

        MFXInit();

        initParams();
        SETPARS(&m_par, MFX_PAR);

        frame_allocator::AllocatorType type = frame_allocator::SOFTWARE;
        if (g_tsImpl>=0){
            if (g_tsImpl==MFX_IMPL_VIA_D3D11)
                type = frame_allocator::HARDWARE_DX11;
            else
                type = frame_allocator::HARDWARE;
        }

        frame_allocator::AllocMode alloc = frame_allocator::ALLOC_MAX;
        frame_allocator::LockMode lock = frame_allocator::ENABLE_ALL;
        frame_allocator::OpaqueAllocMode opaque_alloc = frame_allocator::ALLOC_INTERNAL;
        frame_allocator allocator(type, alloc, lock, opaque_alloc);
        m_pFrameAllocator = &allocator;

#if defined(_WIN32) || defined(_WIN64)
        setHandle();
#endif

        g_tsStatus.expect(tc.expected_sts);
        Query();

        initParams();
        SETPARS(&m_par, MFX_PAR);

        g_tsStatus.expect(tc.expected_sts);
        Init();

        GetVideoParam();
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_interlace);
}