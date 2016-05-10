/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_cqp_hrd
{

#define FRAME_WIDTH         1920
#define FRAME_HEIGHT        1088

#define TARGET_KBPS         10000
#define MAX_KBPS            12000
#define BUFFER_SIZE_IN_KB   1500
#define INITIAL_DELAY_IN_KB 750
#define FRAME_RATE          30

#define BIG_BUFFER_SIZE_IN_KB   4800
#define BIG_INITIAL_DELAY_IN_KB 2400
#define PROFILE                 100
#define LEVEL_CORRECT           41
#define LEVEL_INCORRECT         40

#define QP_CORRECT              26

inline bool IsOn(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_ON;
}

inline bool IsOff(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_OFF;
}

mfxExtBuffer* GetExtendedBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    if (extBuf != 0)
    {
        for (mfxU16 i = 0; i < numExtBuf; i++)
            if (extBuf[i] != 0 && extBuf[i]->BufferId == id)
                return (extBuf[i]);
    }

    return 0;
}

#define CHECK(cond,status) if(sts == MFX_ERR_NONE && cond == 0) sts = status;

class TestSuite : public tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}

    int RunTest(unsigned int id);

    enum
    {
        MFXPAR  = 1,
        MFXPAR2 = 2,
        COPAR   = 3,
        CO2PAR  = 4,
        MFXCTRL = 5
    };

    enum
    {
        QueryFunc            = 0x01,
        InitFunc             = 0x02,
        QueryIOSurfFunc      = 0x04,
        ResetFunc            = 0x08,
        EncodeFrameAsyncFunc = 0x10,
        BitstreamCheck       = 0x20
    };

    enum
    {
        CQP_HRD_DISABLED   = 0x0000,
        CQP_HRD_ENABLED    = 0x0001,
        CBR                = 0x0002,
        VBR                = 0x0004,
        CHECK_LEVEL        = 0x0008,
        CHECK_BP_SEI       = 0x0010,
        CHECK_PT_SEI       = 0x0020,
        CHECK_RP_SEI       = 0x0040,
        CHECK_PARAM_CHANGE = 0x0080,
        CHECK_ZERO_CTRL    = 0x0100,
        CHECK_ZERO_SURF    = 0x0200
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 func;
        mfxU32 controlFlags;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];
    static const unsigned int n_cases;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /* Query
       Cases 00-03 - check that CQP HRD mode is triggered by correct parameters set,
       Cases 04-19 - check that CQP HRD mode is rejected because of incorrect parameters set,
       and encoder switched to regular CQP mode
       Cases 20-23 - check that incorrect (too low) level is fixed to correct one based on HRD parameters
       Cases 24-29 - check that CQP HRD mode supresses insertion of BP, PT and RP SEI*/
    {/*00*/ MFX_ERR_NONE, QueryFunc, CQP_HRD_ENABLED | CBR, {}},
    {/*01*/ MFX_ERR_NONE, QueryFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*02*/ MFX_ERR_NONE, QueryFunc, CQP_HRD_ENABLED | VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, MAX_KBPS}}},
    {/*03*/ MFX_ERR_NONE, QueryFunc, CQP_HRD_ENABLED | VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       0}}},
    {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   0}}},
    {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0}}},
    {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*10*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_UNKNOWN}}},
    {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_UNKNOWN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_UNKNOWN}}},
    {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_UNKNOWN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON}}},
    {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*18*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiVclHrdParameters, MFX_CODINGOPTION_ON}}},
    {/*19*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiVclHrdParameters, MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*20*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_LEVEL, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB}}},
    {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_LEVEL, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | VBR | CHECK_LEVEL, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB}}},
    {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | VBR | CHECK_LEVEL, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.PicTimingSEI, MFX_CODINGOPTION_ON}}},
    {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.PicTimingSEI, MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_RP_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.RecoveryPointSEI, MFX_CODINGOPTION_ON}}},
    {/*27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_RP_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.RecoveryPointSEI, MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*28*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_BP_SEI, {
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BufferingPeriodSEI, MFX_BPSEI_IFRAME}}},
    {/*29*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QueryFunc, CQP_HRD_ENABLED | CBR | CHECK_BP_SEI, {
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BufferingPeriodSEI, MFX_BPSEI_IFRAME},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},

    /* Init
        Cases 30-33 - check that CQP HRD mode is triggered by correct parameters set
        Cases 34-49 - check that CQP HRD mode is rejected because of incorrect parameters set,
        and encoder switched to regular CQP mode
        Cases 50-53 - check that default level set correctly based on HRD parameters
        Cases 54-57 - check that incorrect (too low) level is fixed to correct one based on HRD parameters
        Cases 58-63 - check that CQP HRD mode supresses insertion of BP, PT and RP SEI*/
    {/*30*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI | CHECK_RP_SEI, {}},
    {/*31*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*32*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | VBR | CHECK_PT_SEI | CHECK_RP_SEI,
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, MAX_KBPS}},
    {/*33*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | VBR | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       0}}},
    {/*35*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*36*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   0}}},
    {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*38*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0}}},
    {/*39*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*40*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_UNKNOWN}}},
    {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_UNKNOWN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF}}},
    {/*43*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiNalHrdParameters, MFX_CODINGOPTION_OFF},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*44*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_UNKNOWN}}},
    {/*45*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_UNKNOWN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON}}},
    {/*47*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance,   MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*48*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiVclHrdParameters, MFX_CODINGOPTION_ON}}},
    {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_DISABLED, {
        {COPAR, &tsStruct::mfxExtCodingOption.VuiVclHrdParameters, MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*50*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB}}},
    {/*51*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*52*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | VBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*53*/ MFX_ERR_NONE, InitFunc, CQP_HRD_ENABLED | VBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       0},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB}}},
    {/*54*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB}}},
    {/*55*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*56*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | VBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB}}},
    {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | VBR | CHECK_LEVEL | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile,     PROFILE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,       LEVEL_INCORRECT},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BIG_BUFFER_SIZE_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, BIG_INITIAL_DELAY_IN_KB},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.PicTimingSEI, MFX_CODINGOPTION_ON}}},
    {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.PicTimingSEI, MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*60*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_RP_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.RecoveryPointSEI, MFX_CODINGOPTION_ON},}},
    {/*61*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_RP_SEI, {
        {COPAR, &tsStruct::mfxExtCodingOption.RecoveryPointSEI, MFX_CODINGOPTION_ON},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*62*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_BP_SEI, {
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BufferingPeriodSEI, MFX_BPSEI_IFRAME}}},
    {/*63*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, InitFunc, CQP_HRD_ENABLED | CBR | CHECK_BP_SEI, {
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BufferingPeriodSEI, MFX_BPSEI_IFRAME},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},

    /* QueryIOSurf
        Cases 64-67 - Just check that CQP HRD mode doesn't lead to error or warning*/
    {/*64*/ MFX_ERR_NONE, QueryIOSurfFunc, CQP_HRD_ENABLED | CBR, {}},
    {/*65*/ MFX_ERR_NONE, QueryIOSurfFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*66*/ MFX_ERR_NONE, QueryIOSurfFunc, CQP_HRD_ENABLED | VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, MAX_KBPS}}},
    {/*67*/ MFX_ERR_NONE, QueryIOSurfFunc, CQP_HRD_ENABLED | VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, MAX_KBPS},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},

    /* Reset
        Cases 68-69 - Check that Reset w/o CQP HRD parameters change works correctly
        Cases 70-73 - Check that CQP HRD parameters change via Reset works correctly*/
    {/*68*/ MFX_ERR_NONE, ResetFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI | CHECK_RP_SEI, {}},
    {/*69*/ MFX_ERR_NONE, ResetFunc, CQP_HRD_ENABLED | VBR | CHECK_PT_SEI | CHECK_RP_SEI, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, MAX_KBPS}}},
    {/*70*/ MFX_ERR_NONE, ResetFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI | CHECK_RP_SEI | CHECK_PARAM_CHANGE, {
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS * 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB * 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB * 2},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB}}},
    {/*71*/ MFX_ERR_NONE, ResetFunc, CQP_HRD_ENABLED | VBR | CHECK_PT_SEI | CHECK_RP_SEI | CHECK_PARAM_CHANGE, {
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS * 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS * 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB * 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB * 2},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB}}},
    {/*72*/ MFX_ERR_NONE, ResetFunc, CQP_HRD_ENABLED | CBR | CHECK_PT_SEI | CHECK_RP_SEI | CHECK_PARAM_CHANGE, {
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS / 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB / 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB / 2},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB}}},
    {/*73*/ MFX_ERR_NONE, ResetFunc, CQP_HRD_ENABLED | VBR | CHECK_PT_SEI | CHECK_RP_SEI | CHECK_PARAM_CHANGE, {
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS / 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS / 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB / 2},
        {MFXPAR,  &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB / 2},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.TargetKbps,       TARGET_KBPS},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.MaxKbps,          MAX_KBPS},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB,   BUFFER_SIZE_IN_KB},
        {MFXPAR2, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, INITIAL_DELAY_IN_KB}}},

    /* EncodeFrameAsync
        Cases 74-75 - Check that correct QP is accepted in runtime
        Cases 76-78 - Check that zero mfxEncodeCtrl is correctly processed in runtime
        Cases 79-83 - Check that "bad" runtime QP (0 or > 52) is correctly processed in runtime*/
    {/*74*/ MFX_ERR_NONE, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.QP, QP_CORRECT}}},
    {/*75*/ MFX_ERR_NONE, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.QP, QP_CORRECT},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.FrameType, MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*76*/ MFX_ERR_NULL_PTR, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR | CHECK_ZERO_CTRL, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    {/*77*/ MFX_ERR_NULL_PTR, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR | CHECK_ZERO_CTRL, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    {/*78*/ MFX_ERR_NONE, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR | CHECK_ZERO_CTRL | CHECK_ZERO_SURF, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2}}},
    {/*79*/ MFX_ERR_INVALID_VIDEO_PARAM, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.QP, 0}}},
    {/*80*/ MFX_ERR_INVALID_VIDEO_PARAM, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.QP, 0},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.FrameType, MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*81*/ MFX_ERR_INVALID_VIDEO_PARAM, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.QP, 52},}},
    {/*82*/ MFX_ERR_INVALID_VIDEO_PARAM, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.QP, 52},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.FrameType, MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 1}}},
    {/*83*/ MFX_ERR_NONE, EncodeFrameAsyncFunc, CQP_HRD_ENABLED | CBR | CHECK_ZERO_SURF, {
        {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXCTRL, &tsStruct::mfxEncodeCtrl.QP, 0}}},

     /* Check within output bitstream that:
        - Correct HRD information is written to SPS->VUI
        - No SEI encoded */
    {/*84*/ MFX_ERR_NONE, BitstreamCheck, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8},
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BRefType, 2}}},
    {/*85*/ MFX_ERR_NONE, BitstreamCheck, CQP_HRD_ENABLED | VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8},
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BRefType, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,    MAX_KBPS}}},
    {/*86*/ MFX_ERR_NONE, BitstreamCheck, CQP_HRD_ENABLED | CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  16},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,  8},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 2},
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BRefType,  2}}},
    {/*87*/ MFX_ERR_NONE, BitstreamCheck, CQP_HRD_ENABLED | VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,  16},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,  8},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 2},
        {CO2PAR, &tsStruct::mfxExtCodingOption2.BRefType,  2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,     MAX_KBPS}}}
};

