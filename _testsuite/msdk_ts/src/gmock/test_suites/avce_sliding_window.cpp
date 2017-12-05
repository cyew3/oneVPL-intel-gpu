/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define TEST_NAME avce_sliding_window

namespace TEST_NAME
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
        static const mfxU32 max_num_ctrl = 10;
        static const mfxU32 max_num_ctrl_par = 10;
        struct tc_struct
        {
            mfxStatus sts;
            struct tctrl {
                mfxU32 type;
                const tsStruct::Field* field;
                mfxU32 par[max_num_ctrl_par];
            } ctrl[max_num_ctrl];
        };
        static const tc_struct test_case[];
        enum CTRL_TYPE
        {
            STAGE = 0xF0000000
            , INIT = 0x80000000
            , RESET = 0x40000000
            , QUERY = 0x20000000
            , MFXVPAR = 1 << 1
            , BUFPAR_2 = 1 << 2
            , BUFPAR_3 = 1 << 3
        };
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 15 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*01*/ MFX_ERR_NONE,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 4500 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { INIT | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 4500 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { INIT | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*07*/ MFX_ERR_UNSUPPORTED,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 6000 } }
        }
        },
        {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_VBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
#if !defined(LINUX_TARGET_PLATFORM_BXT) && !defined (LINUX_TARGET_PLATFORM_BXTMIN) && !defined (LINUX_TARGET_PLATFORM_CFL)
        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 15 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*10*/ MFX_ERR_NONE,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 4500 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { INIT | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 4500 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { INIT | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*16*/ MFX_ERR_UNSUPPORTED,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 6000 } }
        }
        },
        {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{} },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_QVBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 5000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
#endif  // !defined(LINUX_TARGET_PLATFORM_BXT) && !defined (LINUX_TARGET_PLATFORM_BXTMIN) && !defined (LINUX_TARGET_PLATFORM_CFL)
#if defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN) || defined (LINUX_TARGET_PLATFORM_CFL)
        {/*18*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 15 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_CBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*19*/ MFX_ERR_NONE,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_CBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*20*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | INIT | RESET | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_CBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 2500 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_CBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_CBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
        {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        { { QUERY | BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCSize,{ 30 } },
        { BUFPAR_3, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps,{ } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,{ MFX_RATECONTROL_CBR } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,{ 30 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD,{ 1 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,{ 2000 } },
        { MFXVPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,{ 2000 } }
        }
        },
#endif  // defined(LINUX_TARGET_PLATFORM_BXT) || defined (LINUX_TARGET_PLATFORM_BXTMIN) || defined (LINUX_TARGET_PLATFORM_CFL)
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        MFXInit();
        m_session = tsSession::m_session;
        mfxExtCodingOption3& ext3 = m_par;

        for (mfxU32 i = 0; i < max_num_ctrl; i++)
        {
            if (tc.ctrl[i].type & MFXVPAR)
            {
                if (tc.ctrl[i].field)
                {
                    tsStruct::set(&m_par, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
                }

            }

            if (tc.ctrl[i].type & BUFPAR_3)
            {
                if (tc.ctrl[i].field)
                {
                    tsStruct::set(&ext3, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
                }
            }
        }

        g_tsStatus.expect(tc.sts);

        mfxVideoParam out = m_par;

        if (tc.ctrl[0].type & QUERY)
        {
            Query(m_session, &m_par, &out);
        }

        if (tc.ctrl[0].type & INIT)
        {
            Init(m_session, &out);
        }

        if (tc.ctrl[0].type & RESET)
        {
            Reset(m_session, &out);
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}