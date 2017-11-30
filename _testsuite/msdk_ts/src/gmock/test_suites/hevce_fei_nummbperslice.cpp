/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_fei_nummbperslice {

    class TestSuite : tsVideoEncoder, tsBitstreamProcessor, tsParserHEVCAU
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, MSDK_PLUGIN_TYPE_FEI)
#ifdef MANUAL_DEBUG_MODE
            , m_tsBsWriter("test_out.h265")
#endif
        {
            m_bs_processor = this;

            m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            m_pPar->mfx.FrameInfo.Width  = m_pPar->mfx.FrameInfo.CropW = 4096;
            m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 2160;

            m_filler = new tsRawReader(g_tsStreamPool.Get("YUV/rally_4096x2160_30fps_1s_YUV.yuv"), m_pPar->mfx.FrameInfo);
            g_tsStreamPool.Reg();

            m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            m_pPar->AsyncDepth = 1;

            mfxExtFeiHevcEncFrameCtrl& control = m_ctrl;

            control.SubPelMode = 3;         // quarter-pixel motion estimation
            control.SearchWindow = 5;       // 48 SUs 48x40 window full search
            control.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
                                            // enable internal L0/L1 predictors: 1 - spatial predictors
            control.MultiPred[0] = control.MultiPred[1] = 1;
        }

        ~TestSuite()
        {
            delete m_filler;
        }

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

        enum {
            MFX_PAR = 1,
            CDO2_PAR = 2,
        };

        struct tc_struct
        {
            // Expected Init() status
            mfxStatus exp_sts;

            struct
            {
                // Number of the params set
                mfxU32 ext_type;
                // Field name
                const  tsStruct::Field* f;
                // Field value
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

#ifdef MANUAL_DEBUG_MODE
        tsBitstreamWriter m_tsBsWriter;
#endif
    };

#define mfx_Width           tsStruct::mfxVideoParam.mfx.FrameInfo.Width
#define mfx_Height          tsStruct::mfxVideoParam.mfx.FrameInfo.Height
#define mfx_CropW           tsStruct::mfxVideoParam.mfx.FrameInfo.CropW
#define mfx_CropH           tsStruct::mfxVideoParam.mfx.FrameInfo.CropH

#define SET_RESOLUTION(W, H) { MFX_PAR, &mfx_Width,  W}, \
                             { MFX_PAR, &mfx_Height, H}, \
                             { MFX_PAR, &mfx_CropW,  W}, \
                             { MFX_PAR, &mfx_CropH,  H}

#define SET_RESOLUTION_176x144   SET_RESOLUTION(176,144)
#define SET_RESOLUTION_352x288   SET_RESOLUTION(352,288)
#define SET_RESOLUTION_720x480   SET_RESOLUTION(720,480)
#define SET_RESOLUTION_1280x720  SET_RESOLUTION(1280,720)
#define SET_RESOLUTION_1920x1088 SET_RESOLUTION(1920,1088)
#define SET_RESOLUTION_4096x2160 SET_RESOLUTION(4096,2160)

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // 176x144
        {/*00*/ MFX_ERR_NONE,
            { SET_RESOLUTION_176x144,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 1 },
        } },
        {/*01*/ MFX_ERR_NONE,
            { SET_RESOLUTION_176x144,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 6 },
        } },
        {/*02*/ MFX_ERR_NONE,
            { SET_RESOLUTION_176x144,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 30 },
        } },
        {/*03*/ MFX_ERR_NONE,
            { SET_RESOLUTION_176x144,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0 },
        } },

        // 352x288
        {/*04*/ MFX_ERR_NONE,
            { SET_RESOLUTION_352x288,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 1 },
        } },
        {/*05*/ MFX_ERR_NONE,
            { SET_RESOLUTION_352x288,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 30 },
        } },
        {/*06*/ MFX_ERR_NONE,
            { SET_RESOLUTION_352x288,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 99 },
        } },
        {/*07*/ MFX_ERR_NONE,
            { SET_RESOLUTION_352x288,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0 },
        } },

        // 720x480
        {/*08*/ MFX_ERR_NONE,
            { SET_RESOLUTION_720x480,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 1 },
        } },
        {/*09*/ MFX_ERR_NONE,
            { SET_RESOLUTION_720x480,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 23 },
        } },
        {/*10*/ MFX_ERR_NONE,
            { SET_RESOLUTION_720x480,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 150 },
        } },
        {/*11*/ MFX_ERR_NONE,
            { SET_RESOLUTION_720x480,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0 },
        } },

        // 1280x720
        {/*12*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1280x720,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 1 },
        } },
        {/*13*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1280x720,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 40 },
        } },
        {/*14*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1280x720,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 95 },
        } },
        {/*15*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1280x720,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0 },
        } },

        // 1920x1088
        {/*16*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1920x1088,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 1 },
        } },
        {/*17*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1920x1088,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 30 },
        } },
        {/*18*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1920x1088,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 180 },
        } },
        {/*19*/ MFX_ERR_NONE,
            { SET_RESOLUTION_1920x1088,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0 },
        } },

        // 4096x2160
        {/*20*/ MFX_ERR_NONE,
            { SET_RESOLUTION_4096x2160,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 1 },
        } },
        {/*21*/ MFX_ERR_NONE,
            { SET_RESOLUTION_4096x2160,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 60 },
        } },
        {/*22*/ MFX_ERR_NONE,
            { SET_RESOLUTION_4096x2160,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 180 },
        } },
        {/*23*/ MFX_ERR_NONE,
            { SET_RESOLUTION_4096x2160,
            { CDO2_PAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0 },
        } },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        // create mfx session
        MFXInit();

        // load required plugin
        Load();

        SETPARS(m_par, MFX_PAR);
        SETPARS(m_par, CDO2_PAR);

        g_tsStatus.expect(tc.exp_sts);
        Init();

        EncodeFrames(4);

        TS_END;
        return 0;
    }

    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        mfxStatus sts = MFX_ERR_NONE;
