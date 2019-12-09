/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

File Name: hevce_intra_refresh.cpp
\* ****************************************************************************** */

/*!
\file hevce_intra_refresh.cpp
\brief Gmock test hevce_intra_refresh

Description:
This suite tests HEVC encoder\n

Algorithm:
- Initializing MSDK lib
- Set suite params (HEVC encoder params)
- Set case params
- Set expected Query() status
- Call Query() and check IntRef values if necessary (invalid values)
- Set case params
- Set expected Init() status
- Call Init()
- If returned status is MFX_ERR_NONE call GetVideoParam() and check IntRef values were changed
- Reinitialize MSDK lib
- Call Init()
- Set case params
- Set expected Reset() status
- Call Reset()
- Check returned status

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

#include <cmath>

/*! \brief Main test name space */
namespace hevce_intra_refresh{

    using namespace BS_HEVC2;

#ifdef _DEBUG
#define DEBUG_STREAM "hevce_dynamic_intra_refresh.h265"
#endif

    enum
    {
        CDO2_PAR  = 1,
        QUERY_EXP = 2,
        INIT_EXP  = 3,
        RESET_EXP = 4,
    };

    enum
    {
        UNSUPPORTED = 0,
        DUALPIPE_SUPPORTED = 0x1,
        LOWPOWER_SUPPORTED = 0x2,
        PERFORM_ENCODING = 0x4,
    };

    static int  getPlatfromFlags()
    {
        int ret = 0;

        if (g_tsHWtype >= MFX_HW_CNL && g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
        {
            ret = LOWPOWER_SUPPORTED;
        }
        else
        {
            ret = DUALPIPE_SUPPORTED;
        }

        return ret;
    }

    struct tc_struct{
        //! Expected Query() status
        mfxStatus q_sts;
        //! Expected Init() status
        mfxStatus i_sts;
        //! Expected Reset() status
        mfxStatus r_sts;

        int mask;

        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair{
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxI32 v;
        }set_par[MAX_NPARS];
    };

    //!\brief Main test class
    class TestSuite:public tsVideoEncoder , public tsParserHEVC2
#ifndef DEBUG_STREAM
    , public tsBitstreamProcessor
#else
    , public tsBitstreamWriter
#endif
    {
    private:

    const mfxU16 NumFrames = 6;
    mfxU8 minCuSize = 8;
    mfxU8 widthInMinCu;
    mfxU8 heightInMinCu;
    std::vector<mfxU8> pCu;

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    inline bool IsHEVCSlice(Bs32u nut) { return (nut <= 21) && ((nut < 10) || (nut > 15)); }
    void fillBitMap(mfxU8* pMap, mfxU8 value, Bs32u log2CbSize, mfxU32 lineShift);
    bool CheckRollingI();
    bool firstFrame = true;

    public:
        //! \brief A constructor (HEVC encoder)
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC2(PARSE_SSD)
#ifdef DEBUG_STREAM
            , tsBitstreamWriter(DEBUG_STREAM)
#endif
        {
            m_bs_processor = this;
        }
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

        inline bool IsIntra(CU* cu) { return cu->PredMode == MODE_INTRA; };
    };

void TestSuite::fillBitMap(mfxU8* pMap, mfxU8 value, Bs32u log2CbSize, mfxU32 lineShift = 0) {
        //Set cuSizeInMinCu as size of CU in blocks 8x8
        mfxU32 cuSizeInMinCu = (mfxU32)pow(2, log2CbSize - 3);
        if (!lineShift)
            lineShift = cuSizeInMinCu;
        for (Bs32u i = 0; i < cuSizeInMinCu; i++) {
            for (Bs32u j = 0; j < cuSizeInMinCu; j++) {
                *(pMap++) = value;
            }
            pMap += lineShift - cuSizeInMinCu;
        }
    }

bool TestSuite::CheckRollingI() {
        for (auto isCuRefreshed : pCu) {
            if (!isCuRefreshed)
                return false;
        }
        return true;
    }

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    if (bs.Data)
    SetBuffer0(bs);

    mfxU32 checked = 0;

    while (checked++ < nFrames)
    {
        auto& hdr = ParseOrDie();

        //IntraRefresh does not makes sense in the case I frames.
        if (firstFrame) {
            firstFrame = false;
            continue;
        }

        for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
        {
            if (!IsHEVCSlice(pNALU->nal_unit_type))
                continue;
            auto& slice = *pNALU->slice;
            for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next)
            {
                auto cu = pCTU->Cu;
                while (cu)
                {
                    if (IsIntra(cu))
                        fillBitMap(&pCu[cu->x / minCuSize + cu->y / minCuSize * widthInMinCu], 1, cu->log2CbSize, widthInMinCu);
                    cu = cu->Next;
                }
            }
        }
#ifdef DEBUG_STREAM
        g_tsLog << "Intra:\n";
        mfxU32 arrayIndex = 0;
        for (int i = 0; i < heightInMinCu; i++) {
            for (int j = 0; j < widthInMinCu; j++) {
                g_tsLog << (int)pCu[arrayIndex++];
            }
            g_tsLog << "\n";
        }
#endif
    }
#ifdef DEBUG_STREAM
    tsBitstreamWriter::ProcessBitstream(bs, nFrames);
#endif
    bs.DataLength = 0;
    return MFX_ERR_NONE;
}

tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType <mfxVideoParam> par;
        par.mfx.LowPower                = g_tsConfig.lowpower;
        par.mfx.CodecId                 = MFX_CODEC_HEVC;
        par.mfx.CodecLevel              = MFX_LEVEL_HEVC_41;
        par.mfx.CodecProfile            = MFX_PROFILE_HEVC_MAIN;
        par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        par.mfx.FrameInfo.Width         = 352;
        par.mfx.FrameInfo.Height        = 288;
        par.mfx.FrameInfo.CropW         = 352;
        par.mfx.FrameInfo.CropH         = 288;
        par.mfx.TargetKbps              = 5000;
        par.mfx.MaxKbps                 = par.mfx.TargetKbps;
        par.mfx.RateControlMethod       = 3;
        par.mfx.QPI                     = 25;
        par.mfx.QPP                     = 26;
        par.mfx.QPB                     = 27;
        par.mfx.GopRefDist              = 1;
        par.mfx.GopPicSize              = 4;
        par.mfx.TargetUsage             = MFX_TARGETUSAGE_1;
        par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        mfxExtCodingOption2* co2 = (mfxExtCodingOption2* )par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
        co2->IntRefType      = 1;
        co2->IntRefCycleSize = 2;
        co2->IntRefQPDelta   = 0;
        return par;
    }




#define co2_IntRefType &tsStruct::mfxExtCodingOption2.IntRefType
#define co2_IntRefCycleSize &tsStruct::mfxExtCodingOption2.IntRefCycleSize
#define co2_IntRefQPDelta &tsStruct::mfxExtCodingOption2.IntRefQPDelta

