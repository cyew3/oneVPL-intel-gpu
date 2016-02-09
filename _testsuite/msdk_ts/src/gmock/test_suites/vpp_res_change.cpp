#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_res_change
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
        RESET1,
        RESET2,
    };

    struct tc_struct
    {
        mfxStatus sts_reset_1;
        mfxStatus sts_reset_2;
        mfxU32 mode;
        mfxU32 frames_init;
        mfxU32 frames_reset_1;
        mfxU32 frames_reset_2;
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
    {/*00*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*01*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*04*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*05*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*06*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*09*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // FrameRateExtN
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*14*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*15*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*16*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*17*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*18*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*19*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 25},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // mfxExtVPPFrameRateConversion
    {/*20*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*21*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*22*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*23*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*24*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*25*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*26*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*27*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*28*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*29*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_FRAME_INTERPOLATION},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // PicStruct
    {/*30*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*31*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*32*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*33*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*34*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*35*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*36*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*37*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*38*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*39*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICTYPE_TOPFIELD},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // FourCC
    {/*40*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*41*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*42*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*43*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*44*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*45*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*46*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*47*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*48*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*49*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // mfxExtVPPDenoise
    {/*50*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*51*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*52*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*53*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*54*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*55*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*56*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*57*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*58*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*59*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // mfxExtVPPDetail
    {/*60*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*61*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*62*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*63*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*64*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*65*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*66*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*67*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*68*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*69*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 30},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // mfxExtVPPProcAmp
    {/*70*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*71*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*72*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*73*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*74*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*75*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*76*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*77*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*78*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*79*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 20.5},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    // mfxExtVPPImageStab
    {/*80*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*81*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*82*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*83*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*84*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*85*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*86*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*87*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*88*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*89*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_UPSCALE},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },

    {/*90*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*91*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*92*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*93*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*94*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 0, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*95*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    },
    {/*96*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, RESET1, 5, 0, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
        }
    },
    {/*97*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 640},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*98*/ MFX_ERR_NONE, MFX_ERR_NONE, RESET2, 5, 5, 5,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 480},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 480},
        }
    },
    {/*99*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESET2, 5, 5, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING},

            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.In.Height, 288},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Width, 352},
            {RESET1, &tsStruct::mfxVideoParam.vpp.Out.Height, 288},

            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.In.Height, 576},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Width, 720},
            {RESET2, &tsStruct::mfxVideoParam.vpp.Out.Height, 576},
        }
    }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.AsyncDepth = 1;

    SETPARS(&m_par, MFX_PAR);

    mfxStatus sts_init    = MFX_ERR_NONE,
              sts_reset_1 = tc.sts_reset_1,
              sts_reset_2 = tc.sts_reset_2;

    if (g_tsOSFamily == MFX_OS_FAMILY_LINUX && m_par.ExtParam[0]->BufferId == MFX_EXTBUFF_VPP_IMAGE_STABILIZATION)
        sts_init = sts_reset_1 = sts_reset_2 = MFX_WRN_FILTER_SKIPPED;

    g_tsStatus.expect(sts_init);
    Init(m_session, m_pPar);

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();
    AllocSurfaces();

    std::string stream_name = "forBehaviorTest/valid_yuv.yuv";
    tsRawReader stream(g_tsStreamPool.Get(stream_name), m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_surf_in_processor = &stream;

    g_tsStatus.expect(MFX_ERR_NONE);
    ProcessFrames(tc.frames_init);

    tsExtBufType<mfxVideoParam> par_reset (m_par);
    SETPARS(&par_reset, RESET1);

    par_reset.vpp.In.CropH = par_reset.vpp.In.Height;
    par_reset.vpp.In.CropW = par_reset.vpp.In.Width;
    par_reset.vpp.Out.CropH = par_reset.vpp.Out.Height;
    par_reset.vpp.Out.CropW = par_reset.vpp.Out.Width;

    g_tsStatus.expect(sts_reset_1);
    Reset(m_session, &par_reset);

    for (mfxU32 i = 0; i< m_pSurfPoolIn->PoolSize(); i++)
    {
        mfxFrameSurface1* surf = m_pSurfPoolIn->GetSurface();
        surf->Info.Width = surf->Info.CropW = par_reset.vpp.In.Width;
        surf->Info.Height = surf->Info.CropH = par_reset.vpp.In.Height;
    }

    for (mfxU32 i = 0; i< m_pSurfPoolOut->PoolSize(); i++)
    {
        mfxFrameSurface1* surf = m_pSurfPoolOut->GetSurface();
        surf->Info.Width = surf->Info.CropW = par_reset.vpp.Out.Width;
        surf->Info.Height = surf->Info.CropH = par_reset.vpp.Out.Height;
    }

    g_tsStatus.expect(MFX_ERR_NONE);
    ProcessFrames(tc.frames_reset_1);

    if (tc.mode == RESET2)
    {
        tsExtBufType<mfxVideoParam> par_reset2 (m_par);
        SETPARS(&par_reset2, RESET2);

        par_reset2.vpp.In.CropH = par_reset2.vpp.In.Height;
        par_reset2.vpp.In.CropW = par_reset2.vpp.In.Width;
        par_reset2.vpp.Out.CropH = par_reset2.vpp.Out.Height;
        par_reset2.vpp.Out.CropW = par_reset2.vpp.Out.Width;

        g_tsStatus.expect(sts_reset_2);
        Reset(m_session, &par_reset2);

        for (mfxU32 i = 0; i< m_pSurfPoolIn->PoolSize(); i++)
        {
            mfxFrameSurface1* surf = m_pSurfPoolIn->GetSurface();
            surf->Info.Width = surf->Info.CropW = par_reset2.vpp.In.Width;
            surf->Info.Height = surf->Info.CropH = par_reset2.vpp.In.Height;
        }

        for (mfxU32 i = 0; i< m_pSurfPoolOut->PoolSize(); i++)
        {
            mfxFrameSurface1* surf = m_pSurfPoolOut->GetSurface();
            surf->Info.Width = surf->Info.CropW = par_reset2.vpp.Out.Width;
            surf->Info.Height = surf->Info.CropH = par_reset2.vpp.Out.Height;
        }

        g_tsStatus.expect(MFX_ERR_NONE);
        ProcessFrames(tc.frames_reset_2);
    }

    g_tsStatus.expect(MFX_ERR_NONE);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_res_change);

}
