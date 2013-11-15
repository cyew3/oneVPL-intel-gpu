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
#include "mock_mfx_plugins.h"

inline bool operator == (const mfxPluginUID &lhs, const mfxPluginUID & rhs) {
    return !memcmp(lhs.Data, rhs.Data, sizeof(mfxPluginUID));
}

inline bool operator != (const mfxPluginUID &lhs, const mfxPluginUID & rhs) {
    return !(lhs == rhs);
}

inline std::ostream & operator << (std::ostream &os, const mfxPluginUID &lhs) {
    os<<lhs.Data[0] << "-";
    os<<lhs.Data[1] << "-";
    os<<lhs.Data[2] << "-";
    os<<lhs.Data[3] << "-";
    os<<lhs.Data[4] << "-";
    os<<lhs.Data[5] << "-";
    os<<lhs.Data[6] << "-";
    os<<lhs.Data[7] << "-";
    os<<lhs.Data[8] << "-";
    os<<lhs.Data[9] << "-";
    os<<lhs.Data[10] << "-";
    os<<lhs.Data[11] << "-";
    os<<lhs.Data[12] << "-";
    os<<lhs.Data[13] << "-";
    os<<lhs.Data[14] << "-";
    os<<lhs.Data[15] << "-";
    return os;
}



SUITE(DispatcherWithPlugins) {

    const char rootPluginPath[] = "Software\\Intel\\MediaSDK\\Dispatch\\Plugin";
    struct WhenRegistryContainsOnePlugin {

        MockPlugin &mockDecoder;
        MFXPluginAdapter<MFXEncoderPlugin> mAdapter ;
        mfxPluginParam plgParams;
        mfxPluginUID guid1;
        mfxPlugin mfxPlg ;
        mfxSession session;
        WhenRegistryContainsOnePlugin();

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

        DECLARE_MOCK_METHOD2(mfxStatus , Create, mfxPluginUID , mfxPlugin* );

        TEST_METHOD_TYPE(Create) createArgs;

        ~WhenRegistryContainsOnePlugin() {
            RegDeleteTreeA(HKEY_LOCAL_MACHINE, rootPluginPath);
        }
    };

    struct MockPluginCreator {
        
    };
    
    WhenRegistryContainsOnePlugin *g_context;

    EXTERN_C mfxStatus __declspec(dllexport) MFX_CDECL CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
        return g_context->Create(uid, plugin);
    }

    WhenRegistryContainsOnePlugin::WhenRegistryContainsOnePlugin():
        mockDecoder(*new MockPlugin()),
        mAdapter(&mockDecoder)
    {
        mfxPlg = mAdapter.operator mfxPlugin();

        //default plugin
        mfxPluginUID guid0 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        guid1 = guid0;
        
        plgParams.CodecId = MFX_CODEC_HEVC;
        plgParams.PluginUID = guid1;
        plgParams.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
        //set path to mock encoder plugin
        createKey(plgParams, "N2", "mfx_pipeline_tests_d.exe", true);

        //each test uses blank mock object
        g_context = this;
    }
    //////////////////////////////////////////////////////////////////////////

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WrongGuid) {
        mfxPluginUID guid1 = {};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WrongCodecID) {
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_AVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WronType) {
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_GENERAL, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, PluginInit_Equals_to_Null_reult_in_LoadFailure) {
        mfxPlg.PluginInit = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, PluginClose_Equals_to_Null_reult_in_LoadFailure) {
        mfxPlg.PluginClose = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, GetPluginParam_Equals_to_Null_reult_in_LoadFailure) {
        mfxPlg.GetPluginParam = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }
    
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, EXECUTE_Equals_to_Null_reult_in_LoadFailure) {
        mfxPlg.Execute = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, FreeResource_Equals_to_Null_reult_in_LoadFailure) {
        mfxPlg.FreeResources = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, EncodeFrameSubmit_Equals_to_Null_reult_in_LoadFailure) {
        mfxPlg.Video->EncodeFrameSubmit = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE!= MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, DecodeCallbacks_AND_VPPcallbacks_Equals_to_Null_result_in_LoadSUCCESS_for_encoder_plugin) {
        mfxPlg.Video->DecodeFrameSubmit = NULL;
        mfxPlg.Video->DecodeHeader = NULL;
        mfxPlg.Video->GetPayload = NULL;
        mfxPlg.Video->VPPFrameSubmit = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockDecoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, Loader_calls_CreatePlugin_with_registered_guid_and_not_null_mfxPlugin) {
        mfxSession session;
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1);

        TEST_METHOD_TYPE(Create) params;
        _Create.WasCalled(&params);

        CHECK_EQUAL(guid1, params.value0);
        CHECK(NULL != params.value1);

        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, MFXClose_destroy_loaded_plugin) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockDecoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1);
        MFXClose(session);
        TEST_METHOD_TYPE(MockPlugin::PluginClose) plgClose;
        CHECK(mockDecoder._PluginClose.WasCalled());
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, MFXUserUnload_destroys_only_loaded_plugin) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockDecoder._GetPluginParam.WillReturn(getPluginParams);
        
        TEST_METHOD_TYPE(MockPlugin::PluginClose) plgClose;

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1);
        mfxPluginUID guid0 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x17};
        
        CHECK(MFX_ERR_NONE != MFXVideoUSER_UnLoad(session, guid0));
        CHECK(!mockDecoder._PluginClose.WasCalled());

        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_UnLoad(session, guid1));
        CHECK(mockDecoder._PluginClose.WasCalled());

        //MFXClose cannot close alreadyclosed plugin
        MFXClose(session);
        CHECK(!mockDecoder._PluginClose.WasCalled());
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, UnloadSucess_if_loadSucess) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockDecoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1));
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_UnLoad(session, guid1));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, Enumerate_wrong_type) {
        mfxSession session;
        mfxPluginDescription dsc;
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        int oneMorePlugin = 0;
        while (MFX_ERR_NONE == MFXVideoUSER_Enumerate(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, oneMorePlugin++, &dsc));
        CHECK_EQUAL(1, oneMorePlugin); 
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testEnumerate) {
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
        CHECK_EQUAL(guid1, dsc.PluginUID);
        CHECK_EQUAL("N2", dsc.Name);
        MFXClose(session);
    }
}