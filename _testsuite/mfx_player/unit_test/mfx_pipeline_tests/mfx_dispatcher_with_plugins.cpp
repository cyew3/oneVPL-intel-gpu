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
#include <iomanip>

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

#define MAKE_HEX( x ) \
    std::setw(2) << std::setfill('0') << std::hex << (int)( x )

    const char rootPluginPath[] = "Software\\Intel\\MediaSDK\\Dispatch\\Plugin";

    struct MFXAPIVerFinder {
        mfxVersion mVersion;
        MFXAPIVerFinder() {
            mfxSession session;
            MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
            MFXQueryVersion(session, &mVersion);
            MFXClose(session);
        }
        operator mfxVersion () const {
            return mVersion;
        }
    };
    

    struct WhenRegistryContainsNoPlugin {
        static MFXAPIVerFinder g_MfxApiVersion;
        static WhenRegistryContainsNoPlugin *g_context;

        MockPlugin &mockEncoder;
        MFXPluginAdapter<MFXEncoderPlugin> mAdapter ;
        mfxPluginParam plgParams;
        mfxPluginUID guid1;
        mfxPlugin mfxPlg ;
        mfxSession session;
        mfxU16 pluginVer;
        std::vector<std::string> dirsToRemove;
        WhenRegistryContainsNoPlugin() 
            : mockEncoder(*new MockPlugin())
            , mAdapter(&mockEncoder)
        {
                mfxPlg = mAdapter.operator mfxPlugin();

                //default plugin
                mfxPluginUID guid0 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
                guid1 = guid0;

                //each test uses blank mock object
                g_context = this;

                //any plugins should be loaded with no problems
                pluginVer = 0;

                //setting up default plugin
                plgParams.CodecId = MFX_CODEC_HEVC;
                plgParams.PluginUID = guid1;
                plgParams.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
                plgParams.APIVersion = g_MfxApiVersion;
        }

        void createKey(mfxPluginParam &pluginParams, const std::string &name, const std::string &path, int isDefault) {
            HKEY hk1;
            RegCreateKeyA(HKEY_LOCAL_MACHINE, rootPluginPath, &hk1);
            HKEY hk;
            RegCreateKeyA(hk1, name.c_str(), &hk);

            RegSetValueExA(hk, "codecid", 0, REG_DWORD, (BYTE*)&pluginParams.CodecId, sizeof(pluginParams.CodecId));
            RegSetValueExA(hk, "default", 0, REG_DWORD, (BYTE*)&(int)isDefault, sizeof(isDefault));
            RegSetValueExA(hk, "GUID", 0, REG_BINARY, (BYTE*)&pluginParams.PluginUID.Data, sizeof(pluginParams.PluginUID.Data));
            //RegSetValueExA(hk, "Name", 0, REG_SZ, (BYTE*)name.c_str(), name.length());
            RegSetValueExA(hk, "Path", 0, REG_SZ, (BYTE*)path.c_str(), path.length());
            RegSetValueExA(hk, "Type", 0, REG_DWORD, (BYTE*)&pluginParams.Type, sizeof(pluginParams.Type));
            mfxU32 version = pluginParams.PluginVersion;
            RegSetValueExA(hk, "PlgVer", 0, REG_DWORD, (BYTE*)&version, sizeof(version));
            RegSetValueExA(hk, "APIVer", 0, REG_DWORD, (BYTE*)&pluginParams.APIVersion.Version, sizeof(pluginParams.APIVersion.Version));
            RegCloseKey(hk);
            RegCloseKey(hk1);
        }

       

        DECLARE_MOCK_METHOD2(mfxStatus , Create, mfxPluginUID , mfxPlugin* );

        TEST_METHOD_TYPE(Create) createArgs;

        ~WhenRegistryContainsNoPlugin() {
            RegDeleteTreeA(HKEY_LOCAL_MACHINE, rootPluginPath);
            for (size_t i = 0; i < dirsToRemove.size(); i++)
            {
                system((std::string("rmdir /S /Q ") + dirsToRemove[i].c_str()).c_str());
            }
        }
    };

    MFXAPIVerFinder WhenRegistryContainsNoPlugin::g_MfxApiVersion;
    WhenRegistryContainsNoPlugin * WhenRegistryContainsNoPlugin::g_context = NULL;

    EXTERN_C mfxStatus __declspec(dllexport) MFX_CDECL CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
        return WhenRegistryContainsNoPlugin::g_context->Create(uid, plugin);
    }

    class  WhenRegistryContainsOnePlugin :public WhenRegistryContainsNoPlugin {
    public:
        WhenRegistryContainsOnePlugin ()
        {
            //set path to mock encoder plugin
            createKey(plgParams, "N2", "mfx_pipeline_tests_d.exe", true);
        }
    };

    class WhenTestLoadingFileSystemPlugin : public WhenRegistryContainsNoPlugin {
        HMODULE h;
    public :
        WhenTestLoadingFileSystemPlugin() 
            : h() {
        }
        void createPluginInFS(
            mfxPluginParam &pluginParams
            , bool bCreateCFG
            , bool bPopulateCFGWith1Plugin
            , bool bSameAsFileName
            , const std::string &dirName
            , const std::string &dll_postfix
            , const std::string &name
            , const std::string &path) 
        {
            std::string currentModuleName;
            std::string currentModuleName2;
            currentModuleName.resize(512);
            GetModuleFileNameA(NULL, &currentModuleName.front(), currentModuleName.size());
            if (GetLastError() != 0) 
            {
                return;
            }
            size_t pos = currentModuleName.find_last_of("\\");
            std::string currentModuleName3 = currentModuleName.substr(pos + 1);
            currentModuleName2 = currentModuleName;
            currentModuleName.resize(pos+1);
            std::stringstream ss;
            for (int i = 0; i < 16; i++)
            {
                ss<<MAKE_HEX(pluginParams.PluginUID.Data[i]);
            }
            if (dirName.empty()) {
                currentModuleName.append(ss.str());
            }
            else {
                currentModuleName.append(dirName);
            }
            //need to create folder
            CreateDirectoryA(currentModuleName.c_str(), NULL);
            dirsToRemove.push_back(currentModuleName);


            if (bCreateCFG) {
                FILE * plgFile = fopen((currentModuleName + "\\plugin.cfg").c_str(), "w");
                if (plgFile)
                {
                    if (bPopulateCFGWith1Plugin) {
                        fprintf(plgFile, "PlgVer     = %d\n\n\n", pluginParams.PluginVersion);
                        fprintf(plgFile, "FileName32 = \"%s%s\"\n\n\n\n", currentModuleName3.c_str(), dll_postfix.c_str());
                        fprintf(plgFile, "FileName64 = \"%s%s\"\n", currentModuleName3.c_str(), dll_postfix.c_str());
                    }
                    fclose(plgFile);
                }
            }
            //bSameAsFileName ? dll_postfix : "some_plugin.dll"
            std::string targetModule = currentModuleName+"\\"+currentModuleName3.c_str() + dll_postfix ;
            system((std::string("mklink ") + targetModule + " " + currentModuleName2.c_str() ).c_str());
            /*h = LoadLibraryA(targetModule.c_str());
            FARPROC proc = GetProcAddress(h, "SetContext");
            ((void (MFX_CDECL*)(WhenRegistryContainsNoPlugin*))proc)(this);*/
        }
        ~WhenTestLoadingFileSystemPlugin()
        {
            //FreeLibrary(h);
        }
    };
    //////////////////////////////////////////////////////////////////////////

    TEST_FIXTURE(WhenTestLoadingFileSystemPlugin, HIVE_build_SUCESS_with_FS_plugin) {

        createPluginInFS(plgParams, true, true, true, "", "_plugin.dll", "N2", "mfx_pipeline_tests_d.exe");

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        mfxVersion verToRequest = g_MfxApiVersion;
        verToRequest.Minor--;

        MFXInit(MFX_IMPL_SOFTWARE, &verToRequest, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }


    TEST_FIXTURE(WhenTestLoadingFileSystemPlugin, HIVE_build_FAILURE_with_FS_plugin_IF_dirname_isnot_valid_hex) {

        createPluginInFS(plgParams, true, true, true, "01020304050607080z10111213141516", "_plugin.dll", "N2", "mfx_pipeline_tests_d.exe");

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        mfxVersion verToRequest = g_MfxApiVersion;
        verToRequest.Minor--;

        MFXInit(MFX_IMPL_SOFTWARE, &verToRequest, &session);
        CHECK(MFX_ERR_NONE!=MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenTestLoadingFileSystemPlugin, HIVE_build_FAILURE_with_FS_plugin_IF_NO_PLUGIN_description) {

        createPluginInFS(plgParams, false, false, true, "", "_plugin.dll", "N2", "mfx_pipeline_tests_d.exe");

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        mfxVersion verToRequest = g_MfxApiVersion;
        verToRequest.Minor--;

        MFXInit(MFX_IMPL_SOFTWARE, &verToRequest, &session);
        CHECK(MFX_ERR_NONE!=MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenTestLoadingFileSystemPlugin, HIVE_build_FAILURE_with_FS_plugin_IF_NO_PLUGIN_name_too_long) {

        createPluginInFS(plgParams, true, true, false, "",
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"

        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"

        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"

        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"

        "11111111111111111111111111111111_plugin.dll"
        , "N2", "mfx_pipeline_tests_d.exe");

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        mfxVersion verToRequest = g_MfxApiVersion;
        verToRequest.Minor--;

        MFXInit(MFX_IMPL_SOFTWARE, &verToRequest, &session);
        CHECK(MFX_ERR_NONE!=MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenTestLoadingFileSystemPlugin, HIVE_build_FAILURE_with_FS_plugin_IF_PLUGIN_description_is_wrong_format) {

        createPluginInFS(plgParams, true, false, true,  "", "_plugin.dll", "N2", "mfx_pipeline_tests_d.exe");

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        mfxVersion verToRequest = g_MfxApiVersion;
        verToRequest.Minor--;

        MFXInit(MFX_IMPL_SOFTWARE, &verToRequest, &session);
        CHECK(MFX_ERR_NONE!=MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsNoPlugin, HIVE_build_SUCESS_if_real_lib_higher_than_requested_and_meet_plg_requirement_MFXInit_version) {

        createKey(plgParams, "N2", "mfx_pipeline_tests_d.exe", true);

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);
        
        mfxVersion verToRequest = g_MfxApiVersion;
        verToRequest.Minor--;

        MFXInit(MFX_IMPL_SOFTWARE, &verToRequest, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsNoPlugin, HIVE_build_error_if_loaded_plugin_API_version_is_higher_than_MFXInit_version) {

        plgParams.APIVersion.Minor++;
        createKey(plgParams, "N2", "mfx_pipeline_tests_d.exe", true);

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsNoPlugin, HIVE_build_error_if_loaded_plugin_API_version_is_lower_than_MFXInit_version) {

        plgParams.APIVersion.Minor--;
        createKey(plgParams, "N2", "mfx_pipeline_tests_d.exe", true);

        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsNoPlugin, HIVE_build_error_if_loaded_plugin_version_is_different) {

        plgParams.PluginVersion = 100;
        plgParams.CodecId = MFX_CODEC_HEVC;
        plgParams.PluginUID = guid1;
        plgParams.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
        //set path to mock encoder plugin
        createKey(plgParams, "N2", "mfx_pipeline_tests_d.exe", true);


        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        plgParams.PluginVersion=101;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_Guid_equal_toNULL) {
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NULL_PTR, MFXVideoUSER_Load(session, NULL, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testUnLoad_Guid_equal_toNULL) {
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NULL_PTR, MFXVideoUSER_UnLoad(session, NULL));
        MFXClose(session);
    }
    
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_session_equals_toNULL) {
        CHECK_EQUAL(MFX_ERR_NULL_PTR, MFXVideoUSER_Load(NULL, &guid1, pluginVer));
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testUnLoad_session_equals_toNULL) {
        CHECK_EQUAL(MFX_ERR_NULL_PTR, MFXVideoUSER_UnLoad(NULL, &guid1));
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WrongGuid) {
        mfxPluginUID guid1 = {};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NOT_FOUND, MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WrongVersion) {
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        pluginVer = 10500;
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, testLoad_WrongAPI) {
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }


    TEST_FIXTURE(WhenRegistryContainsOnePlugin, PluginInit_Equals_to_Null_result_in_LoadFailure) {
        mfxPlg.PluginInit = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, PluginClose_Equals_to_Null_result_in_LoadFailure) {
        mfxPlg.PluginClose = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, GetPluginParam_Equals_to_Null_result_in_LoadFailure) {
        mfxPlg.GetPluginParam = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }
    
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, EXECUTE_Equals_to_Null_result_in_LoadFailure) {
        mfxPlg.Execute = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, FreeResource_Equals_to_Null_result_in_LoadFailure) {
        mfxPlg.FreeResources = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE != MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }
    TEST_FIXTURE(WhenRegistryContainsOnePlugin, EncodeFrameSubmit_Equals_to_Null_reult_in_LoadFailure) {
        mfxPlg.Video->EncodeFrameSubmit = NULL;

        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &mfxPlg;
        this->_Create.WillReturn(createArgs);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK(MFX_ERR_NONE!= MFXVideoUSER_Load(session, &guid1, pluginVer));
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
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, Loader_calls_CreatePlugin_with_registered_guid_and_not_null_mfxPlugin) {
        mfxSession session;
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, &guid1, pluginVer);

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
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, &guid1, pluginVer);
        MFXClose(session);
        CHECK(mockEncoder._PluginClose.WasCalled());
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, MFXUserUnload_destroys_only_loaded_plugin) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);
        
        TEST_METHOD_TYPE(MockPlugin::PluginClose) plgClose;

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, &guid1, pluginVer);
        mfxPluginUID guid0 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x17};
        
        CHECK(MFX_ERR_NONE != MFXVideoUSER_UnLoad(session, &guid0));
        CHECK(!mockEncoder._PluginClose.WasCalled());

        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_UnLoad(session, &guid1));
        CHECK(mockEncoder._PluginClose.WasCalled());

        //MFXClose cannot close alreadyclosed plugin
        MFXClose(session);
        CHECK(!mockEncoder._PluginClose.WasCalled());
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, LoadFailedIfCreatePlugin_reported_error) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NOT_FOUND;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        //not passtru error reported from plugin
        CHECK_EQUAL(MFX_ERR_UNKNOWN, MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, SecondLoadFailed_of_plugin_of_same_type_with_undefined_behavior) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillDefaultReturn(&createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillDefaultReturn(&getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, &guid1, pluginVer));
        //see manual
        CHECK_EQUAL(MFX_ERR_UNDEFINED_BEHAVIOR, MFXVideoUSER_Load(session, &guid1, pluginVer));
        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, PluginCloseNoCalled_IF_createPluginReturned_incorrect_structure) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        plg.Video->EncodeFrameSubmit = 0;
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, &guid1, pluginVer);
        CHECK(!mockEncoder._PluginClose.WasCalled());

        MFXClose(session);
    }

    TEST_FIXTURE(WhenRegistryContainsOnePlugin, UnloadSucess_if_loadSucess) {
        TEST_METHOD_TYPE(Create) createArgs;
        mfxPlugin plg = mAdapter.operator mfxPlugin();
        createArgs.ret_val = MFX_ERR_NONE;
        createArgs.value1  = &plg;
        this->_Create.WillReturn(createArgs);

        TEST_METHOD_TYPE(MockPlugin::GetPluginParam) getPluginParams;
        getPluginParams.value0 = plgParams;
        mockEncoder._GetPluginParam.WillReturn(getPluginParams);

        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_Load(session, &guid1, pluginVer));
        CHECK_EQUAL(MFX_ERR_NONE, MFXVideoUSER_UnLoad(session, &guid1));
        MFXClose(session);
    }

}