#ifdef MANUAL_DEBUG_MODE
        sts = m_tsBsWriter.ProcessBitstream(bs, nFrames);
#endif

        // count number of CTU (32x32) per frame
        mfxU32 nCTU = MSDK_ALIGN32(m_par.mfx.FrameInfo.Width) * MSDK_ALIGN32(m_par.mfx.FrameInfo.Height) >> 10;

        mfxExtCodingOption2& co2 = m_par;

        // expected number of CTU per Slice
        mfxU16 exp_nCtuPerSlice;
        // we expect specified NumMbPerSlice. if NumMbPerSlice == 0, then it will be only ONE slice per frame.
        if (co2.NumMbPerSlice != 0)
            exp_nCtuPerSlice = co2.NumMbPerSlice;
        else
            exp_nCtuPerSlice = nCTU;

        // vector to store number of cu per each slice
        std::vector <Bs32u> numCUsPerSlice;
        // value (address) of the beginning of the previous slice
        Bs32u previous_segment_address = 0;

        while (nFrames--)
        {
            numCUsPerSlice.clear();

            UnitType& au = ParseOrDie();
            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                if (au.nalu[i] == NULL)
                    return MFX_ERR_NULL_PTR;
                Bs16u type = au.nalu[i]->nal_unit_type;
                //check slice segment only
                if (type > BS_HEVC::CRA_NUT || ((type > BS_HEVC::RASL_R) && (type < BS_HEVC::BLA_W_LP)))
                {
                    continue;
                }

                if (au.nalu[i]->slice == NULL)
                    return MFX_ERR_NULL_PTR;
                auto& s = *au.nalu[i]->slice;

                if (s.segment_address != 0)
                    numCUsPerSlice.push_back(s.segment_address - previous_segment_address);

                previous_segment_address = s.segment_address;
            }
            // add number of CTU of the last slice
            numCUsPerSlice.push_back(nCTU - previous_segment_address);

            for (size_t i = 0; i < numCUsPerSlice.size(); i++)
            {
                g_tsLog << "i = " << i << ", numCU = " << numCUsPerSlice[i] << "\n";
            }

            for (size_t i = 0; i < numCUsPerSlice.size() - 1; i++)
            {
                EXPECT_EQ(numCUsPerSlice[i], exp_nCtuPerSlice)
                    << "ERROR: numCU per Slice is not as expected and it is not the last slice.\n";
            }

            // size of numCUsPerSlice vector will NOT be equal zero
            EXPECT_LE(numCUsPerSlice[numCUsPerSlice.size() - 1], exp_nCtuPerSlice)
                << "ERROR: Num CU per Slice exceeds expected value.\n";
        }

        bs.DataLength = 0;

        return sts;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_nummbperslice);
}
