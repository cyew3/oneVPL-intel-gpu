/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: mpeg2d_bs_dataflag.cpp
\* ****************************************************************************** */

/*!
\file mpeg2d_bs_dataflag.cpp
\brief Gmock test mpeg2d_bs_dataflag

Description:
This suite tests MPG2 decoder\n

Algorithm:
- Initialize MSDK
- Set test suite params
- Set test case params (Stream name, Dataflag, Expected DecodeHeader status)
- Reg stream (from tc params)
- Set expected status (from tc params)
- Call DecodeHeader() function
- Check return status

*/
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace mpeg2d_bs_dataflag{

    enum{
        MFX_PAR = 1,
        MFX_BS = 2,
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct{
        /*! \brief path to stream (Example : "forBehaviorTest/dec_eos/SPSPPSheader.h264" )*/
        const char* stream;
        /*! \brief Expected DecodeHeader() return status*/
        mfxStatus dh_exp;
        /*! \brief Structure contains params for some fields of decoder */

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
    class TestSuite:tsVideoDecoder{
    public:
        //! \brief A constructor (MPG2 decoder)
        TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2) { }
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
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType<mfxVideoParam> par;
        par.AsyncDepth = 1;
        par.mfx.CodecId = MFX_CODEC_MPEG2;
        par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        return par;
    }
    /*! \note
    #01 Send Sequence header + extension. Data Flag was not set, expected MFX_ERR_NONE\n
    #02 Send Sequence header + extension. Data Flag set, expected MFX_ERR_NONE\n
    #03 Send Sequence header + extension. Data Flag set, expected MFX_ERR_NONE\n
    #Arguable part. Ask splotnik, varistar, why is expected status MORE_DATA or UNDEFINED_BEHAVIOR\n
    #04 Send Sequence header + extension. Data Flag set, extensions data unexpectedly ends. Expected MFX_ERR_MORE_DATA\n
    #05 Send Sequence header + extension. Data Flag set, extensions data unexpectedly ends. Expected MFX_ERR_MORE_DATA\n
    #06 Send Sequence header + extension. Data Flag was not set, extensions data unexpectedly ends. Expected MFX_ERR_MORE_DATA\n
    */
    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ "forBehaviorTest/dec_eos/SeqHeadermpeg2.mpg", MFX_ERR_NONE,
                    {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1},
                     {MFX_BS, &tsStruct::mfxBitstream.DataFlag, 0}}},
        {/*01*/ "forBehaviorTest/dec_eos/SeqHeadermpeg2.mpg", MFX_ERR_NONE,
                    {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1},
                     {MFX_BS, &tsStruct::mfxBitstream.DataFlag, 1}}},
        {/*02*/ "forBehaviorTest/dec_eos/SeqHeadermpeg2.mpg", MFX_ERR_NONE,
                    {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1},
                     {MFX_BS, &tsStruct::mfxBitstream.DataFlag, 2}}},
        {/*03*/ "forBehaviorTest/dec_eos/SeqHeadermpeg2_incomplete.mpg", MFX_ERR_MORE_DATA,
                    {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1},
                     {MFX_BS, &tsStruct::mfxBitstream.DataFlag, 2}}},
        {/*04*/ "forBehaviorTest/dec_eos/SeqHeadermpeg2_incomplete.mpg", MFX_ERR_MORE_DATA,
                    {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1},
                     {MFX_BS, &tsStruct::mfxBitstream.DataFlag, 1}}},
        {/*05*/ "forBehaviorTest/dec_eos/SeqHeadermpeg2_incomplete.mpg", MFX_ERR_MORE_DATA,
                    {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1},
                     {MFX_BS, &tsStruct::mfxBitstream.DataFlag, 0}}},
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];
        MFXInit();

        m_par = initParams();
        SETPARS (&m_par, MFX_PAR);
        SETPARS(m_pBitstream, MFX_BS);

        const char* sname = g_tsStreamPool.Get(tc.stream);
        g_tsStreamPool.Reg();
        tsBitstreamReader reader(sname, 1000);
        m_bs_processor = &reader;
        m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

        g_tsStatus.expect(tc.dh_exp);
        DecodeHeader();

        MFXClose();
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(mpeg2d_bs_dataflag);
}
