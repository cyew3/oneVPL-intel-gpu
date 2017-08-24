/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

File Name: vp9e_init.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace vp9e_init
{
    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9){}
        ~TestSuite() { }

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            mfxU32 sub_type;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);
        static const unsigned int n_cases;

        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_p010;
        static const unsigned int n_cases_ayuv;
        static const unsigned int n_cases_y410;

    private:
        enum
        {
            MFX_PAR,
            NONE,
            SESSION_NULL,

            PAR_NULL,
            _2_CALL,
            _2_CALL_CLOSE,
            PIC_STRUCT,
            RESOLUTION,
            CROP,
            XY,
            WH,
            XW,
            YH,
            W,
            H,
            ZEROED,
            CHROMA_FORMAT,
            INVALID,
            FRAME_RATE,
            EXT_BUFF,
            RATE_CONTROL,
            NOT_LOAD_PLUGIN,
            CALL_QUERY,
            MEMORY_TYPE
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00 Call with correct default params*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }
        },
        {/*01 Check System memory*/ MFX_ERR_NONE, MEMORY_TYPE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
            }
        },
        {/*02 Check Opaque memory*/ MFX_ERR_NONE, MEMORY_TYPE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY }
            }
        },
        {/*03 Null session handler*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },
        {/*04 Null params handler*/ MFX_ERR_NULL_PTR, PAR_NULL, NONE, {} },
        {/*05 Twice call init*/ MFX_ERR_UNDEFINED_BEHAVIOR, _2_CALL, NONE, {} },
        {/*06 Twice call close*/ MFX_ERR_NONE, _2_CALL_CLOSE, NONE, {} },

        //PicStruct
        {/*07 Check Field_tff*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
        {/*08 Check Field_bff*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
        {/*09 Check incorrect PictStruct value*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
        {/*10 Check incorrect PictStruct value*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
        {/*11 Check zero resolution*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, ZEROED,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
            }
        },
        {/*12 Height is 0*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
            }
        },
        {/*13 Width is 0*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 }
            }
        },
        {/*14 Check too big resolution*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096 + 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096 + 1 }
            }
        },

        //Crops
        {/*15 Invalid crop region*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
        {/*16 Invalid crop region*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
        {/*17 Valid crop region*/ MFX_ERR_NONE, CROP, W, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 } },
        {/*18 Valid crop region*/ MFX_ERR_NONE, CROP, H, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 } },
        {/*19 Crop region exceeds frame size*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, WH,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 + 10 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 }
            }
        },
        {/*20 Valid crop region*/ MFX_ERR_NONE, CROP, XW,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
            }
        },
        {/*21 Valid crop region*/ MFX_ERR_NONE, CROP, YH,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },
        {/*22 Valid crop region*/ MFX_ERR_NONE, CROP, W,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
            }
        },
        {/*23 Valid crop region*/ MFX_ERR_NONE, CROP, H,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
            }
        },

        //chroma format
        {/*24 Incorrect chroma format*/ MFX_ERR_INVALID_VIDEO_PARAM, CHROMA_FORMAT, INVALID, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_RESERVED1 } },
        {/*25 Incorrect nominator for frame rate*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
        {/*26 Incorrect denominator for frame rate*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },
        {/*27 Empty ext_buffer*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, NONE, {} },

        //encoded order [currently feature is not supoorted]
        {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1 } },

        //native
        {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, NOT_LOAD_PLUGIN, NONE, {} },

        //call query
        {/*30*/ MFX_ERR_NONE, CALL_QUERY, NONE, {} },
        {/*31 Check async 1*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1 },
            }
        },
        {/*32 Check async 10*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 10 },
            }
        },
        {/*33 Chroma format is not set*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 0 },
            }
        },
        {/*34 Bit depth luma is not set*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0 },
            }
        },
        {/*35 Bit depth luma is wrong*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 9 },
            }
        },
        {/*36 Bit depth chroma is not set*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0 },
            }
        },
        {/*37 Bit depth chroma is wrong*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 9 },
            }
        },
        {/*38 Bit depths and chroma format are not set*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 0 },
            }
        },
        {/*39 Invalid LowPowerValue*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ADAPTIVE },
            }
        },
        {/*40 Invalid LowPowerValue*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, 1 },
        }
        },
        {/*41 Unsupported LowPowerValue*/ MFX_ERR_UNSUPPORTED, NONE, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
        }
        },

        /* Check this on Post-Si (on Pre-Si long hanging)
        {MFX_ERR_MEMORY_ALLOC, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 0xffff },
            }
        },
        */
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const unsigned int TestSuite::n_cases_nv12 = n_cases;
    const unsigned int TestSuite::n_cases_p010 = n_cases;
    const unsigned int TestSuite::n_cases_ayuv = n_cases;
    const unsigned int TestSuite::n_cases_y410 = n_cases;

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxStatus sts;

        MFXInit();

        if (tc.type != NOT_LOAD_PLUGIN)
            Load();
        else
            m_loaded = true;

        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        SETPARS(m_pPar, MFX_PAR);

        InitAndSetAllocator();

        if (tc.type == EXT_BUFF)
        {
            m_pPar->NumExtParam = 1;
            m_pParOut->NumExtParam = 1;
            m_pPar->ExtParam = NULL;
            m_pParOut->ExtParam = NULL;
        }

        sts = tc.sts;

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        if (tc.type == EXT_BUFF)
        {
            if (tc.sub_type == NONE)
                sts = MFX_ERR_NULL_PTR;
            else
                if (tc.sts != MFX_ERR_NONE)
                    sts = MFX_ERR_UNSUPPORTED;
        }

        if (m_pPar->IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            AllocSurfaces();

            m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
            mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

            MFXVideoENCODE_QueryIOSurf(m_session, m_pPar, &m_request);

            AllocOpaque(m_request, *osa);
        }

        mfxVideoParam *orig_par = NULL;

        if (tc.type == CALL_QUERY)
            Query();

        if (tc.type == PAR_NULL)
        {
            m_pPar = NULL;
        }
        else
        {
            orig_par = new mfxVideoParam();
            memcpy(orig_par, m_pPar, sizeof(mfxVideoParam));
        }

        if ((tc.type == _2_CALL) || (tc.type == _2_CALL_CLOSE))
        {
            Init(m_session, m_pPar);
            if (tc.type == _2_CALL_CLOSE)
                Close();
        }

        //call tested function
        g_tsStatus.expect(sts);
        TRACE_FUNC2(MFXVideoENCODE_Init, m_session, m_pPar);

        if (tc.type != SESSION_NULL) {
            sts = MFXVideoENCODE_Init(m_session, m_pPar);
        } else {
            sts = MFXVideoENCODE_Init(NULL, m_pPar);
        }

        g_tsStatus.check(sts);

        if ((orig_par) && (tc.sts == MFX_ERR_NONE))
        {
            EXPECT_EQ(0, memcmp(orig_par, m_pPar, sizeof(mfxVideoParam)))
                << "ERROR: Input parameters must not be changed in Init()";
            delete orig_par;
        }

        if(tc.type == SESSION_NULL) {
            g_tsStatus.expect(MFX_ERR_UNKNOWN);
        }

        if ((sts < MFX_ERR_NONE) && (tc.type != _2_CALL) && (tc.type != _2_CALL_CLOSE))
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        else
            g_tsStatus.expect(MFX_ERR_NONE);

        Close();

        TS_END;

        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_init,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_init, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_init,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases_ayuv);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_init, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
}
