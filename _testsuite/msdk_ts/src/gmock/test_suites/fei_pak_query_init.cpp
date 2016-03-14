#include "ts_pak.h"
#include "ts_struct.h"

namespace fei_pak_query_init
{

class TestSuite : tsVideoPAK
{
public:
    TestSuite() : tsVideoPAK(MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;

    private:
        struct tc_par;

        enum
        {
            NULL_SESSION    = 0x1,
            NULL_PARAMS     = 0x2,
        /******* QueryMode *******/
            QUERY_INPLACE   = 0x100,
            QUERY_DIFIO     = 0x200,
        };

        enum
        {
            MFXPAR = 1,
            MFXFEIPAR,
        };

        struct tc_struct
        {
            mfxStatus sts_query;
            mfxStatus sts_init;
            mfxU32 mode;
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
        {/*00*/ MFX_ERR_INVALID_HANDLE, MFX_ERR_INVALID_HANDLE, NULL_SESSION},
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NULL_PTR, NULL_PARAMS},

        // IOPattern cases
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},
        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, 0x80}},

        // RateControlMethods (only CQP supported)
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPI, 21},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPP, 23},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 24}}},
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPI, 0xffff},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPP, 23},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 24}}},
        {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPI, 21},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPP, 0xffff},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 24}}},
        {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPI, 21},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPP, 23},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 0xffff}}},
        {/*09*/ MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPI, 21},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPP, 23},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.QPB, 24},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 1}}},
        {/*10*/ MFX_ERR_UNSUPPORTED, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0}}},
        {/*11*/ MFX_ERR_UNSUPPORTED, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 600}}},
        {/*12*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.Accuracy, 10},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.Convergence, 50}}},
        {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4}},
        {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 4}},

        /* invalid values for each field for Init() function */
        {/*15*/ MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC, 0, {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 0xffff}},
        {/*16*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.Protected, 0xffff}},
        {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, 0x50}},
        {/*18*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, 0x40}},
        {/*19*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 0xffffffff}},
        {/*20*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecId, 0xff}},
        {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, 0x40}},
        {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, 0xffff}},

        // frame_mbs_only_flag violation in H264 spec
        {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_AVC_42}}},
        {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_AVC_52}}},
        {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_AVC_2}}},
        {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_AVC_1}}},

        {/*27*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_AVC_42},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 3840},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 2160},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
        // DPB limatation /
        {/*28*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_AVC_22},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame , 4}}},
        {/*29*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_AVC_22},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 3840},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 2160},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame , 4}}},

        {/*30*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE}}},
        {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_CONSTRAINED_BASELINE}}},
        {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_PROGRESSIVE_HIGH}}},
        {/*33*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_CONSTRAINED_HIGH}}},
        {/*34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 8}},
        {/*35*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0xffff}},
        {/*36*/ MFX_ERR_UNSUPPORTED, MFX_ERR_UNSUPPORTED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0xffff}},
        {/*37*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 0xf0}},
        {/*38*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 8},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 9},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, MFX_GOP_STRICT}}},
        {/*39*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE}}},
        {/*40*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_CONSTRAINED_BASELINE}}},
        {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_CONSTRAINED_HIGH}}},
        {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_DEVICE_FAILED, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16}},
        {/*43*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 0xff},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0x80},
                                  {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0x60}}},
        {/*44*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 0xffff}},
        {/*45*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.EncodedOrder, 0xffff}},
        {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.Interleaved, 0xffff}},
        {/*47*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.Quality, 0xffff}},
        {/*48*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0xffff}},
        {/*49*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0xffff}},
        {/*50*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0xffff}},
        {/*51*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, 0xffff}},
        {/*52*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0xf1}},
        {/*53*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0}},
        {/*54*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4128}},
        {/*55*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4128}},
        {/*56*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0}},
        {/*57*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 16}},
        {/*58*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 16}},
        {/*59*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0x310}}},
        {/*60*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0x310}}},
        {/*61*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0x310}}},
        {/*62*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0x61}}},
        {/*63*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0x61}}},
        {/*64*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0x61}}},
        {/*65*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0x61}}},
        {/*66*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0x61}}},
        {/*67*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0x61}}},
        {/*68*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0xffff}},
        {/*69*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0xffff}},
        {/*70*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0xffff}},
        {/*71*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0xffff}},

        {/*72*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 752},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 32},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720}}},
        {/*73*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 512},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 32},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480}}},
        {/*74*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 64},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 688},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720}}},
        {/*75*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 64},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 448},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480}}},

        /* YUV420, Cropping alignment based on Luma and Chroma */
        {/*76*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 33},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 64},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720}}},
        {/*77*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 32},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 65},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480}}},
        {/*78*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 32},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 65},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480}}},

        {/*79*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0xffffffff},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
        {/*80*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0}}},
        {/*81*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
        {/*82*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 0xffff}},
        {/*83*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 0xffff}},
        {/*84*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x80}},
        {/*85*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF}},
        {/*86*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF}},
        {/*87*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_REPEATED}},
        {/*88*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FRAME_DOUBLING}},
        {/*89*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FRAME_TRIPLING}},
        {/*90*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 0xff}},
        {/*91*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422}}},
        {/*92*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
        {/*93*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV411}}},
        {/*94*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422H}}},
        {/*95*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422V}}},
        {/*96*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
        {/*97*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
        {/*98*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4}},
        {/*99*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXFEIPAR, &tsStruct::mfxExtFeiParam.Func, MFX_FEI_FUNCTION_ENC}},
        {/*100*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFXFEIPAR, &tsStruct::mfxExtFeiParam.SingleFieldProcessing, 0x40}},
        {/*101*/ MFX_ERR_NONE, MFX_ERR_NONE, QUERY_INPLACE},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        if (!(tc.mode & NULL_SESSION))
        {
            MFXInit();
        }

        if (m_impl == MFX_IMPL_HARDWARE)
            m_par.IOPattern =  MFX_IOPATTERN_IN_VIDEO_MEMORY;
        else
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        // set up parameters for case
        SETPARS(m_pPar, MFXPAR);
        mfxExtFeiParam& feiParam = m_par;
        SETPARS(&feiParam, MFXFEIPAR);

        //m_par.NumExtParam = 0;

        bool hw_surf = m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;
        if (hw_surf && !(tc.mode & NULL_SESSION))
            SetFrameAllocator(m_session, m_pVAHandle);

        if (tc.mode & NULL_PARAMS)
        {
            m_pPar = 0;
        }

        /*******************Query() and Init() test**********************/
        g_tsStatus.expect(tc.sts_query);
        if (tc.mode & QUERY_INPLACE)
        {
            Query(m_session, m_pPar, m_pParOut);
        } else {
            tsExtBufType<mfxVideoParam> par_out;
            mfxExtFeiParam& feiParamOut = par_out;
            par_out.mfx.CodecId = m_par.mfx.CodecId;
            feiParamOut.Func = feiParam.Func;
            feiParamOut.SingleFieldProcessing = feiParam.SingleFieldProcessing;
            Query(m_session, m_pPar, &par_out);
        }

        g_tsStatus.expect(tc.sts_init);
        Init(m_session, m_pPar);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(fei_pak_query_init);
}

