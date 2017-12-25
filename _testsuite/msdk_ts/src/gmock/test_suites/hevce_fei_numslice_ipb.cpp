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

namespace hevce_fei_numslice_ipb
{
    class TestSuite
        : tsVideoEncoder, tsBitstreamProcessor, tsParserHEVCAU
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
#ifdef MANUAL_DEBUG_MODE
            , m_tsBsWriter("test_stream.h265")
#endif
        {
            m_bs_processor = this;
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            m_par.AsyncDepth = 1;

            m_par.mfx.GopRefDist = 2;
            m_par.mfx.GopPicSize = 5;

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
            CDO3_PAR = 2
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
#define mfx_NumSliceI       tsStruct::mfxExtCodingOption3.NumSliceI
#define mfx_NumSliceP       tsStruct::mfxExtCodingOption3.NumSliceP
#define mfx_NumSliceB       tsStruct::mfxExtCodingOption3.NumSliceB

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
        { CDO3_PAR, &mfx_NumSliceI, 0},
        { CDO3_PAR, &mfx_NumSliceP, 0},
        { CDO3_PAR, &mfx_NumSliceB, 0}
        }},
        {/*01*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { CDO3_PAR, &mfx_NumSliceI, 5 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        } },
        {/*02*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 2 },
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        } },
        {/*03*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 3 }
        } },
        {/*04*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { CDO3_PAR, &mfx_NumSliceI, 1 },
        { CDO3_PAR, &mfx_NumSliceP, 2 },
        { CDO3_PAR, &mfx_NumSliceB, 3 }
        }},
        {/*05*/ MFX_ERR_NONE,
        { SET_RESOLUTION_720x480,
        { CDO3_PAR, &mfx_NumSliceI, 60 },
        { CDO3_PAR, &mfx_NumSliceP, 20 },
        { CDO3_PAR, &mfx_NumSliceB, 30 }
        }},
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_720x480,
        { CDO3_PAR, &mfx_NumSliceI, 10 },
        { CDO3_PAR, &mfx_NumSliceP, 320},
        { CDO3_PAR, &mfx_NumSliceB, 150}
        }},
        {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_720x480,
        { CDO3_PAR, &mfx_NumSliceI, 300 },
        { CDO3_PAR, &mfx_NumSliceP, 300 },
        { CDO3_PAR, &mfx_NumSliceB, 300 }
        }},

        // 1920x1080
        {/*08*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        }},
        {/*09*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 7 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        } },
        {/*10*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 5 },
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        } },
        {/*11*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 5 }
        } },
        {/*12*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 1 },
        { CDO3_PAR, &mfx_NumSliceP, 2 },
        { CDO3_PAR, &mfx_NumSliceB, 3 }
        }},
        {/*13*/ MFX_ERR_NONE,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 80 },
        { CDO3_PAR, &mfx_NumSliceP, 120},
        { CDO3_PAR, &mfx_NumSliceB, 33 }
        }},
        {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 100},
        { CDO3_PAR, &mfx_NumSliceP, 260},
        { CDO3_PAR, &mfx_NumSliceB, 50 }
        }},
        {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_1920x1088,
        { CDO3_PAR, &mfx_NumSliceI, 350 },
        { CDO3_PAR, &mfx_NumSliceP, 300 },
        { CDO3_PAR, &mfx_NumSliceB, 300 }
        }},

        // 4096x2160
        {/*16*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        }},
        {/*17*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 8 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        } },
        {/*18*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 10},
        { CDO3_PAR, &mfx_NumSliceB, 0 }
        } },
        {/*19*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 0 },
        { CDO3_PAR, &mfx_NumSliceP, 0 },
        { CDO3_PAR, &mfx_NumSliceB, 2 }
        } },
        {/*20*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 1 },
        { CDO3_PAR, &mfx_NumSliceP, 2 },
        { CDO3_PAR, &mfx_NumSliceB, 3 }
        }},
        {/*21*/ MFX_ERR_NONE,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 100},
        { CDO3_PAR, &mfx_NumSliceP, 29 },
        { CDO3_PAR, &mfx_NumSliceB, 37 }
        }},
        {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 50 },
        { CDO3_PAR, &mfx_NumSliceP, 2  },
        { CDO3_PAR, &mfx_NumSliceB, 300}
        } },
        {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { SET_RESOLUTION_4096x2160,
        { CDO3_PAR, &mfx_NumSliceI, 300 },
        { CDO3_PAR, &mfx_NumSliceP, 300 },
        { CDO3_PAR, &mfx_NumSliceB, 300 }
        }}
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
        SETPARS(m_par, CDO3_PAR);

        mfxExtCodingOption3 co3 = m_par;
        mfxU16 exp_numslice_i = co3.NumSliceI;
        mfxU16 exp_numslice_p = co3.NumSliceP;
        mfxU16 exp_numslice_b = co3.NumSliceB;

        g_tsStatus.expect(tc.exp_sts);
        Query();

        if (tc.exp_sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
            EXPECT_NE(exp_numslice_i, co3.NumSliceI) << "ERROR: NumSliceI was not changed";
            EXPECT_NE(exp_numslice_p, co3.NumSliceP) << "ERROR: NumSliceP was not changed";
            EXPECT_NE(exp_numslice_b, co3.NumSliceB) << "ERROR: NumSliceB was not changed";
            return 0;
        }

        Init();

        EncodeFrames(5);

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

        while (nFrames--)
        {
            mfxU32 slice_count = 0;

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
                slice_count++;
            }

            mfxExtCodingOption3 co3 = m_par;

            switch (bs.FrameType) {
            case MFX_FRAMETYPE_I:
                EXPECT_EQ(co3.NumSliceI, slice_count);
                break;
            case MFX_FRAMETYPE_P:
                EXPECT_EQ(co3.NumSliceP, slice_count);
                break;
            case MFX_FRAMETYPE_B:
                EXPECT_EQ(co3.NumSliceB, slice_count);
                break;
            default:
                g_tsLog << "Unexpected frame type in ProcessBitstream()\n";
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
                break;
            }
        }

        bs.DataLength = 0;
        return sts;
    }
    TS_REG_TEST_SUITE_CLASS(hevce_fei_numslice_ipb);
}