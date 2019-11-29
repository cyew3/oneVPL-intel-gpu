/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019-2020 Intel Corporation. All Rights Reserved.

File Name: avce_partial_output.cpp
\* ****************************************************************************** */

/*!
\file avce_partial_output.cpp
\brief Gmock test avce_partial_output

Description:
This suite tests partial output for AVC encoder\n

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
- Encode bitstream and check the stream for validity / presense of partial output accoring to selected mode
*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_partial_output{

    enum{
        MFX_PO = 1,
        MFX_PAR = 2,
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct{
        //! Expected Query() status
        mfxStatus q_sts;
        //! Expected Init() status
        mfxStatus i_sts;
        /*! \brief Structure contains params for some fields of encoder */

        bool encode;
        const char* stream;

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
        //! \brief Encodes stream
        void encode(int n, const char* stream);
        //! \brief Checks stream
        void check_bs(mfxU16 numSlice);
        //! \brief Set of test cases for HW lib
        static const tc_struct test_case[];
        //! \brief The number of frames to encode
        mfxI16 n_frames;
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        n_frames = 10;

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

        par.AsyncDepth = 1;
        par.mfx.NumSlice = 3;
        par.mfx.LowPower = MFX_CODINGOPTION_ON;
        par.mfx.TargetUsage = 7;
        par.AddExtBuffer(MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM, sizeof(mfxExtPartialBitstreamParam));

        return par;
    }

    void TestSuite::encode(int n, const char* stream){

        mfxI32 submitted = 0;
        mfxI32 encoded = 0;

        AllocSurfaces();

        mfxExtPartialBitstreamParam* po2 = (mfxExtPartialBitstreamParam*)m_par.GetExtBuffer(MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM);
        tsRawReader reader(stream, m_par.mfx.FrameInfo, n);

        while (encoded < n){
            m_pSurf = 0;
            if (submitted < n){
                m_pSurf = GetSurface();
                submitted++;
            }

            mfxStatus sts;
            if(m_pSurf) sts = reader.ProcessSurface(*m_pSurf);

            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);

            if(sts == MFX_ERR_NONE) {
                mfxStatus s;
                /* For slices check number of slices in output
                   for blocks check that the whole stream contains the expected number of blocks
                   for any check that any block exists (number of partial outputs > 0)
                */
                int numPartialOutputs = 0;
                int effectiveLength = m_pBitstream->DataLength + m_pBitstream->DataOffset;
                do {
                    s = MFXVideoCORE_SyncOperation(m_session, m_syncpoint, MFX_INFINITE);

                    if(MFX_ERR_NONE == s) {
                        numPartialOutputs++;
                    }
                    else if(MFX_ERR_NONE_PARTIAL_OUTPUT == s) {
                        numPartialOutputs++;
                    }
                    else if(MFX_ERR_NONE > s) {
                        g_tsLog << "[ ERROR ] MSDK error code reported out of SyncOperation: " << s << " \n";
                        throw tsFAIL;
                    }
                } while(s == MFX_ERR_NONE_PARTIAL_OUTPUT);
                encoded++;

                if (po2->Granularity == MFX_PARTIAL_BITSTREAM_SLICE) {
                    EXPECT_EQ(m_par.mfx.NumSlice, numPartialOutputs) << "ERROR: number of slices not equal to number of outputs";
                }
                else if (po2->Granularity == MFX_PARTIAL_BITSTREAM_BLOCK && po2->BlockSize > 0) {
                    int numBlocks = (m_pBitstream->DataLength + m_pBitstream->DataOffset - effectiveLength + po2->BlockSize - 1) / po2->BlockSize;
                    EXPECT_EQ(numBlocks, numPartialOutputs) << "ERROR: number of blocks not equal to number of block in output";
                }
                else if (po2->Granularity == MFX_PARTIAL_BITSTREAM_ANY) {
                    EXPECT_GT(numPartialOutputs, 1) << "ERROR: no partial outputs";
                }
            }
            else
                if(sts == MFX_ERR_MORE_DATA) {
                    if(!m_pSurf)
                        break;
                }
                else {
                    g_tsLog << "[ ERROR ] MSDK error code reported out of EncodeFrameAsync: " << sts << " \n";
                    throw tsFAIL;
                }
        }
    }

    void TestSuite::check_bs(mfxU16 numSlice) {
        BSErr err = BS_ERR_NONE;
        tsParserAVC2 parser(BS_AVC2::INIT_MODE_PARSE_SD);
        tsParserAVC2::UnitType *pAU;
        Bs32s n_slices;

        parser.set_buffer(m_bitstream.Data + m_bitstream.DataOffset, m_bitstream.DataLength);
        parser.set_trace_level(0);
        while ((err = parser.parse_next_au(pAU)) == BS_ERR_NONE)
        {
            n_slices = 0;
            for (Bs32u i = 0; i < pAU->NumUnits; i++)
            {
                if (pAU->nalu[i]->nal_unit_type == BS_AVC2::SLICE_NONIDR ||
                    pAU->nalu[i]->nal_unit_type == BS_AVC2::SLICE_IDR)
                    n_slices++;
            }
            EXPECT_EQ(numSlice, n_slices);
        }
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE,false,
        "forBehaviorTest/foreman_cif.yuv",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240},
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.Granularity,  MFX_PARTIAL_BITSTREAM_NONE},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.BlockSize,  0},
        }},
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, true,
        "forBehaviorTest/park_scene_1920x1088_30_content_1920x1080.yuv",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 3},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.Granularity,  MFX_PARTIAL_BITSTREAM_SLICE},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.BlockSize,  0},
        }},
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, true,
        "forBehaviorTest/park_scene_1920x1088_30_content_1920x1080.yuv",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 8},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.Granularity,  MFX_PARTIAL_BITSTREAM_SLICE},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.BlockSize,  0},
        }},
        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, true,
        "forBehaviorTest/park_scene_1920x1088_30_content_1920x1080.yuv",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.Granularity,  MFX_PARTIAL_BITSTREAM_BLOCK},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.BlockSize,  4096},
        }},
        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, true,
        "forBehaviorTest/park_scene_1920x1088_30_content_1920x1080.yuv",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.Granularity,  MFX_PARTIAL_BITSTREAM_BLOCK},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.BlockSize,  4097},
        }},
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, true,
        "forBehaviorTest/park_scene_1920x1088_30_content_1920x1080.yuv",
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
            {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.Granularity,  MFX_PARTIAL_BITSTREAM_ANY},
            {MFX_PO, &tsStruct::mfxExtPartialBitstreamParam.BlockSize,  0},
        }},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        tc_struct temp_tc = test_case[id];
        auto& tc = temp_tc;

        if (g_tsImpl == MFX_IMPL_SOFTWARE) {
             g_tsLog << "SKIPPED for software implementation\n";
             return 0;
        }

        MFXInit();
        m_par = initParams();

        SETPARS(&m_par, MFX_PAR);
        SETPARS(&m_par, MFX_PO);

        const char* stream = g_tsStreamPool.Get(tc.stream);
        g_tsStreamPool.Reg();

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        mfxExtPartialBitstreamParam* po2 = (mfxExtPartialBitstreamParam*)m_par.GetExtBuffer(MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM);
        mfxU16 before_granulatiry = 0;
        mfxU16 before_block_size = 0;
        if (po2) {
            before_granulatiry = po2->Granularity;
            before_block_size = po2->BlockSize;
        }

        mfxStatus exp = tc.q_sts;
        g_tsStatus.expect(exp);
        Query();

        if (exp == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM || exp == MFX_ERR_UNSUPPORTED) {
            EXPECT_NE(before_granulatiry, po2->Granularity) << "ERROR: Granularity was not changed";
            EXPECT_NE(before_block_size, po2->BlockSize) << "ERROR: BlockSize was not changed";
        }

        if (po2) {
            before_granulatiry = po2->Granularity;
            before_block_size = po2->BlockSize;
        }
        exp = tc.i_sts;
        g_tsStatus.expect(exp);
        Init();

        if (exp == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM){
            GetVideoParam();
            EXPECT_NE(before_granulatiry, po2->Granularity) << "ERROR: Granularity was not changed";
            EXPECT_NE(before_block_size, po2->BlockSize) << "ERROR: BlockSize was not changed";
        }

        if (exp == MFX_ERR_NONE && tc.encode){
            AllocBitstream(1000000 * n_frames);
            encode(n_frames, stream);
            check_bs(m_par.mfx.NumSlice);
        }
        TS_END;

        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_partial_output);
}
