#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_level_profile
{

class TestSuite : tsVideoEncoder
{
public:
    static const unsigned int n_cases;

    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
    ~TestSuite() {}

    enum
    {
        MFXPAR = 1,
        SLICE,
        RESOLUTION,
        FRAME_RATE,
        NONE
    };

    int RunTest(unsigned int id);

private:
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
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
    // --- MAIN Profile ---//
    //------- Level 1 -------//
    // ok resolution, ok framerate
    {/*00*/ MFX_ERR_NONE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 128},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 96},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 256},
       },
    },
    // ok res, wrong framerate
    {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 160},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*03*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 128},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 96},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 2},
       },
    },
    // wrong slices
    {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 128},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 96},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 30},
       },
    },

    //------- Level 2 -------//
    // ok res, ok framerate
    {/*05*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 15},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
       },
    },
    // ok res, wrong framerate
    {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*08*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 5},
       },
    },
    // wrong slices
    {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 352},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 30},
       },
    },

    //------- Level 2.1 -------//
    // ok res, ok framerate
    {/*10*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 640},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 384},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480},
       },
    },
    // ok res, wrong framerate
    {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 640},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 384},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 48},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*13*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 640},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 384},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 6},
       },
    },
    // wrong slices
    {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 640},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 384},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 34},
       },
    },

    //------- Level 3 -------//
    // ok res, ok framerate
    {/*15*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 960},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 544},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 960},
       },
    },
    // ok res, wrong framerate
    {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 960},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 544},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*18*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 960},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 544},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 9},
       },
    },
    // wrong slices
    {/*19*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 960},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 544},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 34},
       },
    },

    //------- Level 3.1 -------//
    // ok res, ok framerate
    {/*20*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 736},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 960},
       },
    },
    // ok res, wrong framerate
    {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 736},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*23*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 736},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 12},
       },
    },
    // wrong slices
    {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 736},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 44},
       },
    },

    //------- Level 4 -------//
    // ok res, ok framerate
    {/*25*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1216},
       },
    },
    // ok res, wrong framerate
    {/*27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*28*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 17},
       },
    },
    // wrong slices
    {/*29*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 76},
       },
    },

    //------- Level 4.1  -------//
    // ok res, ok framerate
    {/*30*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1216},
       },
    },
    // ok res, wrong framerate
    {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 80},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*33*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 17},
       },
    },
    // wrong slices
    {/*34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 2048},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 76},
       },
    },

    //------- Level 5  -------//
    // ok res, ok framerate
    {/*35*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3180},
       },
    },
    // ok res, wrong framerate
    {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*38*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 34},
       },
    },
    // wrong slices
    {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 300},
       },
    },

    //------- Level 5.1  -------//
    // ok res, ok framerate
    {/*40*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3180},
       },
    },
    // ok res, wrong framerate
    {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 80},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*43*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 34},
       },
    },
    // wrong slices
    {/*44*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 300},
       },
    },

    //------- Level 5.2  -------//
    // ok res, ok framerate
    {/*45*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 120},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*46*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3180},
       },
    },
    // ok res, wrong framerate
    {/*47*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 140},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*48*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 34},
       },
    },
    // wrong slices
    {/*49*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3840},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2176},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 300},
       },
    },

    //------- Level 6  -------//
    // ok res, ok framerate
    {/*50*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*51*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3180},
       },
    },
    // ok res, wrong framerate
    {/*52*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*53*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 68},
       },
    },
    // wrong slices
    {/*54*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 700},
       },
    },

    //------- Level 6.1  -------//
    // ok res, ok framerate
    {/*55*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*56*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3180},
       },
    },
    // ok res, wrong framerate
    {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 80},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*58*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 68},
       },
    },
    // wrong slices
    {/*59*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 700},
       },
    },

    //------- Level 6.2  -------//
    // ok res, ok framerate
    {/*60*/ MFX_ERR_NONE, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 120},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // wrong resolution
    {/*61*/ MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3180},
       },
    },
    // ok res, wrong framerate
    {/*62*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 140},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
       },
    },
    // ok slices
    {/*63*/ MFX_ERR_NONE, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 68},
       },
    },
    // wrong slices
    {/*64*/ MFX_ERR_INVALID_VIDEO_PARAM, SLICE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 700},
       },
    },

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    MFXInit();

    tsExtBufType<mfxVideoParam> par_out(m_par);
    SETPARS(m_pPar, MFXPAR);

    m_par.mfx.FrameInfo.CropW = 0;
    m_par.mfx.FrameInfo.CropH = 0;

    //load plugin
    Load();

    g_tsStatus.expect(tc.sts);
    bool skip = false;

    if (m_loaded)
    {
        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(mfxU8)*16))
        {
            if (m_par.mfx.FrameInfo.Width > 3840 || m_par.mfx.FrameInfo.Height > 2160)
            {
                g_tsStatus.expect(MFX_ERR_NONE);    // for destructor (close/unload/etc.)
                skip = true;
                g_tsLog << "Case skipped. GACC doesn't support width > 3840 and height > 2160 \n";
            }
        }
        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(mfxU8)* 16))
        {
            if (tc.sts == MFX_ERR_INVALID_VIDEO_PARAM)
            {
                if (tc.type == SLICE)
                {
                    g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
                }
                if (tc.type == RESOLUTION)
                {
                    g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                }
            }
            if (g_tsStatus.m_expected != MFX_ERR_UNSUPPORTED)
            {
                m_par.mfx.FrameInfo.Width = TS_MIN(m_par.mfx.FrameInfo.Width, 8192);
                m_par.mfx.FrameInfo.Height = TS_MIN(m_par.mfx.FrameInfo.Height, 4096);
            }
        }
        else
        {
            if ((tc.type == SLICE) && (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM))
            {
                m_par.mfx.NumSlice = TS_MIN(m_par.mfx.NumSlice, 34);
            }
        }
    }

    if (!skip)
        Init();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_level_profile);
}