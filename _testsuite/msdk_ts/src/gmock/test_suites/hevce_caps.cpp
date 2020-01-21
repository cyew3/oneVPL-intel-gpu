/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018-2019 Intel Corporation. All Rights Reserved.

File Name: hevce_caps.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include <map>

namespace hevce_caps
{
    const mfxU32 TableCapsLowpower[][7] =
    {
        //  Platform          |MaxNumOfROI|SliceStructure|LCUSizeSupported|MaxPicWidth|MaxPicHeight|SliceByteSizeCtrl|SliceLevelReportSupport|
        /*  SKL, KBL, CFL, GLK */{ 0, 0, 0, 0, 0, 0, 0 },
        /*  CNL                */{ 8, 2, 4, 8192, 8192, 1, 1 },
        /*  ICL, JSL           */{ 16, 4, 4, 8192, 8192, 1, 1 },
        /*  LKF                */{ 16, 4, 4, 4096, 4096, 1, 1 },
    };

    const mfxU32 TableCapsLegacy[][7] =
    {
        //  Platform          |MaxNumOfROI|SliceStructure|LCUSizeSupported|MaxPicWidth|MaxPicHeight|SliceByteSizeCtrl|SliceLevelReportSupport|
        /*  SKL, KBL, CFL, GLK */{ 16, 4, 0, 4096, 2176, 0, 0 },
        /*  CNL                */{ 0, 4, 6, 4096, 2176, 0, 1 },
        /*  ICL, JSL           */{ 16, 4, 6, 8192, 8192, 0, 1 },
        /*  LKF                */{ 16, 4, 6, 4096, 4096, 0, 1 },
    };

    struct tc_struct
    {
        mfxStatus   sts;
        mfxU8 lowpower;
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_CODINGOPTION_ON },
        {/*01*/ MFX_ERR_NONE, MFX_CODINGOPTION_OFF },
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    int GetPlatformIndex(HWType& hwType)
    {
        switch (hwType)
        {
        case MFX_HW_SKL:
        case MFX_HW_KBL:
        case MFX_HW_GLK:
        case MFX_HW_CFL:
        case MFX_HW_APL: return 0;
        case MFX_HW_CNL: return 1;
        case MFX_HW_JSL:
        case MFX_HW_ICL: return 2;
        case MFX_HW_LKF: return 3;
        default: return -1;
        }
    }

    mfxStatus CompareCaps(std::map <std::string, std::pair<mfxU32, mfxU32>>& mapCaps) {
        mfxStatus checkSts = MFX_ERR_NONE;

        for (auto const& it : mapCaps)
        {
            if (it.second.first != it.second.second) {
                checkSts = MFX_ERR_UNSUPPORTED;
                g_tsLog << "Error: Wrong " << it.first << "!\n";
                g_tsLog << "Actual: " << it.second.first << "!\n";
                g_tsLog << "Expected: " << it.second.second << "!\n";
            }
        }

        return checkSts;
    }

    int TestSuite::RunTest(const unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        mfxStatus sts = MFX_ERR_NONE;
        g_tsConfig.lowpower = tc.lowpower;

        MFXInit();

        int plt = GetPlatformIndex(g_tsHWtype);

        if ((0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                throw tsSKIP;
            }

            if (g_tsHWtype < MFX_HW_CNL && tc.lowpower == MFX_CODINGOPTION_ON)
            {
                g_tsLog << "WARNING: Unsupported lowpower!\n";
                throw tsSKIP;
            }

            if (plt == -1)
            {
                g_tsLog << "WARNING: No caps values for platform!\n";
                throw tsSKIP;
            }
        }

        ENCODE_CAPS_HEVC capsPlatform = {};
        std::map <std::string, std::pair<mfxU32, mfxU32>> mapCaps;
        const mfxU32 *platformCaps;

        if (tc.lowpower == MFX_CODINGOPTION_ON)
            platformCaps = TableCapsLowpower[plt];
        else
            platformCaps = TableCapsLegacy[plt];

        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);
        g_tsStatus.check(GetCaps(&caps, &capSize));

        mapCaps["MaxNumOfROI"] = std::make_pair((mfxU32)caps.MaxNumOfROI, platformCaps[0]);
        mapCaps["SliceStructure"] = std::make_pair((mfxU32)caps.SliceStructure, platformCaps[1]);
        mapCaps["LCUSizeSupported"] = std::make_pair((mfxU32)caps.LCUSizeSupported, platformCaps[2]);
        mapCaps["MaxPicWidth"] = std::make_pair(caps.MaxPicWidth, platformCaps[3]);
        mapCaps["MaxPicHeight"] = std::make_pair(caps.MaxPicHeight, platformCaps[4]);
        mapCaps["SliceByteSizeCtrl"] = std::make_pair((mfxU32)caps.SliceByteSizeCtrl, platformCaps[5]);
        mapCaps["SliceLevelReportSupport"] = std::make_pair((mfxU32)caps.SliceLevelReportSupport, platformCaps[6]);

        sts = CompareCaps(mapCaps);
        g_tsStatus.check(sts);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_caps);
};