#define TEST_CASE_PARAMETERS(Type, IntRefType, IntRefCycleSize, IntRefQPDelta) \
    { Type, co2_IntRefType, (IntRefType) },                                    \
    { Type, co2_IntRefCycleSize, (IntRefCycleSize) },                          \
    { Type, co2_IntRefQPDelta, (IntRefQPDelta) }

    const tc_struct TestSuite::test_case[] =
    {
        // VALID VERTICAL PARAMS
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  1, 2, 4),
          TEST_CASE_PARAMETERS(QUERY_EXP, 1, 2, 4),
          TEST_CASE_PARAMETERS(INIT_EXP,  1, 2, 4),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 2, 4) } },
        // VALID HORIZONTAL PARAMS
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  2, 2, 4),
          TEST_CASE_PARAMETERS(QUERY_EXP, 2, 2, 4),
          TEST_CASE_PARAMETERS(INIT_EXP,  2, 2, 4),
          TEST_CASE_PARAMETERS(RESET_EXP, 2, 2, 4) } },
        // VALID WITH NEGATIVE IntRefQPDelta
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  1, 2, -24),
          TEST_CASE_PARAMETERS(QUERY_EXP, 1, 2, -24),
          TEST_CASE_PARAMETERS(INIT_EXP,  1, 2, -24),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 2, -24) } },
        // INVALID IntRefType
        {/*03*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  3, 2, 4),
          TEST_CASE_PARAMETERS(QUERY_EXP, 2, 2, 4),
          TEST_CASE_PARAMETERS(INIT_EXP,  3, 2, 4),
          TEST_CASE_PARAMETERS(RESET_EXP, 3, 2, 4) } },
        // INVALID IntRefQPDelta = 53
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  1, 2, 53),
          TEST_CASE_PARAMETERS(QUERY_EXP, 1, 2, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  1, 2, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 2, 0) } },
        // INVALID IntRefQPDelta = -53
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  1, 2, -53),
          TEST_CASE_PARAMETERS(QUERY_EXP, 1, 2, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  1, 2, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 2, 0) } },
        // INVALID RefCicleSize > par.mfx.GopPicSize
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  1, 10, 0),
          TEST_CASE_PARAMETERS(QUERY_EXP, 0, 0, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  0, 0, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 0, 0, 0) } },
        // VALID ALL ZEROES
        {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  0, 0, 0),
          TEST_CASE_PARAMETERS(QUERY_EXP, 0, 0, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  0, 0, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 2, 0) } },
        // INVALID ALL -1
        {/*08*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  -1, -1, -1),
          TEST_CASE_PARAMETERS(QUERY_EXP, 0, 0, -1),
          TEST_CASE_PARAMETERS(INIT_EXP,  -1, -1, -1),
          TEST_CASE_PARAMETERS(RESET_EXP, -1, -1, -1) } },
        // VALID mfxVideoParam.mfx.GopRefDist < mfxVideoParam.mfx.GopPicSize - 1
        {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { { CDO2_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
          TEST_CASE_PARAMETERS(CDO2_PAR,  1, 2, 0),
          TEST_CASE_PARAMETERS(QUERY_EXP, 1, 2, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  1, 2, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 2, 0) } },
        // INVALID mfxVideoParam.mfx.GopRefDist > mfxVideoParam.mfx.GopPicSize - 1 (Case for platforms with DualPipe).
        {/*10*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { { CDO2_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10 },
          { QUERY_EXP, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3 },
          { INIT_EXP, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3 },
          { RESET_EXP, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10 },
          TEST_CASE_PARAMETERS(CDO2_PAR,  1, 2, 0),
          TEST_CASE_PARAMETERS(QUERY_EXP, 1, 2, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  1, 2, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 2, 0) } },
        // VALID zero IntRefQPDelta
        {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE, (mfxStatus)0,
          LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED,
        { TEST_CASE_PARAMETERS(CDO2_PAR,  1, 2, 0) } },
        // VALID VERTICAL PARAMS WITH ENCODE
        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED | PERFORM_ENCODING,
        { { CDO2_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 },
          TEST_CASE_PARAMETERS(CDO2_PAR,  1, 5, 0),
          TEST_CASE_PARAMETERS(QUERY_EXP, 1, 5, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  1, 5, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 1, 5, 0) } },
        // VALID HORIZONTAL PARAMS WITH ENCODE
        {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        LOWPOWER_SUPPORTED | DUALPIPE_SUPPORTED | PERFORM_ENCODING,
        { { CDO2_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 },
          TEST_CASE_PARAMETERS(CDO2_PAR,  2, 5, 0),
          TEST_CASE_PARAMETERS(QUERY_EXP, 2, 5, 0),
          TEST_CASE_PARAMETERS(INIT_EXP,  2, 5, 0),
          TEST_CASE_PARAMETERS(RESET_EXP, 2, 5, 0) } }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];
        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS)
        {
            g_tsLog << "SKIPPED! Unsupported on Linux platforms!\n";
            throw tsSKIP;
        }
        m_par = initParams();

        SETPARS(&m_par, CDO2_PAR);

        if ((getPlatfromFlags() & tc.mask) != DUALPIPE_SUPPORTED && (getPlatfromFlags() & tc.mask) != LOWPOWER_SUPPORTED)
        {
            if (getPlatfromFlags() != tc.mask)
            {
                g_tsLog << "\n\nWARNING: SKIP test unsupported platform\n\n";
                throw tsSKIP;
            }
        }

        MFXInit();

        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

        if (g_tsHWtype)
            g_tsStatus.expect(MFX_ERR_NONE);
        g_tsStatus.check(this->GetCaps(&caps, &capSize));
        if (0 == caps.RollingIntraRefresh)
        {
            if ((g_tsHWtype < MFX_HW_SKL) ||
                (g_tsConfig.lowpower != MFX_CODINGOPTION_ON && (g_tsHWtype >= MFX_HW_CNL)))
            {
                g_tsLog << "\n\nWARNING: SKIP test unsupported platform.\n\n";
                throw tsSKIP;
            } else 
            {
                g_tsLog << "Error! Platform is unsupported but expected it is.\n\n";
                throw tsFAIL;
            }
        } else
        {
            if ((g_tsHWtype < MFX_HW_SKL) ||
                (g_tsConfig.lowpower != MFX_CODINGOPTION_ON && (g_tsHWtype >= MFX_HW_CNL)))
            {
                g_tsLog << "Error! Platform is supported but expected it isn't.\n\n";
                throw tsFAIL;
            }
        }

        auto stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
        g_tsStreamPool.Reg();
        tsRawReader reader(stream, m_par.mfx.FrameInfo, 100);
        m_filler = &reader;

        Load();

        tsExtBufType<mfxVideoParam> out_par;

        out_par = m_par;
        m_pParOut = &out_par;
        g_tsStatus.expect(tc.q_sts);
        Query();
        EXPECT_TRUE(COMPAREPARS(out_par, QUERY_EXP)) << "Error! Parameters mismatch.";

        SETPARS(&m_par, CDO2_PAR);

        out_par = m_par;
        m_pParOut = &out_par;
        g_tsStatus.expect(tc.i_sts);
        Init();
        if (g_tsStatus.m_status >= MFX_ERR_NONE)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            GetVideoParam(m_session, &out_par);
            EXPECT_TRUE(COMPAREPARS(out_par, INIT_EXP)) << "ERROR: GetVideoParam(after INIT) parameters doesn't match expectation\n";
        }
        else
            EXPECT_TRUE(COMPAREPARS(out_par, INIT_EXP)) << "Error! Parameters mismatch.";

        if (PERFORM_ENCODING & tc.mask)
        {
            //Create matrix with min Cu size.
            widthInMinCu = m_par.mfx.FrameInfo.Width / minCuSize;
            heightInMinCu = m_par.mfx.FrameInfo.Height / minCuSize;
            pCu.resize(widthInMinCu * heightInMinCu, 0);
            EncodeFrames(NumFrames);
            SyncOperation();
            EXPECT_TRUE(CheckRollingI()) << "Error! RollingIntraRefresh is not successful.";
        }

        // Case #11 tests only Query and Init functions
        if (id < 11)
        {
            MFXClose();

            MFXInit();

            Load();
            m_par = initParams();

            Init();
            SETPARS(&m_par, CDO2_PAR);

            out_par = m_par;
            m_pParOut = &out_par;
            g_tsStatus.expect(tc.r_sts);

            Reset();
            if (g_tsStatus.m_status >= MFX_ERR_NONE)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                GetVideoParam(m_session, &out_par);
                EXPECT_TRUE(COMPAREPARS(out_par, RESET_EXP)) << "ERROR: GetVideoParam(after RESET) parameters doesn't match expectation\n";
            }
            else
                EXPECT_TRUE(COMPAREPARS(out_par, RESET_EXP)) << "Error! Parameters mismatch.";
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(hevce_intra_refresh);
}