mfxStatus ValidateParameters(mfxVideoParam &par, mfxU32 controlFlags)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxExtCodingOption  * opt  = (mfxExtCodingOption*)GetExtendedBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION);
    mfxExtCodingOption2 * opt2 = (mfxExtCodingOption2*)GetExtendedBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION2);

    if (controlFlags == TestSuite::CQP_HRD_DISABLED)
    {
        // check that CQP HRD mode is disabled, and regular CQP is used instead
        CHECK(par.mfx.RateControlMethod == MFX_RATECONTROL_CQP, MFX_ERR_ABORTED);
        CHECK(0 <= par.mfx.QPI && par.mfx.QPI <= 51, MFX_ERR_ABORTED);
        CHECK(0 <= par.mfx.QPP && par.mfx.QPP <= 51, MFX_ERR_ABORTED);
        CHECK(0 <= par.mfx.QPB && par.mfx.QPB <= 51, MFX_ERR_ABORTED);
        CHECK(opt, MFX_ERR_ABORTED);
        CHECK(!IsOn(opt->VuiNalHrdParameters), MFX_ERR_ABORTED);

        if (sts != MFX_ERR_NONE)
            g_tsLog << "CQP HRD mode isn't switched to regular CQP mode...\n";
    }
    else if (controlFlags & TestSuite::CQP_HRD_ENABLED)
    {
        // check that CQP HRD mode is enabled, and correct parameters are applied
        CHECK(par.mfx.RateControlMethod == MFX_RATECONTROL_CQP, MFX_ERR_ABORTED);
        CHECK(par.mfx.TargetKbps == TARGET_KBPS, MFX_ERR_ABORTED);
        CHECK(par.mfx.FrameInfo.FrameRateExtN == FRAME_RATE, MFX_ERR_ABORTED);
        CHECK(par.mfx.FrameInfo.FrameRateExtD == 1, MFX_ERR_ABORTED);
        if (controlFlags & TestSuite::CHECK_LEVEL)
        {
            CHECK(par.mfx.BufferSizeInKB == BIG_BUFFER_SIZE_IN_KB, MFX_ERR_ABORTED);
            CHECK(par.mfx.InitialDelayInKB == BIG_INITIAL_DELAY_IN_KB, MFX_ERR_ABORTED);
            CHECK(par.mfx.CodecLevel == LEVEL_CORRECT, MFX_ERR_ABORTED);
        }
        else
        {
            CHECK(par.mfx.BufferSizeInKB == BUFFER_SIZE_IN_KB, MFX_ERR_ABORTED);
            CHECK(par.mfx.InitialDelayInKB == INITIAL_DELAY_IN_KB, MFX_ERR_ABORTED);
        }

        if (controlFlags & TestSuite::VBR)
        {
            CHECK(par.mfx.MaxKbps == MAX_KBPS, MFX_ERR_ABORTED);
        }
        else
        {
            CHECK(par.mfx.MaxKbps == 0, MFX_ERR_ABORTED);
        }


        CHECK(opt, MFX_ERR_ABORTED);
        CHECK(IsOn(opt->VuiNalHrdParameters), MFX_ERR_ABORTED);
        CHECK(IsOff(opt->NalHrdConformance), MFX_ERR_ABORTED);
        if (controlFlags & TestSuite::CHECK_PT_SEI)
        {
            CHECK(IsOff(opt->PicTimingSEI), MFX_ERR_ABORTED);
        }
        else
        {
            CHECK(!IsOn(opt->PicTimingSEI), MFX_ERR_ABORTED);
        }

        if (controlFlags & TestSuite::CHECK_RP_SEI)
        {
            CHECK(IsOff(opt->RecoveryPointSEI), MFX_ERR_ABORTED);
        }
        else
        {
            CHECK(!IsOn(opt->RecoveryPointSEI), MFX_ERR_ABORTED);
        }

        if (controlFlags & TestSuite::CHECK_BP_SEI)
        {
            CHECK(opt2, MFX_ERR_ABORTED);
            CHECK(opt2->BufferingPeriodSEI == MFX_BPSEI_DEFAULT, MFX_ERR_ABORTED);
        }

        if (sts != MFX_ERR_NONE)
            g_tsLog << "CQP HRD mode isn't configured correctly...\n";

    }
    else
    {
        g_tsLog << "Incorrect test implementation...\n";
    }

    return sts;
};

