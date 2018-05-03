/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*!

\file avce_mfe_query.cpp
\brief Gmock test avce_mfe_query

Description:

This test checks behavior of the encoding query for various values of the MFE
parameters (fields MFMode and MaxNumFrames) combined with other parameters that
affects the MFE functionality

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run Query
- Check query status
- Check output MFE parameters

*/

#include "ts_encoder.h"

namespace
{

typedef struct
{
    mfxSession      session;
    mfxVideoParam*  pPar;
}InitPar;

enum QueryMode
{
    NullIn = 0,
    InOut  = 1,
};

enum COpt {
    COptNone           = 0,
    COptMAD            = 1 << 0,
    COptIntRefType     = 1 << 1,
    COptMBQP           = 1 << 2,
    COptDisableSkipMap = 1 << 3,

    COpt2All = COptMAD | COptIntRefType,
    COpt3All = COptMBQP | COptDisableSkipMap,
};

typedef struct
{
    mfxU32    func;
    mfxStatus sts;

    mfxU16    width;
    mfxU16    height;
    mfxU16    cropW;
    mfxU16    cropH;

    mfxU16    numSlice;
    mfxU16    cOpt;

    mfxU16    inMFMode;
    mfxU16    inMaxNumFrames;
    mfxU16    expMFMode;
    mfxU16    expMaxNumFrames;
} tc_struct;

tc_struct test_case[] =
{
        // Test case when input query parameter is null.
        // None zero output MFE parameters are expected
        {/*00*/ NullIn, MFX_ERR_NONE, /*width*/720, /*height*/480,
                /*cropW*/480, /*cropW*/480, 0, COptNone,
                0, 0, 0, 0},

        // Test cases when inMFMode = MFX_MF_AUTO
        // Negative status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected
        // when input MaxNumFrames exceeds the maximum supported limit

        {/*01*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/0,
                MFX_MF_AUTO, /*expMaxNumFrames*/0},
        {/*02*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*03*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*04*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
        {/*05*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/4,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},

        {/*06*/ InOut, MFX_ERR_NONE,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*07*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        {/*08*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
        {/*09*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/4,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},

        {/*10*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*11*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        {/*12*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_AUTO, /*inMaxNumFrames*/2, MFX_MF_AUTO, 2},
        {/*13*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_AUTO, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*14*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*15*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_AUTO, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*16*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*17*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_AUTO, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*18*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_AUTO, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*19*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        // Test cases when inMFMode = MFX_MF_DEFAULT
        // Mode MFX_MF_DEFAULT is not supported when MaxNumFrames > 0.
        // Status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected in these cases

        {/*20*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/0,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/0},
        {/*21*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*22*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*23*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
        {/*24*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/4,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},

        {/*25*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*26*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        {/*27*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
        {/*28*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/4,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},

        {/*29*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*30*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        {/*31*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, 2},
        {/*32*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*33*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*34*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*35*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*36*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*37*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*38*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},


        // Test cases when inMFMode = MFX_MF_DISABLED
        // Mode MFX_MF_DISABLED is not supported when MaxNumFrames > 1.
        // Status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected in these cases

        {/*39*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/0,
                MFX_MF_DISABLED, /*expMaxNumFrames*/0},
        {/*40*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*41*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*42*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
        {/*43*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/4,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},

        {/*44*/ InOut, MFX_ERR_NONE,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*45*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        {/*46*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
        {/*47*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/4,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},

        {/*48*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*49*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        {/*50*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, 2},
        {/*51*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_DISABLED, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*52*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*53*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_DISABLED, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*54*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*55*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_DISABLED, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*56*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*57*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        // Test cases when inMFMode = MFX_MF_MANUAL
        // Negative status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected
        // when input MaxNumFrames exceeds the maximum supported limit

        {/*58*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/0,
                MFX_MF_MANUAL, /*expMaxNumFrames*/0},
        {/*59*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/1,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*60*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},
        {/*61*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/3,
                MFX_MF_MANUAL, /*expMaxNumFrames*/3},
        {/*62*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/4,
                MFX_MF_MANUAL, /*expMaxNumFrames*/3},

        {/*63*/ InOut, MFX_ERR_NONE,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/1,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*64*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},

        {/*65*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/3,
                MFX_MF_MANUAL, /*expMaxNumFrames*/3},
        {/*66*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/4,
                MFX_MF_MANUAL, /*expMaxNumFrames*/3},

        {/*67*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/1,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*68*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, COptNone,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},

        {/*69*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, 2},
        {/*70*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMAD,
                MFX_MF_MANUAL, /*inMaxNumFrames*/3,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},

        {/*71*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},
        {/*72*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptMBQP,
                MFX_MF_MANUAL, /*inMaxNumFrames*/3,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},

        {/*73*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},
        {/*74*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptDisableSkipMap,
                MFX_MF_MANUAL, /*inMaxNumFrames*/3,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},

        {/*75*/ InOut, MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_MANUAL, /*inMaxNumFrames*/1,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*76*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, COptIntRefType,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},

        // Test cases when inMFMode > MFX_MF_MANUAL
        // Mode values that are more than MFX_MF_MANUAL are not supported.
        // Status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected in these cases

        {/*77*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/0,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/0},
        {/*78*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*79*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*80*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
        {/*81*/ InOut, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, COptNone,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/4,
                MFX_MF_AUTO, /*expMaxNumFrames*/3},
};

void set_params(tsVideoEncoder& enc, tc_struct& tc,
        tsExtBufType<mfxVideoParam>& pout)
{
    enc.m_par.mfx.FrameInfo.Width = MSDK_ALIGN16(tc.width);
    enc.m_par.mfx.FrameInfo.CropW = MSDK_ALIGN16(tc.cropW);
    enc.m_par.mfx.FrameInfo.CropH = MSDK_ALIGN16(tc.cropH);

    if (tc.width > 1920 && tc.height > 1088) {
        enc.m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        enc.m_par.mfx.FrameInfo.Height = MSDK_ALIGN32(tc.height);
    }
    else {
        enc.m_par.mfx.FrameInfo.Height = MSDK_ALIGN16(tc.height);
    }

    enc.m_par.mfx.NumSlice = tc.numSlice;
    pout.mfx.CodecId = enc.m_par.mfx.CodecId;

    mfxExtMultiFrameParam& mfp = enc.m_par;
    mfp.MFMode = tc.inMFMode;
    mfp.MaxNumFrames = tc.inMaxNumFrames;

    if (tc.cOpt & COpt2All) {
        mfxExtCodingOption2& copt2 = enc.m_par;

        if (tc.cOpt & COptMAD) {
            copt2.EnableMAD = MFX_CODINGOPTION_ON;
        }

        if (tc.cOpt & COptIntRefType) {
            copt2.IntRefType = 1;
        }

        mfxExtCodingOption2& copt2_out = pout;
    }
    if (tc.cOpt & COpt3All) {
        mfxExtCodingOption3& copt3 = enc.m_par;

        if (tc.cOpt & COptMBQP) {
            copt3.EnableMBQP = MFX_CODINGOPTION_ON;
        }

        if (tc.cOpt & COptDisableSkipMap) {
            copt3.MBDisableSkipMap = MFX_CODINGOPTION_ON;
        }

        mfxExtCodingOption3& copt3_out = pout;
    }

    // Update expected values for platforms != MFX_HW_SKL
    if (g_tsHWtype != MFX_HW_SKL) {
        if (tc.inMaxNumFrames) {
            tc.expMaxNumFrames = 1;

            if (tc.inMaxNumFrames != tc.expMaxNumFrames) {
                tc.sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            if (tc.inMFMode == MFX_MF_DISABLED) {
                tc.expMFMode == MFX_MF_DISABLED;
            }
        }
    }
}

int test(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    InitPar par = {enc.m_session, &enc.m_par};
    mfxStatus sts = MFX_ERR_NONE;

    tsExtBufType<mfxVideoParam> pout;
    mfxExtMultiFrameParam& mfe_out = pout;
    set_params(enc, tc, pout);

    g_tsStatus.expect(tc.sts);

    if (InOut == tc.func) {
        sts = enc.Query(enc.m_session, &enc.m_par, &pout);

        EXPECT_EQ(tc.expMaxNumFrames, mfe_out.MaxNumFrames);
        EXPECT_EQ(tc.expMFMode, mfe_out.MFMode);
    }
    else if (NullIn == tc.func) {
        sts = enc.Query(enc.m_session, 0, &pout);

        EXPECT_NE(tc.expMFMode, mfe_out.MFMode);
        EXPECT_NE(tc.expMaxNumFrames, mfe_out.MaxNumFrames);
    }
    else {
        std::cout << "ERROR: unknown test case function " << tc.func << "\n";
        throw tsFAIL;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_mfe_query, test, sizeof(test_case)/sizeof(tc_struct));

};
