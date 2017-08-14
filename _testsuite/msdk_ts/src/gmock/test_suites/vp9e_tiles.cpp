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
#include "mfx_ext_buffers.h"
#if defined (WIN32)||(WIN64)
#include "windows.h"
#endif

namespace vp9e_tiles
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

#define ITER_LENGTH 2
#define NO_ENCODING 0

#define SET_NUM_ROWS(x) MFX_EXT_VP9PARAM_SET, &tsStruct::mfxExtVP9Param.NumTileRows, x
#define SET_NUM_COLS(x) MFX_EXT_VP9PARAM_SET, &tsStruct::mfxExtVP9Param.NumTileColumns, x
#define CHECK_NUM_ROWS(x) MFX_EXT_VP9PARAM_CHECK, &tsStruct::mfxExtVP9Param.NumTileRows, x
#define CHECK_NUM_COLS(x) MFX_EXT_VP9PARAM_CHECK, &tsStruct::mfxExtVP9Param.NumTileColumns, x
#define SET_W(x) MFX_PAR_SET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, x
#define SET_H(x) MFX_PAR_SET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, x
#define SET_FW(x) MFX_EXT_VP9PARAM_SET, &tsStruct::mfxExtVP9Param.FrameWidth, x
#define SET_FH(x) MFX_EXT_VP9PARAM_SET, &tsStruct::mfxExtVP9Param.FrameHeight, x

#define IVF_SEQ_HEADER_SIZE_BYTES 32
#define IVF_PIC_HEADER_SIZE_BYTES 12
#define MAX_IVF_HEADER_SIZE IVF_SEQ_HEADER_SIZE_BYTES + IVF_PIC_HEADER_SIZE_BYTES

#define MAX_NUM_ROWS 4
#define MAX_NUM_TILES 16
#define MAX_TILE_WIDTH 4096
#define MIN_TILE_WIDTH 256

#define MAX_SUPPORTED_FRAME_WIDTH 7680
#define MAX_PIPES_SUPPORTED 2
#define BASIC_NUM_TILE_COLS 4

#define MAX_U16 0xffff

