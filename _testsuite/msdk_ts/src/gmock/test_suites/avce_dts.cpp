/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

File Name: avce_dts.cpp

\* ****************************************************************************** */

/*!
\file avce_dts.cpp
\brief Gmock test avce_dts (tests Decode Time Stamp)

Description:
This suite tests AVC encoder\n
4 test cases with different resolutions\n


Algorithm:
- Call method initParams and set initial parameters
- Init encoder
- Check init status
- Encode frame
- Check encode status
- Check Decode Time Stamp


*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <vector>

//! \brief Main namespace of the test suite
namespace avce_dts
{

    enum
    {
        MFX_PAR = 1
    };

    template <typename T>
    T abs(T num)
    {
        return (num < 0) ? -1*num : num;
    }

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

    //! \brief Main test class
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
        //! \brief Initialize params common mfor whole test suite
        void initParams();
        //! Set of test cases
        static const tc_struct test_case[];
    };

    void TestSuite::initParams()
    {
        m_par.mfx.CodecId                = MFX_CODEC_AVC;
        m_par.mfx.CodecProfile           = MFX_PROFILE_AVC_HIGH;
        m_par.mfx.TargetKbps             = 5000;
        m_par.mfx.MaxKbps                = m_par.mfx.TargetKbps;
        m_par.mfx.GopPicSize             = 256;
        m_par.mfx.RateControlMethod      = MFX_RATECONTROL_CBR;
        m_par.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Width        = 352;
        m_par.mfx.FrameInfo.Height       = 288;
        m_par.mfx.FrameInfo.CropW        = 352;
        m_par.mfx.FrameInfo.CropH        = 288;
        m_par.IOPattern                  = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}} }, // tc.b_frames + 1;
        {/*01*/ {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2}} }, // tc.b_frames + 1;
        {/*02*/ {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4}} }, // tc.b_frames + 1;
        {/*03*/ {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25000},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8}} }, // tc.b_frames + 1;
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        
        initParams();
        SETPARS(&m_par, MFX_PAR);
        mfxU16 b_frames = m_par.mfx.GopRefDist - 1;
        double frN = m_par.mfx.FrameInfo.FrameRateExtN;
        mfxU32 frD = m_par.mfx.FrameInfo.FrameRateExtD;


        Init();
        QueryIOSurf();
        AllocSurfaces();
        GetVideoParam();
        AllocBitstream();

        std::vector<mfxI64> ts;

        int delay = ((b_frames) ? 1 : 0)+b_frames/3;

        for (int i = 0, c = 0; i < 50; ++c)
        {
            m_pSurf = GetSurface();
            tsRawReader reader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo, m_par.mfx.FrameInfo.FrameRateExtN);
            g_tsStreamPool.Reg();
            m_filler = &reader;

            mfxI64 cts = c*frD*(90000/frN);
            ts.push_back(cts - delay*frD*(90000/frN));
            m_pSurf->Data.TimeStamp = cts;

            mfxStatus sts = EncodeFrameAsync();

            if (sts == MFX_ERR_NONE)
            {
                SyncOperation();

                mfxI64 dts = m_bitstream.DecodeTimeStamp;
                mfxI64 val = abs(dts - ts.at(i));
                mfxI64 expected = abs(90*frD/frN);
                EXPECT_LE(val, expected) << "ERROR: wrong DTS.\nFrame: " << i << ", expected: " << ts.at(i) << "\n";
                if (val > expected)
                    break;
                m_bitstream.DataLength = 0;
                m_bitstream.DataOffset = 0;
                i++;
            }
            else if (sts == MFX_ERR_MORE_DATA)
            {
                if (reader.m_eos)
                    break;
            }
            else
            {
                EXPECT_EQ(MFX_ERR_NONE, sts) << "ERROR: Unexpected status from EncodeFrameAsync:" << sts << "\n";
                break;
            }
        }

        TS_END;
        return 0;
    }
    //! \brief Reg test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_dts);
}