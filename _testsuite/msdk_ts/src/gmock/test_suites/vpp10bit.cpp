/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp10bit
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR = 1,
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
    // P010->P010, Shift = 0->Shift = 0, 720x480->720x480
    {/*00*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*01*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*02*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*03*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->P010, Shift = 1->Shift = 1, 720x480->720x480
    {/*04*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*05*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*06*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*07*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->P010, Shift = 0->Shift = 0, 720x480->1440x960
    {/*08*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*09*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*10*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*11*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->P010, Shift = 1->Shift = 1, 720x480->1440x960
    {/*12*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*13*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*14*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*15*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->P010, Shift = 0->Shift = 0, 720x480->368x240
    {/*16*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*17*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*18*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*19*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->P010, Shift = 1->Shift = 1, 720x480->368x240
    {/*20*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*21*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*22*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*23*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 0->Shift = 0, 720x480->720x480
    {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*26*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*27*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*28*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*29*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 1->Shift = 1, 720x480->720x480
    {/*30*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*33*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 1->Shift = 1, 720x480->720x480
    {/*34*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*35*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*36*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*37*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 0->Shift = 0, 720x480->1440x960
    {/*38*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*39*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*40*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*41*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*42*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*43*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 1->Shift = 1, 720x480->1440x960
    {/*44*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*45*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*46*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*47*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 0->Shift = 0, 720x480->368x240
    {/*48*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*50*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*51*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*52*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*53*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 1->Shift = 1, 720x480->368x240
    {/*54*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*55*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*56*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*57*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 0->Shift = 0, 720x480->720x480
    {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*60*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*61*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*62*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*63*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 1->Shift = 1, 720x480->720x480
    {/*64*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*65*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*66*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*67*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 0->Shift = 0, 720x480->1440x960
    {/*68*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*69*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*70*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*71*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*72*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*73*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 1->Shift = 1, 720x480->1440x960
    {/*74*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*75*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*76*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*77*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 0->Shift = 0, 720x480->368x240
    {/*78*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*79*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*80*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*81*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*82*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*83*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 1->Shift = 1, 720x480->368x240
    {/*84*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*85*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*86*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*87*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P210->P210, Shift = 0->Shift = 0, 720x480->720x480
    {/*88*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 1->Shift = 1, 720x480->720x480
    {/*89*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 0->Shift = 0, 720x480->1440x960
    {/*90*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 1->Shift = 1, 720x480->1440x960
    {/*91*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 0->Shift = 0, 720x480->368x240
    {/*92*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 1->Shift = 1, 720x480->368x240
    {/*93*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 0->Shift = 0, 720x480->720x480
    {/*94*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 1->Shift = 1, 720x480->720x480
    {/*95*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 0->Shift = 0, 720x480->1440x960
    {/*96*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 1->Shift = 1, 720x480->1440x960
    {/*97*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 0->Shift = 0, 720x480->368x240
    {/*98*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 1->Shift = 1, 720x480->368x240
    {/*99*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 0->Shift = 0, 720x480->720x480
    {/*100*/MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 1->Shift = 1, 720x480->720x480
    {/*101*/MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 0->Shift = 0, 720x480->1440x960
    {/*102*/MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 1->Shift = 1, 720x480->1440x960
    {/*103*/MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 0->Shift = 0, 720x480->368x240
    {/*104*/MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 1->Shift = 1, 720x480->368x240
    {/*105*/MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    m_par.AsyncDepth = 1;

    SETPARS(m_pPar, MFX_PAR);

    mfxStatus sts_query = tc.sts_query,
              sts_init  = tc.sts_init;

    if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS)
    {
        sts_query = MFX_WRN_PARTIAL_ACCELERATION;
        sts_init  = MFX_WRN_PARTIAL_ACCELERATION;
    }
    else
    {
        if (g_tsHWtype >= MFX_HW_KBL && m_par.vpp.Out.FourCC == MFX_FOURCC_P010
            && sts_query == MFX_WRN_PARTIAL_ACCELERATION
            && sts_init  == MFX_WRN_PARTIAL_ACCELERATION)
        {
            // KBL on Windows supports p010 output
            if (!m_par.vpp.Out.BitDepthLuma || !m_par.vpp.Out.BitDepthChroma)
            {
                sts_query = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                sts_init  = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            else
            {
                sts_query = MFX_ERR_NONE;
                sts_init  = MFX_ERR_NONE;
            }
        }
    }

    if (g_tsImpl == MFX_IMPL_SOFTWARE)
    {
        sts_query = MFX_ERR_NONE;
        sts_init  = MFX_ERR_NONE;
    }

    if (m_par.vpp.In.FourCC == MFX_FOURCC_P210)
    {
        sts_query = MFX_WRN_PARTIAL_ACCELERATION;
        sts_init  = MFX_WRN_PARTIAL_ACCELERATION;
    }

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();

    tsExtBufType<mfxVideoParam> par_out;
    par_out=m_par;

    g_tsStatus.expect(sts_query);
    Query(m_session, m_pPar, &par_out);

    g_tsStatus.expect(sts_init);
    Init(m_session, m_pPar);

    g_tsStatus.expect(MFX_ERR_NONE);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp10bit);

}
