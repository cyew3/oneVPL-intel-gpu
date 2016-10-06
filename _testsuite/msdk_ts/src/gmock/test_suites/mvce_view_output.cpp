/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

File Name: mvce_view_output.cpp

\* ****************************************************************************** */

/*!
\file mvce_view_output.cpp
\brief Gmock test mvce_view_output

Description:
This suite tests MVC encoding\n
4 test cases with differend ViewOutput and bitstreams\n
1)\Same bitstream with enabled ViewOutput\n
2)\Different bitstreams with enabled ViewOutput\n
3)Disabled ViewOutput\n
4)Unknown ViewOutput\n
2 test cases with different action\n
1)Query with incompatible ViewIds and enabled ViewOutput\n
2)Init with incompatible ViewIds and enabled ViewOutput\n

Algorithm:
- Call method initParams and set initial parameters\n
First set of test cases:
- Initialize Media SDK lib
- Query the number of input surface frames required for encoding
- Initialize the encoding operation
- Perform the encoding and return the compressed bitstream
- Check result of the encoding
- Syncronize the operation\n
Second  set of test cases:
- Set some ExtMVCSeqDesc parameters
- Initialize Media SDK lib or Query the number of input surface frames required for encoding
- Check View parameters of the ExtMVCSeqDesc parameters
*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include "ts_mvc_warning.h"

//!Namespace of MVC encoder test
namespace mvce_view_output
{
    /*!\enum Enumerator with special test cases params
    */
    enum
    {
        MFX_PAR         = 1,
        FREE_SERF_PAR1  = 2,
        FREE_SERF_PAR2  = 3,
        DIFF_BITSTREAMS = 4,
        EXT_COD         = 5,
        EXT_SEQ_DESC    = 6,
        NOT_INITIALIZED
    };

    struct tc_struct
    {
        //! Value to check result of encoding
        mfxStatus encodeFrameAsyncSts;
        //! Value to check synchronization of the operation
        mfxStatus syncOperationSts;
        //! Value to check query for the number of input surface frames
        mfxStatus querySts;
        //! Value to check initialization of Media SDK lib
        mfxStatus initSts;
        //! Value to check obtained video parameters
        mfxStatus getVideoParamSts;
        //! Value to check that are different bitstreams
        mfxU16 mode;

        /*!\brief Structure with test-cases params
        */
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
    class TestSuite:tsVideoEncoder
    {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() : tsVideoEncoder() { }
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
        //! \brief Wipe bitstream
        void WipeBitstream();
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams()
    {
        tsExtBufType <mfxVideoParam> par;

        par.mfx.CodecId                 = MFX_CODEC_AVC;
        par.mfx.CodecProfile            = 128;
        par.mfx.TargetKbps              = 5000;
        par.mfx.MaxKbps                 = par.mfx.TargetKbps;
        par.mfx.RateControlMethod       = MFX_RATECONTROL_CBR;
        par.mfx.GopRefDist              = 1;
        par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.Width         = 352;
        par.mfx.FrameInfo.Height        = 288;
        par.mfx.FrameInfo.CropW         = 352;
        par.mfx.FrameInfo.CropH         = 288;
        par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        par.AddExtBuffer(MFX_EXTBUFF_MVC_SEQ_DESC, sizeof(mfxExtMVCSeqDesc));
        mfxExtMVCSeqDesc* sd = (mfxExtMVCSeqDesc* )par.GetExtBuffer(MFX_EXTBUFF_MVC_SEQ_DESC);
        sd->NumView                     = 2;

        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION, sizeof(mfxExtCodingOption));
        mfxExtCodingOption* co = (mfxExtCodingOption* )par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION);
        co->ViewOutput                  = MFX_CODINGOPTION_ON;
        return par;
    }

    const tc_struct TestSuite::test_case[] =
    {
        //SAME BITSTREAM WITH ViewOutput = ON
        {/*00*/ MFX_ERR_MORE_BITSTREAM, MFX_ERR_NONE,
        MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, NOT_INITIALIZED,
        {{FREE_SERF_PAR1, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 0},
        {FREE_SERF_PAR2, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 1}}
        },
        //DIFFERENT BITSTREAMS WITH ViewOutput = ON
        {/*01*/ MFX_ERR_MORE_BITSTREAM, MFX_ERR_NONE,
        MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, DIFF_BITSTREAMS,
        {{FREE_SERF_PAR1, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 0},
        {FREE_SERF_PAR2, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 1}}
        },
        //ViewOutput = OFF
        {/*03*/ MFX_ERR_MORE_DATA, MFX_ERR_NONE,
        MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, NOT_INITIALIZED,
        {{FREE_SERF_PAR1, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 0},
        {FREE_SERF_PAR2, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 1},
        {EXT_COD, &tsStruct::mfxExtCodingOption.ViewOutput, MFX_CODINGOPTION_OFF}}
        },
        //ViewOutput = UNKNOWN
        {/*04*/ MFX_ERR_MORE_DATA, MFX_ERR_NONE,
        MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, NOT_INITIALIZED,
        {{FREE_SERF_PAR1, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 0},
        {FREE_SERF_PAR2, &tsStruct::mfxFrameSurface1.Info.FrameId.ViewId, 1},
        {EXT_COD, &tsStruct::mfxExtCodingOption.ViewOutput, MFX_CODINGOPTION_UNKNOWN}}
        },
        //INCOMPATIBLE ViewId, Query WITH ViewOutput = ON
        //{/*05*/ MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN,
        //MFX_ERR_UNSUPPORTED, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, NOT_INITIALIZED,
        //{{EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumViewIdAlloc, 2},
        //{EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumViewId, 2},
        //{EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumViewAlloc, 2},
        //{EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumView, 2},
        //{EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumOPAlloc, 1},
        //{EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumOP, 1}}
        //},
        //INCOMPATIBLE ViewId, Init WITH ViewOutput = ON
        {/*06*/ MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN,
        MFX_ERR_UNKNOWN, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, NOT_INITIALIZED,
        {{EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumViewIdAlloc, 2},
        {EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumViewId, 2},
        {EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumViewAlloc, 2},
        {EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumView, 2},
        {EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumOPAlloc, 1},
        {EXT_SEQ_DESC, &tsStruct::mfxExtMVCSeqDesc.NumOP, 1}}
        }
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    void TestSuite::WipeBitstream()
    {
        m_bitstream.DataLength = 0;
        m_bitstream.DataOffset = 0;
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_MVC_SUPPORT();

        auto& tc = test_case[id];

        m_par = initParams();
        MFXInit();

        if(tc.querySts != MFX_ERR_UNKNOWN || tc.initSts != MFX_ERR_UNKNOWN)
        {
            SETPARS(&m_par, EXT_SEQ_DESC);

            mfxExtMVCSeqDesc* sd = (mfxExtMVCSeqDesc* )m_par.GetExtBuffer(MFX_EXTBUFF_MVC_SEQ_DESC);
            sd->ViewId = new mfxU16[2];
            sd->ViewId[0] = 3;
            sd->ViewId[1] = 4;
            
            sd->View = new mfxMVCViewDependency[2];
            sd->View[0].ViewId = 3;
            sd->View[1].ViewId = 4;

            sd->OP = new mfxMVCOperationPoint();
            sd->OP->NumViews = 2;
            sd->OP->NumTargetViews = 2;
            sd->OP->TargetViewId = sd->ViewId;

            if(tc.querySts != MFX_ERR_UNKNOWN)
            {
                g_tsStatus.expect(tc.querySts);
                Query();
                EXPECT_EQ(sd->ViewId[0], 0) <<
                                "ERROR: sd->ViewId[0] is not equal to 0 \n";
            }
            else
            {
                g_tsStatus.expect(tc.initSts);
                Init();
                g_tsStatus.expect(tc.getVideoParamSts);
                GetVideoParam();
                EXPECT_EQ(sd->ViewId[0], 0) <<
                                "ERROR: sd->ViewId[0] is not equal to 0 \n";
                EXPECT_EQ(sd->View[0].ViewId, 0) <<
                                "ERROR: sd->View[0].ViewId is not equal to 0 \n";
            }

            delete [] sd->ViewId;
            delete [] sd->View;
            delete sd->OP;
        }
        else
        {
            if(tc.encodeFrameAsyncSts == MFX_ERR_MORE_DATA)
            {
                SETPARS(&m_par, EXT_COD);
            }

            Init();
            QueryIOSurf();
            AllocSurfaces();

            g_tsStatus.expect(MFX_ERR_NOT_ENOUGH_BUFFER);
            GetVideoParam();

            AllocBitstream();
            m_pSurf = GetSurface();
            tsRawReader reader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo);
            g_tsStreamPool.Reg();
            m_filler = &reader;

            tsExtBufType<mfxBitstream> bistream0, bistream1;
            if(tc.mode == DIFF_BITSTREAMS)
            {
                bistream0 = m_bitstream;
                AllocBitstream();
                bistream1 = m_bitstream;
                WipeBitstream();
            }

            for(int i = 0; i < 5; ++i)
            {
                if(tc.mode == DIFF_BITSTREAMS)
                {
                    m_pBitstream = &bistream0;
                }

                SETPARS(m_pSurf, FREE_SERF_PAR1);
                mfxStatus sts = EncodeFrameAsync();
                g_tsStatus.expect(tc.encodeFrameAsyncSts);
                g_tsStatus.check(sts);

                if(tc.encodeFrameAsyncSts == MFX_ERR_MORE_BITSTREAM)
                {
                    g_tsStatus.expect(tc.syncOperationSts);
                    SyncOperation();
                }

                m_pSurf = GetSurface();

                if(tc.mode == DIFF_BITSTREAMS)
                {
                    m_pBitstream = &bistream0;
                }

                SETPARS(m_pSurf, FREE_SERF_PAR2);
                sts = EncodeFrameAsync();
                if(tc.encodeFrameAsyncSts == MFX_ERR_MORE_DATA)
                {
                    g_tsStatus.expect(MFX_ERR_NONE);
                }
                g_tsStatus.check(sts);

                SyncOperation();
                if(tc.mode == DIFF_BITSTREAMS)
                {
                    m_bitstream = bistream0;
                    WipeBitstream();
                    m_bitstream = bistream1;
                }
                WipeBitstream();
                m_pSurf = GetSurface();
            }
        }
        TS_END;
        return 0;
    }
    //!\brief Reg the test suite into test system
    TS_REG_TEST_SUITE_CLASS(mvce_view_output);
}