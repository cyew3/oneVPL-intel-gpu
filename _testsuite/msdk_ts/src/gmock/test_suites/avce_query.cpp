/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2021 Intel Corporation. All Rights Reserved.

File Name: avce_query.cpp

\* ****************************************************************************** */


#include "ts_encoder.h"
#include "ts_struct.h"

#if defined(MFX_ONEVPL)
#include "mfxpavp.h"
#endif

namespace avce_query
{

#define MAX_WIDTH  4096
#define MAX_HEIGHT 4096

    enum {
        MFX_PAR,
        CO2PAR,
        CO3PAR,
        TARGET_USAGE,
        EXT_BUFF,
        SESSION_NULL,
        SET_ALLOCK,
        AFTER,
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
        ALIGNMENT_HW
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
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC){}
        ~TestSuite() { }

        static const unsigned int n_cases;

        template<mfxU32 fourcc>
        int RunTest_Subtype(unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:

        static const tc_struct test_case[];

        void ValidateParameters(tc_struct tc)
        {
            switch (tc.type)
            {
                case TARGET_USAGE:
                {
                    tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
                    break;
                }
                default: break;
            }
        }

#define ALLOCATE_EXTBUFF_IN_OUT(buff_id, buff_class)   \
{ \
                        (*buff) = &(new buff_class())->Header; \
                        (*buff_out) = &(new buff_class())->Header; \
                        memset((*buff), 0, sizeof(buff_class)); \
                        memset((*buff_out), 0, sizeof(buff_class)); \
                        (*buff_out)->BufferId = (*buff)->BufferId = buff_id; \
                        (*buff_out)->BufferSz = (*buff)->BufferSz = sizeof(buff_class); \
}

        void CreateBuffer(mfxU32 buff_id, mfxExtBuffer** buff, mfxExtBuffer** buff_out)
        {
            switch (buff_id)
            {
                case MFX_EXTBUFF_CODING_OPTION_SPSPPS:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtCodingOptionSPSPPS);
                    break;

                case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtVideoSignalInfo);
                    break;

                case MFX_EXTBUFF_CODING_OPTION:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtCodingOption);
                    break;

