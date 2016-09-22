/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: avce_nummbperslice.cpp
\* ****************************************************************************** */

/*!
\file avce_nummbperslice.cpp
\brief Gmock test avce_nummbperslice

Description:
This suite tests AVC encoder\n

Algorithm:
- Initializing MSDK lib
- Set suite params (AVC encoder params)
- Set case params (depends on HW type and Lib Implementation)
- Set expected status
- Call Query() function
- Check returned status (if it isnt MFX_ERR_NONE check NumMBPerSlice was zeroed)
- Set expected status
- Call Init() function
- Check returned status (if it is >MFX_ERR_NONE (WRN) call GetVideoparam and check NumMBPerSlice was changed)
- Alloc Bitstream
- Encode bitstream and check NumMBPerSlice value

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_nummbperslice{

    enum{
        MFX_PAR = 1,
        CDO2_PAR = 2,
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct{
        //! Expected Query() status
        mfxStatus q_sts;
        //! Expected Init() status
        mfxStatus i_sts;
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
        //! \brief Initialize params common for whole test suite
        tsExtBufType<mfxVideoParam> initParams();
        //! \brief Checks bitstream, here it compares expected and actual values of first mb in slice
        void check_bs(mfxU16 first_mb_in_slice, mfxU16 first_mb_in_slice_expected);
        //! \brief Encodes stream
        void encode(int n, mfxU16 first_mb_in_slice, mfxU16 first_mb_in_slice_expected);
        //! \brief Set of test cases for HW lib
        static const tc_struct test_case[];
        //! \brief Set of test cases for SW lib
        static const tc_struct test_case_sw[];
        //! \brief The number of frames to encode
        mfxI16 n_frames;
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        n_frames = 4;

        tsExtBufType<mfxVideoParam> par;
        par.mfx.CodecId                 = MFX_CODEC_AVC;
        par.mfx.RateControlMethod       = MFX_RATECONTROL_VBR;
        par.mfx.MaxKbps                 = 4000;
        par.mfx.TargetKbps              = 3000;
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

        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof (mfxExtCodingOption2));
        return par;
    }

    void TestSuite::check_bs(mfxU16 first_mb_in_slice, mfxU16 first_mb_in_slice_expected){
        BS_H264_au* hdr = 0;
        tsParserH264AU parser(m_bitstream);
        for (int i=0; i<TestSuite::n_frames; i++){
            hdr = parser.Parse();
            Bs32u n = hdr->NumUnits;
            BS_H264_au::nalu_param* nalu = hdr->NALU;
            int n_slices = 0;

            for (Bs32u j=0; j<n; j++){
                int type = nalu[j].nal_unit_type;
                std::cout << "NALU_TYPE: 0x%02X " << type << "\n";
                if (type == 5 || type == 1){
                     n_slices +=1;
                         for (int k = 0; k < n_slices-1; k++){
                            slice_header* slice = nalu[j].slice_hdr;
                            first_mb_in_slice = slice->first_mb_in_slice;
                            EXPECT_EQ (0, first_mb_in_slice%first_mb_in_slice_expected * k);
                         }
                     }
                }
            }
    }

    void TestSuite::encode(int n, mfxU16 first_mb_in_slice, mfxU16 first_mb_in_slice_expected){

        mfxU32 submitted = 0;
        mfxU32 encoded = 0;

        AllocSurfaces();
        tsParserH264AU::UnitType* pAU = 0;
        while (n){
            m_pSurf = 0;
            if (submitted < (mfxU32) n){
                m_pSurf = GetSurface();
                submitted++;
            }

            mfxStatus sts = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);
                if (sts == MFX_ERR_NONE){
                    SyncOperation();
                    encoded++;
                }
                else
                    if (sts == MFX_ERR_MORE_DATA){
                        if (!m_pSurf)
                            break;
                    }
                    else
                        break;
        }


        std::cout << n << " frames encoded\n";
    }

    const tc_struct TestSuite::test_case_sw[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0},
        }},
        {/*01*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 1},
        }},
        {/*02*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 2},
        }},
        {/*03*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 22},
        }},
        {/*04*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 44},
        }},
        {/*05*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 120},
        }},
        {/*06*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 480},
        }},
    };

    const tc_struct TestSuite::test_case[] =
    {

        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  176},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 144},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0},
        }},
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  176},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 144},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 11},
        }},
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 176},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 144},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 22},
        }},
        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  176},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 144},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 44},
        }},
        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 22},
        }},
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 44},
        }},
        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 88},
        }},
        {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 100},
        }},
        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 9},
        }},
        {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 10},
        }},
        {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 45},
        }},
        {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 180},
        }},
        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 200},
        }},
        {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 22},
        }},
        {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 961},
        }},
        {/*15*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 120},
        }},
        {/*16*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 240},
        }},
        {/*17*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 360},
        }},
        {/*18*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 380},
        }},
        {/*19*/ MFX_ERR_NONE, MFX_ERR_NONE,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 460},
        }},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        tc_struct temp_tc = test_case[id];

        if(g_tsImpl == MFX_IMPL_SOFTWARE || g_tsHWtype < MFX_HW_HSW) {
            if(id > (sizeof(test_case_sw)/sizeof(test_case_sw[0]))) {
                return 0;
            } else{
                temp_tc = test_case_sw[id];
            }
        }


        auto& tc = temp_tc;

        MFXInit();
        m_par = initParams();
        SETPARS(&m_par, MFX_PAR);
        SETPARS(&m_par, CDO2_PAR);

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width - 32;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height - 32;

        mfxExtCodingOption2* co2 = (mfxExtCodingOption2*) m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        mfxU16 before_query = co2->NumMbPerSlice;
        mfxStatus exp = tc.q_sts;
        g_tsStatus.expect(exp);
        Query();

        if (exp == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM || exp == MFX_ERR_UNSUPPORTED){
            EXPECT_NE(before_query, co2->NumMbPerSlice) << "ERROR: NumMbPerSlice was not changed";
        }

        SETPARS(&m_par, CDO2_PAR);
        mfxU16 before_init = co2->NumMbPerSlice;
        exp = tc.i_sts;
        g_tsStatus.expect(exp);
        Init();

        if (exp == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM){
            GetVideoParam();
            EXPECT_NE(before_init, co2->NumMbPerSlice) << "ERROR: NumMbPerSlice was not changed";;
        }

        if (exp == MFX_ERR_NONE && co2->NumMbPerSlice != 0){
            AllocBitstream();
            encode(n_frames, before_query, co2->NumMbPerSlice);
            check_bs(before_query, co2->NumMbPerSlice);
        }
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_nummbperslice);
}

