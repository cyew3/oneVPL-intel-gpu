/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: vc1_sethandle_16.cpp
\* ****************************************************************************** */

/*!
\file vc1_sethandle_16.cpp
\brief Gmock test vc1_sethandle_16

Description:
This suite tests VC1 sethandle style (Before 1.5 version it was possible to call SetHandle() after several funcs)\n

Algorithm:
- Initialize session
- Define frame allocator
- Initialize Decoder
- Call SetHandle (before or after main function(defined in tc params))
- Check return status (depends on SetHandle call style and impl version)

\author Nikolay Korshunov nikolay.korshunov@intel.com
*/
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace vc1_sethandle_16
{

    //! \enum Enum with values to identify function to call in current test case
    enum FUNCTION
    {
        QUERY = 1,
        QUERY_IO_SURF = 2,
        DECODE_HEADER = 3
    };

    /*!\brief Structure of test suite parameters
    */

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! True if SetHandle() is called after main function
        bool old_style;
        //! True if calling Sethandle() after func is ok (impl v1.4)
        bool old_version_enable;
        //! Test case func to call
        FUNCTION func;
        //! Expected return status of func
        mfxStatus func_exp;
        //! Stream name
        const char* stream;
    };

    //!\brief Main test class
    class TestSuite:tsSession
    {
    public:
        //! \brief A constructor
        TestSuite(): tsSession() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Initialize params of Decoder
        //! \param dec - link to decoder
        void initParams(tsVideoDecoder& dec);
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    void TestSuite::initParams(tsVideoDecoder& dec)
    {
        dec.m_par.mfx.CodecId = MFX_CODEC_VC1;
        dec.m_par.mfx.CodecProfile = MFX_PROFILE_VC1_MAIN;
        dec.m_par.mfx.CodecLevel = MFX_LEVEL_VC1_2;
        dec.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        dec.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        dec.m_par.mfx.FrameInfo.CropW = 320;
        dec.m_par.mfx.FrameInfo.CropH = 240;
        dec.m_par.mfx.FrameInfo.CropX = 0;
        dec.m_par.mfx.FrameInfo.CropY = 0;
        dec.m_par.mfx.FrameInfo.Width = 320;
        dec.m_par.mfx.FrameInfo.Height = 240;
        dec.m_par.mfx.BufferSizeInKB = 200;
        dec.m_par.AsyncDepth = 1;
        dec.m_par.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ false, false, QUERY, MFX_ERR_NONE },
        {/*01*/ false, false, QUERY_IO_SURF, MFX_ERR_NONE, },
        {/*02*/ false, false, DECODE_HEADER, MFX_ERR_NONE, "forBehaviorTest/valid_vc1.vc1" },
        {/*03*/ true,  false, QUERY, MFX_ERR_UNDEFINED_BEHAVIOR },
        {/*04*/ true,  false, QUERY_IO_SURF, MFX_ERR_UNDEFINED_BEHAVIOR },
        {/*05*/ true,  false, DECODE_HEADER, MFX_ERR_NONE, "forBehaviorTest/valid_vc1.vc1" },
        {/*06*/ false, true,  QUERY, MFX_ERR_NONE },
        {/*07*/ false, true,  QUERY_IO_SURF, MFX_ERR_NONE, },
        {/*08*/ false, true,  DECODE_HEADER, MFX_ERR_NONE, "forBehaviorTest/valid_vc1.vc1" },
        {/*09*/ true,  true,  QUERY, MFX_ERR_NONE },
        {/*10*/ true,  true,  QUERY_IO_SURF, MFX_ERR_NONE },
        {/*11*/ true,  true,  DECODE_HEADER, MFX_ERR_NONE, "forBehaviorTest/valid_vc1.vc1" },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        if(g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }

        auto& tc = test_case[id];
        if(tc.old_version_enable) {
            m_pVersion->Minor = 4;
        }
        MFXInit();

        mfxHDL hdl;
        mfxHandleType hdl_type = mfxHandleType::MFX_HANDLE_D3D9_DEVICE_MANAGER;
        frame_allocator::AllocatorType type = frame_allocator::AllocatorType::HARDWARE;
        if(g_tsImpl == MFX_IMPL_VIA_D3D11) {
            type = frame_allocator::AllocatorType::HARDWARE_DX11;
            hdl_type = mfxHandleType::MFX_HANDLE_D3D11_DEVICE;
        }
        frame_allocator al(type,
                           frame_allocator::AllocMode::ALLOC_MAX,
                           frame_allocator::LockMode::ENABLE_ALL,
                           frame_allocator::OpaqueAllocMode::ALLOC_EMPTY);

        tsVideoDecoder dec;
        initParams(dec);
        dec.m_pFrameAllocator = &al;
        dec.m_pFrameAllocator->get_hdl(hdl_type, hdl);

        if(!tc.old_style) {
            g_tsStatus.expect(tc.func_exp);
            dec.SetHandle(m_session, hdl_type, hdl);
        }

        switch(tc.func) {
            case QUERY:
                dec.Query(m_session, dec.m_pPar, dec.m_pParOut);
                break;
            case QUERY_IO_SURF:
                dec.QueryIOSurf(m_session, dec.m_pPar, dec.m_pRequest);
                break;
            case DECODE_HEADER:
            {
                const char* sname = g_tsStreamPool.Get(tc.stream);
                g_tsStreamPool.Reg();
                tsBitstreamReader reader(sname, 1000);
                dec.m_bs_processor = &reader;
                dec.m_pBitstream = dec.m_bs_processor->ProcessBitstream(dec.m_bitstream);
                dec.DecodeHeader(m_session, dec.m_pBitstream, dec.m_pPar);
                break;
            }
            default:
                g_tsLog << "Unsupported function name";
                return 1;
        }
        if(tc.old_style) {
            g_tsStatus.expect(tc.func_exp);
            dec.SetHandle(m_session, hdl_type, hdl);
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vc1_sethandle_16);
}