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
    {/*01*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*02*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*03*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
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
    {/*09*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*10*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*11*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
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
    {/*17*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*18*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*19*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
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
    {/*24*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*25*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*26*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*27*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 1->Shift = 1, 720x480->720x480
    {/*28*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*29*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*30*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*31*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 0->Shift = 0, 720x480->1440x960
    {/*32*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*33*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*34*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*35*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 1->Shift = 1, 720x480->1440x960
    {/*36*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*37*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*38*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*39*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 0->Shift = 0, 720x480->368x240
    {/*40*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*41*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*42*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*43*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->A2RGB10, Shift = 1->Shift = 1, 720x480->368x240
    {/*44*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*45*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*46*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*47*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 0->Shift = 0, 720x480->720x480
    {/*48*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*49*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*50*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*51*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 1->Shift = 1, 720x480->720x480
    {/*52*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*53*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*54*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*55*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 0->Shift = 0, 720x480->1440x960
    {/*56*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*57*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*58*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*59*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 1->Shift = 1, 720x480->1440x960
    {/*60*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*61*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*62*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*63*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 0->Shift = 0, 720x480->368x240
    {/*64*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*65*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*66*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*67*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P010->NV12, Shift = 1->Shift = 1, 720x480->368x240
    {/*68*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*69*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*70*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*71*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },

    // P210->P210, Shift = 0->Shift = 0, 720x480->720x480
    {/*72*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 1->Shift = 1, 720x480->720x480
    {/*73*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 0->Shift = 0, 720x480->1440x960
    {/*74*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 1->Shift = 1, 720x480->1440x960
    {/*75*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 0->Shift = 0, 720x480->368x240
    {/*76*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->P210, Shift = 1->Shift = 1, 720x480->368x240
    {/*77*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 0->Shift = 0, 720x480->720x480
    {/*78*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 1->Shift = 1, 720x480->720x480
    {/*79*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 0->Shift = 0, 720x480->1440x960
    {/*80*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 1->Shift = 1, 720x480->1440x960
    {/*81*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 0->Shift = 0, 720x480->368x240
    {/*82*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->A2RGB10, Shift = 1->Shift = 1, 720x480->368x240
    {/*83*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 0->Shift = 0, 720x480->720x480
    {/*84*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 1->Shift = 1, 720x480->720x480
    {/*85*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 0->Shift = 0, 720x480->1440x960
    {/*86*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 1->Shift = 1, 720x480->1440x960
    {/*87*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 1},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1440},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 960},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 0->Shift = 0, 720x480->368x240
    {/*88*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P210},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 368},
           {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 240},
           {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },

    // P210->NV12, Shift = 1->Shift = 1, 720x480->368x240
    {/*89*/ MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION, 0, {
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

    if (g_tsImpl == MFX_IMPL_SOFTWARE)
    {
        sts_query = MFX_ERR_NONE;
        sts_init = MFX_ERR_NONE;
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
