/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

File Name: le_avce_query.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

#if defined(MFX_ONEVPL)
#include "mfxpavp.h"
#endif

namespace le_avce_query
{

#define INVALID_WIDTH   4096
#define INVALID_HEIGHT  4096

    enum
    {
        MFX_PAR,
        TARGET_USAGE,
        SESSION_NULL,
        SET_ALLOCK,
        IN_PAR_NULL,
        OUT_PAR_NULL,
        RESOLUTION,
        DELTA,
        CROP,
        IO_PATTERN,
        FRAME_RATE,
        PIC_STRUCT,
        PROTECTED,
        W_GT_MAX,
        H_GT_MAX,
        NONE,
        ALIGNMENT_HW,
        RATE_CONTROL
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        mfxU32 sub_type;
        mfxHyperMode hyper_encode_mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxI32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() { }

        static const unsigned int n_cases;

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:

        static const tc_struct test_case[];

        mfxU32 AlignResolutionByPlatform(mfxU16 value)
        {
            mfxU32 alignment = g_tsHWtype >= MFX_HW_ICL ? 8 : 16;
            return (value + (alignment - 1)) & ~(alignment - 1);
        }

        mfxU32 AlignSurfaceSize(mfxU16 value)
        {
            mfxU32 surfAlignemnet = 16;
            return (value + (surfAlignemnet - 1)) & ~(surfAlignemnet - 1);
        }
    };

