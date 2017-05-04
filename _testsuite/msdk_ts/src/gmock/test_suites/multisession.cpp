/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

File Name: multisession.cpp
\* ****************************************************************************** */

/*!
\file multisession.cpp
\brief Gmock test multisession

Description:
This suite tests multisession possibility\n

Algorithm:
- Initialize main session
- Set Handle and Frame Allocator of main session
- Create 2d session by cloning or Init and joining
- Call GetHandle for 2d session(MFX_ERR_NOT_FOUND expected)
- Call GetHandle for main session(MFX_ERR_NONE expected)
- Set Handle of 2d session
- Call GetHandle for 2d session(MFX_ERR_NONE expected)
- Initialize encoder with main session (enc_0)
- Initialize encoder with 2d session (enc_1) (MFX_ERR_INVALID_VIDEO_PARAM expected)
- Set frame allocator of 2d session
- Initialize encoder with 2d session (enc_1) (MFX_ERR_NONE expected)
- Compare allocators surfaces
- Close encoders and sessions
*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace multisession
{

    enum
    {
        MFX_PAR = 1,
    };

    //! \brief Enum with possible test case modes
    enum
    {
        JOIN = 1,
        CLONE = 2,
    };

    /*!\brief Structure of test suite parameters    */
    struct tc_struct
    {
        //! Test case mode (now JOIN or CLONE)
        mfxU8 mode;
    };

    //!\brief Main test class
    class TestSuite:tsSession
    {
    public:
        //! \brief A constructor
        TestSuite(): tsSession() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize encoder params
        //! \param enc - pointer to encoder to initialize
        void initEncoderParams(tsVideoEncoder* enc);
        //! \brief Set of test cases
        static const tc_struct test_case[];
        //! \brief Initialize tsSession
        //! \param s - session pointer
        /*! This function is required because MFXInit automatically sets handle on Linux platform. In this suite it should be done manually*/
        void init(tsSession* s);
        /*! \brief sets handle for session*/
        /*! \param s - session pointer
            \param al - link to allocator
            \param hdl - link to handle
            \param hdl_type - handle type*/
        mfxFrameAllocator* setHandle(tsSession* s, frame_allocator &al, mfxHDL &hdl, mfxHandleType &hdl_type);
    };

    void TestSuite::initEncoderParams(tsVideoEncoder* enc)
    {
        enc->m_par.mfx.RateControlMethod       = MFX_RATECONTROL_CBR;
        enc->m_par.mfx.TargetKbps              = 5000;
        enc->m_par.mfx.MaxKbps                 = enc->m_par.mfx.TargetKbps;
        enc->m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        enc->m_par.mfx.FrameInfo.FrameRateExtN = 30;
        enc->m_par.mfx.FrameInfo.FrameRateExtD = 1;
        enc->m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        enc->m_par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        enc->m_par.mfx.FrameInfo.Width         = 352;
        enc->m_par.mfx.FrameInfo.CropW         = 352;
        enc->m_par.mfx.FrameInfo.Height        = 288;
        enc->m_par.mfx.FrameInfo.CropH         = 288;
        enc->m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ JOIN},
        {/*01*/ CLONE}
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    void TestSuite::init(tsSession* s)
    {
        TRACE_FUNC3(MFXInit, s->m_impl, s->m_pVersion, s->m_pSession);
        g_tsStatus.check(::MFXInit(s->m_impl, s->m_pVersion, s->m_pSession));
        TS_TRACE(s->m_pVersion);
        TS_TRACE(s->m_pSession);
        s->m_initialized = (g_tsStatus.get() >= 0);
    }

    mfxFrameAllocator* TestSuite::setHandle(tsSession* s, frame_allocator &al, mfxHDL &hdl, mfxHandleType &hdl_type)
    {
#if defined (LIN32) || (LIN64)
        s->m_pVAHandle = &al;
        s->m_pVAHandle->get_hdl(hdl_type, hdl);
        SetHandle(s->m_session, hdl_type, hdl);
        return s->m_pVAHandle;
#else
        s->m_pFrameAllocator = &al;
        s->m_pFrameAllocator->get_hdl(hdl_type, hdl);
        SetHandle(s->m_session, hdl_type, hdl);
        return s->m_pFrameAllocator;
#endif
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        frame_allocator::AllocatorType al_type;
        if(g_tsImpl != MFX_IMPL_SOFTWARE) {
            if(g_tsImpl == MFX_IMPL_VIA_D3D11){
                al_type = frame_allocator::AllocatorType::HARDWARE_DX11;
            } else {
                al_type = frame_allocator::AllocatorType::HARDWARE;
            }
        } else {
            g_tsLog << "This test case is not realized for this type of implementaion";
            return 1;
        }

        frame_allocator al1(al_type,
                            frame_allocator::AllocMode::ALLOC_MAX,
                            frame_allocator::LockMode::ENABLE_ALL,
                            frame_allocator::OpaqueAllocMode::ALLOC_EMPTY);
        frame_allocator al2(al_type,
                            frame_allocator::AllocMode::ALLOC_MAX,
                            frame_allocator::LockMode::ENABLE_ALL,
                            frame_allocator::OpaqueAllocMode::ALLOC_EMPTY);

        init(this);

        mfxHDL hdl;
        mfxHandleType hdl_type;
        mfxFrameAllocator* p = setHandle(this, al1, hdl, hdl_type);
        SetFrameAllocator(m_session, p);

        tsSession t_ses_1;

        if (tc.mode == JOIN){
            init(&t_ses_1);
            MFXJoinSession(t_ses_1.m_session);
        } else if(tc.mode == CLONE) {
            TRACE_FUNC2(MFXCloneSession,m_session, t_ses_1.m_pSession);
            MFXCloneSession(m_session, t_ses_1.m_pSession);
        } else {
            g_tsLog << "Unexpected test case mode\n";
            return 1;
        }

        g_tsStatus.expect(MFX_ERR_NOT_FOUND);
        TRACE_FUNC3(MFXVideoCORE_GetHandle, t_ses_1.m_session, hdl_type, &hdl);
        g_tsStatus.check(MFXVideoCORE_GetHandle(t_ses_1.m_session, hdl_type, &hdl));

        g_tsStatus.expect(MFX_ERR_NONE);
        TRACE_FUNC3(MFXVideoCORE_GetHandle, m_session, hdl_type, &hdl);
        g_tsStatus.check(MFXVideoCORE_GetHandle(m_session, hdl_type, &hdl));

        p = setHandle(&t_ses_1, al2, hdl, hdl_type);

        g_tsStatus.expect(MFX_ERR_NONE);
        TRACE_FUNC3(MFXVideoCORE_GetHandle, t_ses_1.m_session, hdl_type, &hdl);
        g_tsStatus.check(MFXVideoCORE_GetHandle(t_ses_1.m_session, hdl_type, &hdl));

        tsVideoEncoder enc_0(MFX_CODEC_AVC);
        initEncoderParams(&enc_0);

        tsVideoEncoder enc_1(MFX_CODEC_AVC);
        initEncoderParams(&enc_1);

        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        enc_1.Init(t_ses_1.m_session, enc_1.m_pPar);

        SetFrameAllocator(t_ses_1.m_session, p);

        g_tsStatus.expect(MFX_ERR_NONE);
        enc_1.Init(t_ses_1.m_session, enc_1.m_pPar);

        g_tsStatus.expect(MFX_ERR_NONE);
        enc_0.Init(m_session, enc_0.m_pPar);

        EXPECT_EQ(al1.cnt_surf(), al2.cnt_surf()) << "FAILED: sessions should not share frame allocator" << std::endl;

        enc_1.Close(t_ses_1.m_session);
        enc_1.FreeSurfaces();

        enc_0.Close(m_session);
        enc_0.FreeSurfaces();
        t_ses_1.MFXClose();
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(multisession);
}