#define MFX_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )

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
        QUERY = 0x0100,
        INIT = 0x0200,
        GET_VIDEO_PARAM = 0x0400,
        CHECK_FULL_INITIALIZATION = QUERY | INIT | GET_VIDEO_PARAM,
        RESET = 0x0800,
        MAX_TILE_ROWS = 0x1000 | CHECK_FULL_INITIALIZATION,
        BASIC_TILE_COLS = 0x2000 | CHECK_FULL_INITIALIZATION,
        MAX_TILE_COLS = 0x4000 | CHECK_FULL_INITIALIZATION,
        MAX_TILE_W = 0x8000 | CHECK_FULL_INITIALIZATION,
        MIN_TILE_W = 0x010000 | CHECK_FULL_INITIALIZATION,
        SCALABLE_PIPE = 0x020000,
        UNALIGNED_RESOL = 0x040000 | CHECK_FULL_INITIALIZATION,
        DYNAMIC_CHANGE = 0x080000 | CHECK_FULL_INITIALIZATION,
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
        QUERY,
        INIT,
        GET_VIDEO_PARAM,
        CHECK_FULL_INITIALIZATION,
        RESET,
        MAX_TILE_ROWS,
        BASIC_TILE_COLS,
        // MAX_TILE_COLS, // this case requires 8K encoding - disabled for Pre-Si
        MIN_TILE_W,
        // MAX_TILE_W,    // this case requires 8K encoding - disabled for Pre-Si
        SCALABLE_PIPE,
        UNALIGNED_RESOL,
        DYNAMIC_CHANGE,
        //EXPERIMENTAL
    };

    enum
    {
        MFX_PAR_SET = 0,
        MFX_PAR_CHECK = 1,
        MFX_EXT_VP9PARAM_SET = 2,
        MFX_EXT_VP9PARAM_CHECK = 3
    };

    enum
    {
        SET = 0,
        GET = 1,
        CHECK = 2
    };

    const tc_struct test_cases[] =
    {
        // below cases check support of maximum number tiles rows
        {/*00*/ MFX_ERR_NONE, MAX_TILE_ROWS | CQP | TU1,
            {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*01*/ MFX_ERR_NONE, MAX_TILE_ROWS | CQP | TU4,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*02*/ MFX_ERR_NONE, MAX_TILE_ROWS | CQP | TU7,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*03*/ MFX_ERR_NONE, MAX_TILE_ROWS | CBR | TU1,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*04*/ MFX_ERR_NONE, MAX_TILE_ROWS | CBR | TU4,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*05*/ MFX_ERR_NONE, MAX_TILE_ROWS | CBR | TU7,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*06*/ MFX_ERR_NONE, MAX_TILE_ROWS | VBR | TU1,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*07*/ MFX_ERR_NONE, MAX_TILE_ROWS | VBR | TU4,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },
        {/*08*/ MFX_ERR_NONE, MAX_TILE_ROWS | VBR | TU7,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) }}
        },

        // below cases check support of basic tile columns
        {/*09*/ MFX_ERR_NONE, BASIC_TILE_COLS | CQP | TU1,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*10*/ MFX_ERR_NONE, BASIC_TILE_COLS | CQP | TU4,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*11*/ MFX_ERR_NONE, BASIC_TILE_COLS | CQP | TU7,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*12*/ MFX_ERR_NONE, BASIC_TILE_COLS | CBR | TU1,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*13*/ MFX_ERR_NONE, BASIC_TILE_COLS | CBR | TU4,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*14*/ MFX_ERR_NONE, BASIC_TILE_COLS | CBR | TU7,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*15*/ MFX_ERR_NONE, BASIC_TILE_COLS | VBR | TU1,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*16*/ MFX_ERR_NONE, BASIC_TILE_COLS | VBR | TU4,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },
        {/*17*/ MFX_ERR_NONE, BASIC_TILE_COLS | VBR | TU7,
           {{ ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(BASIC_NUM_TILE_COLS) }}
        },

        // below case checks support of max tile columns
        {/*18*/ MFX_ERR_NONE, MAX_TILE_COLS | CQP,
            {{ ITER_LENGTH, SET_W(5632), SET_H(4608), SET_NUM_ROWS(1), SET_NUM_COLS(MAX_NUM_TILES) }}
        },

        // below cases check support of tile rows together with tile columns
        // rows and cols together are supported only for scalable pipeline. In this case number of cols is limited with MAX_PIPES_SUPPORTED
        {/*19*/ MFX_ERR_NONE, SCALABLE_PIPE | CQP | TU1 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*20*/ MFX_ERR_NONE, SCALABLE_PIPE | CQP | TU4 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*21*/ MFX_ERR_NONE, SCALABLE_PIPE | CQP | TU7 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*22*/ MFX_ERR_NONE, SCALABLE_PIPE | CBR | TU1 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*23*/ MFX_ERR_NONE, SCALABLE_PIPE | CBR | TU4 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*24*/ MFX_ERR_NONE, SCALABLE_PIPE | CBR | TU7 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*25*/ MFX_ERR_NONE, SCALABLE_PIPE | VBR | TU1 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*26*/ MFX_ERR_NONE, SCALABLE_PIPE | VBR | TU4 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*27*/ MFX_ERR_NONE, SCALABLE_PIPE | VBR | TU7 | CHECK_FULL_INITIALIZATION,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },

        // below case checks support of smallest possible tile width
        // also this case checks tiles for frames smaller than surface
        {/*28*/ MFX_ERR_NONE, MIN_TILE_W | CQP,
           {{ ITER_LENGTH, SET_W(704), SET_H(576), SET_FW(MIN_TILE_WIDTH * 2), SET_FH(576),
              SET_NUM_ROWS(1), SET_NUM_COLS(2) }}
        },
        // below case checks support of biggest possible tile width
        {/*29*/ MFX_ERR_NONE, MAX_TILE_W | CQP,
           {{ ITER_LENGTH, SET_W(MFX_MIN(MAX_SUPPORTED_FRAME_WIDTH, MAX_TILE_WIDTH * 2)), SET_H(6288),
              SET_NUM_ROWS(1), SET_NUM_COLS(2) }}
        },
        // below cases check for support of tiles for unaligned surface resolutions
        {/*30*/ MFX_ERR_NONE, UNALIGNED_RESOL | CQP,
        { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_FW(1408-98), SET_FH(1152-50),
            SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) } }
        },
        {/*31*/ MFX_ERR_NONE, UNALIGNED_RESOL | CQP,
        { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_FW(1408-98), SET_FH(1152-50),
            SET_NUM_ROWS(1), SET_NUM_COLS(4) } }
        },

        // below cases check reaction of Query to incorrect NumTileRows
        // NumTileRows not power of 2
        {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(3), SET_NUM_COLS(1),
                                                      CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }}
        },
        // NumTileRows is power of 2 but bigger than number of SB in Height
        {/*33*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(176), SET_H(144), SET_NUM_ROWS(4), SET_NUM_COLS(1),
                                                    CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }}
        },
        // NumTileRows is bigger than max supported by VP9
        {/*34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS + 1), SET_NUM_COLS(1),
                                                      CHECK_NUM_ROWS(MAX_NUM_ROWS), CHECK_NUM_COLS(1) }}
        },

        // below cases check reaction of Query to incorrect NumTileColumns
        // NumTileColumns not power of 2
        {/*35*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(3),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }}
        },
        // NumTileColumns is power of 2 but bigger than minimum tile width (256) allows
        {/*36*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(704), SET_H(576), SET_NUM_ROWS(1), SET_NUM_COLS(4),
                                                    CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }}
        },
        // NumTileColumns is power of 2 but smaller than maximum tile width (4096) allows
        {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(1),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }}
        },
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*38*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(MAX_NUM_TILES + 1),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(MAX_NUM_TILES) }}
        },
        // NumTileColumns is bigger than number of pipes for scalable encoding
        {/*39*/ MFX_ERR_UNSUPPORTED, QUERY | SCALABLE_PIPE,
            {{ NO_ENCODING, SET_W(2816), SET_H(2304), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED * 2),
                                                      CHECK_NUM_ROWS(0), CHECK_NUM_COLS(0) } }
        },

        // below cases check reaction of Init to incorrect NumTileRows
        // NumTileRows not power of 2
        {/*40*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(3), SET_NUM_COLS(1) }}
        },
        // NumTileRows is power of 2 but bigger than number of SB in Height
        {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{ NO_ENCODING, SET_W(176), SET_H(144), SET_NUM_ROWS(4), SET_NUM_COLS(1) }}
        },
        // NumTileRows is bigger than max supported by VP9
        {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS + 1), SET_NUM_COLS(1) }}
        },

        // below cases check reaction of Init to incorrect NumTileColumns
        // NumTileColumns not power of 2
        {/*43*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(3) }}
        },
        // NumTileColumns is power of 2 but bigger than minimum tile width (256) allows
        {/*44*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{ NO_ENCODING, SET_W(704), SET_H(576), SET_NUM_ROWS(1), SET_NUM_COLS(4) }}
        },
        // NumTileColumns is power of 2 but smaller than maximum tile width (4096) allows
        {/*45*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(1) }}
        },
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(MAX_NUM_TILES + 1) }}
        },
        // NumTileColumns is bigger than number of pipes for scalable encoding
        {/*47*/ MFX_ERR_INVALID_VIDEO_PARAM, INIT | SCALABLE_PIPE,
            {{ NO_ENCODING, SET_W(2816), SET_H(2304), SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED * 2) }}
        },

        // below cases check reaction of Init + GetVideoParam to incorrect NumTileRows
        // NumTileRows not power of 2
        {/*48*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(3), SET_NUM_COLS(1),
                                                      CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }}
        },
        // NumTileRows is power of 2 but bigger than number of SB in Height
        {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(176), SET_H(144), SET_NUM_ROWS(4), SET_NUM_COLS(1),
                                                    CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }}
        },
        // NumTileRows is bigger than max supported by VP9
        {/*50*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_ROWS + 1), SET_NUM_COLS(1),
                                                      CHECK_NUM_ROWS(MAX_NUM_ROWS), CHECK_NUM_COLS(1) }}
        },

        // below cases check reaction of Init + GetVideoParam to incorrect NumTileColumns
        // NumTileColumns not power of 2
        {/*51*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(3),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }}
        },
        // NumTileColumns is power of 2 but bigger than minimum tile width (256) allows
        {/*52*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(704), SET_H(576), SET_NUM_ROWS(1), SET_NUM_COLS(4),
                                                    CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }}
        },
        // NumTileColumns is power of 2 but smaller than maximum tile width (4096) allows
        {/*53*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(1),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }}
        },
        // NumTileColumns is 0 - check that is set to proper default value for big resolution
        {/*54*/ MFX_ERR_NONE, INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(0), SET_NUM_COLS(0),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }}
        },
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*55*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  INIT | GET_VIDEO_PARAM,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(MAX_NUM_TILES + 1),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(MAX_NUM_TILES) }}
        },

        // below cases check reaction of Reset to incorrect NumTileRows
        // NumTileRows not power of 2
        {/*56*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(3), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }
            }
        },
        // NumTileRows is power of 2 but bigger than number of SB in Height
        {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(176), SET_H(144), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(4), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }
            }
        },
        // NumTileRows is bigger than max supported by VP9
        {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(MAX_NUM_ROWS + 1), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(MAX_NUM_ROWS), CHECK_NUM_COLS(1) }
            }
        },

        // below cases check reaction of Reset to incorrect NumTileColumns
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(3),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }
            }
        },
        // NumTileColumns is power of 2 but bigger than minimum tile width (256) allows
        {/*60*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(4),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }
            }
        },
        // NumTileColumns is power of 2 but smaller than maximum tile width (4096) allows
        {/*61*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(2) }, // 8K is too big to encode for Pre-Si. TODO: do real encoding here for Post-Si.
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }
            }
        },
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*62*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(1) }, // 8K is too big to encode for Pre-Si. TODO: do real encoding here for Post-Si.
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(MAX_NUM_TILES + 1),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(MAX_NUM_TILES) }
            }
        },
        // NumTileColumns is bigger than number of pipes for scalable encoding
        {/*63*/ MFX_ERR_INVALID_VIDEO_PARAM, RESET | CQP | SCALABLE_PIPE,
            {
                { ITER_LENGTH, SET_W(2816), SET_H(2304), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED * 2) }
            }
        },

        // below test cases check on-the-fly change of number of tile rows and tile columns for CQP
        {/*64*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*65*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
            }
        },
        {/*66*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*67*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP | SCALABLE_PIPE | CHECK_FULL_INITIALIZATION,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
                { ITER_LENGTH, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
            }
        },

        // below test cases check on-the-fly change of number of tile rows and tile columns for CBR
        {/*68*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*69*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
            }
        },
        {/*70*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*71*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR | SCALABLE_PIPE | CHECK_FULL_INITIALIZATION,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
                { ITER_LENGTH, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
            }
        },

        // below test cases check on-the-fly change of number of tile rows and tile columns for VBR
        {/*72*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*73*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
            }
        },
        {/*74*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*75*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR | SCALABLE_PIPE | CHECK_FULL_INITIALIZATION,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
                { ITER_LENGTH, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED) },
            }
        },
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
        mfxExtCodingOptionDDI m_extDDI;
        mfxExtEncoderResetOption m_extResetOpt;
        mfxU32 m_firstFrame;
        mfxU32 m_numFramesToEncode;

        mfxExtBuffer* m_extBufs[MAX_EXT_BUFFERS][2];

        Iteration(
            const mfxVideoParam& defaults,
            const tc_struct::params_for_iteration& iterPar,
            mfxU32 fourcc,
            mfxU32 type,
            mfxU32 firstFrame);
    };

    Iteration::Iteration(
        const mfxVideoParam& defaults,
        const tc_struct::params_for_iteration& iterPar,
        mfxU32 fourcc,
        mfxU32 type,
        mfxU32 firstFrame)
        : m_firstFrame(firstFrame)
        , m_numFramesToEncode(iterPar.num_frames_in_iteration)
    {
        // prepare parameters to "set" encoder configuration
        // e.g. pass it to Query, Init or Reset
        m_param[SET] = defaults;
        SETPARSITER(&m_param[SET], MFX_PAR_SET);
        m_param[SET].NumExtParam = 0;
        m_param[SET].ExtParam = m_extBufs[SET];

        InitExtBuffer(MFX_EXTBUFF_VP9_PARAM, m_extParam[SET]);
        SETPARSITER(&m_extParam[SET], MFX_EXT_VP9PARAM_SET);
        if (false == IsZeroExtBuf(m_extParam[SET]))
        {
            m_param[SET].ExtParam[m_param[SET].NumExtParam++] = (mfxExtBuffer*)&m_extParam[SET];
        }
        SetAdditionalParams(m_param[SET], m_extParam[SET], fourcc, type);

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
        SETPARSITER(&m_param[CHECK], MFX_PAR_CHECK); // overwrite "set" parameterss by "check" parameters which were provided explicitly
        m_param[CHECK].NumExtParam = 0; // no need in attaching ext buffers for "check" mfxVideoParam
        m_extParam[CHECK] = m_extParam[SET];
        SETPARSITER(&m_extParam[CHECK], MFX_EXT_VP9PARAM_CHECK); // overwrite "set" parameterss by "check" parameters which were provided explicitly
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

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        std::vector<Iteration*>* m_pIterations;
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        mfxU32 m_testType;
        mfxU32 m_testId;
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
        {
            // first iteration always has biggest possible resolution
            // we use it to allocate buffers to store decoded frame
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
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    /*void  DumpCodedFrame(const mfxBitstream& bs, mfxU32 testId)
    {
        // Dump stream to file
        const int encoded_size = bs.DataLength;
        char fileName[100];
        sprintf(fileName, "vp9e_encoded_tiles_%d.vp9", testId);
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

        bool operator ()(Iteration const * pIter)
        {
            return m_frameIdx >= pIter->m_firstFrame &&
                m_frameIdx < pIter->m_firstFrame + pIter->m_numFramesToEncode;
        }

        mfxU32 m_frameIdx;
    };

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

    inline mfxU32 CeilLog2(mfxU32 x) { mfxU32 l = 0; while (x > (1U << l)) l++; return l; }

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

        mfxU32 profileExpected = m_pPar->mfx.CodecProfile - 1;
        if (hdr.uh.profile != profileExpected)
        {
            ADD_FAILURE() << "ERROR: profile in uncompressed header of frame " << hdr.FrameOrder << " is incorrect: " << hdr.uh.profile
                << ", expected " << profileExpected; throw tsFAIL;
        }

        mfxU32 log2RowsExpected = CeilLog2(iter.m_extParam[CHECK].NumTileRows);
        if (hdr.uh.log2_tile_rows != log2RowsExpected)
        {
            ADD_FAILURE() << "ERROR: log2_tile_rows in uncompressed header of frame " << hdr.FrameOrder << " is incorrect: " << hdr.uh.log2_tile_rows
                << ", expected " << log2RowsExpected; throw tsFAIL;
        }
        mfxU32 log2ColsExpected = CeilLog2(iter.m_extParam[CHECK].NumTileColumns);
        if (hdr.uh.log2_tile_cols != log2ColsExpected)
        {
            ADD_FAILURE() << "ERROR: log2_tile_cols in uncompressed header of frame " << hdr.FrameOrder << " is incorrect: " << hdr.uh.log2_tile_cols
                << ", expected " << log2ColsExpected; throw tsFAIL;
        }

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

#if defined (WIN32)||(WIN64)
#define FAIL_ON_ERROR(STS)                                                   \
{                                                                            \
    if (STS != ERROR_SUCCESS)                                                \
    {                                                                        \
        ADD_FAILURE() << "ERROR: failed to set or restore registry values";  \
        throw tsFAIL;                                                        \
    }                                                                        \
}

#ifdef UNICODE
    typedef std::wstring tstring;
#else
    typedef std::string tstring;
#endif

    struct RegValue
    {
        HKEY    rootKey;
        tstring subKey;
        tstring valueName;
        bool    existedBefore;
        mfxU32  initialValueData;
        mfxU32  newValueData;
    };

    class tsRegistryEditor
    {
        std::vector<RegValue> m_regValues;
    public:
        void SetRegKey(
            HKEY rootKey,
            const tstring& subKey,
            const tstring& valueName,
            mfxU32 valueData,
            bool forceZero = false);
        void RestoreRegKeys();
        ~tsRegistryEditor();
    };

    void tsRegistryEditor::SetRegKey(
        HKEY rootKey,
        const tstring& subKey,
        const tstring& valueName,
        mfxU32 valueData,
        bool forceZero)
    {
        HKEY hkey;
        long sts = RegOpenKeyEx(rootKey, subKey.c_str(), 0, KEY_ALL_ACCESS, &hkey);
        FAIL_ON_ERROR(sts);

        RegValue value = {};
        value.rootKey = rootKey;
        value.subKey = subKey;
        value.valueName = valueName;
        value.existedBefore = true;

        DWORD size = sizeof(DWORD);
        sts = RegGetValue(rootKey, subKey.c_str(), valueName.c_str(), RRF_RT_REG_DWORD, NULL, (PVOID)&value.initialValueData, &size);
        if (sts == ERROR_FILE_NOT_FOUND)
        {
            value.existedBefore = false;
            value.initialValueData = 0;
        }
        else FAIL_ON_ERROR(sts);

        bool needToSetValue = (valueData != value.initialValueData || value.existedBefore == false && valueData == 0 && forceZero == true);

        if (needToSetValue)
        {
            sts = RegSetValueEx(hkey, valueName.c_str(), 0, REG_DWORD, (LPBYTE)&valueData, sizeof(DWORD));
            FAIL_ON_ERROR(sts);
            value.newValueData = valueData;
            m_regValues.push_back(value);
        }

        sts = RegCloseKey(hkey);
        FAIL_ON_ERROR(sts);
    }

    void tsRegistryEditor::RestoreRegKeys()
    {
        for (std::vector<RegValue>::iterator i = m_regValues.begin(); i != m_regValues.end(); i++)
        {
            HKEY hkey;
            long sts = RegOpenKeyEx(i->rootKey, i->subKey.c_str(), 0, KEY_ALL_ACCESS, &hkey);
            FAIL_ON_ERROR(sts);

            if (i->existedBefore == false)
            {
                sts = RegDeleteValue(hkey, i->valueName.c_str());
            }
            else
            {
                sts = RegSetValueEx(hkey, i->valueName.c_str(), 0, REG_DWORD, (LPBYTE)&i->initialValueData, sizeof(DWORD));
            }
            FAIL_ON_ERROR(sts);

            sts = RegCloseKey(hkey);
            FAIL_ON_ERROR(sts);
        }

        m_regValues.clear();
    }

    tsRegistryEditor::~tsRegistryEditor()
    {
        RestoreRegKeys();
    }


#endif

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
        if (expected.NumTileRows != real.NumTileRows)
        {
            ADD_FAILURE() << "ERROR: function " << function << " returned incorrect value for parameter NumTileRows: "  << real.NumTileRows <<
                ", expected " << expected.NumTileRows; throw tsFAIL;
        }
        if (expected.NumTileColumns != real.NumTileColumns)
        {
            ADD_FAILURE() << "ERROR: function " << function << " returned incorrect value for parameter NumTileColumns: "  << real.NumTileColumns <<
                ", expected " << expected.NumTileColumns; throw tsFAIL;
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

        if (false == NeedToRun(tc.type))
        {
            return 0;
        }

        if (tc.type & SCALABLE_PIPE && tc.type & (CBR | VBR))
        {
            // don't run cases for scalable pipeline with BRC - return respective error instead
            // known driver issue - scalable pipes cannot work with HuC (HuC is required for BRC)
            // TODO: remove once support of scalable pipline together with HuC is unblocked in the driver
            ADD_FAILURE() << "ERROR: Scalable pipeline isn't supported together with BRC";
            throw tsFAIL;
        }

        if (g_tsHWtype < MFX_HW_ICL) // unsupported on platform less ICL
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            mfxStatus sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
            g_tsStatus.check(sts);
            return 0;
        }

#if defined (WIN32)|(WIN64)
        // do "magic" with registry to unblock scalable and non-scalable encoding for tiles
        // TODO: remove once:
        // (1) support of scalable pipline together with HuC is unblocked in the driver ("VP9 Encode HUC Enable" eliminated)
        // (2) driver is capable to dynamically switch between scalable and non-scalable pipelines ("Disable Media Encode Scalability" eliminated)
        tsRegistryEditor reg;
        const HKEY rootKey = HKEY_CURRENT_USER;

        if (tc.type & SCALABLE_PIPE)
        {
            reg.SetRegKey(rootKey, TEXT("Software\\Intel\\Display\\DXVA"), TEXT("VP9 Encode HUC Enable"), 0);
            reg.SetRegKey(rootKey, TEXT("Software\\Intel\\Display\\DXVA"), TEXT("Disable Media Encode Scalability"), 0);
        }
        else
        {
            reg.SetRegKey(rootKey, TEXT("Software\\Intel\\Display\\DXVA"), TEXT("VP9 Encode HUC Enable"), 1);
            reg.SetRegKey(rootKey, TEXT("Software\\Intel\\Display\\DXVA"), TEXT("Disable Media Encode Scalability"), 1);
        }
#endif

        // prepare pool of input streams
        std::map<Resolution, std::string> inputStreams;
        if (fourcc == MFX_FOURCC_NV12)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/salesman_176x144_50.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/salesman_352x288_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/salesman_704x576_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/salesman_1408X1152_50.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/salesman_2816x2304_50.yuv"));
            inputStreams.emplace(Resolution(5632, 4608), g_tsStreamPool.Get("forBehaviorTest/salesman_5632x4608_2.yuv"));
            inputStreams.emplace(Resolution(7680, 6288), g_tsStreamPool.Get("forBehaviorTest/salesman_7680x6288_2.yuv"));
        }
        else if (fourcc == MFX_FOURCC_AYUV)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_ayuv.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_352x288_24_ayuv_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/Kimono1_704x576_24_ayuv_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1408X1152_24_ayuv_10.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/Kimono1_2816x2304_24_ayuv_10.yuv"));
            inputStreams.emplace(Resolution(5632, 4608), g_tsStreamPool.Get("forBehaviorTest/Kimono1_5632x4608_24_ayuv_2.yuv"));
            inputStreams.emplace(Resolution(7680, 6288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x6288_24_ayuv_2.yuv"));
        }
        else if (fourcc == MFX_FOURCC_P010)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_p010_shifted.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_352x288_24_p010_shifted_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/Kimono1_704x576_24_p010_shifted_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1408X1152_24_p010_shifted_10.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/Kimono1_2816x2304_24_p010_shifted_10.yuv"));
            inputStreams.emplace(Resolution(5632, 4608), g_tsStreamPool.Get("forBehaviorTest/Kimono1_5632x4608_24_p010_shifted_2.yuv"));
            inputStreams.emplace(Resolution(7680, 6288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x6288_24_p010_shifted_2.yuv"));
        }
        else if (fourcc == MFX_FOURCC_Y410)
        {
            inputStreams.emplace(Resolution(176, 144), g_tsStreamPool.Get("forBehaviorTest/Kimono1_176x144_24_y410.yuv"));
            inputStreams.emplace(Resolution(352, 288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_352x288_24_y410_50.yuv"));
            inputStreams.emplace(Resolution(704, 576), g_tsStreamPool.Get("forBehaviorTest/Kimono1_704x576_24_y410_50.yuv"));
            inputStreams.emplace(Resolution(1408, 1152), g_tsStreamPool.Get("forBehaviorTest/Kimono1_1408X1152_24_y410_10.yuv"));
            inputStreams.emplace(Resolution(2816, 2304), g_tsStreamPool.Get("forBehaviorTest/Kimono1_2816x2304_24_y410_10.yuv"));
            inputStreams.emplace(Resolution(5632, 4608), g_tsStreamPool.Get("forBehaviorTest/Kimono1_5632x4608_24_y410_2.yuv"));
            inputStreams.emplace(Resolution(7680, 6288), g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x6288_24_y410_2.yuv"));
        }
        g_tsStreamPool.Reg();

        std::vector<Iteration*> iterations;

        // prepare all iterations for the test
        mfxU32 totalFramesToEncode = 0;
        const mfxU32 numIterations = CalcIterations(tc, totalFramesToEncode);
        mfxU32 iterationStart = 0;

        std::map<mfxU32, mfxFrameSurface1*> inputSurfaces;
        std::map<mfxU32, mfxFrameSurface1*>* pInputSurfaces = totalFramesToEncode ?
            &inputSurfaces : 0;

        if (totalFramesToEncode && tc.type & SCALABLE_PIPE)
        {
            // workaround for decoder issue - it crashes simics on attempt to decode frames with both tile rows and columns
            // TODO: remove this once decoder issue is fixed
            pInputSurfaces = 0;
        }

        for (mfxU8 idx = 0; idx < numIterations; idx++)
        {
            const mfxVideoParam& defaults = (idx == 0) ? *m_pPar : iterations[idx - 1]->m_param[CHECK];
            Iteration* pIter = new Iteration(defaults, tc.iteration_par[idx], fourcc, tc.type, iterationStart);

            iterationStart += pIter->m_numFramesToEncode;
            iterations.push_back(pIter);
        }

        // prepare surface feeder
        const mfxFrameInfo& fi = iterations[0]->m_param[CHECK].mfx.FrameInfo;
        const mfxU32 frameWidth = iterations[0]->m_extParam[CHECK].FrameWidth;
        const mfxU32 frameHeight = iterations[0]->m_extParam[CHECK].FrameHeight;
        tsRawReader* feeder = new SurfaceFeeder(
            pInputSurfaces,
            iterations[0]->m_param[SET],
            iterations[0]->m_firstFrame,
            inputStreams[Resolution(fi.Width, fi.Height)].c_str(),
            fi);

        feeder->m_disable_shift_hack = true;  // this hack adds shift if fi.Shift != 0!!! Need to disable it.

        m_filler = feeder;

        // run the test
        MFXInit(); TS_CHECK_MFX;
        Load();

        // prepare bitstream checker
        BitstreamChecker bs(&iterations, pInputSurfaces, tc.type, id);
        m_bs_processor = &bs;

        *m_pPar = iterations[0]->m_param[SET];
        mfxVideoParam* pOutPar = &iterations[0]->m_param[GET];

        // QUERY SECTION
        if (tc.type & QUERY)
        {
            g_tsStatus.expect(tc.sts);
            TS_TRACE(m_pPar);
            TRACE_FUNC3(MFXVideoENCODE_Query, m_session, m_pPar, pOutPar);
            mfxStatus sts = MFXVideoENCODE_Query(m_session, m_pPar, pOutPar);
            TS_TRACE(pOutPar);
            g_tsStatus.check(sts);
            CheckOutputParams("Query", *iterations[0]);
        }

        // INIT SECTION
        if ((tc.type & (INIT | RESET)) || totalFramesToEncode)
        {
            g_tsStatus.expect(tc.sts);
            TS_TRACE(m_pPar);
            // workaround for issue with [scalable pipeline + HuC]:
            // refresh of CABAC contexts should be disabled - attach special ext buffer
            // TODO: remove once support of scalable pipline together with HuC is unblocked in the driver
            if (tc.type & SCALABLE_PIPE)
            {
                mfxExtCodingOptionDDI& extDdi = iterations[0]->m_extDDI;
                InitExtBuffer(MFX_EXTBUFF_DDI, extDdi);
                extDdi.RefreshFrameContext = MFX_CODINGOPTION_OFF;
                m_pPar->ExtParam[m_pPar->NumExtParam++] = (mfxExtBuffer*)&extDdi;
            }
            TRACE_FUNC2(MFXVideoENCODE_Init, m_session, m_pPar);
            mfxStatus sts = MFXVideoENCODE_Init(m_session, m_pPar);
            // workaround for issue with [scalable pipeline + HuC]:
            // detach special ext buffer
            // TODO: remove once support of scalable pipline together with HuC is unblocked in the driver
            if (tc.type & SCALABLE_PIPE)
            {
                m_pPar->ExtParam[--m_pPar->NumExtParam] = 0;
            }
            TS_TRACE(&m_pPar);
            if (tc.type & INIT)
            {
                g_tsStatus.check(sts);
            }
            m_initialized = sts >= MFX_ERR_NONE;
        }

        // GET_VIDEO_PARAM SECTION
        if (tc.type & GET_VIDEO_PARAM && tc.sts >= MFX_ERR_NONE)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            TRACE_FUNC2(MFXVideoENCODE_GetVideoParam, m_session, pOutPar);
            mfxStatus sts = MFXVideoENCODE_GetVideoParam(m_session, pOutPar);
            TS_TRACE(pOutPar);
            g_tsStatus.check(sts);
            CheckOutputParams("GetVideoParam", *iterations[0]);
        }

        if (totalFramesToEncode)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            AllocSurfaces(); TS_CHECK_MFX;
            AllocBitstream(); TS_CHECK_MFX;
        }

        if (iterations[0]->m_numFramesToEncode)
        {
            EncodeFrames(iterations[0]->m_numFramesToEncode);
        }

        for (mfxU8 idx = 1; idx < numIterations; idx++)
        {
            *m_pPar = iterations[idx]->m_param[SET];
            g_tsStatus.expect(tc.sts);
            TS_TRACE(m_pPar)
            TRACE_FUNC2(MFXVideoENCODE_Reset, m_session, m_pPar);
            mfxStatus sts = MFXVideoENCODE_Reset(m_session, m_pPar);
            if (tc.type & RESET)
            {
                g_tsStatus.check(sts);
            }
            pOutPar = &iterations[idx]->m_param[GET];
            TRACE_FUNC2(MFXVideoENCODE_GetVideoParam, m_session, pOutPar);
            sts = MFXVideoENCODE_GetVideoParam(m_session, pOutPar);
            TS_TRACE(pOutPar);
            if (tc.type & RESET && tc.sts >= MFX_ERR_NONE)
            {
                CheckOutputParams("Reset", *iterations[idx]);
            }

            if (iterations[idx]->m_numFramesToEncode)
            {
                EncodeFrames(iterations[idx]->m_numFramesToEncode);
            }
        }

        g_tsStatus.expect(MFX_ERR_NONE);
        if (m_initialized)
        {
            Close();
        }

        for (std::vector<Iteration*>::iterator i = iterations.begin(); i != iterations.end(); i++)
        {
            if (*i != 0)
            {
                delete *i;
                *i = 0;
            }
        }
        iterations.clear();

        if (m_filler)
        {
            delete m_filler;
            m_filler = 0;
        }

        return 0;
        TS_END;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_tiles, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_tiles, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_tiles, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases_ayuv);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_tiles, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
}