    const tc_struct TestSuite::test_case[] =
    {
        /* Target Usage */
        {/*00*/ MFX_ERR_NONE, TARGET_USAGE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 } },
        {/*01*/ MFX_ERR_NONE, TARGET_USAGE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 2 } },
        {/*02*/ MFX_ERR_NONE, TARGET_USAGE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 3 } },
        {/*03*/ MFX_ERR_NONE, TARGET_USAGE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 } },
        {/*04*/ MFX_ERR_NONE, TARGET_USAGE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 5 } },
        {/*05*/ MFX_ERR_NONE, TARGET_USAGE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 6 } },
        {/*06*/ MFX_ERR_NONE, TARGET_USAGE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 } },

        /* Rate Control Method */
        // single fallback if method is not supported in dual mode
        {/*07*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*08*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*09*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*10*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*11*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*12*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*13*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*14*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*15*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*16*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*17*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        {/*18*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
        // forced single mode
        {/*19*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*20*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*21*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*22*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*23*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        // forced dual mode
        {/*24*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*25*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*26*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*27*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*28*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },

        /* session */
        {/*29*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, MFX_HYPERMODE_ADAPTIVE, {} },
        {/*30*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, MFX_HYPERMODE_OFF, {} },
        {/*31*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, MFX_HYPERMODE_ON, {} },

        /* set alloc handle */
        {/*32*/ MFX_ERR_NONE, SET_ALLOCK, NONE, MFX_HYPERMODE_ADAPTIVE, { } },
        {/*33*/ MFX_ERR_NONE, SET_ALLOCK, NONE, MFX_HYPERMODE_OFF, { } },
        {/*34*/ MFX_ERR_NONE, SET_ALLOCK, NONE, MFX_HYPERMODE_ON, { } },

        /* zero param */
        {/*35*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, MFX_HYPERMODE_ON, {} },
        {/*36*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, MFX_HYPERMODE_ON, {} },

        /* IOPattern */
        {/*37*/ MFX_ERR_NONE, IO_PATTERN, NONE, MFX_HYPERMODE_ON, { } },
        {/*38*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },
        {/*39*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, IO_PATTERN, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },

        /* Unsupported FourCC */
        {/*40*/ MFX_ERR_UNSUPPORTED, NONE, NONE, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
                                                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0 },}},

        /* Resolution */
        {/*41*/ MFX_ERR_UNSUPPORTED, RESOLUTION, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3 } },
        {/*42*/ MFX_ERR_UNSUPPORTED, RESOLUTION, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3 } },
        {/*43*/ MFX_ERR_UNSUPPORTED, RESOLUTION, W_GT_MAX, MFX_HYPERMODE_ON, {} },
        {/*44*/ MFX_ERR_UNSUPPORTED, RESOLUTION, H_GT_MAX, MFX_HYPERMODE_ON, {} },
        {/*45*/ MFX_ERR_NONE, RESOLUTION, NONE, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },}},

        /* PicStruct */
        // LowPower require Progressive
        {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
        {/*47*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
        {/*48*/ MFX_ERR_NONE, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } },
        {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
        {/*50*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },

        /* Crops */
        {/*51*/ MFX_ERR_UNSUPPORTED, CROP, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
        {/*52*/ MFX_ERR_UNSUPPORTED, CROP, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
        {/*53*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 } },
        {/*54*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 } },
        {/*55*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 },
                                                                      { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 }}},
        {/*56*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },}},
        {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },}},
        {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },}},
        {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },}},

        /* frame rate */
        {/*60*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
        {/*61*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },

        /* chroma format */
        {/*62*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 255 } },

        /* num thread */
        {/*63*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 1 } },
        {/*64*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 4 } },

        /* gop ref dist */
        {/*65*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 } },

        /* got opt flag */
        {/*66*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 2 } },

        /* protected */
        {/*67*/ MFX_ERR_NONE, PROTECTED, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP } },
        {/*68*/ MFX_ERR_NONE, PROTECTED, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP } },
        {/*69*/ MFX_ERR_UNSUPPORTED, PROTECTED, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0xfff } },
};

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;
        mfxHDL hdl;
        mfxHandleType type;
        mfxStatus sts = tc.sts;

        // set default parameters
        std::unique_ptr<mfxVideoParam> tmp_par(new mfxVideoParam);
        m_pParOut = tmp_par.get();
        *m_pParOut = m_par;

        MFXInit();

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_RGB4)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        m_par.IOPattern = (g_tsImpl == MFX_IMPL_HARDWARE) ?
            MFX_IOPATTERN_IN_SYSTEM_MEMORY :
            (g_tsImpl & MFX_IMPL_VIA_D3D11) ? MFX_IOPATTERN_IN_VIDEO_MEMORY : m_par.IOPattern;

        bool isSw = (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY);

        if (!GetAllocator())
        {
            if (m_pVAHandle)
                SetAllocator(m_pVAHandle, true);
            else
                UseDefaultAllocator(isSw);
        }

        if (tc.sub_type != DELTA) {  // not dependent from resolution params
            SETPARS(m_pPar, MFX_PAR);

            m_pPar->mfx.FrameInfo.Width = 736;
            m_pPar->mfx.FrameInfo.Height = 480;

            if (tc.type == RESOLUTION)
            {
                if (tc.sub_type == W_GT_MAX) {
                    m_pPar->mfx.FrameInfo.Width = INVALID_WIDTH + 16;
                }
                if (tc.sub_type == H_GT_MAX) {
                    m_pPar->mfx.FrameInfo.Height = INVALID_HEIGHT + 16;
                }
            }

        }
        else
        {   // pars depend on resolution
            for (mfxU32 i = 0; i < MAX_NPARS; i++) {
                if (tc.set_par[i].f && tc.set_par[i].ext_type == MFX_PAR) {
                    if (tc.type == RESOLUTION)
                    {
                        if (tc.set_par[i].f->name.find("Width") != std::string::npos) {
                            m_pPar->mfx.FrameInfo.Width = ((m_pPar->mfx.FrameInfo.Width + 15) & ~15) + tc.set_par[i].v;
                        }
                        if (tc.set_par[i].f->name.find("Height") != std::string::npos)
                            m_pPar->mfx.FrameInfo.Height = ((m_pPar->mfx.FrameInfo.Height + 15) & ~15) + tc.set_par[i].v;
                    }
                    if (tc.type == CROP)
                    {
                        if (tc.set_par[i].f->name.find("CropX") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropX += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropY") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropY += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropW") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropW += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropH") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropH += tc.set_par[i].v;
                    }
                }
            }
        }

        if ((tc.type == SET_ALLOCK)
            && (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
                && (!m_pVAHandle)))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();

            if (!isSw && !m_is_handle_set)
            {
                m_pFrameAllocator->get_hdl(type, hdl);
                SetHandle(m_session, type, hdl);
            }
        }

        // set the options for dual GPU mode
        m_pPar->mfx.LowPower = MFX_CODINGOPTION_ON;
        m_pPar->mfx.IdrInterval = 0;
        m_pPar->mfx.GopPicSize = 30;
        if (tc.type != RATE_CONTROL)
            m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_VBR;

        // set the options required for AVC
        m_pPar->mfx.CodecId = MFX_CODEC_AVC;
        m_pPar->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
        m_pPar->mfx.TargetKbps = m_pPar->mfx.MaxKbps = 10000;

        if (tc.type != PIC_STRUCT)
            m_pPar->mfx.FrameInfo.PicStruct = 1;

        if (tc.type == PIC_STRUCT && (tc.set_par->v == MFX_PICSTRUCT_FIELD_TFF || tc.set_par->v == MFX_PICSTRUCT_FIELD_BFF))
            m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;

        if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            m_pPar->mfx.QPI = m_pPar->mfx.QPP = m_pPar->mfx.QPB = 24;

        // add the buffer to enable dual GPU mode        
        m_par.AddExtBuffer(MFX_EXTBUFF_HYPER_MODE_PARAM, sizeof(mfxExtHyperModeParam));
        mfxExtBuffer* hyperModeParam = m_par.GetExtBuffer(MFX_EXTBUFF_HYPER_MODE_PARAM);
        mfxExtHyperModeParam* hyperModeParamTmp = (mfxExtHyperModeParam*)hyperModeParam;
        hyperModeParamTmp->Mode = tc.hyper_encode_mode;

        m_pPar->NumExtParam = 1;
        m_pParOut->NumExtParam = 1;
        m_pPar->ExtParam = &hyperModeParam;
        m_pParOut->ExtParam = &hyperModeParam;
        
        if (tc.type == IN_PAR_NULL)
            m_pPar = NULL;

        if (tc.type == OUT_PAR_NULL)
            m_pParOut = NULL;

        g_tsStatus.expect(sts);
        g_tsStatus.disable_next_check();
        //call tested function
        if (tc.type != SESSION_NULL)
            sts = Query(m_session, m_pPar, m_pParOut);
        else
            sts = Query(NULL, m_pPar, m_pParOut);

        g_tsStatus.check(sts);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_avce_8b_420_nv12_query, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_avce_8b_444_rgb4_query, RunTest_Subtype<MFX_FOURCC_RGB4>, n_cases);
}
