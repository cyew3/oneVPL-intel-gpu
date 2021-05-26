/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2021 Intel Corporation. All Rights Reserved.

File Name: avce_init.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

#define INVALID_WIDTH   16384
#define INVALID_HEIGHT  16384
#define QP 26
#define ICQ_QUALITY 51

namespace avce_init
{
    inline bool IsOn(mfxU32 coding_option)
    {
        return coding_option == MFX_CODINGOPTION_ON;
    }

    class TestSuite : tsVideoEncoder
    {
    private:

        enum
        {
            MFX_PAR = 0,
            SESSION_NULL,
            PAR_NULL,
            _2_CALL,
            _2_CALL_CLOSE,
            CALL_QUERY,
            RESOLUTION,
            RESOLUTION_W,
            RESOLUTION_H,
            NOT_ALLIGNED,
            ZEROED,
            CROP,
            XY,
            WH,
            XW,
            YH,
            W,
            H,
            INVALID_MAX,
            BIG,
            IO_PATTERN,
            FRAME_RATE,
            SLICE,
            CHROMA_FORMAT,
            NONE,
            BUFFER_SIZE,
            BUFFER_SIZE_DEFAULT,
            INITIAL_DELAY_DEFAULT,
            DEFAULTS,
            EXT_CO2,
            EXT_CO3,
            Compatibility = 0x100,
            RATE_CONTROL = 0x200,
            PIC_STRUCT = 0x400,
            EXT_BUFF = 0x800
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

#define ALLOCATE_EXTBUFF(buff_id, buff_class)   \
{ \
                        (*buff) = &(new buff_class())->Header; \
                        memset((*buff), 0, sizeof(buff_class)); \
                        (*buff)->BufferId =  buff_id; \
                        (*buff)->BufferSz =  sizeof(buff_class); \
}

        void CreateBuffer(mfxU32 buff_id, mfxExtBuffer** buff)
        {
            switch (buff_id)
            {
            case MFX_EXTBUFF_CODING_OPTION_SPSPPS:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOptionSPSPPS);
                break;

            case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVideoSignalInfo);
                break;

            case MFX_EXTBUFF_CODING_OPTION:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOption);
                break;

            case MFX_EXTBUFF_CODING_OPTION2:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOption2);
                break;

