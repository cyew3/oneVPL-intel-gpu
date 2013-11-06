/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfxsession.h"
#include "mfxplugin.h"
#include "winerror.h"

SUITE(DispatcherWithPlugins) {
    const char rootPluginPath[] = "Software\\Intel\\MediaSDK\\Dispatch\\Plugin";
    struct WhenRegistryContainsOnePlugin {
        WhenRegistryContainsOnePlugin() {
            //default plugin
            mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
            mfxPluginParam plg;
            plg.CodecId = MFX_CODEC_HEVC;
            plg.PluginUID = guid1;
            plg.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
            //set path to mock encoder plugin
            createKey(plg, "N2", "mock_encoder_plugin.dll", true);
        }

        void createKey(mfxPluginParam &pluginParams, const std::string &name, const std::string &path, int isDefault) {
            HKEY hk1;
            RegCreateKeyA(HKEY_LOCAL_MACHINE, rootPluginPath, &hk1);
            HKEY hk;
            RegCreateKeyA(hk1, name.c_str(), &hk);

            RegSetValueExA(hk, "codecid", 0, REG_DWORD, (BYTE*)&pluginParams.CodecId, sizeof(pluginParams.CodecId));
            RegSetValueExA(hk, "default", 0, REG_DWORD, (BYTE*)&(int)isDefault, sizeof(isDefault));
            RegSetValueExA(hk, "GUID", 0, REG_BINARY, (BYTE*)&pluginParams.PluginUID.Data, sizeof(pluginParams.PluginUID.Data));
            RegSetValueExA(hk, "Name", 0, REG_SZ, (BYTE*)name.c_str(), name.length());
            RegSetValueExA(hk, "Path", 0, REG_SZ, (BYTE*)path.c_str(), path.length());
            RegSetValueExA(hk, "Type", 0, REG_DWORD, (BYTE*)&pluginParams.Type, sizeof(pluginParams.Type));
            RegCloseKey(hk);
            RegCloseKey(hk1);
        }
        
        ~WhenRegistryContainsOnePlugin() {
            RegDeleteTreeA(HKEY_LOCAL_MACHINE, rootPluginPath);
        }
    };
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WrongGuid) {
        mfxSession session;
        mfxPluginUID guid1 = {};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WrongCodecID) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_AVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WronType) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_GENERAL, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, LoadSuccess) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoadAndUnload) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_UnLoad(session, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testEnumerate_wrong_type) {
        mfxSession session;
        mfxPluginDescription dsc;
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        int oneMorePlugin = 0;
        while (MFX_ERR_NONE == MFXVideoUSER_Enumerate(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, oneMorePlugin++, &dsc));
        CHECK_EQUAL(1, oneMorePlugin); 
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testEnumerate) {
        mfxSession session;
        mfxPluginDescription dsc = {};
        std::vector<mfxChar> name;
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        int oneMorePlugin = 0;
        while (MFX_ERR_NONE == MFXVideoUSER_Enumerate(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, oneMorePlugin++, &dsc));
        dsc.NameAlloc = dsc.NameLength;
        name.resize(dsc.NameAlloc);
        dsc.Name = &name.front();
        oneMorePlugin = 0;
        while (MFX_ERR_NONE == MFXVideoUSER_Enumerate(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, oneMorePlugin++, &dsc));
        
        CHECK_EQUAL(2, oneMorePlugin);
        MFXClose(session);
    }
}