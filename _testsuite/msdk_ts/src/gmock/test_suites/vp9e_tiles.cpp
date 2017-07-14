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

#define MFX_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
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
        SCALABLE_PIPE = 0x020000 | CHECK_FULL_INITIALIZATION,
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
        //SCALABLE_PIPE,  // scalable encoding is blocked so far because it cannot work with HuC enabled.
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
        {/*19*/ MFX_ERR_NONE, SCALABLE_PIPE | CQP | TU1,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*20*/ MFX_ERR_NONE, SCALABLE_PIPE | CQP | TU4,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*21*/ MFX_ERR_NONE, SCALABLE_PIPE | CQP | TU7,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*22*/ MFX_ERR_NONE, SCALABLE_PIPE | CBR | TU1,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*23*/ MFX_ERR_NONE, SCALABLE_PIPE | CBR | TU4,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*24*/ MFX_ERR_NONE, SCALABLE_PIPE | CBR | TU7,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*25*/ MFX_ERR_NONE, SCALABLE_PIPE | VBR | TU1,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*26*/ MFX_ERR_NONE, SCALABLE_PIPE | VBR | TU4,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
        },
        {/*27*/ MFX_ERR_NONE, SCALABLE_PIPE | VBR | TU7,
            { { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(MAX_NUM_TILES/MAX_PIPES_SUPPORTED), SET_NUM_COLS(MAX_PIPES_SUPPORTED) } }
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
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*54*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY,
            {{ NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(MAX_NUM_TILES + 1),
                                                      CHECK_NUM_ROWS(1), CHECK_NUM_COLS(MAX_NUM_TILES) }}
        },

        // below cases check reaction of Reset to incorrect NumTileRows
        // NumTileRows not power of 2
        {/*55*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(3), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }
            }
        },
        // NumTileRows is power of 2 but bigger than number of SB in Height
        {/*56*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(176), SET_H(144), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(4), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(2), CHECK_NUM_COLS(1) }
            }
        },
        // NumTileRows is bigger than max supported by VP9
        {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(MAX_NUM_ROWS + 1), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(MAX_NUM_ROWS), CHECK_NUM_COLS(1) }
            }
        },

        // below cases check reaction of Reset to incorrect NumTileColumns
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(3),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }
            }
        },
        // NumTileColumns is power of 2 but bigger than minimum tile width (256) allows
        {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { ITER_LENGTH, SET_W(704), SET_H(576), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(4),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }
            }
        },
        // NumTileColumns is power of 2 but smaller than maximum tile width (4096) allows
        {/*60*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(2) }, // 8K is too big to encode for Pre-Si. TODO: do real encoding here for Post-Si.
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(1),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(2) }
            }
        },
        // NumTileColumns is bigger than max number of tiles for VP9
        {/*61*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESET | CQP,
            {
                { NO_ENCODING, SET_W(7680), SET_H(6288), SET_NUM_ROWS(1), SET_NUM_COLS(1) }, // 8K is too big to encode for Pre-Si. TODO: do real encoding here for Post-Si.
                { NO_ENCODING, SET_NUM_ROWS(1), SET_NUM_COLS(MAX_NUM_TILES + 1),
                               CHECK_NUM_ROWS(1), CHECK_NUM_COLS(MAX_NUM_TILES) }
            }
        },
        // NumTileColumns is bigger than number of pipes for scalable encoding
        {/*62*/ MFX_ERR_INVALID_VIDEO_PARAM, RESET | CQP | SCALABLE_PIPE,
            {
                { NO_ENCODING, SET_W(2816), SET_H(2304), SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { NO_ENCODING, SET_NUM_ROWS(MAX_NUM_ROWS), SET_NUM_COLS(MAX_PIPES_SUPPORTED * 2) }
            }
        },

        // below test cases check on-the-fly change of number of tile rows and tile columns for CQP
        {/*63*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*64*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
            }
        },
        {/*65*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*66*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CQP | SCALABLE_PIPE,
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
        {/*67*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*68*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
            }
        },
        {/*69*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*70*/ MFX_ERR_NONE, DYNAMIC_CHANGE | CBR | SCALABLE_PIPE,
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
        {/*71*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*72*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
            }
        },
        {/*73*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR,
            {
                { ITER_LENGTH, SET_W(1408), SET_H(1152),
                               SET_NUM_ROWS(4), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(4) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(1) },
                { ITER_LENGTH, SET_NUM_ROWS(1), SET_NUM_COLS(2) },
                { ITER_LENGTH, SET_NUM_ROWS(2), SET_NUM_COLS(1) },
            }
        },
        {/*74*/ MFX_ERR_NONE, DYNAMIC_CHANGE | VBR | SCALABLE_PIPE,
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

    mfxU32 GetDefaultTargetKbps(const Resolution& resol)
    {
        if (resol <= Resolution(176, 144)) return 100;
        if (resol <= Resolution(352, 288)) return 400;
        if (resol <= Resolution(704, 576)) return 1600;
        if (resol <= Resolution(1408, 1152)) return 6400;
        if (resol <= Resolution(2816, 2304)) return 25600;
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
                mfxU32 w = extPar.FrameWidth ? extPar.FrameWidth : par.mfx.FrameInfo.Width;
                mfxU32 h = extPar.FrameHeight ? extPar.FrameHeight : par.mfx.FrameInfo.Height;
                par.mfx.TargetKbps = GetDefaultTargetKbps(Resolution(w, h));
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
        mfxU32 m_firstFrame;
        mfxU32 m_numFramesToEncode;

        mfxExtBuffer* m_extBufs[MAX_EXT_BUFFERS][2];

        Iteration(
            const mfxVideoParam& defaults,
            const tc_struct::params_for_iteration& iterPar,
            mfxU32 type,
            mfxU32 firstFrame);
    };

    Iteration::Iteration(
        const mfxVideoParam& defaults,
        const tc_struct::params_for_iteration& iterPar,
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
        SetAdditionalParams(m_param[SET], m_extParam[SET], type);

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

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        std::vector<Iteration*>* m_pIterations;
        std::map<mfxU32, mfxU8*>* m_pInputFrames;
        mfxU32 m_testType;
        mfxU32 m_testId;
        mfxU8* m_pDecodedFrame;
    public:
        BitstreamChecker(
            std::vector<Iteration*>* pIterations,
            std::map<mfxU32, mfxU8*>* pSurfaces,
            mfxU32 testType,
            mfxU32 testId)
            : tsVideoDecoder(MFX_CODEC_VP9)
            , m_pIterations(pIterations)
            , m_pInputFrames(pSurfaces)
            , m_testType(testType)
            , m_testId(testId)
        {
            // first iteration always has biggest possible resolution
            // we use it to allocate buffers to store decoded frame
            mfxU32 w = (*m_pIterations)[0]->m_param[CHECK].mfx.FrameInfo.Width;
            mfxU32 h = (*m_pIterations)[0]->m_param[CHECK].mfx.FrameInfo.Height;
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
            mfxU32 w = iter.m_extParam[CHECK].FrameWidth;
            mfxU32 h = iter.m_extParam[CHECK].FrameHeight;
            mfxU32 pitch = (m_pSurf->Data.PitchHigh << 16) + m_pSurf->Data.PitchLow;
            //DumpDecodedFrame(m_pSurf->Data.Y, m_pSurf->Data.UV, w, h, pitch);
            for (mfxU32 i = 0; i < h; i++)
            {
                memcpy(m_pDecodedFrame + i*w, m_pSurf->Data.Y + i*pitch, w);
            }

            mfxU8* pInputFrame = (*m_pInputFrames)[hdr.FrameOrder];
            mfxF64 psnr = PSNR_Y_8bit(pInputFrame, m_pDecodedFrame, w * h);
            const mfxF64 minPsnr = GetMinPSNR(iter.m_param[CHECK]);
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

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_cases[id];

        if (false == NeedToRun(tc.type))
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
        inputStreams.emplace(Resolution(5632, 4608), g_tsStreamPool.Get("forBehaviorTest/salesman_5632x4608_2.yuv"));
        inputStreams.emplace(Resolution(7680, 6288), g_tsStreamPool.Get("forBehaviorTest/salesman_7680x6288_2.yuv"));
        g_tsStreamPool.Reg();

        bool errorExpected = false;

        std::vector<Iteration*> iterations;
        std::map<mfxU32, mfxU8*> inputFrames;
        std::map<mfxU32, mfxU8*>* pInputFrames = errorExpected ?
            0 : &inputFrames; // if error is expected from Reset, there is no need to check PSNR
                              // and thus no need to save input frames

        // prepare all iterations for the test
        mfxU32 totalFramesToEncode = 0;
        const mfxU32 numIterations = CalcIterations(tc, totalFramesToEncode);
        mfxU32 iterationStart = 0;

        for (mfxU8 idx = 0; idx < numIterations; idx++)
        {
            const mfxVideoParam& defaults = (idx == 0) ? *m_pPar : iterations[idx - 1]->m_param[CHECK];
            Iteration* pIter = new Iteration(defaults, tc.iteration_par[idx], tc.type, iterationStart);

            iterationStart += pIter->m_numFramesToEncode;
            iterations.push_back(pIter);
        }

        // prepare surface feeder
        const mfxFrameInfo& fi = iterations[0]->m_param[CHECK].mfx.FrameInfo;
        const mfxU32 frameWidth = iterations[0]->m_extParam[CHECK].FrameWidth;
        const mfxU32 frameHeight = iterations[0]->m_extParam[CHECK].FrameHeight;
        tsRawReader* feeder = new SurfaceFeeder(
            pInputFrames,
            frameWidth ? frameWidth : fi.Width,
            frameHeight ? frameHeight : fi.Height,
            iterations[0]->m_firstFrame,
            inputStreams[Resolution(fi.Width, fi.Height)].c_str(),
            fi);

        m_filler = feeder;

        // prepare bitstream checker
        BitstreamChecker bs(&iterations, pInputFrames, tc.type, id);
        m_bs_processor = &bs;

        // run the test
        MFXInit(); TS_CHECK_MFX;
        Load();

        *m_pPar = iterations[0]->m_param[SET];
        mfxVideoParam* pOutPar = &iterations[0]->m_param[GET];

        // QUERY SECTION
        if (tc.type & QUERY)
        {
            g_tsStatus.expect(tc.sts);
            TS_TRACE(m_pPar);
            mfxStatus sts = MFXVideoENCODE_Query(m_session, m_pPar, pOutPar);
            TS_TRACE(pOutPar);
            g_tsStatus.check(sts);
            CheckOutputParams("Query", *iterations[0]);
        }

        // INIT SECTION
        if (tc.type & INIT || RESET | totalFramesToEncode)
        {
            g_tsStatus.expect(tc.sts);
            TS_TRACE(m_pPar);
            mfxStatus sts = MFXVideoENCODE_Init(m_session, m_pPar);
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
            mfxStatus sts = MFXVideoENCODE_Reset(m_session, m_pPar);
            if (tc.type & RESET)
            {
                g_tsStatus.check(sts);
            }
            pOutPar = &iterations[idx]->m_param[GET];
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

        if (pInputFrames)
        {
            for (std::map<mfxU32, mfxU8*>::iterator i = inputFrames.begin(); i != inputFrames.end(); i++)
            {
                delete[] i->second;
            }
            inputFrames.clear();
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

    TS_REG_TEST_SUITE_CLASS(vp9e_tiles);
}