            case MFX_EXTBUFF_CODING_OPTION3:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOption3);
                break;

            case MFX_EXTBUFF_VPP_DOUSE:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVPPDoUse);
                break;

            case MFX_EXTBUFF_VPP_AUXDATA:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVppAuxData);
                break;

            case MFX_EXTBUFF_AVC_REFLIST_CTRL:
                ALLOCATE_EXTBUFF(buff_id, mfxExtAVCRefListCtrl);
                break;

            case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVPPFrameRateConversion);
                break;

            case MFX_EXTBUFF_PICTURE_TIMING_SEI:
                ALLOCATE_EXTBUFF(buff_id, mfxExtPictureTimingSEI);
                break;

            case MFX_EXTBUFF_ENCODER_CAPABILITY:
                ALLOCATE_EXTBUFF(buff_id, mfxExtEncoderCapability);
                break;

            case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                ALLOCATE_EXTBUFF(buff_id, mfxExtEncoderResetOption);
                break;

            case NONE:
            {
                *buff = nullptr;
                break;
            }

            default: break;
            }
        }

    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() { }
        template<mfxU32 fourcc>
        int RunTest_Subtype(unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);
        static const unsigned int n_cases;

        mfxStatus Init(mfxSession session)
        {
            mfxVideoParam orig_par;

            memcpy(&orig_par, m_pPar, sizeof(mfxVideoParam));

            TRACE_FUNC2(MFXVideoENCODE_Init, session, m_pPar);

            g_tsStatus.check(MFXVideoENCODE_Init(session, m_pPar));

            m_initialized = (g_tsStatus.get() >= 0);

            if (m_bCheckLowPowerAtInit && g_tsConfig.lowpower != MFX_CODINGOPTION_UNKNOWN)
            {
                EXPECT_EQ(g_tsConfig.lowpower, m_pPar->mfx.LowPower)
                    << "ERROR: external configuration of LowPower doesn't equal to real value\n";
            }

            EXPECT_EQ(0, memcmp(&orig_par, m_pPar, sizeof(mfxVideoParam)))
                << "ERROR: Input parameters must not be changed in Init()";

            return g_tsStatus.get();
        }

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
               // //FourCC for RExt
               // {/*00*/ MFX_ERR_NONE, NONE, NONE},
               // //FourCC
               // {/*01*/ MFX_ERR_NONE, NONE, NONE,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
               //     }
               // },
               // {/*02*/ MFX_ERR_NONE, NONE, NONE,
               //     {
               //        { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
               //        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 1 } // For the video memory shifts surface
               //     }
               // },
               // //null session
               // {/*03*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },
               // //null par
               // {/*04*/ MFX_ERR_NULL_PTR, PAR_NULL, NONE, {} },
               // //2 calls
               // {/*05*/ MFX_ERR_UNDEFINED_BEHAVIOR, _2_CALL, NONE, {} },
               // {/*06*/ MFX_ERR_UNDEFINED_BEHAVIOR, _2_CALL, NONE, {} },
               // {/*07*/ MFX_ERR_NONE, _2_CALL_CLOSE, NONE, {} },
               // {/*08*/ MFX_ERR_NONE, _2_CALL_CLOSE, NONE, {} },
               // //PicStruct
          {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_UNKNOWN } },
          {/*01*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
          {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
          {/*03*/ MFX_ERR_NONE, PIC_STRUCT | Compatibility, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } },
          {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
               //  //Resolution
               //  {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, INVALID_MAX },
               //  {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION_W, INVALID_MAX },
               //  {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION_H, INVALID_MAX },
               //  {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, ZEROED,
               //      {
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
               //      }
               //  },
               //  {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION_W, NOT_ALLIGNED, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 + 1 } },
               //  {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION_H, NOT_ALLIGNED, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 736 + 1 } },
               //  //Crops
               //  {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
               //  {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
               //  {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, W, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 } },
               //  {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, H, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 } },
               //  {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, CROP, WH,
               //      {
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 }
               //      }
               //  },
               //  {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, XW,
               //      {
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
               //      }
               //  },
               //  {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, YH,
               //      {
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
               //      }
               //  },
               //  {/*27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, W,
               //      {
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
               //      }
               //  },
               //  {/*28*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, H,
               //      {
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
               //      }
               //  },
               //  //chroma format
               //  {/*29*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHROMA_FORMAT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_RESERVED1 } },
               //  //num slice
               //  {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 65535 } },
               //  //frame rate
               //  {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
               //  {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },
               //  {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, FRAME_RATE, BIG,
               //      {
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 310 },
               //          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 }
               //      }
               //  },
               //  //Ext Buffers
               //  {/*34*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, {} },
               //  {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, {} },
               //  {/*36*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, {} },
               //  {/*37*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, {} },
               //  {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, {} },
               //  {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, {} },
               //  {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, {} },
               //  {/*41*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, {} },
               //  {/*42*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, {} },
               //  {/*43*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_AVC_REFLIST_CTRL, {} },
               //  {/*44*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_ENCODER_RESET_OPTION, {} },
               //  {/*45*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, NONE, {} },
          {/*05*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_NO } },
          {/*06*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_VERTICAL } },
          {/*07*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_HORIZONTAL } },
          {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, EXT_BUFF | Compatibility, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_SLICE } },
          {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, 0x100 } },
               // {/*51*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION3, {} },
          {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF | Compatibility, MFX_EXTBUFF_CODING_OPTION3, { EXT_CO3, &tsStruct::mfxExtCodingOption3.FadeDetection, MFX_CODINGOPTION_ON } },
               // //Rate Control Method
               // {/*53*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
               //                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 } } },
               // {/*54*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
               //                                             { MFX_PAR,& tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 } } },
               // {/*55*/ MFX_ERR_NONE, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
          {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL | Compatibility, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
               // {/*57*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
               // {/*58*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
               // {/*59*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
               // {/*60*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
               // {/*61*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
               // {/*62*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
               // {/*63*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
               // //encoded order
               // {/*64*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1 } },
               // //call query
               // {/*65*/ MFX_ERR_NONE, CALL_QUERY, NONE, {} },
               // // BRC modes with different BufferSizeInKB and InitialDelayInKB values
               // {/*66*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
               //                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 } } },
               // {/*67*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
               //                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 } } },
               // {/*68*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP }},
               // {/*69*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
               // {/*70*/ MFX_ERR_UNSUPPORTED, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
               // {/*71*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
               // {/*72*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
               // {/*73*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
               // {/*74*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
               //                                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 } } },
               // {/*75*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
               //                                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 } } },
               // {/*76*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
               // {/*77*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
               // {/*78*/ MFX_ERR_UNSUPPORTED, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
               // {/*79*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
               // {/*80*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
               // {/*81*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
               // {/*82*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
               //                                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 } } },
               // {/*83*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
               //                                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 } } },
               // {/*84*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
               // {/*85*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
               // {/*86*/ MFX_ERR_UNSUPPORTED, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
               // {/*87*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
               // {/*88*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
               // {/*89*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
               // {/*90*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
               //                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 } } },
               // {/*91*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
               //                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 } } },
               // {/*92*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
               // {/*93*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
               // {/*94*/ MFX_ERR_UNSUPPORTED, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
               // {/*95*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
               // {/*96*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
               // {/*97*/ MFX_ERR_INVALID_VIDEO_PARAM, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
               // // Parallel BRC with LowPower ON
               // {/*98*/ MFX_ERR_NONE, EXT_BUFF | Compatibility, MFX_EXTBUFF_CODING_OPTION2,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 },
               //         { EXT_CO2, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_PYRAMID },
               //     }
               // },
               // {/*99*/ MFX_ERR_NONE, EXT_BUFF | Compatibility, MFX_EXTBUFF_CODING_OPTION2,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 },
               //         { EXT_CO2, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_PYRAMID },
               //     }
               // },
               // // ICQ Mode with LowPower ON
               // {/*100*/ MFX_ERR_NONE, Compatibility, NONE,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 22 },
               //     }
               // },
               // // QP range with LowPower ON
               // {/*101*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, Compatibility, NONE,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 6 },
               //     }
               // },
               // {/*102*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, Compatibility, NONE,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 6 },
               //     }
               // },
               // // NumActiveRef with LowPower ON
               // {/*103*/ MFX_ERR_NONE, EXT_BUFF | Compatibility, MFX_EXTBUFF_CODING_OPTION3,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_BEST_QUALITY  },
               //         { EXT_CO3, &tsStruct::mfxExtCodingOption3.NumRefActiveP[0], 4 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 20 },
               //     }
               // },
               // {/*104*/ MFX_ERR_NONE, EXT_BUFF | Compatibility, MFX_EXTBUFF_CODING_OPTION3,
               //     {
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_BEST_QUALITY  },
               //         { EXT_CO3, &tsStruct::mfxExtCodingOption3.NumRefActiveBL0[0], 4 },
               //         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 20 },
               //     }
               // },
          // QP range
          {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 9 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 }} },
          {/*13*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 10 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 }} },
          {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 9 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 }} },
          {/*15*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 51 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 }} },
          {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 52 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3 }} },
          {/*17*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 30 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 9 }} },
          {/*18*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 30 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 17 }} },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(unsigned int id)
    {
        tc_struct tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxHDL hdl;
        mfxHandleType type;
        mfxExtBuffer* buff_in = nullptr;
        mfxStatus sts = tc.sts;

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
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

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

        if (tc.type & EXT_BUFF)
        {
            CreateBuffer(tc.sub_type, &buff_in);
            m_pPar->NumExtParam = 1;
            m_pPar->ExtParam = &buff_in;
            SETPARS(buff_in, EXT_CO2);
            SETPARS(buff_in, EXT_CO3);
        }

        if ((tc.type & Compatibility) && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON || g_tsHWtype < MFX_HW_DG2))
        {
            g_tsLog << "[ SKIPPED ] VDEnc Compatibility tests are targeted only for VDEnc!\n\n\n";
            throw tsSKIP;
        }

        if (tc.type == RESOLUTION && tc.sub_type == INVALID_MAX) {
            m_par.mfx.FrameInfo.Width = INVALID_WIDTH;
            m_par.mfx.FrameInfo.Height = INVALID_HEIGHT;
        }
        else if (tc.type == RESOLUTION_W && tc.sub_type == INVALID_MAX) {
            m_par.mfx.FrameInfo.Width = INVALID_WIDTH;
        }
        else if (tc.type == RESOLUTION_H && tc.sub_type == INVALID_MAX) {
            m_par.mfx.FrameInfo.Height = INVALID_HEIGHT;
        }

        sts = tc.sts;

        if (tc.type & EXT_BUFF)
        {
            if ((tc.sub_type == MFX_EXTBUFF_ENCODER_RESET_OPTION) ||
                (tc.sub_type == MFX_EXTBUFF_VIDEO_SIGNAL_INFO))
                sts = MFX_ERR_NONE;
            else if (tc.sub_type == MFX_EXTBUFF_ENCODER_CAPABILITY && g_tsOSFamily == MFX_OS_FAMILY_WINDOWS && ((g_tsImpl & MFX_IMPL_VIA_D3D11)))
                sts = MFX_ERR_NONE;
        }

        if (tc.type == BUFFER_SIZE && IsOn(g_tsConfig.lowpower) && g_tsOSFamily == MFX_OS_FAMILY_LINUX &&
            m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        {
            // ICQ is not supported on VDENC Linux
            sts = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if ((tc.type & RATE_CONTROL) || tc.type == BUFFER_SIZE)
        {
            if ((m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_AVBR) && (tc.sts == MFX_ERR_NONE))
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            if (tc.sts < MFX_ERR_NONE)
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (tc.type == BUFFER_SIZE)
        {
            switch (tc.sub_type)
            {
            case BUFFER_SIZE_DEFAULT:
            {
                // We need to set a quite large InitialDelayInKB,
                // it should be more than bufferSizeInKB = m_par.mfx.TargetKbps / 4
                // It is the same as bufferSizeInKB = m_par.mfx.TargetKbps* DEFAULT_CPB_IN_SECONDS(=2) / 8
                // so DEFAULT_CPB_IN_SECONDS will be increased
                // to make sure that BufferSizeInKB >= InitialDelayInKB after MFXVideoENCODE_Init
                m_par.mfx.BufferSizeInKB = 0;
                m_par.mfx.InitialDelayInKB = m_par.mfx.TargetKbps * 3 / 8;
                break;
            }
            case INITIAL_DELAY_DEFAULT:
            {
                // If InitialDelayInKB == 0 it is calculated as BufferSizeInKB / 2
                // For MFX_RATECONTROL_CQP mode BufferSizeInKB should be >= rawBytes / 1000
                // Maximum of rawBytes = (w * h * 3 * BitDepthLuma + 7) / 8;
                // For others BufferSizeInKB = <valid_non_zero_value>
                m_par.mfx.BufferSizeInKB = (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * m_par.mfx.FrameInfo.BitDepthLuma + 7) / 8 / 1000;
                m_par.mfx.InitialDelayInKB = 0;
                break;
            }
            case DEFAULTS:
            {
                m_par.mfx.BufferSizeInKB = 0;
                m_par.mfx.InitialDelayInKB = 0;
                break;
            }
            case NONE:
            {
                m_par.mfx.BufferSizeInKB = (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * m_par.mfx.FrameInfo.BitDepthLuma + 7) / 8 / 1000;
                m_par.mfx.InitialDelayInKB = m_par.mfx.TargetKbps * 3 / 8;
                break;
            }
            default: break;
            }
            if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                m_pPar->mfx.QPI = QP;
                m_pPar->mfx.QPP = QP;
                m_pPar->mfx.QPB = QP;
            }
            if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
            {
                m_pPar->mfx.ICQQuality = ICQ_QUALITY;
            }
        }

        if (tc.type == CALL_QUERY)
            Query();

        if (tc.type == PAR_NULL)
        {
            m_pPar = nullptr;
        }

        if ((tc.type == _2_CALL) || (tc.type == _2_CALL_CLOSE))
        {
            this->Init(m_session);
            if (tc.type == _2_CALL_CLOSE)
                Close();
        }

        //call tested function
        g_tsStatus.expect(sts);
        std::unique_ptr<mfxExtBuffer> upBufIn(buff_in);
        if (tc.type != SESSION_NULL)
            sts = this->Init(m_session);
        else
            sts = this->Init(NULL);

        if (tc.type & BUFFER_SIZE && sts == MFX_ERR_NONE)
        {
            mfxVideoParam get_par = {};
            GetVideoParam(m_session, &get_par);
            if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA)
                //InitialDelayInKB is not used if RateControlMethod is not HRD compliant
            {
                if (tc.sub_type == NONE)
                {
                    EXPECT_EQ(get_par.mfx.BufferSizeInKB, m_par.mfx.BufferSizeInKB)
                        << "ERROR: BufferSizeInKB must not be changed";
                    EXPECT_EQ(get_par.mfx.InitialDelayInKB, m_par.mfx.InitialDelayInKB)
                        << "ERROR: InitialDelayInKB must not be changed";
                }
                else
                {
                    EXPECT_GE(get_par.mfx.BufferSizeInKB, get_par.mfx.InitialDelayInKB)
                        << "ERROR: BufferSizeInKB must not be less than InitialDelayInKB";
                }
            }
            else
            {
                EXPECT_GT(get_par.mfx.BufferSizeInKB, 0u)
                    << "ERROR: BufferSizeInKB must be greater than 0";
            }
        }

        if ((sts < MFX_ERR_NONE) && (tc.type != _2_CALL) && (tc.type != _2_CALL_CLOSE))
            g_tsStatus.disable_next_check();

        g_tsStatus.expect(MFX_ERR_NONE);
        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(avce_init, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(avce_8b_444_rgb4_init, RunTest_Subtype<MFX_FOURCC_RGB4>, n_cases);
}
