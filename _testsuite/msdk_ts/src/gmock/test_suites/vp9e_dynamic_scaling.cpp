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

#define SETPARSEXT(p, type, idx)                                                                                                                                                            \
for(mfxU32 i = 0; i < MAX_NPARS; i++)                                                                                                                                                       \
{                                                                                                                                                                                           \
    if(tc.iteration_par[idx].set_par[i].f && tc.iteration_par[idx].set_par[i].ext_type == type)                                                                                             \
    {                                                                                                                                                                                       \
        SetParam(p, tc.iteration_par[idx].set_par[i].f->name, tc.iteration_par[idx].set_par[i].f->offset, tc.iteration_par[idx].set_par[i].f->size, tc.iteration_par[idx].set_par[i].v);    \
    }                                                                                                                                                                                       \
}

#define MAX_ITERATIONS 10
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

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_VP9)
        {}
        ~TestSuite()
        {
        }

        int RunTest(unsigned int id);

    private:
        static const tc_struct test_case[];
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
        // below cases do single 2x upscale
        {/*09*/ MFX_ERR_NONE, CQP | TU1 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        {/*10*/ MFX_ERR_NONE, CQP | TU4 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        {/*11*/ MFX_ERR_NONE, CQP | TU7 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
         {/*12*/ MFX_ERR_NONE, CBR | TU1 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        {/*13*/ MFX_ERR_NONE, CBR | TU4 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        {/*14*/ MFX_ERR_NONE, CBR | TU7 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
         {/*15*/ MFX_ERR_NONE, VBR | TU1 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        {/*16*/ MFX_ERR_NONE, VBR | TU4 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        {/*17*/ MFX_ERR_NONE, VBR | TU7 | SIMPLE_UPSCALE,
            {
                { 0, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) }
            },
        },
        // below cases do single maximum allowed upscale (16x)
        {/*18*/ MFX_ERR_NONE, CQP | MAX_SCALES,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
            },
        },
        {/*19*/ MFX_ERR_NONE, CBR | MAX_SCALES,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
            },
        },
        {/*20*/ MFX_ERR_NONE, VBR | MAX_SCALES,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
            },
        },
        // below cases do consequent maximum allowed downscales (2x) till 16x accumulated downscale is reached
        // and then do maximum allowed upscale (16x)
        {/*21*/ MFX_ERR_NONE, CQP | MAX_SCALES,
            {
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
            },
        },
        {/*22*/ MFX_ERR_NONE, CBR | MAX_SCALES,
            {
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) },
            },
        },
        {/*23*/ MFX_ERR_NONE, VBR | MAX_SCALES,
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
        {/*24*/ MFX_ERR_NONE, CQP | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
            },
        },
        {/*25*/ MFX_ERR_NONE, CBR | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
            },
        },
        {/*26*/ MFX_ERR_NONE, VBR | DS_ON_EACH_FRAME,
            {
                { 1, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(176, 144) },
            },
        },
        // below cases do consequent 2x upscales on each frame
        {/*27*/ MFX_ERR_NONE, CQP | DS_ON_EACH_FRAME,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(176, 144) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(704, 576) },
            },
        },
        {/*28*/ MFX_ERR_NONE, CBR | DS_ON_EACH_FRAME,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(176, 144) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(704, 576) },
            },
        },
        {/*29*/ MFX_ERR_NONE, VBR | DS_ON_EACH_FRAME,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { 1, SET_RESOLUTION(176, 144) },
                { 1, SET_RESOLUTION(352, 288) },
                { 1, SET_RESOLUTION(704, 576) },
            },
        },
        // below cases interleave various downscales and upscales
        {/*30*/ MFX_ERR_NONE, CQP | UPSCALE_DOWNSCALE_INTERLEAVED,
            {
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
            },
        },
        {/*31*/ MFX_ERR_NONE, CBR | UPSCALE_DOWNSCALE_INTERLEAVED,
            {
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
                { ITER_LENGTH, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, SET_RESOLUTION(1408, 1152) },
            },
        },
        {/*32*/ MFX_ERR_NONE, VBR | UPSCALE_DOWNSCALE_INTERLEAVED,
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
        {/*33*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF,
            {
                { 4,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                     SET_RESOLUTION(352, 288) } },
                { 4, SET_RESOLUTION(176, 144) },
                { 4, SET_RESOLUTION(352, 288) },
            },
        },
        {/*34*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF,
            {
                { 5,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                     SET_RESOLUTION(352, 288) } },
                { 5, SET_RESOLUTION(176, 144) },
                { 5, SET_RESOLUTION(352, 288) },
            },
        },
        {/*35*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF,
            {
                { 6,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3,
                     SET_RESOLUTION(352, 288) } },
                { 6, SET_RESOLUTION(176, 144) },
                { 6, SET_RESOLUTION(352, 288) },
            },
        },
        {/*36*/ MFX_ERR_NONE, CQP | TU4 | MULTIREF,
            {
                { 3,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2,
                     SET_RESOLUTION(352, 288) } },
                { 3, SET_RESOLUTION(176, 144) },
                { 3, SET_RESOLUTION(352, 288) },
            },
        },
        {/*37*/ MFX_ERR_NONE, CQP | TU4 | MULTIREF,
            {
                { 4,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2,
                     SET_RESOLUTION(352, 288) } },
                { 4, SET_RESOLUTION(176, 144) },
                { 4, SET_RESOLUTION(352, 288) },
            },
        },
        // below cases do resolution change with key-frame
        {/*38*/ MFX_ERR_NONE, CQP| KEY_FRAME,
            {
                { GOP_SIZE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, GOP_SIZE,
                              SET_RESOLUTION(352, 288) } },
                { ITER_LENGTH, SET_RESOLUTION(176, 144) }
            },
        },
        {/*39*/ MFX_ERR_NONE, CQP | KEY_FRAME,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH,{ MFX_EXT_RESET_OPTION, &tsStruct::mfxExtEncoderResetOption.StartNewSequence, MFX_CODINGOPTION_ON,
                                SET_RESOLUTION(176, 144) } }
            },
        },
        // below cases do single downscale and then single uscale from and to resolutions aligned to 16 but not equal to surface size
        {/*40*/ MFX_ERR_NONE, CQP | TU1 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*41*/ MFX_ERR_NONE, CQP | TU4 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*42*/ MFX_ERR_NONE, CQP | TU7 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*43*/ MFX_ERR_NONE, CBR | TU1 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*44*/ MFX_ERR_NONE, CBR | TU4 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*45*/ MFX_ERR_NONE, CBR | TU7 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*46*/ MFX_ERR_NONE, VBR | TU1 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*47*/ MFX_ERR_NONE, VBR | TU4 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        {/*48*/ MFX_ERR_NONE, VBR | TU7 | NOT_EQUAL_TO_SURF_SIZE,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352), FRAME_HEIGHT(240) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(120) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704), FRAME_HEIGHT(480) }
            },
        },
        // below cases do single downscale and then single uscale from and to resolutions not aligned to 16
        // without change of Aspect Ratio (DAR)
        {/*49*/ MFX_ERR_NONE, CQP | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*50*/ MFX_ERR_NONE, CQP | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*51*/ MFX_ERR_NONE, CQP | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*52*/ MFX_ERR_NONE, CBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*53*/ MFX_ERR_NONE, CBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*54*/ MFX_ERR_NONE, CBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*55*/ MFX_ERR_NONE, VBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*56*/ MFX_ERR_NONE, VBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        {/*57*/ MFX_ERR_NONE, VBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-20), FRAME_HEIGHT(288-20) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-40), FRAME_HEIGHT(576-40) }
            },
        },
        // below cases do single downscale and then single uscale from and to resolutions not aligned to 16
        // with change of Aspect Ratio (DAR)
        {/*58*/ MFX_ERR_NONE, CQP | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*59*/ MFX_ERR_NONE, CQP | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*60*/ MFX_ERR_NONE, CQP | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*61*/ MFX_ERR_NONE, CBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*62*/ MFX_ERR_NONE, CBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*63*/ MFX_ERR_NONE, CBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*64*/ MFX_ERR_NONE, VBR | TU1 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*65*/ MFX_ERR_NONE, VBR | TU4 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        {/*66*/ MFX_ERR_NONE, VBR | TU7 | NOT_EQUAL_TO_SURF_SIZE | NOT_ALIGNED | CHANGE_OF_ASPECT_RATIO,
            {
                { 0, SET_RESOLUTION(704, 576) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(352-30), FRAME_HEIGHT(288-30) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-10), FRAME_HEIGHT(144-10) },
                { ITER_LENGTH, WIDTH(704), HEIGHT(576), FRAME_WIDTH(704-50), FRAME_HEIGHT(576-50) }
            },
        },
        // below cases do too aggressive downscale (>2x) to check that correct error is returned
        {/*67*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(142) }
            },
        },
        {/*68*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(174), FRAME_HEIGHT(144) }
            },
        },
        // below cases do too aggressive upscale (>16x) to check that correct error is returned
        {/*69*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(142) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
            },
        },
        {/*70*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(174), FRAME_HEIGHT(144) },
                { ITER_LENGTH, SET_RESOLUTION(2816, 2304) }
            },
        },
        // below cases do scaling to resolution change not aligned to 2 (not aligned to chroma)
        // TODO: make proper changes for REXTs with different chroma samplings
        {/*71*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176-1), FRAME_HEIGHT(144) }
            },
        },
        {/*72*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(144-1) }
            },
        },
        // below cases do scaling to properly aligned resolutions which are bigger than size of current surface
        {/*73*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176+2), FRAME_HEIGHT(144) }
            },
        },
        {/*74*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(352, 288) },
                { ITER_LENGTH, WIDTH(176), HEIGHT(144), FRAME_WIDTH(176), FRAME_HEIGHT(144+2) }
            },
        },
        // below cases do scaling to properly aligned resolutions which are bigger than size of initial surface
        {/*75*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(176+2), FRAME_HEIGHT(144) }
            },
        },
        {/*76*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, CQP | ERROR_EXPECTED,
            {
                { ITER_LENGTH, SET_RESOLUTION(176, 144) },
                { ITER_LENGTH, WIDTH(352), HEIGHT(288), FRAME_WIDTH(176), FRAME_HEIGHT(144+2) }
            },
        },
        // case below tryes to use dynamic scaling together with temporal scalability
        {/*77*/ MFX_ERR_INVALID_VIDEO_PARAM, CQP | ERROR_EXPECTED,
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
        {/*78*/ MFX_ERR_NONE, CQP | TU1 | MULTIREF | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304),
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
        {/*79*/ MFX_ERR_NONE, CBR | TU1 | MULTIREF | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304),
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
        {/*80*/ MFX_ERR_NONE, VBR | TU1 | MULTIREF | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304),
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
        {/*81*/ MFX_ERR_NONE, CQP | TU4 | MULTIREF | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304),
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
        {/*82*/ MFX_ERR_NONE, CBR | TU4 | MULTIREF | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304),
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
        {/*83*/ MFX_ERR_NONE, VBR | TU4 | MULTIREF | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304),
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
        {/*84*/ MFX_ERR_NONE, CQP | TU7 | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
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
        {/*85*/ MFX_ERR_NONE, CBR | TU7 | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
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
        {/*86*/ MFX_ERR_NONE, VBR | TU7 | STRESS,
            {
                { 0, SET_RESOLUTION(2816, 2304) },
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
        {/*87*/ MFX_ERR_NONE, CQP | TU1 | REAL_LIFE,
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
        {/*88*/ MFX_ERR_NONE, CQP | TU1 | REAL_LIFE,
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
        {/*89*/ MFX_ERR_NONE, CQP | TU1 | REAL_LIFE,
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
        {/*90*/ MFX_ERR_NONE, CQP | TU4 | REAL_LIFE,
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
        {/*91*/ MFX_ERR_NONE, CQP | TU4 | REAL_LIFE,
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
        {/*92*/ MFX_ERR_NONE, CQP | TU4 | REAL_LIFE,
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
        {/*93*/ MFX_ERR_NONE, CQP | TU7 | REAL_LIFE,
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
        {/*94*/ MFX_ERR_NONE, CQP | TU7 | REAL_LIFE,
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
        {/*95*/ MFX_ERR_NONE, CQP | TU7 | REAL_LIFE,
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
    };

    const unsigned int TestSuite::n_cases = sizeof(test_cases) / sizeof(tc_struct);

    class SurfaceFeeder : public tsRawReader
    {
        std::map<mfxU32, mfxU8*>* m_pInputFrames;
        mfxU32 m_frameOrder;
        mfxU32 m_frameWidth;
        mfxU32 m_frameHeight;
    public:
        SurfaceFeeder(
            std::map<mfxU32, mfxU8*>* pSurfaces,
            mfxU32 actualFrameWidth,
            mfxU32 actualFrameHeight,
            mfxU32 frameOrder,
            const char* fname,
            mfxFrameInfo fi,
            mfxU32 n_frames = 0xFFFFFFFF)
            : tsRawReader(fname, fi, n_frames)
            , m_pInputFrames(pSurfaces)
            , m_frameOrder(frameOrder)
            , m_frameWidth(actualFrameWidth)
            , m_frameHeight(actualFrameHeight) {};

        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
    };

    mfxFrameSurface1* SurfaceFeeder::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        mfxFrameSurface1* pSurf = tsSurfaceProcessor::ProcessSurface(ps, pfa);
        if (m_pInputFrames)
        {
            mfxU32 w = m_frameWidth;
            mfxU32 h = m_frameHeight;
            mfxU32 pitch = (pSurf->Data.PitchHigh << 16) + pSurf->Data.PitchLow;

            mfxU8* pBuf = new mfxU8[w * h];
            for (mfxU32 i = 0; i < h; i++)
            {
                memcpy(pBuf + i * w, pSurf->Data.Y + i * pitch, w);
            }
            (*m_pInputFrames)[m_frameOrder] = pBuf;
        }
        m_frameOrder++;

        return pSurf;
    }

    struct Iteration
    {
        mfxVideoParam m_param;
        mfxExtVP9Param m_extParam;
        mfxExtEncoderResetOption m_extResetOpt;
        mfxExtVP9TemporalLayers m_extTempLayers;
        tsRawReader* m_pRawReader;
        mfxU32 m_firstFrame;
        mfxU32 m_numFramesToEncode;
    };

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        std::vector<Iteration>* m_pIterations;
        std::map<mfxU32, mfxU8*>* m_pInputFrames;
        mfxU32 m_testType;
        mfxU32 m_testId;
        mfxU8* m_pDecodedFrame;
        mfxU8 m_dpbSlotsForPrevFrame;
    public:
        BitstreamChecker(
            std::vector<Iteration>* pIterations,
            std::map<mfxU32, mfxU8*>* pSurfaces,
            mfxU32 testType,
            mfxU32 testId)
            : tsVideoDecoder(MFX_CODEC_VP9)
            , m_pIterations(pIterations)
            , m_pInputFrames(pSurfaces)
            , m_testType(testType)
            , m_testId(testId)
            , m_dpbSlotsForPrevFrame(0)
        {
            // first iteration always has biggest possible resolution
            // we use it to allocate buffers to store decoded frame
            mfxU32 w = (*m_pIterations)[0].m_param.mfx.FrameInfo.Width;
            mfxU32 h = (*m_pIterations)[0].m_param.mfx.FrameInfo.Height;
            m_pDecodedFrame = new mfxU8[w*h];

            mfxFrameInfo& fi = m_pPar->mfx.FrameInfo;
            mfxInfoMFX& mfx = m_pPar->mfx;
            fi.Width = fi.CropW = w;
            fi.Height = fi.CropH = h;
            fi.AspectRatioW = fi.AspectRatioH = 1;
            fi.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            mfx.CodecProfile = MFX_PROFILE_VP9_0;
            mfx.CodecLevel = 0;
            m_pPar->AsyncDepth = 1;
            m_par_set = true;

            if (m_pInputFrames)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                AllocSurfaces();
                Init();
            }
        };
        ~BitstreamChecker()
        {
            delete[] m_pDecodedFrame;
        }
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

    struct FindIterByFrameIdx
    {
        FindIterByFrameIdx(mfxU32 frameIdx) : m_frameIdx(frameIdx) {}

        bool operator ()(Iteration const & iter)
        {
            return m_frameIdx >= iter.m_firstFrame &&
                m_frameIdx < iter.m_firstFrame + iter.m_numFramesToEncode;
        }

        mfxU32 m_frameIdx;
    };

    mfxF64 PSNR_Y_8bit(mfxU8 *ref, mfxU8 *src, mfxU32 length)
    {
        mfxF64 max = (1 << 8) - 1;
        mfxI32 diff = 0;
        mfxU64 dist = 0;

        for (mfxU32 y = 0; y < length; y++)
        {
            diff = ref[y] - src[y];
            dist += (diff * diff);
        }

        return (10. * log10(max * max * ((mfxF64)length / dist)));
    }

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
        std::vector<Iteration>::iterator curIter =
            std::find_if(m_pIterations->begin(), m_pIterations->end(), FindIterByFrameIdx(hdr.FrameOrder));

        mfxU32 expectedWidth = curIter->m_extParam.FrameWidth;
        if (hdr.uh.width != expectedWidth)
        {
            ADD_FAILURE() << "ERROR: frame_width_minus_1 in uncompressed header of frame " << hdr.FrameOrder << " is incorrect: " << hdr.uh.width - 1
                << ", expected " << expectedWidth - 1; throw tsFAIL;
        }

        mfxU32 expectedHeight = curIter->m_extParam.FrameHeight;
        if (hdr.uh.height != expectedHeight)
        {
            ADD_FAILURE() << "ERROR: frame_height_minus_1 in uncompressed header of frame " << hdr.FrameOrder << " is incorrect: " << hdr.uh.height - 1
                << ", expected " << expectedHeight - 1; throw tsFAIL;
        }

        if (hdr.FrameOrder && hdr.FrameOrder == curIter->m_firstFrame)
        {
            // new iteration just started - need to check all borderline conditions
            if (curIter == m_pIterations->begin())
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            std::vector<Iteration>::iterator prevIter = curIter - 1;

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
                if (prevIter->m_extParam.FrameWidth != curIter->m_extParam.FrameWidth ||
                    prevIter->m_extParam.FrameHeight != curIter->m_extParam.FrameHeight)
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
        if (m_pInputFrames)
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
            if (m_pInputFrames->find(hdr.FrameOrder) == m_pInputFrames->end())
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            // copy decoded frame to buffer for PSNR calculations
            mfxU32 w = curIter->m_extParam.FrameWidth;
            mfxU32 h = curIter->m_extParam.FrameHeight;
            mfxU32 pitch = (m_pSurf->Data.PitchHigh << 16) + m_pSurf->Data.PitchLow;
            //DumpDecodedFrame(m_pSurf->Data.Y, m_pSurf->Data.UV, w, h, pitch);
            for (mfxU32 i = 0; i < h; i++)
            {
                memcpy(m_pDecodedFrame + i*w, m_pSurf->Data.Y + i*pitch, w);
            }

            mfxU8* pInputFrame = (*m_pInputFrames)[hdr.FrameOrder];
            mfxF64 psnr = PSNR_Y_8bit(pInputFrame, m_pDecodedFrame, w * h);
            const mfxF64 minPsnr = GetMinPSNR(curIter->m_param);
            if (psnr < minPsnr)
            {
                ADD_FAILURE() << "ERROR: PSNR-Y of frame " << hdr.FrameOrder << " is equal to " << psnr << " and lower than threshold: " << minPsnr;
                throw tsFAIL;
            }

            delete[] pInputFrame;
            m_pInputFrames->erase(hdr.FrameOrder);
        }

        bs.DataLength = bs.DataOffset = 0;

        return MFX_ERR_NONE;
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

    mfxU32 GetDefaultTargetKbps(const Resolution& resol)
    {
        if (resol <= Resolution(176, 144)) return 100;
        if (resol <= Resolution(352, 288)) return 400;
        if (resol <= Resolution(704, 576)) return 1600;
        if (resol <= Resolution(1408, 1152)) return 6400;
        if (resol <= Resolution(2816, 2304)) return 25600;

        return 0;
    }

    void SetAdditionalParams(mfxVideoParam& par, const mfxExtVP9Param& extPar, mfxU32 testType)
    {
        par.mfx.FrameInfo.CropX = par.mfx.FrameInfo.CropY = 0;
        par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.Width;
        par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.Height;

        if (testType & CQP) par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        else if (testType & CBR) par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        else if (testType & VBR) par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;

        if (testType & TU1) par.mfx.TargetUsage = MFX_TARGETUSAGE_1;
        else if (testType & TU4) par.mfx.TargetUsage = MFX_TARGETUSAGE_4;
        else if (testType & TU7) par.mfx.TargetUsage = MFX_TARGETUSAGE_7;

        bool setBitrateAndQP = (testType & REAL_LIFE) == 0;
        if (setBitrateAndQP)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                par.mfx.QPI = par.mfx.QPP = 100;
            }
            else
            {
                par.mfx.TargetKbps = GetDefaultTargetKbps(Resolution(extPar.FrameWidth, extPar.FrameHeight));
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

    mfxU32 CalcNumOfIterations(const tc_struct& tc)
    {
        // calculate number of iterations set by tc_struct
        mfxU32 numIterations = 0;
        while (false == IsZero(tc.iteration_par[numIterations]) &&
            numIterations < MAX_ITERATIONS)
        {
            numIterations++;
        }

        return numIterations;
    }

#define SET_PARS_FOR_ITERATION(i)                                                    \
    m_filler = iterations[i].m_pRawReader;                                           \
    *m_pPar = iterations[i].m_param;                                                 \
    m_pPar->NumExtParam = 1;                                                         \
    pExtBuf[0] = (mfxExtBuffer*)&iterations[i].m_extParam;                           \
    if (false == IsZeroExtBuf(iterations[i].m_extResetOpt))                          \
    {                                                                                \
        pExtBuf[m_pPar->NumExtParam] = (mfxExtBuffer*)&iterations[i].m_extResetOpt;  \
        m_pPar->NumExtParam++;                                                       \
    }                                                                                \
    if (false == IsZeroExtBuf(iterations[i].m_extTempLayers))                        \
    {                                                                                \
        pExtBuf[m_pPar->NumExtParam] = (mfxExtBuffer*)&iterations[i].m_extTempLayers;\
        m_pPar->NumExtParam++;                                                       \
    }                                                                                \
    m_pPar->ExtParam = pExtBuf;                                                      \
    numFramesToEncode = iterations[i].m_numFramesToEncode;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_cases[id];

        mfxU32 allowedSubTypes = 0;
        for (std::vector<TestSubtype>::const_iterator subtype = executeTests.begin(); subtype != executeTests.end(); subtype++)
        {
            allowedSubTypes |= *subtype;
        }

        if (tc.type & ~allowedSubTypes)
        {
            return 0;
        }

        if (g_tsHWtype < MFX_HW_CNL) // unsupported on platform less CNL
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            mfxStatus sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
            g_tsStatus.check(sts);
            return 0;
        }


        // prepare pool of input streams
        std::map<Resolution, std::string> inputStreams;
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
        g_tsStreamPool.Reg();

        bool errorExpected = (tc.type & ERROR_EXPECTED) != 0;

        // prepare all parameters, ext buffers and input surface readers for test pipeline
        std::vector<Iteration> iterations;
        std::map<mfxU32, mfxU8*> inputFrames;
        std::map<mfxU32, mfxU8*>* pInputFrames = errorExpected ?
            0 : &inputFrames; // if error is expected from Reset, there is no need to check PSNR
                              // and thus no need to save input frames

        const mfxU32 numIterations = CalcNumOfIterations(tc);
        mfxU32 iterationStart = 0;

        for (mfxU8 idx = 0; idx < numIterations; idx++)
        {
            Iteration iter;

            // prepare mfxVideoParam
            if (idx == 0)
            {
                // for 1st iteration get defaults from the test
                iter.m_param = *m_pPar;
            }
            else
            {
                // for other iterations get defaults from previus iteration
                iter.m_param = iterations[idx - 1].m_param;
            }
            SETPARSEXT(&iter.m_param, MFX_PAR, idx);

            // prepare ext buffer with FrameWidth/FrameHeight
            Zero(iter.m_extParam);
            iter.m_extParam.Header.BufferId = MFX_EXTBUFF_VP9_PARAM;
            iter.m_extParam.Header.BufferSz = sizeof(mfxExtVP9Param);
            SETPARSEXT(&iter.m_extParam, MFX_EXT_VP9PARAM, idx);

            SetAdditionalParams(iter.m_param, iter.m_extParam, tc.type);

            // prepare mfxExtEncoderResetOption
            Zero(iter.m_extResetOpt);
            iter.m_extResetOpt.Header.BufferId = MFX_EXTBUFF_ENCODER_RESET_OPTION;
            iter.m_extResetOpt.Header.BufferSz = sizeof(mfxExtEncoderResetOption);
            SETPARSEXT(&iter.m_extResetOpt, MFX_EXT_RESET_OPTION, idx);

            // prepare mfxExtVp9TemporalLayers
            Zero(iter.m_extTempLayers);
            iter.m_extTempLayers.Header.BufferId = MFX_EXTBUFF_VP9_TEMPORAL_LAYERS;
            iter.m_extTempLayers.Header.BufferSz = sizeof(mfxExtVP9TemporalLayers);
            SETPARSEXT(&iter.m_extTempLayers, MFX_EXT_VP9_TL, idx);

            // prepare frame numbers (iteration start and duration)
            iter.m_numFramesToEncode = tc.iteration_par[idx].num_frames_in_iteration;
            iter.m_firstFrame = iterationStart;
            iterationStart += iter.m_numFramesToEncode;

            // prepare m_fillers
            const mfxFrameInfo& fi = iter.m_param.mfx.FrameInfo;
            tsRawReader* feeder = new SurfaceFeeder(
                pInputFrames,
                iter.m_extParam.FrameWidth,
                iter.m_extParam.FrameHeight,
                iter.m_firstFrame,
                inputStreams[Resolution(fi.Width, fi.Height)].c_str(),
                fi);
            iter.m_pRawReader = feeder;

            iterations.push_back(iter);
        }

        BitstreamChecker bs(&iterations, pInputFrames, tc.type, id);
        m_bs_processor = &bs;

        mfxU32 numFramesToEncode = 0;

        mfxExtBuffer* pExtBuf[2];
        SET_PARS_FOR_ITERATION(0);

        AllocSurfaces(); TS_CHECK_MFX;
        Init();
        AllocBitstream(); TS_CHECK_MFX;

        if (numFramesToEncode)
        {
            EncodeFrames(numFramesToEncode);
        }

        for (mfxU8 idx = 1; idx < numIterations; idx++)
        {
            SET_PARS_FOR_ITERATION(idx);
            if (iterations[idx].m_firstFrame == 0 && errorExpected)
            {
                // don't expect error/warning from Reset call if it's done right after Init (before actual encoding)
                g_tsStatus.expect(MFX_ERR_NONE);
            }
            else
            {
                g_tsStatus.expect(tc.sts);
            }
            mfxStatus sts = Reset();
            if (sts < 0)
            {
                // Reset returned error which was expected by the test
                // test is passed - need to stop encoding and exit
                break;
            }

            if (numFramesToEncode)
            {
                EncodeFrames(numFramesToEncode);
            }
        }

        g_tsStatus.expect(MFX_ERR_NONE);
        Close();

        // free memory from the heap
        for (mfxU8 i = 0; i < iterations.size(); i++)
        {
            if (iterations[i].m_pRawReader)
            {
                delete iterations[i].m_pRawReader;
                iterations[i].m_pRawReader = 0;
            }
        }
        if (pInputFrames)
        {
            for (std::map<mfxU32, mfxU8*>::iterator i = inputFrames.begin(); i != inputFrames.end(); i++)
            {
                delete[] i->second;
            }
            inputFrames.clear();
        }

        return 0;
        TS_END;
    }

    TS_REG_TEST_SUITE_CLASS(vp9e_dynamic_scaling);
}

