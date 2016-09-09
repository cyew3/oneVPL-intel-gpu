/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

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
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

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

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
       
        {/*00 Call with correct default params*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }
        },
        {/*01 Check System memory*/ MFX_ERR_NONE, MEMORY_TYPE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
            }
        },
        {/*02 Check Opaque memory*/ MFX_ERR_NONE, MEMORY_TYPE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12 },
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

        //Rate Control Metod
        {/*28*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*29*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*30*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*37*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },

        //encoded order
        {/*39*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1 } },

        //native
        {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, NOT_LOAD_PLUGIN, NONE, {} },

        //call query
        {/*41*/ MFX_ERR_NONE, CALL_QUERY, NONE, {} },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        mfxHDL hdl;
        mfxHandleType type;
        mfxStatus sts;

        MFXInit();

        if (tc.type != NOT_LOAD_PLUGIN)
            Load();
        else
            m_loaded = true;

        SETPARS(m_pPar, MFX_PAR);

        // Currently only VIDEO_MEMORY is supported
        if(tc.type != MEMORY_TYPE)
        {
            m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        }

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }

        if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            && (!m_pVAHandle))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            m_pVAHandle = m_pFrameAllocator;
            m_pVAHandle->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
            m_is_handle_set = (g_tsStatus.get() >= 0);
        }

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
            if (g_tsHWtype < MFX_HW_CNL) // unsupported on platform less CNL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
                g_tsStatus.check(sts);
                return 0;
            }
        } else {
            g_tsLog << "WARNING: only HW Platform supported!\n";
            sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
            g_tsStatus.check(sts);
            return 0;
        }

        if (m_pPar->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (tc.type == EXT_BUFF)
        {
            if (tc.sub_type == NONE)
                sts = MFX_ERR_NULL_PTR;
            else
                if (tc.sts != MFX_ERR_NONE)
                    sts = MFX_ERR_UNSUPPORTED;
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

    TS_REG_TEST_SUITE_CLASS(vp9e_init);
}
