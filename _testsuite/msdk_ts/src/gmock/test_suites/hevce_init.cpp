/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: hevce_init.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_init
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
            MFX_EXT_HEVCPARAM,
            MFX_EXT_HEVCREGION,
            EXT_BUFF,
            RATE_CONTROL,
            SESSION_NULL,
            PAR_NULL,
            _2_CALL,
            _2_CALL_CLOSE,
            NOT_LOAD_PLUGIN,
            CALL_QUERY,
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
            BIG,
            IO_PATTERN,
            FRAME_RATE,
            PIC_STRUCT,
            SLICE,
            FOURCC,
            CHROMA_FORMAT,
            INVALID,
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
            case MFX_EXTBUFF_HEVC_PARAM:
            {
                                           (*buff) = (mfxExtBuffer*)(new mfxExtHEVCParam());
                                           (*buff_out) = (mfxExtBuffer*)(new mfxExtHEVCParam());
                                           memset((*buff), 0, sizeof(mfxExtHEVCParam));
                                           (*buff)->BufferId = MFX_EXTBUFF_HEVC_PARAM;
                                           (*buff)->BufferSz = sizeof(mfxExtHEVCParam);
                                           printf("buffer created\n");
                                           break;
            }
            case MFX_EXTBUFF_HEVC_REGION:
            {
                                            (*buff) = (mfxExtBuffer*)(new mfxExtHEVCRegion());
                                            (*buff_out) = (mfxExtBuffer*)(new mfxExtHEVCRegion());
                                            memset((*buff), 0, sizeof(mfxExtHEVCRegion));
                                            (*buff)->BufferId = MFX_EXTBUFF_HEVC_REGION;
                                            (*buff)->BufferSz = sizeof(mfxExtHEVCRegion);
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
        //FourCC
        {/*00*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
            }
        },
        {/*01*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
            }
        },
        {/*02*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }
        },
        {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }
        },
        //null session
        {/*04*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },
        //null par
        {/*05*/ MFX_ERR_NULL_PTR, PAR_NULL, NONE, {} },
        //2 calls
        {/*06*/ MFX_ERR_UNDEFINED_BEHAVIOR, _2_CALL, NONE, {} },
        {/*07*/ MFX_ERR_UNDEFINED_BEHAVIOR, _2_CALL, NONE, {} },
        {/*08*/ MFX_ERR_NONE, _2_CALL_CLOSE, NONE, {} },
        {/*09*/ MFX_ERR_NONE, _2_CALL_CLOSE, NONE, {} },
        //PicStruct
        {/*10*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
        {/*11*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
        {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
        {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
        //Resolution
        {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192 + 32 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320 + 32 }
            }
        },
        {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, ZEROED,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
            }
        },
        {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NOT_ALLIGNED, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 + 1 } },
        {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NOT_ALLIGNED, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 736 + 1 } },
        {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 16384 + 16 } },
        {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 16384 + 16 } },
        //Crops
        {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
        {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
        {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, W, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 } },
        {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, H, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 } },
        {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, WH,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 }
            }
        },
        {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, XW,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
            }
        },
        {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, YH,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },
        {/*27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, W,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
            }
        },
        {/*28*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, H,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
            }
        },
        //chroma format
        {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, CHROMA_FORMAT, INVALID, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_RESERVED1 } },
        //num slice
        {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 65535 } },
        //frame rate
        {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
        {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },
        {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, BIG,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 310 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 }
            }
        },
        //Ext Buffers
        {/*34*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, {} },
        {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, {} },
        {/*36*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, {} },
        {/*37*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, {} },
        {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, {} },
        {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, {} },
        {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, {} },
        {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, {} },
        {/*42*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, {} },
        {/*43*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_AVC_REFLIST_CTRL, {} },
        {/*44*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_ENCODER_RESET_OPTION, {} },
        {/*45*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, NONE, {} },
        //Rate Control Metod
        {/*46*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*47*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*48*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*49*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*50*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*51*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*52*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*53*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*54*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*55*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        {/*56*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
        //encoded order
        {/*57*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1 } },
        //native
        {/*58*/ MFX_ERR_INVALID_VIDEO_PARAM, NOT_LOAD_PLUGIN, NONE, {} },
        //call query
        {/*59*/ MFX_ERR_NONE, CALL_QUERY, NONE, {} },
        //API mfxExtHEVCParam, for HW plugin only
        //in current version PicWidthInLumaSamples/PicHeightInLumaSamples should be aligned on 16 due to hw limitation
        //after this limitation removed, expected status for case #60/#61 would be updated
        {/*60*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_HEVC_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicWidthInLumaSamples, 1920 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicHeightInLumaSamples, 1080 },
            }
        },
        {/*61*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_HEVC_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicWidthInLumaSamples, 1920 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicHeightInLumaSamples, 1088 },
            }
        },
        {/*62*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_HEVC_PARAM,
            {
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicWidthInLumaSamples, 0 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicHeightInLumaSamples, 0 },
            }
        },
        {/*63*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_HEVC_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicWidthInLumaSamples, 1920 + 10 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicHeightInLumaSamples, 1088 + 10 },
            }
        },
        {/*64*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_HEVC_PARAM,
            {
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicWidthInLumaSamples, 32 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicHeightInLumaSamples, 32 },
            }
        },
        {/*65*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_HEVC_PARAM,
            {
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicWidthInLumaSamples, 8 },
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.PicHeightInLumaSamples, 8 },
            }
        },
        //GeneralConstraintFlags is not supported for HEVCE hw plugin upto Gen11
        {/*66*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_HEVC_PARAM,
            {
                { MFX_EXT_HEVCPARAM, &tsStruct::mfxExtHEVCParam.GeneralConstraintFlags, MFX_HEVC_CONSTR_REXT_MAX_12BIT }
            }
        },
        {/*67*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_HEVC_REGION,
            {
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionId, 0},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionType, MFX_HEVC_REGION_SLICE},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionEncoding, MFX_HEVC_REGION_ENCODING_ON}
            }
        },
        {/*68*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_HEVC_REGION,
            {
                {MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2}, //multi slices
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionId, 1},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionType, MFX_HEVC_REGION_SLICE},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionEncoding, MFX_HEVC_REGION_ENCODING_ON}
            }
        },
        {/*69*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_HEVC_REGION,
            {
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionId, 0},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionType, 1}, //unsupported type
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionEncoding, MFX_HEVC_REGION_ENCODING_ON}
            }
        },
        {/*70*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_HEVC_REGION,
            {
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionId, 1}, //>= slice_num
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionType, MFX_HEVC_REGION_SLICE},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionEncoding, MFX_HEVC_REGION_ENCODING_ON}
            }
        },
        {/*71*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_HEVC_REGION,
            {
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionId, 0},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionType, MFX_HEVC_REGION_SLICE},
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionEncoding, 2} //invalid parameter
            }
        },
        {/*72*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_HEVC_REGION,
            {
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionId, 1}, //>= slice_num
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionType, 1}, //unsupported type
                {MFX_EXT_HEVCREGION, &tsStruct::mfxExtHEVCRegion.RegionEncoding, MFX_HEVC_REGION_ENCODING_OFF}
            }
        },
        //FourCC for RExt
        {/*73*/ MFX_ERR_NONE, FOURCC, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P010 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420 }
            }
        },
        {/*74*/ MFX_ERR_NONE, FOURCC, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y210 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422 }
            }
        },
        {/*75*/ MFX_ERR_INVALID_VIDEO_PARAM, FOURCC, INVALID,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y210 }
            }
        },
        {/*76*/ MFX_ERR_NONE, FOURCC, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422 }
            }
        },
        {/*77*/ MFX_ERR_INVALID_VIDEO_PARAM, FOURCC, INVALID,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2 }
            }
        },
        {/*78*/ MFX_ERR_NONE, FOURCC, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y410 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444 }
            }
        },
        {/*79*/ MFX_ERR_INVALID_VIDEO_PARAM, FOURCC, INVALID,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y410 }
            }
        },

        {/*80*/ MFX_ERR_NONE, FOURCC, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_AYUV },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444 }
            }
        },
        {/*81*/ MFX_ERR_INVALID_VIDEO_PARAM, FOURCC, INVALID,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_AYUV },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 0 }
            }
        },
        {/*82*/ MFX_ERR_NONE, FOURCC, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_A2RGB10 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444 }
            }
        },
        {/*83*/ MFX_ERR_INVALID_VIDEO_PARAM, FOURCC, INVALID,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_A2RGB10 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 0 }
            }
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];


        mfxHDL hdl;
        mfxHandleType type;
        mfxExtBuffer* buff_in = NULL;
        mfxExtBuffer* buff_out = NULL;
        mfxStatus sts;

        MFXInit();

        if (tc.type != NOT_LOAD_PLUGIN)
            Load();
        else
            m_loaded = true;

        SETPARS(m_pPar, MFX_PAR);

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
            CreateBuffer(tc.sub_type, &buff_in, &buff_out);
            if (buff_in && buff_out)
                *buff_out = *buff_in;
            m_pPar->NumExtParam = 1;
            m_pParOut->NumExtParam = 1;
            m_pPar->ExtParam = &buff_in;
            m_pParOut->ExtParam = &buff_out;
            //Only set the values for hevce hw plugin
            if (tc.sub_type == MFX_EXTBUFF_HEVC_PARAM &&
                0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) {
                SETPARS((mfxExtHEVCParam *)buff_in, MFX_EXT_HEVCPARAM);
                SETPARS((mfxExtHEVCParam *)buff_out, MFX_EXT_HEVCPARAM);
           }

            if (tc.sub_type == MFX_EXTBUFF_HEVC_REGION) {
                SETPARS((mfxExtHEVCRegion *)buff_in, MFX_EXT_HEVCREGION);
                SETPARS((mfxExtHEVCRegion *)buff_out, MFX_EXT_HEVCREGION);
            }
        }

        sts = tc.sts;

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

            if ((tc.type == FOURCC) && (tc.sub_type == NONE))
            {
                if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_P010) && (g_tsHWtype < MFX_HW_KBL)) {
                    sts = MFX_ERR_INVALID_VIDEO_PARAM;
                } else
                if (((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210) || (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2)) &&
                    (g_tsHWtype >= MFX_HW_ICL) &&
                    (g_tsConfig.lowpower == MFX_CODINGOPTION_ON)) {
                        g_tsLog << "\n\nWARNING: 422 format is not supported in VDENC\n\n\n";
                        throw tsSKIP;
                } else
                if (g_tsHWtype < MFX_HW_ICL)
                    sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }

            if (tc.type == PIC_STRUCT)
            {
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            if ((tc.type == SLICE) && (tc.sts != MFX_ERR_NONE))
            {
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            //if ((tc.type == FRAME_RATE) && (tc.sts < MFX_ERR_NONE))
            //{
            //    if ((m_pPar->mfx.FrameInfo.FrameRateExtD == 0) || (m_pPar->mfx.FrameInfo.FrameRateExtN == 0))
            //        sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            //    else
            //        sts = MFX_ERR_INVALID_VIDEO_PARAM;
            //}

            if (tc.type == EXT_BUFF)
            {
                if ((tc.sub_type == MFX_EXTBUFF_AVC_REFLIST_CTRL) ||
                    (tc.sub_type == MFX_EXTBUFF_ENCODER_RESET_OPTION) ||
                    (tc.sub_type == MFX_EXTBUFF_VIDEO_SIGNAL_INFO))
                    sts = MFX_ERR_NONE;
                else if (tc.sub_type == MFX_EXTBUFF_HEVC_PARAM)
                    if ((((mfxExtHEVCParam *)(m_pPar->ExtParam[0]))->GeneralConstraintFlags != 0) &&  (g_tsHWtype >= MFX_HW_ICL))
                        sts = MFX_ERR_NONE;
                    else
                        sts = tc.sts;
                else if (tc.sub_type == MFX_EXTBUFF_HEVC_REGION)
                    sts = MFX_ERR_INVALID_VIDEO_PARAM;
                else if (tc.sub_type == NONE)
                    sts = MFX_ERR_NULL_PTR;
                else
                    if (tc.sts != MFX_ERR_NONE)
                        sts = MFX_ERR_INVALID_VIDEO_PARAM;

                // encoder aligns PicWidthInLumaSamples and PicHeightInLumaSamples to next multiple of 16 (if necessary),
                //   but libva returns VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED for resolutions < 32x32
                //   so on Linux we expect MFX_ERR_UNSUPPORTED
                if (g_tsOSFamily == MFX_OS_FAMILY_LINUX && tc.sub_type == MFX_EXTBUFF_HEVC_PARAM)
                {
                    mfxU16 W = ((mfxExtHEVCParam *)(m_pPar->ExtParam[0]))->PicWidthInLumaSamples;
                    mfxU16 H = ((mfxExtHEVCParam *)(m_pPar->ExtParam[0]))->PicHeightInLumaSamples;
                    if (W > 0 && W <= 16)
                        sts = MFX_ERR_UNSUPPORTED;
                    if (H > 0 && H <= 16)
                        sts = MFX_ERR_UNSUPPORTED;
                }
            }

            if (tc.type == RATE_CONTROL)
            {
                if ((m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_AVBR) && (tc.sts == MFX_ERR_NONE))
                    sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                if (tc.sts < MFX_ERR_NONE)
                    sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }

            if (tc.type == CROP)
            {
                if (tc.sts < MFX_ERR_NONE)
                {
                    sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                    if ((tc.sub_type == XY) || (tc.sub_type == WH) || (tc.sub_type == W) || (tc.sub_type == H))
                    {
                        sts = MFX_ERR_INVALID_VIDEO_PARAM;
                    }
                }
                else
                {
                    sts = MFX_ERR_NONE;
                }
            }
            if ((tc.type == RESOLUTION) && (tc.sub_type == NOT_ALLIGNED)) //HEVCE_HW need aligned width and height for 32
            {
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }
            else if ((tc.type == RESOLUTION) && (tc.sub_type == ZEROED))
            {
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }
            else
            {
                m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
                m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
            }

        } else {
            //for non hevce hw plugin, return ERR_NONE as no parameters set in mfxExtHEVCParam
            if (tc.type == EXT_BUFF)
            {
                if (tc.sub_type == MFX_EXTBUFF_HEVC_PARAM)
                    sts = MFX_ERR_NONE;
            }
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
        if (tc.type != SESSION_NULL)
            sts = MFXVideoENCODE_Init(m_session, m_pPar);
        else
            sts = MFXVideoENCODE_Init(NULL, m_pPar);

        if (buff_in)
            delete buff_in;
        if (buff_out)
            delete buff_out;

        g_tsStatus.check(sts);
        if ((orig_par) && (tc.sts == MFX_ERR_NONE))
        {
            if (g_tsConfig.lowpower != MFX_CODINGOPTION_UNKNOWN)
            {
                EXPECT_EQ(g_tsConfig.lowpower, m_pPar->mfx.LowPower)
                    << "ERROR: external configuration of LowPower doesn't equal to real value\n";
            }
            EXPECT_EQ(0, memcmp(orig_par, m_pPar, sizeof(mfxVideoParam)))
                << "ERROR: Input parameters must not be changed in Init()";
            delete orig_par;
        }

        if ((sts < MFX_ERR_NONE) && (tc.type != _2_CALL) && (tc.type != _2_CALL_CLOSE))
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        else
            g_tsStatus.expect(MFX_ERR_NONE);
        Close();


        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_init);
}
