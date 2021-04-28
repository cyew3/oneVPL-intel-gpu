/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

File Name: le_hevce_query.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

#if defined(MFX_ONEVPL)
#include "mfxpavp.h"
#endif

namespace le_hevce_query
{

#define INVALID_WIDTH   16384
#define INVALID_HEIGHT  16384

    enum
    {
        MFX_PAR,
        TARGET_USAGE,
        EXT_BUFF,
        SESSION_NULL,
        SET_ALLOCK,
        AFTER,
        IN_PAR_NULL,
        OUT_PAR_NULL,
        RESOLUTION,
        DELTA,  // param depends on resolution
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
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
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

        bool IsHevcHwPlugin()
        {
            return 0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data));
        }

        void CheckOutPar(tc_struct tc)
        {
            switch (tc.type)
            {
            case TARGET_USAGE:
            {
                if (IsHevcHwPlugin())
                {
                    // HW: supported only TU = {1,4,7}. Mapping: 2->1; 3->4; 5->4; 6->7
                    if (tc.set_par[0].v == 1 || tc.set_par[0].v == 4 || tc.set_par[0].v == 7)
                    {
                        tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
                    }
                    else
                    {
                        if (tc.set_par[0].v % 3 == 0)
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f) + 1);
                        else
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f) - 1);
                    }
                }
                break;
            }
            case ALIGNMENT_HW:
                if (IsHevcHwPlugin())
                {
                    mfxExtBuffer* hp;
                    GetBuffer(MFX_EXTBUFF_HEVC_PARAM, m_pPar, &hp);
                    EXPECT_NE_THROW(nullptr, hp, "ERROR: failed to get stream resolution");

                    mfxExtBuffer* hpOut;
                    GetBuffer(MFX_EXTBUFF_HEVC_PARAM, m_pParOut, &hpOut);
                    EXPECT_NE_THROW(nullptr, hpOut, "ERROR: failed to get resulted stream resolution");
                    //check width
                    EXPECT_EQ(AlignResolutionByPlatform(((mfxExtHEVCParam*)hp)->PicWidthInLumaSamples), ((mfxExtHEVCParam*)hpOut)->PicWidthInLumaSamples);
                    tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.Width, AlignSurfaceSize(m_pPar->mfx.FrameInfo.Width));
                    //check height
                    EXPECT_EQ(AlignResolutionByPlatform(((mfxExtHEVCParam*)hp)->PicHeightInLumaSamples), ((mfxExtHEVCParam*)hpOut)->PicHeightInLumaSamples);
                    tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.Height, AlignSurfaceSize(m_pPar->mfx.FrameInfo.Height));
                    //check crops
                    tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, m_pPar->mfx.FrameInfo.CropW);
                    tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, m_pPar->mfx.FrameInfo.CropH);
                }
                break;
            default: break;
            }
        }

        void GetBuffer(mfxU32 buff_id, const mfxVideoParam* par, mfxExtBuffer** buff)
        {
            *buff = nullptr;
            for (int i = 0; i < par->NumExtParam; i++)
                if (par->ExtParam[i]->BufferId == buff_id)
                    *buff = par->ExtParam[i];
        }

        void CreateBuffer(mfxU32 buff_id, mfxExtBuffer** buff, mfxExtBuffer** buff_out)
        {
            switch (buff_id)
            {
            case MFX_EXTBUFF_HYPER_MODE_PARAM:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtHyperModeParam());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtHyperModeParam());
                memset((*buff), 0, sizeof(mfxExtHyperModeParam));
                (*buff)->BufferId = MFX_EXTBUFF_HYPER_MODE_PARAM;
                (*buff)->BufferSz = sizeof(mfxExtHyperModeParam);
                break;
            }
            case MFX_EXTBUFF_HEVC_PARAM:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtHEVCParam());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtHEVCParam());
                memset((*buff), 0, sizeof(mfxExtHEVCParam));
                (*buff)->BufferId = MFX_EXTBUFF_HEVC_PARAM;
                (*buff)->BufferSz = sizeof(mfxExtHEVCParam);
                break;
            }
            case MFX_EXTBUFF_CODING_OPTION_SPSPPS:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtCodingOptionSPSPPS());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtCodingOptionSPSPPS());
                memset((*buff), 0, sizeof(mfxExtCodingOptionSPSPPS));
                (*buff)->BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
                (*buff)->BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
                break;
            }
            case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtVideoSignalInfo());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtVideoSignalInfo());
                memset((*buff), 0, sizeof(mfxExtVideoSignalInfo));
                (*buff)->BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
                (*buff)->BufferSz = sizeof(mfxExtVideoSignalInfo);
                break;
            }
            case MFX_EXTBUFF_CODING_OPTION:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtCodingOption());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtCodingOption());
                memset((*buff), 0, sizeof(mfxExtCodingOption));
                (*buff)->BufferId = MFX_EXTBUFF_CODING_OPTION;
                (*buff)->BufferSz = sizeof(mfxExtCodingOption);
                break;
            }
            case MFX_EXTBUFF_CODING_OPTION2:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtCodingOption2());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtCodingOption2());
                memset((*buff), 0, sizeof(mfxExtCodingOption2));
                (*buff)->BufferId = MFX_EXTBUFF_CODING_OPTION2;
                (*buff)->BufferSz = sizeof(mfxExtCodingOption2);
                break;
            }
            case MFX_EXTBUFF_VPP_DOUSE:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtVPPDoUse());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtVPPDoUse());
                memset((*buff), 0, sizeof(mfxExtVPPDoUse));
                (*buff)->BufferId = MFX_EXTBUFF_VPP_DOUSE;
                (*buff)->BufferSz = sizeof(mfxExtVPPDoUse);
                break;
            }
            case MFX_EXTBUFF_VPP_AUXDATA:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtVppAuxData());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtVppAuxData());
                memset((*buff), 0, sizeof(mfxExtVppAuxData));
                (*buff)->BufferId = MFX_EXTBUFF_VPP_AUXDATA;
                (*buff)->BufferSz = sizeof(mfxExtVppAuxData);
                break;
            }
            case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtVPPFrameRateConversion());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtVPPFrameRateConversion());
                memset((*buff), 0, sizeof(mfxExtVPPFrameRateConversion));
                (*buff)->BufferId = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
                (*buff)->BufferSz = sizeof(mfxExtVPPFrameRateConversion);
                break;
            }
            case MFX_EXTBUFF_PICTURE_TIMING_SEI:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtPictureTimingSEI());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtPictureTimingSEI());
                memset((*buff), 0, sizeof(mfxExtPictureTimingSEI));
                (*buff)->BufferId = MFX_EXTBUFF_PICTURE_TIMING_SEI;
                (*buff)->BufferSz = sizeof(mfxExtPictureTimingSEI);
                break;
            }
            case MFX_EXTBUFF_ENCODER_CAPABILITY:
            {
                (*buff) = (mfxExtBuffer*)(new mfxExtEncoderCapability());
                (*buff_out) = (mfxExtBuffer*)(new mfxExtEncoderCapability());
                memset((*buff), 0, sizeof(mfxExtEncoderCapability));
                (*buff)->BufferId = MFX_EXTBUFF_ENCODER_CAPABILITY;
                (*buff)->BufferSz = sizeof(mfxExtEncoderCapability);
                break;
            }
            case NONE:
            {
                *buff = NULL;
                *buff_out = NULL;
                break;
            }
            default: break;
            }
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

        /* Ext Buffers */
        {/*07*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, MFX_HYPERMODE_ON, {} },
        {/*08*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, MFX_HYPERMODE_ON, {} },
        {/*09*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, MFX_HYPERMODE_ON, {} },
        {/*10*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, MFX_HYPERMODE_ON, {} },
        {/*11*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, MFX_HYPERMODE_ON, {} },
        {/*12*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, MFX_HYPERMODE_ON, {} },
        {/*13*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, MFX_HYPERMODE_ON, {} },
        {/*14*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, MFX_HYPERMODE_ON, {} },
        {/*15*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, MFX_HYPERMODE_ON, {} },
        {/*16*/ MFX_ERR_NULL_PTR, EXT_BUFF, NONE, MFX_HYPERMODE_ON, {} },

        /* Rate Control Method */
        // single fallback if method is not supported in dual mode
        {/*17*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*18*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*19*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*20*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*21*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*22*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*23*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*24*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*25*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*26*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*27*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        {/*28*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
        // forced single mode
        {/*29*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*30*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*31*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*32*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*33*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        // forced dual mode
        {/*34*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*35*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*36*/ MFX_ERR_NONE, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*37*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*38*/ MFX_ERR_UNSUPPORTED, RATE_CONTROL, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },

        /* session */
        {/*39*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, MFX_HYPERMODE_ADAPTIVE, {} },
        {/*40*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, MFX_HYPERMODE_OFF, {} },
        {/*41*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, MFX_HYPERMODE_ON, {} },
        
        /* set alloc handle */
        {/*42*/ MFX_ERR_NONE, SET_ALLOCK, NONE, MFX_HYPERMODE_ON, { } },
        {/*43*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, MFX_HYPERMODE_ON, { } },
        {/*44*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, MFX_HYPERMODE_OFF, { } },
        {/*45*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, MFX_HYPERMODE_ADAPTIVE, { } },
        {/*46*/ MFX_ERR_UNSUPPORTED, SET_ALLOCK, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },

        /* zero param */
        {/*47*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, MFX_HYPERMODE_ON, {} },
        {/*48*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, MFX_HYPERMODE_ADAPTIVE, {} },
        {/*49*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, MFX_HYPERMODE_ADAPTIVE, {} },
        
        /* IOPattern */
        {/*50*/ MFX_ERR_NONE, IO_PATTERN, NONE, MFX_HYPERMODE_ON, { } },
        {/*51*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
        {/*52*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, NONE, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },
        
        /* Unsupported FourCC */
        {/*53*/ MFX_ERR_UNSUPPORTED, NONE, NONE, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
                                                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0 },}},
        
        /* Resolution */
        {/*54*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3 } },
        {/*55*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3 } },
        {/*56*/ MFX_ERR_UNSUPPORTED, RESOLUTION, W_GT_MAX, MFX_HYPERMODE_ON, {} },
        {/*57*/ MFX_ERR_UNSUPPORTED, RESOLUTION, H_GT_MAX, MFX_HYPERMODE_ON, {} },
        {/*58*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NONE, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },}},
        
        /* PicStruct */
        {/*59*/ MFX_ERR_NONE, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
        {/*60*/ MFX_ERR_NONE, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
        {/*61*/ MFX_ERR_NONE, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } },
        {/*62*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
        {/*63*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
        
        /* Crops */
        {/*64*/ MFX_ERR_UNSUPPORTED, CROP, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
        {/*65*/ MFX_ERR_UNSUPPORTED, CROP, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
        {/*66*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 } },
        {/*67*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 } },
        {/*68*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 },
                                                                      { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 }}},
        {/*69*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },}},
        {/*70*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },}},
        {/*71*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },}},
        {/*72*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, MFX_HYPERMODE_ON, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },}},
        
        /* frame rate */
        {/*73*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
        {/*74*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },
        
        /* chroma format */
        {/*75*/ MFX_ERR_UNSUPPORTED, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 255 } },
        
        /* num thread */
        {/*76*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 1 } },
        {/*77*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 4 } },

        /* gop ref dist */
        {/*78*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 } },
        
        /* got opt flag */
        {/*79*/ MFX_ERR_NONE, NONE, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 2 } },
        
        /* protected */
        {/*80*/ MFX_ERR_NONE, PROTECTED, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP } },
        {/*81*/ MFX_ERR_NONE, PROTECTED, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP } },
        {/*82*/ MFX_ERR_UNSUPPORTED, PROTECTED, NONE, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0xfff } },
        
        /* Alignment */
        // single fallback if it is not supported in dual mode
        {/*83*/ MFX_ERR_NONE, ALIGNMENT_HW, NONE, MFX_HYPERMODE_ADAPTIVE, {} },
        {/*84*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8 } },
        {/*85*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 8 } },
        {/*86*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 } },
        {/*87*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ADAPTIVE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 } },
        // forced single mode
        {/*88*/ MFX_ERR_NONE, ALIGNMENT_HW, NONE, MFX_HYPERMODE_OFF, {} },
        {/*89*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8 } },
        {/*90*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 8 } },
        {/*91*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 } },
        {/*92*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_OFF, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 } },
        // forced dual mode
        {/*93*/ MFX_ERR_NONE, ALIGNMENT_HW, NONE, MFX_HYPERMODE_ON, {} },
        {/*94*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8 } },
        {/*95*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 8 } },
        {/*96*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 } },
        {/*97*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, MFX_HYPERMODE_ON, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 } },
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
        mfxExtBuffer* buff_in = NULL;
        mfxExtBuffer* buff_out = NULL;
        mfxStatus sts = tc.sts;
        ENCODE_CAPS_HEVC caps = { 0 };
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);
        std::vector<mfxExtBuffer*> m_parInExtParams;
        std::vector<mfxExtBuffer*> m_parOutExtParams;

        // set default parameters
        std::unique_ptr<mfxVideoParam> tmp_par(new mfxVideoParam);
        m_pParOut = tmp_par.get();
        *m_pParOut = m_par;

        MFXInit();
        Load();

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
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
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

        mfxStatus caps_sts = GetCaps(&caps, &capSize);
        if (caps_sts != MFX_ERR_UNSUPPORTED)
            g_tsStatus.check(caps_sts);

        if (tc.sub_type != DELTA) {  // not dependent from resolution params
            SETPARS(m_pPar, MFX_PAR);

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
                    if (tc.type == RESOLUTION ||
                        (tc.type == ALIGNMENT_HW && IsHevcHwPlugin()))
                    {
                        if (tc.set_par[i].f->name.find("Width") != std::string::npos) {
                            m_pPar->mfx.FrameInfo.Width = ((m_pPar->mfx.FrameInfo.Width + 15) & ~15) + tc.set_par[i].v;
                        }
                        if (tc.set_par[i].f->name.find("Height") != std::string::npos)
                            m_pPar->mfx.FrameInfo.Height = ((m_pPar->mfx.FrameInfo.Height + 15) & ~15) + tc.set_par[i].v;
                    }
                    if (tc.type == CROP ||
                        (tc.type == ALIGNMENT_HW && IsHevcHwPlugin()))
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

        if ((tc.type == SET_ALLOCK) && (tc.sub_type != AFTER)
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
        m_pPar->mfx.IdrInterval = 1;
        m_pPar->mfx.GopPicSize = 30;

        // add the buffer to enable dual GPU mode
        m_par.AddExtBuffer(MFX_EXTBUFF_HYPER_MODE_PARAM, sizeof(mfxExtHyperModeParam));
        mfxExtBuffer* hyperModeParam = m_par.GetExtBuffer(MFX_EXTBUFF_HYPER_MODE_PARAM);
        mfxExtHyperModeParam* hyperModeParamTmp = (mfxExtHyperModeParam*)hyperModeParam;
        hyperModeParamTmp->Mode = tc.hyper_encode_mode;

        // put it in the vector of ext params to combine with others buffers
        m_parInExtParams.push_back(hyperModeParam);
        m_parOutExtParams.push_back(hyperModeParam);

        m_pPar->NumExtParam = m_parInExtParams.size();
        m_pParOut->NumExtParam = m_parOutExtParams.size();
        m_pPar->ExtParam = &m_parInExtParams[0];
        m_pParOut->ExtParam = &m_parOutExtParams[0];
        
        if (tc.type == EXT_BUFF)
        {
            CreateBuffer(tc.sub_type, &buff_in, &buff_out);
            if (buff_in && buff_out)
                *buff_out = *buff_in;
            m_parInExtParams.push_back(buff_in);
            m_parOutExtParams.push_back(buff_out);
            m_pPar->NumExtParam = m_parInExtParams.size();
            m_pParOut->NumExtParam = m_parOutExtParams.size();
            m_pPar->ExtParam = &m_parInExtParams[0];
            m_pParOut->ExtParam = &m_parOutExtParams[0];
        }

        if (IsHevcHwPlugin())
        {
            if ((m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_AVBR) && (tc.sts == MFX_ERR_NONE))
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

            if (!USE_REFACTORED_HEVCE && (tc.type == IO_PATTERN) && (tc.sts == MFX_ERR_UNSUPPORTED))
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

            if (tc.type == EXT_BUFF)
            {
                if ((tc.sub_type == MFX_EXTBUFF_VIDEO_SIGNAL_INFO))
                    sts = MFX_ERR_NONE;
                else if (tc.sub_type == NONE)
                    sts = MFX_ERR_NULL_PTR;
                else if (tc.sub_type == MFX_EXTBUFF_ENCODER_CAPABILITY && (g_tsImpl & MFX_IMPL_VIA_D3D11 || g_tsImpl == MFX_IMPL_HARDWARE))
                    sts = MFX_ERR_NONE;

            }
            if (tc.type == CROP)
            {
                if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                    sts = MFX_ERR_NONE;
            }

            // HW: supported only TU = {1,4,7}. Mapping: 2->1; 3->4; 5->4; 6->7
            if ((!(tc.set_par[0].v == 1 || tc.set_par[0].v == 4 || tc.set_par[0].v == 7)) && (tc.type == TARGET_USAGE))
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (tc.type != RATE_CONTROL)
            m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_VBR;

        if (tc.type == PIC_STRUCT && (tc.set_par->v == MFX_PICSTRUCT_FIELD_TFF || tc.set_par->v == MFX_PICSTRUCT_FIELD_BFF))
            m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;

        if (tc.type == IN_PAR_NULL)
            m_pPar = NULL;

        if (tc.type == OUT_PAR_NULL)
            m_pParOut = NULL;

        if (tc.type == ALIGNMENT_HW)
        {
            CreateBuffer(MFX_EXTBUFF_HEVC_PARAM, &buff_in, &buff_out);
            if (buff_in && buff_out)
                *buff_out = *buff_in;
            ((mfxExtHEVCParam*)buff_in)->PicWidthInLumaSamples = m_par.mfx.FrameInfo.Width;
            ((mfxExtHEVCParam*)buff_in)->PicHeightInLumaSamples = m_par.mfx.FrameInfo.Height;
            m_parInExtParams.push_back(buff_in);
            m_parOutExtParams.push_back(buff_out);
            m_pPar->NumExtParam = m_parInExtParams.size();
            m_pParOut->NumExtParam = m_parOutExtParams.size();
            m_pPar->ExtParam = &m_parInExtParams[0];
            m_pParOut->ExtParam = &m_parOutExtParams[0];
        }

        g_tsStatus.expect(sts);
        g_tsStatus.disable_next_check();
        //call tested function
        if (tc.type != SESSION_NULL)
            sts = Query(m_session, m_pPar, m_pParOut);
        else
            sts = Query(NULL, m_pPar, m_pParOut);

        CheckOutPar(tc);
        if (buff_in)
            delete buff_in;
        if (buff_out)
            delete buff_out;
        g_tsStatus.check(sts);

        if ((tc.type == SET_ALLOCK) && (tc.sub_type == AFTER))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();

            if (!isSw && !m_is_handle_set)
            {
                m_pFrameAllocator->get_hdl(type, hdl);

                if (IsHevcHwPlugin())
                    g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); // device was created by Core in Query() to get HW caps

                SetHandle(m_session, type, hdl);
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_hevce_8b_420_nv12_query, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_hevce_8b_444_rgb4_query, RunTest_Subtype<MFX_FOURCC_RGB4>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_hevce_10b_420_p010_query, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
}