class SurfProc : public tsSurfaceProcessor
{
    tsRawReader m_raw_reader;
    mfxEncodeCtrl* m_pCtrl;
    mfxU32 m_curr_frame;
public:
    SurfProc(const char* fname, mfxFrameInfo& fi, mfxU32 n_frames, mfxEncodeCtrl * ctrl)
        : tsSurfaceProcessor()
        , m_raw_reader(fname, fi, n_frames)
        , m_pCtrl(ctrl)
        , m_curr_frame(0)
    { }

    ~SurfProc() {} ;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        m_pCtrl->QP = 26 + m_curr_frame % 10; // variate QP among frames

        mfxStatus sts = m_raw_reader.ProcessSurface(s);

        m_curr_frame++;
        return sts;
    }
};



class BsChecker : public tsBitstreamProcessor, tsParserH264AU
{
    mfxU32 m_cbrFlag;
public:
    BsChecker(mfxU32 controlFlags)
        : tsParserH264AU(BS_H264_INIT_MODE_DEFAULT)
    {
        m_cbrFlag = (controlFlags & TestSuite::CBR) ? 1 : 0;
        set_trace_level(BS_H264_TRACE_LEVEL_SPS);
    }
    ~BsChecker() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        UnitType& au = ParseOrDie();

        for (Bs32u i = 0; i < au.NumUnits; i++)
        {
            if (au.NALU[i].nal_unit_type == 0x06)
            {
                g_tsLog << "ERROR: in CQP HRD mode there should be no SEI in output bitstream";
                return MFX_ERR_ABORTED;
            }
            if (!(au.NALU[i].nal_unit_type == 0x07))
            {
                continue;
            }

            if (au.NALU[i].SPS == NULL || au.NALU[i].SPS->vui == NULL || au.NALU[i].SPS->vui->hrd == NULL || au.NALU[i].SPS->vui->hrd->cbr_flag == NULL ||
                au.NALU[i].SPS->vui->hrd->bit_rate_value_minus1 == NULL || au.NALU[i].SPS->vui->hrd->cpb_size_value_minus1 == NULL)
            {
                g_tsLog << "ERROR: Can't parse HRD information";
                return MFX_ERR_ABORTED;
            }

            if (au.NALU[i].SPS->vui->nal_hrd_parameters_present_flag == 0 || au.NALU[i].SPS->vui->vcl_hrd_parameters_present_flag != 0)
            {
                g_tsLog << "ERROR: Incorrect HRD flags in SPS/VUI";
                return MFX_ERR_ABORTED;
            }

            if (au.NALU[i].SPS->vui->timing_info_present_flag == 0)
            {
                g_tsLog << "ERROR: timing_info_present_flag = 0 in SPS/VUI";
                return MFX_ERR_ABORTED;
            }

            if (au.NALU[i].SPS->vui->num_units_in_tick == 0)
            {
                g_tsLog << "ERROR: time_scale = 0 in SPS/VUI";
                return MFX_ERR_ABORTED;
            }

            if ((au.NALU[i].SPS->vui->time_scale / au.NALU[i].SPS->vui->num_units_in_tick) != (FRAME_RATE << 1))
            {
                g_tsLog << "ERROR: Incorrect frame rate in SPS/VUI";
                return MFX_ERR_ABORTED;
            }

            if (*(au.NALU[i].SPS->vui->hrd->cbr_flag) != m_cbrFlag)
            {
                g_tsLog << "ERROR: Value of cbr_flag in bitsream is incorrect";
                return MFX_ERR_ABORTED;
            }
            mfxU32 value_m1 = *(au.NALU[i].SPS->vui->hrd->bit_rate_value_minus1);
            mfxU32 scale = au.NALU[i].SPS->vui->hrd->bit_rate_scale;
            mfxU32 BitRate = (value_m1 + 1) * (1 << (6 + scale));
            mfxU32 expectedBitrate = m_cbrFlag ? TARGET_KBPS * 1000 : MAX_KBPS * 1000;
            if (BitRate != expectedBitrate)
            {
                g_tsLog << "ERROR: Value of BitRate is incorrect";
                return MFX_ERR_ABORTED;
            }
            value_m1 = *(au.NALU[i].SPS->vui->hrd->cpb_size_value_minus1);
            scale = au.NALU[i].SPS->vui->hrd->cpb_size_scale;
            mfxU32 CpbSize = (value_m1 + 1) * (1 << (4 + scale));
            if (CpbSize != BUFFER_SIZE_IN_KB * 8000)
            {
                g_tsLog << "ERROR: Value of CpbSize is incorrect";
                return MFX_ERR_ABORTED;
            }
            g_tsLog << "Bitstream parsing is ok\n";
            break;
        }

