/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2018 Intel Corporation. All Rights Reserved.

File Name: hevce_query.cpp

\* ****************************************************************************** */


#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_query
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
        NONE
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
            mfxI32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC){}
        ~TestSuite() { }

        static const unsigned int n_cases;

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:

        static const tc_struct test_case[];

        void CheckOutPar(tc_struct tc)
        {
            switch (tc.type)
            {
                case TARGET_USAGE:
                {
                    if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
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
                    else if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data)))
                    {
                        // GACC: supported only TU = {4,5,6,7}
                        if (tc.set_par[0].v >= 4)
                        {
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
                        }
                        else
                        {
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, 4);
                        }
                    }
                    else // SW: supported TU = {1,2,3,4,5,6,7}
                    {
                        tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));

                    }
                    break;
                }
                default: break;
            }
        }

        void CreateBuffer(mfxU32 buff_id, mfxExtBuffer** buff, mfxExtBuffer** buff_out)
        {
                switch (buff_id)
                {
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
                case MFX_EXTBUFF_AVC_REFLIST_CTRL:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtAVCRefListCtrl());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtAVCRefListCtrl());
                        memset((*buff), 0, sizeof(mfxExtAVCRefListCtrl));
                        (*buff)->BufferId = MFX_EXTBUFF_AVC_REFLIST_CTRL;
                        (*buff)->BufferSz = sizeof(mfxExtAVCRefListCtrl);
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
                case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtEncoderResetOption());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtEncoderResetOption());
                        memset((*buff), 0, sizeof(mfxExtEncoderResetOption));
                        (*buff)->BufferId = MFX_EXTBUFF_ENCODER_RESET_OPTION;
                        (*buff)->BufferSz = sizeof(mfxExtEncoderResetOption);
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
        //Target Usage
        {/*00*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1}},
        {/*01*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 2 } },
        {/*02*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 3 } },
        {/*03*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 } },
        {/*04*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 5 } },
        {/*05*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 6 } },
        {/*06*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 } },
        //Ext Buffers
        {/*07*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, {} },
        {/*08*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, {} },
        {/*09*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, {} },
        {/*10*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, {} },
        {/*11*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, {} },
        {/*12*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, {} },
        {/*13*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, {} },
        {/*14*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, {} },
        {/*15*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, {} },
        {/*16*/ MFX_ERR_NULL_PTR, EXT_BUFF, NONE, {} },
        //Rate Control Metod
        {/*17*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*18*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*19*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*20*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*21*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*22*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*23*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*24*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*25*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*26*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*27*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        {/*28*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
        //session
        {/*29*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },
        //set alloc handle
        {/*30*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
        {/*31*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
        {/*32*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
        {/*33*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
        //zero param
        {/*34*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, {} },
        {/*35*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, {} },
        //IOPattern
        {/*36*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
        {/*37*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
        {/*38*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
        {/*39*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },
        //Unsupported FourCC
        {/*40*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0 },
                                                 }
        },
        //Resolution
        {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3 } },
        {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3 } },
        {/*43*/ MFX_ERR_UNSUPPORTED, RESOLUTION, W_GT_MAX, {} },
        {/*44*/ MFX_ERR_UNSUPPORTED, RESOLUTION, H_GT_MAX, {} },
        {/*45*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                       }
        },
        //PicStruct
        {/*46*/ MFX_ERR_NONE, PIC_STRUCT, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
        {/*47*/ MFX_ERR_NONE, PIC_STRUCT, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
        {/*48*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } },
        {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
        {/*50*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
        //Crops
        {/*51*/ MFX_ERR_UNSUPPORTED, CROP, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
        {/*52*/ MFX_ERR_UNSUPPORTED, CROP, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
        {/*53*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 } },
        {/*54*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 } },
        {/*55*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 },
                                                    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 }
                                                  }
        },
        {/*56*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
                                                               }
        },
        {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
                                                               }
        },
        {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                               }
        },
        {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                                                               }
        },
        //frame rate
        {/*60*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
        {/*61*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },
        //chroma format
        {/*62*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 255 } },
        //num thread
        {/*63*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 1 } },
        {/*64*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 4 } },
        //gop pic size
        {/*65*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 } },
        //gop ref dist
        {/*66*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 } },
        //got opt flag
        {/*67*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 2 } },
        //protected
        {/*68*/
#if (defined(LINUX32) || defined(LINUX64))
            MFX_ERR_UNSUPPORTED,
#else
            MFX_ERR_NONE,
#endif
                          PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP } },
        {/*69*/
#if (defined(LINUX32) || defined(LINUX64))
            MFX_ERR_UNSUPPORTED,
#else
            MFX_ERR_NONE,
#endif
                          PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP } },
        {/*70*/ MFX_ERR_UNSUPPORTED, PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0xfff } },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(tc_struct);

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
        mfxStatus sts;
        ENCODE_CAPS_HEVC caps = {0};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

        if ((0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) ||
            (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data))))
        {
            if ((g_tsHWtype < MFX_HW_SKL) &&
                (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
                g_tsStatus.check(sts);
                return 0;
            }

            if ((g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
                && (fourcc_id == MFX_FOURCC_YUY2 || fourcc_id == MFX_FOURCC_Y210 || fourcc_id == GMOCK_FOURCC_Y212))
            {
                g_tsLog << "\n\nWARNING: 4:2:2 formats are supported in HEVCe DualPipe only!\n\n\n";
                throw tsSKIP;
            }

            if ((g_tsConfig.lowpower == MFX_CODINGOPTION_OFF)
                && (fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are supported in HEVCe VDENC only!\n\n\n";
                throw tsSKIP;
            }
        }

        // set default parameters
        m_pParOut = new mfxVideoParam;
        *m_pParOut = m_par;

        MFXInit();
        Load();

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
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
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
        else if (fourcc_id == GMOCK_FOURCC_P012)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y212)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y412)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

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

        g_tsStatus.check(GetCaps(&caps, &capSize));

        if (tc.sub_type != DELTA) {  // not dependent from resolution params
            SETPARS(m_pPar, MFX_PAR);

            if (tc.type == RESOLUTION)
            {
                if (tc.sub_type == W_GT_MAX) {
                    if (caps.MaxPicWidth) {
                        m_pPar->mfx.FrameInfo.Width = caps.MaxPicWidth + 32;
                    }
                    else {
                        m_pPar->mfx.FrameInfo.Width = INVALID_WIDTH + 16;
                    }
                }
                if (tc.sub_type == H_GT_MAX) {
                    if (caps.MaxPicWidth) {
                        m_pPar->mfx.FrameInfo.Height = caps.MaxPicHeight + 32;
                    }
                    else {
                        m_pPar->mfx.FrameInfo.Height = INVALID_HEIGHT + 16;
                    }
                }
            }

        } else
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
            CreateBuffer(tc.sub_type, &buff_in, &buff_out);
            if (buff_in && buff_out)
                *buff_out = *buff_in;
            m_pPar->NumExtParam = 1;
            m_pParOut->NumExtParam = 1;
            m_pPar->ExtParam = &buff_in;
            m_pParOut->ExtParam = &buff_out;
        }

        g_tsStatus.expect(tc.sts);

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if ((m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_AVBR) && (tc.sts == MFX_ERR_NONE))
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            }

            if ((tc.type == IO_PATTERN) && (tc.sts == MFX_ERR_UNSUPPORTED))
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            }

            if (tc.type == EXT_BUFF)
            {
                if ((tc.sub_type == MFX_EXTBUFF_AVC_REFLIST_CTRL) ||
                    (tc.sub_type == MFX_EXTBUFF_ENCODER_RESET_OPTION) ||
                    (tc.sub_type == MFX_EXTBUFF_VIDEO_SIGNAL_INFO))
                    g_tsStatus.expect(MFX_ERR_NONE);
                else if (tc.sub_type == NONE)
                    g_tsStatus.expect(MFX_ERR_NULL_PTR);

            }
            if (tc.type == CROP)
            {
                if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                {
                    g_tsStatus.expect(MFX_ERR_NONE);
                }
            }

            // HW: supported only TU = {1,4,7}. Mapping: 2->1; 3->4; 5->4; 6->7
            if ((!(tc.set_par[0].v == 1 || tc.set_par[0].v == 4 || tc.set_par[0].v == 7)) && (tc.type == TARGET_USAGE))
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            }
        }
        else if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data)))
        {
            if (tc.type == PROTECTED)
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            // GACC: supported only TU = {4,5,6,7}
            if ((tc.set_par[0].v < 4) && (tc.type == TARGET_USAGE))
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            }
            // different expected status for SW HEVCe and GACC
            if (tc.type == PIC_STRUCT && tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
        }
        else
        {
            if (tc.type == PROTECTED)
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            // different expected status for SW HEVCe and GACC
            if (tc.type == PIC_STRUCT && tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
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

        if (buff_in)
            delete buff_in;
        if (buff_out)
            delete buff_out;

        g_tsStatus.check(sts);
        TS_TRACE(m_pParOut);
        CheckOutPar(tc);

        if ((tc.type == SET_ALLOCK) && (tc.sub_type == AFTER))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();

            if (!m_is_handle_set)
            {
                m_pFrameAllocator->get_hdl(type, hdl);

                if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
                    g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); // device was created by Core in Query() to get HW caps
                SetHandle(m_session, type, hdl);
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_query, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_query, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_query, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_query, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_query, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_query, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_query, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_query, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_query, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
}
