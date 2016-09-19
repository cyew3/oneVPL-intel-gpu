/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace vp9e_query
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
        SESSION_NULL,
        TARGET_USAGE,
        NONE,
        RESOLUTION,
        NOT_ALLIGNED,
        SET_ALLOCK,
        AFTER,
        EXT_BUFF,
        IN_PAR_NULL,
        OUT_PAR_NULL,
        IO_PATTERN,
        MEMORY_TYPE,
        FRAME_RATE,
        PIC_STRUCT,
        CROP,
        XY,
        W,
        H,
        YH,
        XW,
        WH,
        PROTECTED,
        INVALID
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
    {/*00 Target usage 1 'Best Quality'*/ MFX_ERR_NONE, TARGET_USAGE, NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1}},
    {/*01 Target usage 2*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 2 } },
    {/*02 Target usage 3*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 3 } },
    {/*03 Target usage 4 'Balanced'*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 } },
    {/*04 Target usage 5*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 5 } },
    {/*05 Target usage 6*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 6 } },
    {/*06 Target usage 7 'Best Speed'*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 } },

    {/*07*/ MFX_ERR_NULL_PTR, EXT_BUFF, NONE, {} },

    //Rate Control Metod
    {/*08*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
    {/*09*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
    {/*10*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
    {/*11*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
    {/*12*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
    {/*13*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
    {/*14*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
    {/*15*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
    {/*16*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
    {/*17*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
    {/*18*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },

    //session
    {/*19*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },

    //set allock handle
    {/*20*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
    {/*21*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
    {/*22*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
    {/*23*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },

    //zero param
    {/*24*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, {} },
    {/*25*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, {} },

    //IOPattern
    {/*26*/ MFX_ERR_NONE, IO_PATTERN, MEMORY_TYPE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
    {/*27*/ MFX_ERR_NONE, IO_PATTERN, MEMORY_TYPE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
    {/*28*/ MFX_ERR_NONE, IO_PATTERN, MEMORY_TYPE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
    {/*29*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, MEMORY_TYPE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },

    //FourCC
    {/*30*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12 } },
    {/*31*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 } },

    //Resolution
    {/*32 Check zero resolution*/ MFX_ERR_NONE, RESOLUTION, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
        }
    },
    {/*33 Height is 0*/ MFX_ERR_NONE, RESOLUTION, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
        }
    },
    {/*34 Width is 0*/ MFX_ERR_NONE, RESOLUTION, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 }
        }
    },
    {/*35 Check too big resolution*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096 + 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096 + 1 }
        }
    },

    //PicStruct
    {/*36*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*37*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*38*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
    {/*39*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },

    //Crops
    {/*40 Invalid crop region*/ MFX_ERR_UNSUPPORTED, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
    {/*41 Invalid crop region*/ MFX_ERR_UNSUPPORTED, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
    {/*42 Valid crop region*/ MFX_ERR_NONE, CROP, W, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 } },
    {/*43 Valid crop region*/ MFX_ERR_NONE, CROP, H, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 } },
    {/*44 Crop region exceeds frame size*/ MFX_ERR_UNSUPPORTED, CROP, WH,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 + 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 }
        }
    },
    {/*45 Valid crop region*/ MFX_ERR_NONE, CROP, XW,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
        }
    },
    {/*46 Valid crop region*/ MFX_ERR_NONE, CROP, YH,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
        }
    },
    {/*47 Valid crop region*/ MFX_ERR_NONE, CROP, W,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
        }
    },
    {/*48 Valid crop region*/ MFX_ERR_NONE, CROP, H,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
        }
    },

    //frame rate
    {/*49*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
    {/*50*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },

    //gop pic size
    {/*51*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 } },
    //gop ref dist
    {/*52*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 } },
    //got opt flag
    {/*53*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 2 } },

    //chroma format
    {/*54*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 255 } },
    //protected
    {/*55*/
#if (defined(LINUX32) || defined(LINUX64))
            MFX_ERR_UNSUPPORTED,
#else
            MFX_ERR_NONE,
#endif
                          PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP } },
    {/*56*/
#if (defined(LINUX32) || defined(LINUX64))
        MFX_ERR_UNSUPPORTED,
#else
        MFX_ERR_NONE,
#endif
                       PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP } },
    {/*57*/ MFX_ERR_UNSUPPORTED, PROTECTED, INVALID, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0xfff } },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxHDL hdl;
    mfxHandleType type;
    mfxStatus sts;

    // set default parameters
    m_pParOut = new mfxVideoParam;

    MFXInit();

    Load();

    SETPARS(m_pPar, MFX_PAR);

    // Currently only VIDEO_MEMORY is supported
    if(tc.type != MEMORY_TYPE)
    {
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }

    *m_pParOut = m_par;

    if (!GetAllocator())
    {
        if (m_pVAHandle)
            SetAllocator(m_pVAHandle, true);
        else
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
    }

    if ((tc.type == SET_ALLOCK) && (tc.sub_type != AFTER)
         && (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            && (!m_pVAHandle)))
    {
        m_pFrameAllocator = GetAllocator();
        SetFrameAllocator();

        if (!m_is_handle_set)
        {
            m_pFrameAllocator->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
        }
    }

    if (tc.type == EXT_BUFF)
    {
        m_pPar->NumExtParam = 1;
        m_pParOut->NumExtParam = 1;
        m_pPar->ExtParam = NULL;
        m_pParOut->ExtParam = NULL;
    }

    g_tsStatus.expect(tc.sts);

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

    if (tc.type == IN_PAR_NULL)
    {
        m_pPar = NULL;
    }
    if (tc.type == OUT_PAR_NULL)
    {
        m_pParOut = NULL;
    }

    //call tested function
    TRACE_FUNC3(MFXVideoENCODE_Query, m_session, m_pPar, m_pParOut);
    if (tc.type != SESSION_NULL)
        sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
    else
        sts = MFXVideoENCODE_Query(NULL, m_pPar, m_pParOut);

    TRACE_FUNC3(MFXVideoENCODE_Query, m_session, m_pPar, m_pParOut);

    g_tsStatus.check(sts);
    TS_TRACE(m_pParOut);

    if ((tc.type == SET_ALLOCK) && (tc.sub_type == AFTER))
    {
        m_pFrameAllocator = GetAllocator();
        SetFrameAllocator();

        if (!m_is_handle_set)
        {
            m_pFrameAllocator->get_hdl(type, hdl);

            if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
                g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); // device was created by Core in Query() to get HW caps
            SetHandle(m_session, type, hdl);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vp9e_query);
}
