/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <mutex>
#include <algorithm>
#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp9e_dynamic_scaling
{

#define SETPARSITER(p, type)                                                                                                                                                            \
for(mfxU32 i = 0; i < MAX_NPARS; i++)                                                                                                                                                       \
{                                                                                                                                                                                           \
    if(iterPar.set_par[i].f && iterPar.set_par[i].ext_type == type)                                                                                             \
    {                                                                                                                                                                                       \
        SetParam(p, iterPar.set_par[i].f->name, iterPar.set_par[i].f->offset, iterPar.set_par[i].f->size, iterPar.set_par[i].v);    \
    }                                                                                                                                                                                       \
}

#define MAX_ITERATIONS 10
#define MAX_EXT_BUFFERS 4
#define GOP_SIZE 4
#define ITER_LENGTH 2

#define WIDTH(x) MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, x
#define HEIGHT(x) MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, x
#define FRAME_WIDTH(x) MFX_EXT_VP9PARAM, &tsStruct::mfxExtVP9Param.FrameWidth, x
#define FRAME_HEIGHT(x) MFX_EXT_VP9PARAM, &tsStruct::mfxExtVP9Param.FrameHeight, x
#define SET_RESOLUTION(w, h) WIDTH(w), HEIGHT(h), FRAME_WIDTH(w), FRAME_HEIGHT(h)

#define IVF_SEQ_HEADER_SIZE_BYTES 32
#define IVF_PIC_HEADER_SIZE_BYTES 12
#define MAX_IVF_HEADER_SIZE IVF_SEQ_HEADER_SIZE_BYTES + IVF_PIC_HEADER_SIZE_BYTES

#define MAX_REFS 3
#define MAX_U16 0xffff

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        struct params_for_iteration
        {
            mfxU32 num_frames_in_iteration;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        }iteration_par[MAX_ITERATIONS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9) {}
        ~TestSuite() {}
        template <mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_p010;
        static const unsigned int n_cases_ayuv;
        static const unsigned int n_cases_y410;

    private:
        static const tc_struct test_case[];
        int RunTest(const tc_struct& tc, mfxU32 fourcc, const unsigned int id);
    };

    enum TestSubtype
    {
        CQP = 0x01,
        CBR = 0x02,
        VBR = 0x04,
        TU1 = 0x10,
        TU4 = 0x20,
        TU7 = 0x40,
        SIMPLE_DOWNSCALE = 0x0100,
        SIMPLE_UPSCALE = 0x0200,
        MAX_SCALES = 0x0400,
        DS_ON_EACH_FRAME = 0x0800,
        UPSCALE_DOWNSCALE_INTERLEAVED = 0x1000,
        MULTIREF = 0x2000,
        KEY_FRAME = 0x4000,
        NOT_EQUAL_TO_SURF_SIZE = 0x8000,
        NOT_ALIGNED = 0x010000,
        CHANGE_OF_ASPECT_RATIO = 0x020000,
        ERROR_EXPECTED = 0x040000,
        STRESS = 0x080000,
        REAL_LIFE =0x100000,
        EXPERIMENTAL = 0x800000
    };

    const std::vector<TestSubtype> executeTests =
    {
        CQP,
        CBR,
        VBR,
        TU1,
        TU4,
        TU7,
        SIMPLE_DOWNSCALE,
        SIMPLE_UPSCALE,
        MAX_SCALES,
        DS_ON_EACH_FRAME,
        UPSCALE_DOWNSCALE_INTERLEAVED,
        MULTIREF,
        KEY_FRAME,
        NOT_EQUAL_TO_SURF_SIZE,
        NOT_ALIGNED,
        CHANGE_OF_ASPECT_RATIO,
        ERROR_EXPECTED,
        STRESS,
        REAL_LIFE
    };

    enum
    {
        MFX_PAR = 0,
        MFX_EXT_VP9PARAM = 1,
        MFX_EXT_RESET_OPTION = 2,
        MFX_EXT_VP9_TL = 3
    };

    enum
    {
        SET = 0,
        GET = 1,
        CHECK = 2
    };

    const tc_struct test_cases[] =
    {
        // below cases do single 2x downscale
        {/*00*/ MFX_ERR_NONE, SIMPLE_DOWNSCALE | CQP | TU1,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*01*/ MFX_ERR_NONE, CQP | TU4 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*02*/ MFX_ERR_NONE, CQP | TU7 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*03*/ MFX_ERR_NONE, CBR | TU1 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*04*/ MFX_ERR_NONE, CBR | TU4 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*05*/ MFX_ERR_NONE, CBR | TU7 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*06*/ MFX_ERR_NONE, VBR | TU1 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*07*/ MFX_ERR_NONE, VBR | TU4 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*08*/ MFX_ERR_NONE, VBR | TU7 | SIMPLE_DOWNSCALE,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },

        // below cases check that upscale above initial resolution (1st frame's resolution) not supported
        {/*09*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, CQP | TU4 | SIMPLE_UPSCALE | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | TU4 | SIMPLE_UPSCALE | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(288+2) }
            },
        },
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | TU4 | SIMPLE_UPSCALE | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352+2), FRAME_HEIGHT(288) }
            },
        },
        {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | TU4 | ERROR_EXPECTED,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },

        // below case check that upscale above inited max_width and max_height not supported
        {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | TU4 | ERROR_EXPECTED,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 + 8), FRAME_HEIGHT(288 + 8) }
            },
        },

        // below cases do consequent maximum allowed downscales (2x) till 16x accumulated downscale is reached
        // and then do maximum allowed upscale (16x)
        {/*14*/ MFX_ERR_NONE, CQP | MAX_SCALES,
            {
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
            },
        },
        {/*15*/ MFX_ERR_NONE, CBR | MAX_SCALES,
            {
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
            },
        },
        {/*16*/ MFX_ERR_NONE, VBR | MAX_SCALES,
            {
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
            },
        },

        // below cases do consequent 2x downscales on each frame
        {/*17*/ MFX_ERR_NONE, CQP | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
            },
        },
        {/*18*/ MFX_ERR_NONE, CBR | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
            },
        },
        {/*19*/ MFX_ERR_NONE, VBR | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
            },
        },

        // below cases do consequent 2x downscale and then upscales on each frame
        {/*20*/ MFX_ERR_NONE, CQP | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(704, 576) },
            },
        },
        {/*21*/ MFX_ERR_NONE, CBR | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(704, 576) },
            },
        },
        {/*22*/ MFX_ERR_NONE, VBR | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(704, 576) },
            },
        },

        // below cases interleave various downscales and upscales
        {/*23*/ MFX_ERR_NONE, CQP | UPSCALE_DOWNSCALE_INTERLEAVED,
            {
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
            },
        },
        {/*24*/ MFX_ERR_NONE, CBR | UPSCALE_DOWNSCALE_INTERLEAVED,
            {
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
            },
        },
        {/*25*/ MFX_ERR_NONE, VBR | UPSCALE_DOWNSCALE_INTERLEAVED,
            {
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
            },
        },

        // below cases do downscale and upscale together with multiref
        {/*26*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF,
            {
                { 4,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                     SET_RESOLUTION(352, 288) } },
                { 4, SET_RESOLUTION(176, 144) },
                { 4, SET_RESOLUTION(352, 288) },
            },
        },
        {/*27*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF,
            {
                { 5,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                     SET_RESOLUTION(352, 288) } },
                { 5, SET_RESOLUTION(176, 144) },
                { 5, SET_RESOLUTION(352, 288) },
            },
        },
        {/*28*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF,
            {
                { 6,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                     SET_RESOLUTION(352, 288) } },
                { 6, SET_RESOLUTION(176, 144) },
                { 6, SET_RESOLUTION(352, 288) },
            },
        },
        {/*29*/ MFX_ERR_NONE, CQP | TU4 | MULTIREF,
            {
                { 3,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2,
                     SET_RESOLUTION(352, 288) } },
                { 3, SET_RESOLUTION(176, 144) },
                { 3, SET_RESOLUTION(352, 288) },
            },
        },
        {/*30*/ MFX_ERR_NONE, CQP | TU4 | MULTIREF,
            {
                { 4,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2,
                     SET_RESOLUTION(352, 288) } },
                { 4, SET_RESOLUTION(176, 144) },
                { 4, SET_RESOLUTION(352, 288) },
            },
        },

        // below cases do resolution change with key-frame
        {/*31*/ MFX_ERR_NONE, CQP| KEY_FRAME,
            {
                { GOP_SIZE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, GOP_SIZE,
                              SET_RESOLUTION(352, 288) } },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*32*/ MFX_ERR_NONE, CQP | KEY_FRAME,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH,{ MFX_EXT_RESET_OPTION, &tsStruct::mfxExtEncoderResetOption.StartNewSequence, MFX_CODINGOPTION_ON,
                                SET_RESOLUTION(176, 144) } }
            },
        },

        // below cases do 2 downscales and then single uscale from and to resolutions aligned to 16 but not equal to surface size
        {/*33*/ MFX_ERR_NONE, CQP | TU1 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*34*/ MFX_ERR_NONE, CQP | TU4 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*35*/ MFX_ERR_NONE, CQP | TU7 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*36*/ MFX_ERR_NONE, CBR | TU1 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*37*/ MFX_ERR_NONE, CBR | TU4 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*38*/ MFX_ERR_NONE, CBR | TU7 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*39*/ MFX_ERR_NONE, VBR | TU1 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*40*/ MFX_ERR_NONE, VBR | TU4 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*41*/ MFX_ERR_NONE, VBR | TU7 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 1, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },

        // below cases do single downscale and then single uscale from and to resolutions not aligned to 16
        // without change of Aspect Ratio (DAR)
        {/*42*/ MFX_ERR_NONE, CQP | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*43*/ MFX_ERR_NONE, CQP | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*44*/ MFX_ERR_NONE, CQP | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*45*/ MFX_ERR_NONE, CBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*46*/ MFX_ERR_NONE, CBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*47*/ MFX_ERR_NONE, CBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*48*/ MFX_ERR_NONE, VBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*49*/ MFX_ERR_NONE, VBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },
        {/*50*/ MFX_ERR_NONE, VBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176 - 10), FRAME_HEIGHT(144 - 10) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 30), FRAME_HEIGHT(288 - 30) }
            },
        },

        // below cases do single downscale and then single uscale from and to resolutions not aligned to 16
        // with change of Aspect Ratio (DAR)
        {/*51*/ MFX_ERR_NONE, CQP | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*52*/ MFX_ERR_NONE, CQP | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*53*/ MFX_ERR_NONE, CQP | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*54*/ MFX_ERR_NONE, CBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*55*/ MFX_ERR_NONE, CBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*56*/ MFX_ERR_NONE, CBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*57*/ MFX_ERR_NONE, VBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*58*/ MFX_ERR_NONE, VBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },
        {/*59*/ MFX_ERR_NONE, VBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352 - 8), FRAME_HEIGHT(288) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704 - 40), FRAME_HEIGHT(576 - 40) }
            },
        },

        // below cases do too aggressive downscale (>2x) to check that correct error is returned
        {/*60*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(142) }
            },
        },
        {/*61*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(174), FRAME_HEIGHT(144) }
            },
        },

        // below cases do too aggressive upscale (>16x) to check that correct error is returned
        {/*62*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(142) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
            },
        },
        {/*63*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(142) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
            },
        },

        // below cases do scaling to resolution change not aligned to 2 (not aligned to chroma)
        // TODO: make proper changes for REXTs with different chroma samplings
        {/*64*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-1), FRAME_HEIGHT(144) }
            },
        },
        {/*65*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(144-1) }
            },
        },

        // below cases do scaling to properly aligned resolutions which are bigger than size of current surface
        {/*66*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176+2), FRAME_HEIGHT(144) }
            },
        },
        {/*67*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(144+2) }
            },
        },

        // below cases do scaling to properly aligned resolutions which are bigger than size of initial surface
        {/*68*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(176+2), FRAME_HEIGHT(144) }
            },
        },
        {/*69*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(176), FRAME_HEIGHT(144+2) }
            },
        },

        // case below tryes to use dynamic scaling together with temporal scalability
        {/*70*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, MFX_EXT_VP9_TL, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1,
                               MFX_EXT_VP9_TL, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2,
                               SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, MFX_EXT_VP9_TL, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1,
                               MFX_EXT_VP9_TL, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2,
                               SET_RESOLUTION(176, 144) }
            },
        },

        // below cases are stress tests with multiref and many upscales/downscales to not aligned resolutions
        // with change of Aspect Ratio (DAR)
        {/*71*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF | STRESS,
            {
                { 5, WIDTH(2816), HEIGHT(2304),
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,},
                { 5, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                     WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22)},
                { 5, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10)},
                { 5, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 5, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142)},
                { 5, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2)},
                { 5, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114)},
                { 5, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54)},
                { 5, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2)},
            },
        },
        {/*72*/ MFX_ERR_NONE, CBR | TU1 | MULTIREF | STRESS,
            {
                { 5, WIDTH(2816), HEIGHT(2304),
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3, },
                { 5, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 5, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 5, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 5, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 5, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 5, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 5, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 5, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },
        {/*73*/ MFX_ERR_NONE, VBR | TU1 | MULTIREF | STRESS,
            {
                { 5, WIDTH(2816), HEIGHT(2304),
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3, },
                { 5, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 5, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 5, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 5, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 5, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 5, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 5, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 5, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },
        {/*74*/ MFX_ERR_NONE, CQP | TU4 | MULTIREF | STRESS,
            {
                { 4, WIDTH(2816), HEIGHT(2304),
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2, },
                { 4, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 4, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 4, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 4, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 4, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 4, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 4, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 4, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },
        {/*75*/ MFX_ERR_NONE, CBR | TU4 | MULTIREF | STRESS,
            {
                { 4, WIDTH(2816), HEIGHT(2304),
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2, },
                { 4, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 4, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 4, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 4, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 4, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 4, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 4, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 4, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },
        {/*76*/ MFX_ERR_NONE, VBR | TU4 | MULTIREF | STRESS,
            {
                { 4, WIDTH(2816), HEIGHT(2304),
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2, },
                { 4, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 4, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 4, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 4, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 4, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 4, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 4, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 4, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },
        {/*77*/ MFX_ERR_NONE, CQP | TU7 | STRESS,
            {
                { 2, WIDTH(2816), HEIGHT(2304) },
                { 2, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 2, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 2, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 2, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 2, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 2, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 2, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 2, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },
        {/*78*/ MFX_ERR_NONE, CBR | TU7 | STRESS,
            {
                { 2, WIDTH(2816), HEIGHT(2304) },
                { 2, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 2, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 2, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 2, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 2, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 2, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 2, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 2, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },
        {/*79*/ MFX_ERR_NONE, VBR | TU7 | STRESS,
            {
                { 2, WIDTH(2816), HEIGHT(2304) },
                { 2, MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1,
                WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-14), FRAME_HEIGHT(576-22) },
                { 2, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-6), FRAME_HEIGHT(288-10) },
                { 2, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-2), FRAME_HEIGHT(144-4) },
                { 2, WIDTH(2816), HEIGHT(2304), FRAME_WIDTH(2816-98), FRAME_HEIGHT(2304-142) },
                { 2, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-26), FRAME_HEIGHT(1152-2) },
                { 2, WIDTH(1408), HEIGHT(1152), FRAME_WIDTH(1408-110), FRAME_HEIGHT(1152-114) },
                { 2, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-54), FRAME_HEIGHT(576-54) },
                { 2, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-2), FRAME_HEIGHT(576-2) },
            },
        },

        // below cases use streams downscaled from original 1920x1080 stream
        // they do several upscales/downscales for different QPs
        {/*80*/ MFX_ERR_NONE, CQP | TU1 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 50,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*81*/ MFX_ERR_NONE, CQP | TU1 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 120,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 120,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*82*/ MFX_ERR_NONE, CQP | TU1 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 200,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 200,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*83*/ MFX_ERR_NONE, CQP | TU4 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 50,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*84*/ MFX_ERR_NONE, CQP | TU4 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 120,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 120,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*85*/ MFX_ERR_NONE, CQP | TU4 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 200,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 200,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*86*/ MFX_ERR_NONE, CQP | TU7 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 50,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*87*/ MFX_ERR_NONE, CQP | TU7 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 120,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 120,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },
        {/*88*/ MFX_ERR_NONE, CQP | TU7 | REAL_LIFE,
            {
                { ITER_LENGTH, MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 200,
                     MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 200,
                     WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
                { ITER_LENGTH, WIDTH(1440), HEIGHT(816), FRAME_WIDTH(1440), FRAME_HEIGHT(810) },
                { ITER_LENGTH, WIDTH(1280), HEIGHT(720), FRAME_WIDTH(1280), FRAME_HEIGHT(720) },
                { ITER_LENGTH, WIDTH(960), HEIGHT(544), FRAME_WIDTH(960), FRAME_HEIGHT(540) },
                { ITER_LENGTH, WIDTH(640), HEIGHT(368), FRAME_WIDTH(640), FRAME_HEIGHT(360) },
                { ITER_LENGTH, WIDTH(1920), HEIGHT(1088), FRAME_WIDTH(1920), FRAME_HEIGHT(1080) },
            },
        },

        // CASES BELOW DO UPSCALE CHECK (DISABLED, BECAUSE NOT SUPPORTED NOW BY THE DRIVER)
        /*
        // below cases do single 2x upscale
        {MFX_ERR_NONE, CQP | TU1 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },

        {MFX_ERR_NONE, CQP | TU4 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },
        {MFX_ERR_NONE, CQP | TU7 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },
        {MFX_ERR_NONE, CBR | TU1 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },
        { MFX_ERR_NONE, CBR | TU4 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },
        {MFX_ERR_NONE, CBR | TU7 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },
        {MFX_ERR_NONE, VBR | TU1 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },
        {MFX_ERR_NONE, VBR | TU4 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },
        {MFX_ERR_NONE, VBR | TU7 | SIMPLE_UPSCALE,
        {
            { 0, SET_RESOLUTION(352, 288) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(352, 288) }
        },
        },

        // below cases do single maximum allowed upscale (16x)
        {MFX_ERR_NONE, CQP | MAX_SCALES,
        {
            { 0, SET_RESOLUTION(2816, 2304) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
        },
        },
        {MFX_ERR_NONE, CBR | MAX_SCALES,
        {
            { 0, SET_RESOLUTION(2816, 2304) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
        },
        },
        {MFX_ERR_NONE, VBR | MAX_SCALES,
        {
            { 0, SET_RESOLUTION(2816, 2304) },
            { ITER_LENGTH, SET_RESOLUTION(176, 144) },
            { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
        },
        },
        */
    };

    const unsigned int TestSuite::n_cases = sizeof(test_cases) / sizeof(tc_struct);

    const unsigned int TestSuite::n_cases_nv12 = n_cases;
    const unsigned int TestSuite::n_cases_p010 = n_cases;
    const unsigned int TestSuite::n_cases_ayuv = n_cases;
    const unsigned int TestSuite::n_cases_y410 = n_cases;

    class SurfaceFeeder : public tsRawReader
    {
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        mfxU32 m_frameOrder;

        tsSurfacePool surfaceStorage;
    public:
        SurfaceFeeder(
            std::map<mfxU32, mfxFrameSurface1*>* pInSurfaces,
            const mfxVideoParam& par,
            mfxU32 frameOrder,
            const char* fname,
            mfxFrameInfo fi,
            mfxU32 n_frames = 0xFFFFFFFF)
            : tsRawReader(fname, fi, n_frames)
            , m_pInputSurfaces(pInSurfaces)
            , m_frameOrder(frameOrder)
        {
            if (m_pInputSurfaces)
            {
                const mfxU32 numInternalSurfaces = par.AsyncDepth ? par.AsyncDepth : 5;
                mfxFrameAllocRequest req = {};
                req.Info = fi;
                req.NumFrameMin = req.NumFrameSuggested = numInternalSurfaces;
                req.Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
                surfaceStorage.AllocSurfaces(req);
            }
        };

        ~SurfaceFeeder()
        {
            if (m_pInputSurfaces)
            {
                surfaceStorage.FreeSurfaces();
            }
        }

        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
    };

    mfxFrameSurface1* SurfaceFeeder::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        mfxFrameSurface1* pSurf = tsSurfaceProcessor::ProcessSurface(ps, pfa);
        if (m_pInputSurfaces)
        {
            mfxFrameSurface1* pStorageSurf = surfaceStorage.GetSurface();
            pStorageSurf->Data.Locked++;
            tsFrame in = tsFrame(*pSurf);
            tsFrame store = tsFrame(*pStorageSurf);
            store = in;
            (*m_pInputSurfaces)[m_frameOrder] = pStorageSurf;
        }
        m_frameOrder++;

        return pSurf;
    }

    template<class T>
    void Zero(T& obj) { memset(&obj, 0, sizeof(obj)); }

    template<class T>
    bool IsZero(const T& obj)
    {
        T zeroed;
        Zero(zeroed);
        return memcmp(&obj, &zeroed, sizeof(T)) == 0;
    }

    template<class T>
    bool IsZeroExtBuf(const T& obj)
    {
        T zeroed;
        Zero(zeroed);
        zeroed.Header.BufferSz = obj.Header.BufferSz;
        zeroed.Header.BufferId = obj.Header.BufferId;
        return memcmp(&obj, &zeroed, sizeof(T)) == 0;
    }

    template<class T>
    inline void InitExtBuffer(
        mfxU32 extType,
        T& buf)
    {
        Zero(buf);
        buf.Header.BufferSz = sizeof(T);
        buf.Header.BufferId = extType;
    }

    struct Resolution
    {
        mfxU32 w;
        mfxU32 h;

        Resolution(mfxU32 width, mfxU32 height) : w(width), h(height) {};

        bool operator<(const Resolution& res) const
        {
            return w * h < res.w * res.h;
        }

        bool operator<=(const Resolution& res) const
        {
            return w * h <= res.w * res.h;
        }
    };

    mfxU16 GetDefaultTargetKbps(const Resolution& resol, mfxU32 fourcc, mfxU16& brcMult)
    {
        mfxU32 kbps = 0;
        if (resol <= Resolution(176, 144)) kbps = 100;
        else if (resol <= Resolution(352, 288)) kbps = 400;
        else if (resol <= Resolution(704, 576)) kbps = 1600;
        else if (resol <= Resolution(1408, 1152)) kbps = 6400;
        else if (resol <= Resolution(2816, 2304)) kbps = 25600;
        else kbps = 53200;

        mfxU16 mult = (fourcc == MFX_FOURCC_NV12) ? 1 : 2;
        brcMult = mult;

        if (kbps * mult < MAX_U16)
        {
            brcMult = 1;
            return kbps * mult;
        }
        else
        {
            return kbps;
        }
    }

    void SetAdditionalParams(mfxVideoParam& par, const mfxExtVP9Param& extPar, mfxU32 fourcc, mfxU32 testType)
    {
        mfxFrameInfo& fi = par.mfx.FrameInfo;
        if (fourcc == MFX_FOURCC_NV12)
        {
            fi.FourCC = MFX_FOURCC_NV12;
            fi.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
        else if (fourcc == MFX_FOURCC_P010)
        {
            fi.FourCC = MFX_FOURCC_P010;
            fi.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            fi.BitDepthLuma = fi.BitDepthChroma = 10;
            fi.Shift = 1;
        }
        else if (fourcc == MFX_FOURCC_AYUV)
        {
            fi.FourCC = MFX_FOURCC_AYUV;
            fi.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            fi.BitDepthLuma = fi.BitDepthChroma = 8;
        }
        else if (fourcc == MFX_FOURCC_Y410)
        {
            fi.FourCC = MFX_FOURCC_Y410;
            fi.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            fi.BitDepthLuma = fi.BitDepthChroma = 10;
        }
        else
        {
            ADD_FAILURE() << "ERROR: Invalid fourcc in the test: " << fourcc;
            throw tsFAIL;
        }

        fi.CropX = fi.CropY = 0;
        fi.CropW = fi.Width;
        fi.CropH = fi.Height;

        if (testType & CQP) par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        else if (testType & CBR) par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        else if (testType & VBR) par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;

        if (testType & TU1) par.mfx.TargetUsage = MFX_TARGETUSAGE_1;
        else if (testType & TU4) par.mfx.TargetUsage = MFX_TARGETUSAGE_4;
        else if (testType & TU7) par.mfx.TargetUsage = MFX_TARGETUSAGE_7;

        //bool setBitrateAndQP = (testType & REAL_LIFE) == 0;
        bool setBitrateAndQP = true;
        if (setBitrateAndQP)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                par.mfx.QPI = par.mfx.QPP = 100;
            }
            else
            {
                mfxU32 w = extPar.FrameWidth ? extPar.FrameWidth : fi.Width;
                mfxU32 h = extPar.FrameHeight ? extPar.FrameHeight : fi.Height;
                par.mfx.TargetKbps = GetDefaultTargetKbps(Resolution(w, h), fourcc, par.mfx.BRCParamMultiplier);
                if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
                {
                    par.mfx.MaxKbps = static_cast<mfxU16>(par.mfx.TargetKbps * 1.25);
                }
                else
                {
                    par.mfx.MaxKbps = par.mfx.TargetKbps;
                }
                par.mfx.InitialDelayInKB = 0;
                par.mfx.BufferSizeInKB = 0;
            }
        }
        par.AsyncDepth = 1; // TODO: check async operations as well
    }

    class Iteration
    {
    public:
        mfxVideoParam m_param[3];
        mfxExtVP9Param m_extParam[3];
        mfxExtEncoderResetOption m_extResetOpt;
        mfxExtVP9TemporalLayers m_extTL;
        tsRawReader* m_pRawReader;
        mfxU32 m_firstFrame;
        mfxU32 m_numFramesToEncode;

        mfxExtBuffer* m_extBufs[MAX_EXT_BUFFERS][2];

        Iteration(
            const mfxVideoParam& defaults,
            const tc_struct::params_for_iteration& iterPar,
            std::map<mfxU32, mfxFrameSurface1*>* pInSurfaces,
            mfxU32 fourcc,
            mfxU32 type,
            mfxU32 firstFrame,
            std::map<Resolution, std::string>& inputStreams);
    };

    Iteration::Iteration(
        const mfxVideoParam& defaults,
        const tc_struct::params_for_iteration& iterPar,
        std::map<mfxU32, mfxFrameSurface1*>* pInSurfaces,
        mfxU32 fourcc,
        mfxU32 type,
        mfxU32 firstFrame,
        std::map<Resolution, std::string>& inputStreams)
        : m_firstFrame(firstFrame)
        , m_numFramesToEncode(iterPar.num_frames_in_iteration)
    {
        // prepare parameters to "set" encoder configuration
        // e.g. pass it to Query, Init or Reset
        m_param[SET] = defaults;
        SETPARSITER(&m_param[SET], MFX_PAR);
        m_param[SET].NumExtParam = 0;
        m_param[SET].ExtParam = m_extBufs[SET];

        // prepare mfxExtVP9Param
        InitExtBuffer(MFX_EXTBUFF_VP9_PARAM, m_extParam[SET]);
        SETPARSITER(&m_extParam[SET], MFX_EXT_VP9PARAM);
        if (false == IsZeroExtBuf(m_extParam[SET]))
        {
            m_param[SET].ExtParam[m_param[SET].NumExtParam++] = (mfxExtBuffer*)&m_extParam[SET];
        }
        SetAdditionalParams(m_param[SET], m_extParam[SET], fourcc, type);

        // prepare mfxEXtVP9TemporalLayers
        InitExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS, m_extTL);
        SETPARSITER(&m_extTL, MFX_EXT_VP9_TL);
        if (false == IsZeroExtBuf(m_extTL))
        {
            m_param[SET].ExtParam[m_param[SET].NumExtParam++] = (mfxExtBuffer*)&m_extTL;
        }

        // prepare mfxExtEncoderResetOption
        InitExtBuffer(MFX_EXTBUFF_ENCODER_RESET_OPTION, m_extResetOpt);
        SETPARSITER(&m_extResetOpt, MFX_EXT_RESET_OPTION);
        if (false == IsZeroExtBuf(m_extResetOpt))
        {
            m_param[SET].ExtParam[m_param[SET].NumExtParam++] = (mfxExtBuffer*)&m_extResetOpt;
        }

        // prepare parameters to "get" encoder configuration
        // e.g. from Query or GetVideoParam calls
        Zero(m_param[GET]);
        m_param[GET].mfx.CodecId = m_param[SET].mfx.CodecId;
        m_param[GET].ExtParam = m_extBufs[GET];
        InitExtBuffer(MFX_EXTBUFF_VP9_PARAM, m_extParam[GET]);
        m_param[GET].ExtParam[m_param[GET].NumExtParam++] = (mfxExtBuffer*)&m_extParam[GET];

        // prepare parameters to "check"
        // these parameters are equal to "set" except ones which were changed explicitly
        m_param[CHECK] = m_param[SET];
        m_param[CHECK].NumExtParam = 0; // no need in attaching ext buffers for "check" mfxVideoParam
        m_extParam[CHECK] = m_extParam[SET];

        // prepare m_filler
        const mfxFrameInfo& fi = m_param[SET].mfx.FrameInfo;
        tsRawReader* feeder = new SurfaceFeeder(
            pInSurfaces,
            m_param[SET],
            m_firstFrame,
            inputStreams[Resolution(fi.Width, fi.Height)].c_str(),
            fi);
        feeder->m_disable_shift_hack = true; // this hack adds shift if fi.Shift != 0!!! Need to disable it.
        m_pRawReader = feeder;
    }

    mfxU16 GetCodecProfile(mfxU32 fourcc)
    {
        switch (fourcc)
        {
        case MFX_FOURCC_NV12: return MFX_PROFILE_VP9_0;
        case MFX_FOURCC_AYUV: return MFX_PROFILE_VP9_1;
        case MFX_FOURCC_P010: return MFX_PROFILE_VP9_2;
        case MFX_FOURCC_Y410: return MFX_PROFILE_VP9_3;
        default: return 0;
        }
    }

    struct FindIterByFrameIdx
    {
        FindIterByFrameIdx(mfxU32 frameIdx) : m_frameIdx(frameIdx) {}

        bool operator ()(Iteration const * iter)
        {
            return m_frameIdx >= iter->m_firstFrame &&
                m_frameIdx < iter->m_firstFrame + iter->m_numFramesToEncode;
        }

        mfxU32 m_frameIdx;
    };

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        std::vector<Iteration*>* m_pIterations;
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        mfxU32 m_testType;
        mfxU32 m_testId;
        mfxU8 m_dpbSlotsForPrevFrame;
    public:
        BitstreamChecker(
            std::vector<Iteration*>* pIterations,
            std::map<mfxU32, mfxFrameSurface1*>* pSurfaces,
            mfxU32 testType,
            mfxU32 testId)
            : tsVideoDecoder(MFX_CODEC_VP9)
            , m_pIterations(pIterations)
            , m_pInputSurfaces(pSurfaces)
            , m_testType(testType)
            , m_testId(testId)
            , m_dpbSlotsForPrevFrame(0)
        {
            const mfxFrameInfo& targetFi = (*m_pIterations)[0]->m_param[CHECK].mfx.FrameInfo;
            mfxU32 w = targetFi.Width;
            mfxU32 h = targetFi.Height;

            mfxFrameInfo& fi = m_pPar->mfx.FrameInfo;
            mfxInfoMFX& mfx = m_pPar->mfx;
            fi.AspectRatioW = fi.AspectRatioH = 1;
            fi.Width = fi.CropW = w;
            fi.Height = fi.CropH = h;
            fi.FourCC = targetFi.FourCC;
            fi.ChromaFormat = targetFi.ChromaFormat;
            fi.BitDepthLuma = targetFi.BitDepthLuma;
            fi.BitDepthChroma = targetFi.BitDepthChroma;
            fi.Shift = targetFi.Shift;
            mfx.CodecProfile = GetCodecProfile(fi.FourCC);
            mfx.CodecLevel = 0;
            m_pPar->AsyncDepth = 1;
            m_par_set = true;

            if (m_pInputSurfaces)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                AllocSurfaces();
                Init();
            }
        };
        ~BitstreamChecker() {}
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    /*void  DumpCodedFrame(const mfxBitstream& bs, mfxU32 testId)
    {
        // Dump stream to file
        const int encoded_size = bs.DataLength;
        char fileName[100];
        sprintf(fileName, "vp9e_encoded_ds_%d.vp9", testId);
        FILE *fp_vp9 = fopen(fileName, "ab");
        fwrite(bs.Data, encoded_size, 1, fp_vp9);
        fclose(fp_vp9);
    }*/

    /*void DumpDecodedFrame(mfxU8* pY, mfxU8* pUV, mfxU32 w, mfxU32 h, mfxU32 pitch)
    {
        // Dump decoded frame to file
        static FILE *fp_yuv = fopen("vp9e_decoded_ds.yuv", "wb");
        for (mfxU32 i = 0; i < h; i ++)
        {
            fwrite(pY, w, 1, fp_yuv);
            pY += pitch;
        }
        for (mfxU32 i = 0; i < h / 2; i++)
        {
            fwrite(pUV, w, 1, fp_yuv);
            pUV += pitch;
        }
        fflush(fp_yuv);
    }*/

    mfxF64 GetMinPSNR(const mfxVideoParam& par)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            if (par.mfx.QPI <= 50)
            {
                return 40.0;
            }
            if (par.mfx.QPI <= 120)
            {
                return 30.0;
            }

            return 20.0;
        }
        else
        {
            return 30.0;
        }
    }

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        nFrames;

        //DumpCodedFrame(bs, m_testId);

        SetBuffer(bs);

        // parse uncompressed header and check values
        tsParserVP9::UnitType& hdr = ParseOrDie();
        std::vector<Iteration*>::iterator curIter =
            std::find_if(m_pIterations->begin(), m_pIterations->end(), FindIterByFrameIdx(hdr.FrameOrder));

        const Iteration& iter = **curIter;
        const mfxExtVP9Param& extParToCheck = iter.m_extParam[CHECK];

        mfxU32 expectedWidth = extParToCheck.FrameWidth;
        if (hdr.uh.width != expectedWidth)
        {
            ADD_FAILURE() << "ERROR: frame_width_minus_1 in uncompressed header of frame " << hdr.FrameOrder << " is incorrect: " << hdr.uh.width - 1
                << ", expected " << expectedWidth - 1; throw tsFAIL;
        }

        mfxU32 expectedHeight = extParToCheck.FrameHeight;
        if (hdr.uh.height != expectedHeight)
        {
            ADD_FAILURE() << "ERROR: frame_height_minus_1 in uncompressed header of frame " << hdr.FrameOrder << " is incorrect: " << hdr.uh.height - 1
                << ", expected " << expectedHeight - 1; throw tsFAIL;
        }

        if (hdr.FrameOrder && hdr.FrameOrder == iter.m_firstFrame)
        {
            // new iteration just started - need to check all borderline conditions
            if (curIter == m_pIterations->begin())
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            std::vector<Iteration*>::iterator prevIter = curIter - 1;
            const mfxExtVP9Param& prevExtPar = (*prevIter)->m_extParam[CHECK];

            if (m_testType & KEY_FRAME)
            {
                // check that kew-frame is inserted together with resolution change
                if (hdr.uh.frame_type != 0)
                {
                    ADD_FAILURE() << "ERROR: Frame " << hdr.FrameOrder << "isn't key-frame";
                    throw tsFAIL;
                }
            }
            else
            {
                if (prevExtPar.FrameWidth != extParToCheck.FrameWidth ||
                    prevExtPar.FrameHeight != extParToCheck.FrameHeight)
                {
                    // check that no key-frame inserted at the place of resolution change
                    if (hdr.uh.frame_type != 1)
                    {
                        ADD_FAILURE() << "ERROR: Frame " << hdr.FrameOrder << "is key-frame, but there should be no key-frame for dynamic scaling";
                        throw tsFAIL;
                    }

                    // check that reference structure is reset at the place of resolution change
                    if (hdr.uh.refresh_frame_flags != 0xff)
                    {
                        ADD_FAILURE() << "ERROR: reference structure wasn't reset at frame " << hdr.FrameOrder;
                        throw tsFAIL;
                    }

                    //check that immediate prev frame is used as single ref for 1st frame with new resolution
                    mfxU8 lastRefIdx = hdr.uh.ref[0].active_ref_idx;
                    bool isSingleRef = (lastRefIdx == hdr.uh.ref[1].active_ref_idx) &&
                        (lastRefIdx == hdr.uh.ref[2].active_ref_idx);
                    bool isPrevFrame = ((1 << lastRefIdx) & m_dpbSlotsForPrevFrame) != 0;
                    if (!(isSingleRef && isPrevFrame))
                    {
                        ADD_FAILURE() << "ERROR: first frame after resolution change (" << hdr.FrameOrder << ") doesn't use immediate previous frame as single reference";
                        throw tsFAIL;
                    }

                    for (mfxU8 i = 0; i < MAX_REFS; i++)
                    {
                        if (hdr.uh.ref[i].size_from_ref == 1)
                        {
                            ADD_FAILURE() << "ERROR: first frame after resolution change (" << hdr.FrameOrder << ") re-uses resolution of reference frame " << i;
                            throw tsFAIL;
                        }
                    }
                }
            }
        }

        m_dpbSlotsForPrevFrame = hdr.uh.refresh_frame_flags;

        // decode frame and calculate PSNR for Y plane
        if (m_pInputSurfaces)
        {
            const mfxU32 ivfSize = hdr.FrameOrder == 0 ? MAX_IVF_HEADER_SIZE : IVF_PIC_HEADER_SIZE_BYTES;

            m_pBitstream->Data = bs.Data + ivfSize;
            m_pBitstream->DataOffset = 0;
            m_pBitstream->DataLength = bs.DataLength - ivfSize;
            m_pBitstream->MaxLength = bs.MaxLength;

            mfxStatus sts = MFX_ERR_NONE;
            do
            {
                sts = DecodeFrameAsync();
            } while (sts == MFX_WRN_DEVICE_BUSY);

            if (sts < 0)
            {
                ADD_FAILURE() << "ERROR: DecodeFrameAsync for frame " << hdr.FrameOrder << " failed with status: " << sts;
                throw tsFAIL;
            }

            sts = SyncOperation();
            if (sts < 0)
            {
                ADD_FAILURE() << "ERROR: SyncOperation for frame " << hdr.FrameOrder << " failed with status: " << sts;
                throw tsFAIL;
            }

            // frame is decoded correctly - let's calculate and check PSNR for Y plane

            // check that respective source frame is stored
            if (m_pInputSurfaces->find(hdr.FrameOrder) == m_pInputSurfaces->end())
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            mfxU32 w = iter.m_extParam[CHECK].FrameWidth ?
                iter.m_extParam[CHECK].FrameWidth : iter.m_param[CHECK].mfx.FrameInfo.Width;
            mfxU32 h = iter.m_extParam[CHECK].FrameHeight ?
                iter.m_extParam[CHECK].FrameHeight : iter.m_param[CHECK].mfx.FrameInfo.Height;
            //mfxU32 pitch = (m_pSurf->Data.PitchHigh << 16) + m_pSurf->Data.PitchLow;
            //DumpDecodedFrame(m_pSurf->Data.Y, m_pSurf->Data.UV, w, h, pitch);

            mfxFrameSurface1* pInputSurface = (*m_pInputSurfaces)[hdr.FrameOrder];
            tsFrame src = tsFrame(*pInputSurface);
            tsFrame res = tsFrame(*m_pSurf);
            src.m_info.CropX = src.m_info.CropY = res.m_info.CropX = res.m_info.CropY = 0;
            src.m_info.CropW = res.m_info.CropW = w;
            src.m_info.CropH = res.m_info.CropH = h;

            const mfxF64 psnrY = PSNR(src, res, 0);
            const mfxF64 psnrU = PSNR(src, res, 1);
            const mfxF64 psnrV = PSNR(src, res, 2);
            pInputSurface->Data.Locked--;
            m_pInputSurfaces->erase(hdr.FrameOrder);
            const mfxF64 minPsnr = GetMinPSNR(iter.m_param[CHECK]);

            g_tsLog << "INFO: frame[" << hdr.FrameOrder << "]: PSNR-Y=" << psnrY << " PSNR-U="
                << psnrU << " PSNR-V=" << psnrV << " size=" << bs.DataLength << "\n";

            if (psnrY < minPsnr)
            {
                ADD_FAILURE() << "ERROR: PSNR-Y of frame " << hdr.FrameOrder << " is equal to " << psnrY << " and lower than threshold: " << minPsnr;
                throw tsFAIL;
            }
            if (psnrU < minPsnr)
            {
                ADD_FAILURE() << "ERROR: PSNR-U of frame " << hdr.FrameOrder << " is equal to " << psnrU << " and lower than threshold: " << minPsnr;
                throw tsFAIL;
            }
            if (psnrV < minPsnr)
            {
                ADD_FAILURE() << "ERROR: PSNR-V of frame " << hdr.FrameOrder << " is equal to " << psnrV << " and lower than threshold: " << minPsnr;
                throw tsFAIL;
            }
        }

        bs.DataLength = bs.DataOffset = 0;

        return MFX_ERR_NONE;
    }

    mfxU32 CalcIterations(const tc_struct& tc, mfxU32& totalFramesToEncode)
    {
        // calculate number of iterations set by tc_struct
        mfxU32 numIterations = 0;
        totalFramesToEncode = 0;
        while (false == IsZero(tc.iteration_par[numIterations]) &&
            numIterations < MAX_ITERATIONS)
        {
            totalFramesToEncode += tc.iteration_par[numIterations].num_frames_in_iteration;
            numIterations++;
        }

        return numIterations;
    }

    void CheckOutputParams(const std::string& function, const Iteration& iter)
    {
        const mfxExtVP9Param& expected = iter.m_extParam[CHECK];
        const mfxExtVP9Param& real = iter.m_extParam[GET];
        if (expected.FrameWidth != 0 && expected.FrameWidth != real.FrameWidth)
        {
            ADD_FAILURE() << "ERROR: function " << function << " returned incorrect value for parameter FrameWidth: " << real.FrameWidth <<
                ", expected " << expected.FrameWidth; throw tsFAIL;
        }
        if (expected.FrameHeight != 0 && expected.FrameHeight != real.FrameHeight)
        {
            ADD_FAILURE() << "ERROR: function " << function << " returned incorrect value for parameter FrameHeight: " << real.FrameHeight <<
                ", expected " << expected.FrameHeight; throw tsFAIL;
        }
    }

    bool NeedToRun(mfxU32 testType)
    {
        bool needToRun = true;

        mfxU32 allowedSubTypes = 0;
        for (std::vector<TestSubtype>::const_iterator subtype = executeTests.begin(); subtype != executeTests.end(); subtype++)
        {
            allowedSubTypes |= *subtype;
        }

        if (testType & ~allowedSubTypes)
        {
            needToRun = false;
        }

        return needToRun;
    }

    template <mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_cases[id];
        return RunTest(tc, fourcc, id);
    }

    int TestSuite::RunTest(const tc_struct& tc, mfxU32 fourcc, const unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_cases[id];

        if (false == NeedToRun(tc.type))
        {
            return 0;
        }

        if ((fourcc == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
            || ((fourcc == MFX_FOURCC_P010 || fourcc == MFX_FOURCC_AYUV
                || fourcc == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            mfxStatus sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
            g_tsStatus.check(sts);
            return 0;
        }

        // prepare pool of input streams
        std::map<Resolution, std::string> inputStreams;
        if (fourcc == MFX_FOURCC_NV12)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/salesman_176x144_50.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/salesman_352x288_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/salesman_704x576_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/salesman_1408X1152_50.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/salesman_2816x2304_50.yuv"));
            inputStreams.emplace(Resolution(640, 368), g_tsStreamPool.Get("forBehaviorTest/park_scene_640x368_30_content_640x360.yuv"));
            inputStreams.emplace(Resolution(960, 544), g_tsStreamPool.Get("forBehaviorTest/park_scene_960x544_30_content_960x540.yuv"));
            inputStreams.emplace(Resolution(1280, 720), g_tsStreamPool.Get("forBehaviorTest/park_scene_1280x720_30.yuv"));
            inputStreams.emplace(Resolution(1440, 816), g_tsStreamPool.Get("forBehaviorTest/park_scene_1440x816_30_content_1440x810.yuv"));
            inputStreams.emplace(Resolution(1920, 1088), g_tsStreamPool.Get("forBehaviorTest/park_scene_1920x1088_30_content_1920x1080.yuv"));
        }
        else if (fourcc == MFX_FOURCC_AYUV)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_ayuv.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_352x288_24_ayuv_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/Kimono1_704x576_24_ayuv_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1408X1152_24_ayuv_10.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/Kimono1_2816x2304_24_ayuv_10.yuv"));
            inputStreams.emplace(Resolution(640, 368), g_tsStreamPool.Get("forBehaviorTest/Kimono1_640x368_24_ayuv_30_content_640x360.yuv"));
            inputStreams.emplace(Resolution(960, 544), g_tsStreamPool.Get("forBehaviorTest/Kimono1_960x544_24_ayuv_30_content_960x540.yuv"));
            inputStreams.emplace(Resolution(1280, 720), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1280x720_24_ayuv_30.yuv"));
            inputStreams.emplace(Resolution(1440, 816), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1440x816_24_ayuv_30_content_1440x810.yuv"));
            inputStreams.emplace(Resolution(1920, 1088), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1920x1088_24_ayuv_30_content_1920x1080.yuv"));
        }
        else if (fourcc == MFX_FOURCC_P010)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_p010_shifted.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_352x288_24_p010_shifted_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/Kimono1_704x576_24_p010_shifted_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1408X1152_24_p010_shifted_10.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/Kimono1_2816x2304_24_p010_shifted_10.yuv"));
            inputStreams.emplace(Resolution(640, 368), g_tsStreamPool.Get("forBehaviorTest/Kimono1_640x368_24_p010_shifted_30_content_640x360.yuv"));
            inputStreams.emplace(Resolution(960, 544), g_tsStreamPool.Get("forBehaviorTest/Kimono1_960x544_24_p010_shifted_30_content_960x540.yuv"));
            inputStreams.emplace(Resolution(1280, 720), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1280x720_24_p010_shifted_30.yuv"));
            inputStreams.emplace(Resolution(1440, 816), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1440x816_24_p010_shifted_30_content_1440x810.yuv"));
            inputStreams.emplace(Resolution(1920, 1088), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1920x1088_24_p010_shifted_30_content_1920x1080.yuv"));
        }
        else if (fourcc == MFX_FOURCC_Y410)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_y410.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_352x288_24_y410_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/Kimono1_704x576_24_y410_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1408X1152_24_y410_10.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/Kimono1_2816x2304_24_y410_10.yuv"));
            inputStreams.emplace(Resolution(640, 368), g_tsStreamPool.Get("forBehaviorTest/Kimono1_640x368_24_y410_30_content_640x360.yuv"));
            inputStreams.emplace(Resolution(960, 544), g_tsStreamPool.Get("forBehaviorTest/Kimono1_960x544_24_y410_30_content_960x540.yuv"));
            inputStreams.emplace(Resolution(1280, 720), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1280x720_24_y410_30.yuv"));
            inputStreams.emplace(Resolution(1440, 816), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1440x816_24_y410_30_content_1440x810.yuv"));
            inputStreams.emplace(Resolution(1920, 1088), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1920x1088_24_y410_30_content_1920x1080.yuv"));
        }
        g_tsStreamPool.Reg();

        bool errorExpected = (tc.type & ERROR_EXPECTED) != 0;

        std::vector<Iteration*> iterations;

        // prepare all iterations for the test
        mfxU32 totalFramesToEncode = 0;
        const mfxU32 numIterations = CalcIterations(tc, totalFramesToEncode);
        mfxU32 iterationStart = 0;

        std::map<mfxU32, mfxFrameSurface1*> inputSurfaces;
        std::map<mfxU32, mfxFrameSurface1*>* pInputSurfaces = totalFramesToEncode ?
            &inputSurfaces : 0;

        for (mfxU8 idx = 0; idx < numIterations; idx++)
        {
            const mfxVideoParam& defaults = (idx == 0) ? *m_pPar : iterations[idx - 1]->m_param[CHECK];
            Iteration* pIter = new Iteration(
                defaults,
                tc.iteration_par[idx],
                pInputSurfaces,
                fourcc, tc.type,
                iterationStart,
                inputStreams);

            iterationStart += pIter->m_numFramesToEncode;
            iterations.push_back(pIter);
        }

        // run the test
        MFXInit(); TS_CHECK_MFX;
        Load();

        // prepare bitstream checker
        BitstreamChecker bs(&iterations, pInputSurfaces, tc.type, id);
        m_bs_processor = &bs;

        *m_pPar = iterations[0]->m_param[SET];
        mfxVideoParam* pOutPar = &iterations[0]->m_param[GET];

        Init();

        g_tsStatus.expect(MFX_ERR_NONE);
        TRACE_FUNC2(MFXVideoENCODE_GetVideoParam, m_session, pOutPar);
        mfxStatus sts = MFXVideoENCODE_GetVideoParam(m_session, pOutPar);
        TS_TRACE(pOutPar);
        g_tsStatus.check(sts);
        CheckOutputParams("GetVideoParam", *iterations[0]);

        if (totalFramesToEncode)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            AllocSurfaces(); TS_CHECK_MFX;
            AllocBitstream(); TS_CHECK_MFX;
        }

        if (iterations[0]->m_numFramesToEncode)
        {
            m_filler = iterations[0]->m_pRawReader;
            EncodeFrames(iterations[0]->m_numFramesToEncode);
        }

        for (mfxU8 idx = 1; idx < numIterations; idx++)
        {
            if (errorExpected && idx == (numIterations - 1))
            {
                // expected error should happened on the last reset
                g_tsStatus.expect(tc.sts);
            }
            else
            {
                g_tsStatus.expect(MFX_ERR_NONE);
            }
            *m_pPar = iterations[idx]->m_param[SET];
            m_filler = iterations[idx]->m_pRawReader;

            m_pPar->mfx.FrameInfo.CropW = m_pPar->mfx.FrameInfo.Width;
            m_pPar->mfx.FrameInfo.CropH = m_pPar->mfx.FrameInfo.Height;

            mfxStatus sts = Reset();
            if (sts < 0)
            {
                // Reset returned error which was expected by the test
                // test is passed - need to stop encoding and exit
                break;
            }

            pOutPar = &iterations[idx]->m_param[GET];
            TRACE_FUNC2(MFXVideoENCODE_GetVideoParam, m_session, pOutPar);
            sts = MFXVideoENCODE_GetVideoParam(m_session, pOutPar);
            TS_TRACE(pOutPar);
            CheckOutputParams("Reset", *iterations[idx]);

            if (iterations[idx]->m_numFramesToEncode)
            {
                EncodeFrames(iterations[idx]->m_numFramesToEncode);
            }
        }

        g_tsStatus.expect(MFX_ERR_NONE);
        Close();


        for (std::vector<Iteration*>::iterator i = iterations.begin(); i != iterations.end(); i++)
        {
            if (*i != 0)
            {
                if ((*i)->m_pRawReader != 0)
                {
                    delete (*i)->m_pRawReader;
                }
                delete *i;
                *i = 0;
            }
        }
        iterations.clear();

        return 0;
        TS_END;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_dynamic_scaling, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_dynamic_scaling, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_dynamic_scaling, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases_ayuv);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_dynamic_scaling, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
}

