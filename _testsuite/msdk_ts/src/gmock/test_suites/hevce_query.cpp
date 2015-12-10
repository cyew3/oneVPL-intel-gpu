#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_query
{
class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR,
        TARGET_USAGE,
        EXT_BUFF,
        RATE_CONTROL,
        SESSION_NULL,
        SET_ALLOCK,
        AFTER,
        IN_PAR_NULL,
        OUT_PAR_NULL,
        RESOLUTION,
        NOT_ALLIGNED,
        ZEROED,
        CROP,
        XY,
        WH,
        XW,
        YH,
        W,
        H,
        IO_PATTERN,
        FRAME_RATE,
        PIC_STRUCT,
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
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

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
                    // GACC: supported only TU = {4,7}. Mapping: {1,2,3,5}->4; 6->7
                    if (tc.set_par[0].v == 4 || tc.set_par[0].v == 7)
                    {
                        tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
                    }
                    else
                    {
                        if (tc.set_par[0].v == 6)
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f) + 1);
                        else
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

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //Target Usage
    {/*00*/ MFX_ERR_NONE, TARGET_USAGE, NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1}},
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
    {/*21*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
    {/*22*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
    {/*23*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
    {/*24*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
    {/*25*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
    {/*26*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
    {/*27*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
    //session
    {/*28*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },
    //set allock handle
    {/*29*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
    {/*30*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
    {/*31*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
    {/*32*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
    //zero param
    {/*33*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, {} },
    {/*34*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, {} },
    //IOPattern
    {/*35*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
    {/*36*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
    {/*37*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
    {/*38*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },
    //FourCC
    {/*39*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12 } },
    {/*40*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 } },
    //Resolution
    {/*41*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192 + 32 },
                                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320 + 32 }
                                                   },
    },
    {/*42*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NOT_ALLIGNED, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 + 1 } },
    {/*43*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NOT_ALLIGNED, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 736 + 1 } },
    {/*44*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 16384 + 16 } },
    {/*45*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 16384 + 16 } },
    {/*46*/ MFX_ERR_NONE, RESOLUTION, ZEROED, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                   }
    },
    //PicStruct
    {/*47*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*48*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*49*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
    {/*50*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
    //Crops
    {/*51*/ MFX_ERR_UNSUPPORTED, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 10 } },
    {/*52*/ MFX_ERR_UNSUPPORTED, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 10 } },
    {/*53*/ MFX_ERR_UNSUPPORTED, CROP, W, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 } },
    {/*54*/ MFX_ERR_UNSUPPORTED, CROP, H, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 } },
    {/*55*/ MFX_ERR_UNSUPPORTED, CROP, WH, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 }
                                             }
    },
    {/*56*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, XW, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
                                                          }
    },
    {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, YH, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
                                                          }
    },
    {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, W, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
                                                          }
    },
    {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, H, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
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
    {/*68*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 1 } },


};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxHDL hdl;
    mfxHandleType type;
    mfxExtBuffer* buff_in = NULL;
    mfxExtBuffer* buff_out = NULL;
    mfxStatus sts;

    // set defoult parameters
    m_pParOut = new mfxVideoParam;
    *m_pParOut = m_par;

    MFXInit();

    Load();

    SETPARS(m_pPar, MFX_PAR);

    if (!GetAllocator())
    {
        UseDefaultAllocator(
            (m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            );
    }

    if ((tc.type == SET_ALLOCK) && (tc.sub_type != AFTER))
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
        if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
            if (buff_in)
                delete buff_in;
            if (buff_out)
                delete buff_out;
            g_tsStatus.check(sts);
            return 0;
        }
        if ((m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_AVBR) && (tc.sts == MFX_ERR_NONE))
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
        if ((tc.type == PIC_STRUCT)/* && (tc.sts == MFX_ERR_UNSUPPORTED)*/)
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
        if ((tc.type == FRAME_RATE) && (tc.sts == MFX_ERR_UNSUPPORTED))
        {
            g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }
        if ((tc.type == IO_PATTERN) && (tc.sts == MFX_ERR_UNSUPPORTED))
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
        if (tc.type == CROP)
        {
            if (tc.sts == MFX_ERR_UNSUPPORTED)
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
                if ((tc.sub_type == WH) || (tc.sub_type == W) || (tc.sub_type == H))
                {
                    g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
                }
            }
            else if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
            }
        }
        if ((tc.type == RESOLUTION) && (tc.sub_type == NOT_ALLIGNED)) //HEVCE_HW need aligned width and height for 32
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
        else if ((tc.type == RESOLUTION) && (tc.sub_type == ZEROED))
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
        }
        else
        {
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
        }

        // HW: supported only TU = {1,4,7}. Mapping: 2->1; 3->4; 5->4; 6->7
        if ((!(tc.set_par[0].v == 1 || tc.set_par[0].v == 4 || tc.set_par[0].v == 7)) && (tc.type == TARGET_USAGE))
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
    }
    else if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data)))
    {
        // GACC: supported only TU = {4,7}. Mapping: {1,2,3,5}->4; 6->7
        if ((!(tc.set_par[0].v == 4 || tc.set_par[0].v == 7)) && (tc.type == TARGET_USAGE))
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
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
        m_pVAHandle = m_pFrameAllocator;
        m_pVAHandle->get_hdl(type, hdl);
        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
            g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        SetHandle(m_session, type, hdl);
        m_is_handle_set = (g_tsStatus.get() >= 0);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_query);
}