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
#include "ts_fei_warning.h"

namespace hevce_fei_numslice
{
    class TestSuite
        : tsVideoEncoder, tsBitstreamProcessor, tsParserHEVCAU
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
#ifdef MANUAL_DEBUG_MODE
            , m_tsBsWriter("test.h265")
#endif
        {
            m_bs_processor = this;
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            m_par.AsyncDepth = 1;

            mfxExtFeiHevcEncFrameCtrl& control = m_ctrl;

            control.SubPelMode = 3;         // quarter-pixel motion estimation
            control.SearchWindow = 5;       // 48 SUs 48x40 window full search
            control.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
                                            // enable internal L0/L1 predictors: 1 - spatial predictors
            control.MultiPred[0] = control.MultiPred[1] = 1;
        }
        ~TestSuite() {}

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
#ifdef MANUAL_DEBUG_MODE
        tsBitstreamWriter m_tsBsWriter;
#endif
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

        enum {
            MFX_PAR = 1,
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
    };

#define mfx_Width           tsStruct::mfxVideoParam.mfx.FrameInfo.Width
#define mfx_Height          tsStruct::mfxVideoParam.mfx.FrameInfo.Height
#define mfx_CropW           tsStruct::mfxVideoParam.mfx.FrameInfo.CropW
#define mfx_CropH           tsStruct::mfxVideoParam.mfx.FrameInfo.CropH
#define mfx_NumSlice        tsStruct::mfxVideoParam.mfx.NumSlice

#define SET_RESOLUTION(W, H) { MFX_PAR, &mfx_Width,  W}, \
                             { MFX_PAR, &mfx_Height, H}, \
                             { MFX_PAR, &mfx_CropW,  W}, \
                             { MFX_PAR, &mfx_CropH,  H}

#define SET_RESOLUTION_720x480   SET_RESOLUTION(720,480)
#define SET_RESOLUTION_1920x1088 SET_RESOLUTION(1920,1088)
#define SET_RESOLUTION_4096x2160 SET_RESOLUTION(4096,2160)

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // 720x480
        {/*00*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { MFX_PAR, &mfx_NumSlice, 0}
        }},
        {/*01*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { MFX_PAR, &mfx_NumSlice, 10}
        }},
        {/*02*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { MFX_PAR, &mfx_NumSlice, 68}
        }},
        {/*03*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { MFX_PAR, &mfx_NumSlice, 200}
        }},
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_720x480,
        { MFX_PAR, &mfx_NumSlice, 300}
        }},

        // 1920x1080
        {/*05*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { MFX_PAR, &mfx_NumSlice, 0}
        }},
        {/*06*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { MFX_PAR, &mfx_NumSlice, 45}
        }},
        {/*07*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { MFX_PAR, &mfx_NumSlice, 128}
        }},
        {/*08*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { MFX_PAR, &mfx_NumSlice, 200}
        }},
        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_1920x1088,
        { MFX_PAR, &mfx_NumSlice, 300}
        }},

        // 4096x2160
        {/*11*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { MFX_PAR, &mfx_NumSlice, 0 }
        } },
        {/*11*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { MFX_PAR, &mfx_NumSlice, 199 }
        } },
        {/*12*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { MFX_PAR, &mfx_NumSlice, 164 }
        } },
        {/*13*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { MFX_PAR, &mfx_NumSlice, 200 }
        } },
        {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_4096x2160,
        { MFX_PAR, &mfx_NumSlice, 311 }
        } }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        CHECK_HEVC_FEI_SUPPORT();

        MFXInit();

        // load required plugin
        Load();

        SETPARS(m_par, MFX_PAR);

        g_tsStatus.expect(tc.exp_sts);
        Query();

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

        mfxU16 exp_num_ctu_in_slice = nCTU / m_par.mfx.NumSlice;
        mfxU32 num_ctu_in_slice = 0;

        // value (address) of the beginning of the previous slice
        mfxU32 previous_segment_address = 0;
        mfxU16 sliceCount = 0;

        while (nFrames--)
        {
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
                {
                    num_ctu_in_slice = s.segment_address - previous_segment_address;
                    g_tsLog << "s = " << sliceCount << ", num_ctu_in_slice = " << num_ctu_in_slice << "\n";

                    EXPECT_LE(num_ctu_in_slice, exp_num_ctu_in_slice + 1)
                        << "ERROR: Num CTU per Slice exceeds expected value.\n";

                    EXPECT_GE(num_ctu_in_slice, exp_num_ctu_in_slice - 1)
                        << "ERROR: Num CU per Slice is lower than expected value.\n";
                    previous_segment_address = s.segment_address;
                }
                sliceCount++;
            }
        }

        bs.DataLength = 0;
        return sts;
    }
    TS_REG_TEST_SUITE_CLASS(hevce_fei_numslice);
}