                case MFX_EXTBUFF_CODING_OPTION2:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtCodingOption2);
                    break;

                case MFX_EXTBUFF_CODING_OPTION3:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtCodingOption3);
                    break;

                case MFX_EXTBUFF_VPP_DOUSE:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtVPPDoUse);
                    break;

                case MFX_EXTBUFF_VPP_AUXDATA:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtVppAuxData);
                    break;

                case MFX_EXTBUFF_AVC_REFLIST_CTRL:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtAVCRefListCtrl);
                    break;

                case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtVPPFrameRateConversion);
                    break;

                case MFX_EXTBUFF_PICTURE_TIMING_SEI:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtPictureTimingSEI);
                    break;

                case MFX_EXTBUFF_ENCODER_CAPABILITY:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtEncoderCapability);
                    break;

                case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                    ALLOCATE_EXTBUFF_IN_OUT(buff_id, mfxExtEncoderResetOption);
                    break;

                case NONE:
                {
                        *buff = nullptr;
                        *buff_out = nullptr;
                        break;
                }

                default: break;
            }
        }

        mfxStatus Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
        {
            TRACE_FUNC3(MFXVideoENCODE_Query, session, in, out);

            g_tsStatus.check( MFXVideoENCODE_Query(session, in, out) );

            TS_TRACE(out);
            return g_tsStatus.get();
        }

    };

    const tc_struct TestSuite::test_case[] =
    {
            // //Target Usage
            // {/*00*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 } },
            // {/*01*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 2 } },
            // {/*02*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 3 } },
            // {/*03*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 } },
            // {/*04*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 5 } },
            // {/*05*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 6 } },
            // {/*06*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 } },
            // //Ext Buffers
            // {/*07*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, {} },
            // {/*08*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, {} },
            // {/*09*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, {} },
            // {/*10*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, {} },
            // {/*11*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, {} },
            // {/*12*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, {} },
            // {/*13*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, {} },
            // {/*14*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, {} },
            // {/*15*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, {} },
            // {/*16*/ MFX_ERR_NULL_PTR, EXT_BUFF, NONE, {} },
            // //Rate Control Metod
            // {/*17*/ MFX_ERR_NONE, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 10000 } } },
            // {/*18*/ MFX_ERR_NONE, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            //                                     { MFX_PAR,& tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 } } },
            // {/*19*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*00*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
            // {/*21*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
            // {/*22*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
            // {/*23*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
            // {/*24*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
            // {/*25*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
            // {/*26*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
            // {/*27*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
            // {/*28*/ MFX_ERR_NONE, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM },
            //                                     { MFX_PAR,&tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 } } },
            // {/*29*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
            // {/*30*/ MFX_ERR_NONE, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
            //                                     { MFX_PAR,&tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 1 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 10000 },
            //                                     { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 15000 } } },
            // {/*31*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
            // //session
            // {/*32*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },
            // //set alloc handle
            // {/*33*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
            // {/*34*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
            // {/*35*/ MFX_ERR_UNSUPPORTED, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
            // {/*36*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
            // //zero param
            // {/*37*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, {} },
            // {/*38*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, {} },
            // //IOPattern
            // {/*39*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
            // {/*40*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
            // {/*41*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY } },
            // {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },
            // //Unsupported FourCC
            // {/*43*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
            //                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0 },
            //                                          } },
            // //Resolution
            // {/*44*/ MFX_ERR_UNSUPPORTED, RESOLUTION, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3 } },
            // {/*45*/ MFX_ERR_UNSUPPORTED, RESOLUTION, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3 } },
            // {/*46*/ MFX_ERR_UNSUPPORTED, RESOLUTION, W_GT_MAX, {} },
            // {/*47*/ MFX_ERR_UNSUPPORTED, RESOLUTION, H_GT_MAX, {} },
            // {/*48*/ MFX_ERR_NONE, RESOLUTION, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
            //                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
            //                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
            //                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
            //                                         } },
            // //PicStruct
        // the fields aren't using in VDEnc
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_UNKNOWN } },
        {/*02*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
        {/*03*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
        {/*04*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } },
        {/*05*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
            // //Crops
            // {/*54*/ MFX_ERR_UNSUPPORTED, CROP, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
            // {/*55*/ MFX_ERR_UNSUPPORTED, CROP, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
            // {/*56*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 } },
            // {/*57*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 } },
            // {/*58*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 },
            //                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 } } },
            // {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 }, } },
            // {/*60*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 }, } },
            // {/*61*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 }, } },
            // {/*62*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
            //                                                          { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 }, } },
            // //frame rate
            // {/*63*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
            // {/*64*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },
            // //chroma format
            // {/*65*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 255 } },
            // //num thread
            // {/*66*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 1 } },
            // {/*67*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 4 } },
            // //gop pic size
            // {/*68*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 } },
            // //gop ref dist
            // {/*69*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 } },
            // //got opt flag
            // {/*70*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 2 } },
            // //protected
            // {/*71*/
            //#if (defined(LINUX32) || defined(LINUX64))
            //            MFX_ERR_UNSUPPORTED,
            //#else
            //            MFX_ERR_NONE,
            //#endif
            //                          PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP } },
            //        {/*72*/
            //#if (defined(LINUX32) || defined(LINUX64))
            //            MFX_ERR_UNSUPPORTED,
            //#else
            //            MFX_ERR_NONE,
            //#endif
            //                          PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP } },
            //        {/*73*/ MFX_ERR_UNSUPPORTED, PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0xfff } },
        //Ext Buffers, check params of coding options
        {/*06*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { CO2PAR, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_NO } },
        {/*07*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { CO2PAR, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_VERTICAL } },
        {/*08*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { CO2PAR, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_HORIZONTAL } },
        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { CO2PAR, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_SLICE } },
        {/*10*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { CO2PAR, &tsStruct::mfxExtCodingOption2.IntRefType, 0x100 } },
            // {/*79*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION3, {} },
        {/*11*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION3, { CO3PAR, &tsStruct::mfxExtCodingOption3.FadeDetection, MFX_CODINGOPTION_ON } },
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

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;
        mfxHDL hdl;
        mfxHandleType type;
        mfxExtBuffer* buff_in = nullptr;
        mfxExtBuffer* buff_out = nullptr;
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

        if (tc.sub_type != DELTA) {  // not dependent from resolution params
            SETPARS(m_pPar, MFX_PAR);

            if (tc.type == RESOLUTION)
            {
                if (tc.sub_type == W_GT_MAX) {
                    m_pPar->mfx.FrameInfo.Width = MAX_WIDTH + 16;
                }
                if (tc.sub_type == H_GT_MAX) {
                    m_pPar->mfx.FrameInfo.Height = MAX_HEIGHT + 16;
                }
            }
        } else {   // pars depend on resolution
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
            m_pPar->NumExtParam = 1;
            m_pParOut->NumExtParam = 1;
            m_pPar->ExtParam = &buff_in;
            m_pParOut->ExtParam = &buff_out;

            SETPARS(buff_in, CO2PAR);
            SETPARS(buff_in, CO3PAR);

            if ((tc.sub_type == MFX_EXTBUFF_AVC_REFLIST_CTRL) ||
                (tc.sub_type == MFX_EXTBUFF_ENCODER_RESET_OPTION) ||
                (tc.sub_type == MFX_EXTBUFF_VIDEO_SIGNAL_INFO))
                sts = MFX_ERR_NONE;
            else if (tc.sub_type == NONE)
                sts = MFX_ERR_NULL_PTR;
            else if (tc.sub_type == MFX_EXTBUFF_ENCODER_CAPABILITY && (g_tsImpl & MFX_IMPL_VIA_D3D11))
                sts = MFX_ERR_NONE;
        }

        if (tc.type == IN_PAR_NULL)
        {
            m_pPar = nullptr;
        }
        if (tc.type == OUT_PAR_NULL)
        {
            m_pParOut = nullptr;
        }

        g_tsStatus.expect(sts);
        g_tsStatus.disable_next_check();

        //call tested function
        if (tc.type != SESSION_NULL)
            sts = this->Query(m_session, m_pPar, m_pParOut);
        else
            sts = this->Query(NULL, m_pPar, m_pParOut);

        ValidateParameters(tc);
        if (buff_in)
            delete buff_in;
        if (buff_out)
            delete buff_out;

        g_tsStatus.check(sts);

        if ((tc.type == SET_ALLOCK) && (tc.sub_type == AFTER))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();

            if (!m_is_handle_set)
            {
                m_pFrameAllocator->get_hdl(type, hdl);

                g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); // device was created by Core in Query() to get HW caps
                SetHandle(m_session, type, hdl);
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(avce_query, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(avce_8b_444_rgb4_query, RunTest_Subtype<MFX_FOURCC_RGB4>, n_cases);
}