        bs.DataLength = 0;
        bs.DataOffset = 0;

        return MFX_ERR_NONE;
    }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    tc_struct tc = test_case[id];

    MFXInit();

    /*1) Init common paramaters*/
    m_par.mfx.FrameInfo.CropX  = m_par.mfx.FrameInfo.CropY = 0;
    m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = FRAME_WIDTH;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = FRAME_HEIGHT;

    /*2) fill correct configuration for CQP HRD mode (CBR) */
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.TargetKbps        = TARGET_KBPS;
    m_par.mfx.MaxKbps           = 0;
    m_par.mfx.BufferSizeInKB    = BUFFER_SIZE_IN_KB;
    m_par.mfx.InitialDelayInKB  = INITIAL_DELAY_IN_KB;

    m_par.mfx.FrameInfo.FrameRateExtN = FRAME_RATE;
    m_par.mfx.FrameInfo.FrameRateExtD = 1;

    mfxExtCodingOption & co = m_par;
    co.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
    co.NalHrdConformance   = MFX_CODINGOPTION_OFF;

    mfxExtCodingOption2 & co2 = m_par;

    /*3) apply parameters specific for test case */
    SETPARS(m_pPar, MFXPAR);
    SETPARS(&co, COPAR);
    if (tc.controlFlags & CHECK_BP_SEI)
    {
        SETPARS(&co2, CO2PAR);
    }

    /*4) call tested function and validate output */

    if(QueryFunc & tc.func)
    {
        g_tsStatus.expect(tc.sts);
        Query();
        g_tsStatus.check(ValidateParameters(*m_pParOut, tc.controlFlags));
    }
    else if (InitFunc & tc.func)
    {
        g_tsStatus.expect(tc.sts);
        Init();
        GetVideoParam();
        g_tsStatus.check(ValidateParameters(*m_pPar, tc.controlFlags));
        Close();
    }
    else if (QueryIOSurfFunc & tc.func)
    {
        g_tsStatus.expect(tc.sts);
        QueryIOSurf();
    }
    else if (ResetFunc & tc.func)
    {
        Init();
        g_tsStatus.expect(tc.sts);
        if (tc.controlFlags & CHECK_PARAM_CHANGE)
        {
            SETPARS(m_pPar, MFXPAR2);
        }
        Reset();
        GetVideoParam();
        g_tsStatus.check(ValidateParameters(*m_pPar, tc.controlFlags));
        Close();
    }
    else if (EncodeFrameAsyncFunc & tc.func)
    {
        Init();
        AllocSurfaces();
        AllocBitstream();

        if (tc.controlFlags & CHECK_ZERO_SURF)
        {
            m_pCtrl->QP = 26;
            for (mfxI32 i = 0; i < m_par.mfx.GopRefDist + 1; i ++)
            {
                EncodeFrameAsync(m_session, m_pCtrl, GetSurface(), m_pBitstream, m_pSyncPoint);
                if (*m_pSyncPoint)
                {
                    SyncOperation();
                    m_pBitstream->DataOffset = 0;
                    m_pBitstream->DataLength = 0;
                }
            }
        }

        SETPARS(m_pCtrl, MFXCTRL);
        g_tsStatus.expect(tc.sts);
        mfxFrameSurface1 * pSurf = (tc.controlFlags & CHECK_ZERO_SURF) ? 0 : GetSurface();
        mfxEncodeCtrl * pCtrl = (tc.controlFlags & CHECK_ZERO_CTRL) ? 0 : m_pCtrl;
        g_tsStatus.check(EncodeFrameAsync(m_session, pCtrl, pSurf, m_pBitstream, m_pSyncPoint));
        Close();
    }
    else if  (BitstreamCheck & tc.func)
    {
        mfxU32 nframes = 30;
        SetFrameAllocator();

        SurfProc reader(g_tsStreamPool.Get("YUV/matrix_1920x1088_250.yuv"), m_par.mfx.FrameInfo, nframes, m_pCtrl);
        g_tsStreamPool.Reg();
        m_filler = &reader;
        BsChecker bsChecker(tc.controlFlags);
        m_bs_processor = &bsChecker;

        Init();
        EncodeFrames(nframes);
        Close();
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_cqp_hrd);
};
