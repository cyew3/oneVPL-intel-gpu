/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */


#include "ts_struct.h"
#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"
#include "ts_plugin.h"
#if defined(_WIN32) || defined(_WIN64)

#include <DXGI.h>
#include <psapi.h>
#include <windows.h>
#include <tchar.h>
#include <memory>

// include 32/64/debug lib

#define ONEVPL32 "libmfx32-gen.dll"
#define ONEVPL64 "libmfx64-gen.dll"

#else

#include <string>
#include <link.h>

#if defined(__x86_64__)
#define ONEVPLHW "libmfx-gen.so.1.2"
#else
#error Unsupported architecture
#endif

#endif // #if defined(_WIN32) || defined(_WIN64)

typedef mfxStatus (MFX_CDECL* pMFXVideoUSER_Register)(mfxSession session, mfxU32 type, const mfxPlugin* par);
typedef mfxStatus(MFX_CDECL*  pMFXVideoUSER_Unregister)(mfxSession session, mfxU32 type);
struct sRegFunctions
{
    pMFXVideoUSER_Register pReg = 0;
    pMFXVideoUSER_Unregister pUnreg = 0;
};


#if defined(_WIN32) || defined(_WIN64)
static
mfxStatus GetUserRegisterFunc(sRegFunctions &regFunc)
{
    DWORD currProcessID = GetCurrentProcessId();

    std::unique_ptr<void, void(*)(HANDLE)> hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, currProcessID),
        [](HANDLE pHandle) { CloseHandle(pHandle); });

    if (NULL == hProcess.get())
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    // get buffee size for array of loaded libs
    DWORD needed;
    if (!EnumProcessModules(hProcess.get(), nullptr, 0, &needed))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    // get a list of all the loaded modules in this process.
    std::vector<HMODULE>  modules(needed / sizeof(HMODULE));
    if (!EnumProcessModulesEx(hProcess.get(), modules.data(), needed, &needed, LIST_MODULES_ALL))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    const DWORD MAX_FILE_PATH = 1024;
    for (auto module : modules)
    {
        TCHAR moduleName[MAX_PATH];
        if (GetModuleBaseName(hProcess.get(), module, moduleName, MAX_PATH))
        {
            if (_tcsstr(moduleName, _T(ONEVPL32)) != NULL || _tcsstr(moduleName, _T(ONEVPL64)) != NULL)
            {
                regFunc.pReg = (pMFXVideoUSER_Register)GetProcAddress(
                    module, "MFXVideoUSER_Register");

                regFunc.pUnreg = (pMFXVideoUSER_Unregister)GetProcAddress(
                    module, "MFXVideoUSER_Unregister");
 
                return (!regFunc.pReg || !regFunc.pUnreg) ?
                    MFX_ERR_UNDEFINED_BEHAVIOR :MFX_ERR_NONE;

            }
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

#else // #if defined(_WIN32) || defined(_WIN64)
static
int DLGetUserRegisterFunc(struct dl_phdr_info* info, size_t size, void* data)
{
    std::string libPath = info->dlpi_name;
    if (libPath.find(ONEVPLHW) != std::string::npos)
    {
        if (data) {
            void* handle = 0;
            handle = dlopen(info->dlpi_name, RTLD_LAZY);
            if (handle)
            {
                sRegFunctions* pRegFunc = (sRegFunctions*)data;
                pRegFunc->pReg = (pMFXVideoUSER_Register)dlsym(handle, "MFXVideoUSER_Register");
                pRegFunc->pUnreg = (pMFXVideoUSER_Unregister)dlsym(handle, "MFXVideoUSER_Unregister");
            }
            return 0;
        }
    }
    return 0;
}
static
mfxStatus GetUserRegisterFunc(sRegFunctions& regFunc)
{
    dl_iterate_phdr(DLGetUserRegisterFunc, &regFunc);
    if (!regFunc.pReg || !regFunc.pUnreg)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    return MFX_ERR_NONE;
}


#endif // #if defined(_WIN32) || defined(_WIN64)
class TestSuite :
    virtual public tsSession
{
private:
    tsPlugin     g_tsPlugin;

public:
    static const unsigned int n_cases;

    TestSuite() {
    }
    ~TestSuite() {
    }


    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 pluginType;
        mfxU32 pluginId;
        const mfxPluginUID pluginUid;

        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

    int RunTestRegister(unsigned int id);
    template<mfxU32 testType>
    int RunTest_Subtype(const unsigned int id);

};

const TestSuite::tc_struct TestSuite::test_case[] =
{

    {/*00*/ MFX_ERR_NONE,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_HW},
    {/*01*/ MFX_ERR_NONE,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_VP9,  MFX_PLUGINID_VP9E_HW},
    {/*02*/ MFX_ERR_NONE,  MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW},
    {/*03*/ MFX_ERR_NONE,  MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW},
    {/*04*/ MFX_ERR_NONE,  MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_VP9, MFX_PLUGINID_VP9D_HW},

    {/*05*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW},
    {/*06*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_GACC},
    {/*07*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_FEI_HW},
    {/*08*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVC_FEI_ENCODE},
    {/*09*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_DP_GACC},
    {/*10*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_VP8,  MFX_PLUGINID_VP8E_HW},
    {/*11*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_AV1,  MFX_PLUGINID_AV1E_GACC},
    {/*12*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_ENC,    MFX_CODEC_AVC,  MFX_PLUGINID_H264LA_HW},


    {/*13*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW},
    {/*14*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_VP8, MFX_PLUGINID_AV1D_SW},
    {/*15*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_VP9, MFX_PLUGINID_AV1D_HW},


    {/*16*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('C','A','M','R'), MFX_PLUGINID_CAMERA_HW},
    {/*17*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_DECODE, MFX_MAKEFOURCC('C','A','M','R'), MFX_PLUGINID_CAPTURE_HW},
    {/*18*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW},

    {/*19*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_AUDIO_DECODE, 0, MFX_PLUGINID_AACD},
    {/*20*/ MFX_ERR_UNSUPPORTED ,  MFX_PLUGINTYPE_AUDIO_ENCODE, 0, MFX_PLUGINID_AACE},


};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

template<mfxU32 testType>
int TestSuite::RunTest_Subtype(const unsigned int id)
{

    switch (testType)
    {
    case 1:
        return RunTestRegister(id);
    }

    return 0;
}



class TestPlugin
{
private:
    mfxU32 m_pluginType = 0;
    mfxU32 m_pluginId = 0;
    mfxPluginUID m_pluginUid = {};

public:
    bool isAudioPlg() const
    {
        return (MFX_PLUGINTYPE_AUDIO_DECODE == m_pluginType ||
            MFX_PLUGINTYPE_AUDIO_ENCODE == m_pluginType);
    }
    TestPlugin(mfxU32 pluginType, mfxU32 pluginId, mfxPluginUID pluginUid)
    {
        m_pluginType = pluginType;
        m_pluginId = pluginId;
        m_pluginUid = pluginUid;
    }

    mfxStatus GetPluginParam(mfxPluginParam* par) const
    {
        if (!par)
            return MFX_ERR_NULL_PTR;

        par->APIVersion.Version = 1;
        par->CodecId = m_pluginId;
        par->PluginUID = m_pluginUid;
        par->Type = m_pluginType;
        par->ThreadPolicy = MFX_THREADPOLICY_SERIAL;
        par->PluginVersion = 1;
        return MFX_ERR_NONE;
    }
};

namespace TestPluginFunc
{
    mfxStatus PluginInit(mfxHDL, mfxCoreInterface*) { return MFX_ERR_NONE; }
    mfxStatus PluginClose(mfxHDL) { return MFX_ERR_NONE; }
    mfxStatus GetPluginParam(mfxHDL pthis, mfxPluginParam* par)
    {
        if (pthis)
        {
            return ((TestPlugin*)pthis)->GetPluginParam(par);
        }
        return MFX_ERR_NULL_PTR;
    }

    mfxStatus Submit(mfxHDL, const mfxHDL*, mfxU32, const mfxHDL*, mfxU32, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus Execute(mfxHDL, mfxThreadTask, mfxU32, mfxU32)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus FreeResources(mfxHDL, mfxThreadTask, mfxStatus)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioQuery(mfxHDL, mfxAudioParam*, mfxAudioParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioQueryIOSize(mfxHDL, mfxAudioParam*, mfxAudioAllocRequest*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioInit(mfxHDL, mfxAudioParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioReset(mfxHDL, mfxAudioParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioClose(mfxHDL)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus GetAudioParam(mfxHDL, mfxAudioParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioEncodeFrameSubmit(mfxHDL, mfxAudioFrame*, mfxBitstream*, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioDecodeHeader(mfxHDL, mfxBitstream*, mfxAudioParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus AudioDecodeFrameSubmit(mfxHDL, mfxBitstream*, mfxAudioFrame*, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  Query(mfxHDL, mfxVideoParam*, mfxVideoParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  QueryIOSurf(mfxHDL, mfxVideoParam*, mfxFrameAllocRequest*, mfxFrameAllocRequest*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  Init(mfxHDL, mfxVideoParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  Reset(mfxHDL, mfxVideoParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  Close(mfxHDL)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  GetVideoParam(mfxHDL, mfxVideoParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  EncodeFrameSubmit(mfxHDL, mfxEncodeCtrl*, mfxFrameSurface1*, mfxBitstream*, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  DecodeHeader(mfxHDL, mfxBitstream*, mfxVideoParam*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  GetPayload(mfxHDL, mfxU64*, mfxPayload*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  DecodeFrameSubmit(mfxHDL, mfxBitstream*, mfxFrameSurface1*, mfxFrameSurface1**, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  VPPFrameSubmit(mfxHDL, mfxFrameSurface1*, mfxFrameSurface1*, mfxExtVppAuxData*, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  VPPFrameSubmitEx(mfxHDL, mfxFrameSurface1*, mfxFrameSurface1*, mfxFrameSurface1**, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    mfxStatus  ENCFrameSubmit(mfxHDL, mfxENCInput*, mfxENCOutput*, mfxThreadTask*)
    {
        return MFX_ERR_NONE;
    }
    void InitMfxPlugin(TestPlugin& testPlg, mfxPlugin& plg, mfxVideoCodecPlugin& video,
                        mfxAudioCodecPlugin& audio)
    {
        plg = {};
        plg.pthis = &testPlg;
        plg.PluginInit = PluginInit;
        plg.PluginClose = PluginClose;
        plg.GetPluginParam = GetPluginParam;
        plg.Submit = Submit;
        plg.Execute = Execute;
        plg.FreeResources = FreeResources;
        if (testPlg.isAudioPlg())
        {
            audio = {};
            plg.Audio = &audio;
            plg.Audio->Query = AudioQuery;
            plg.Audio->QueryIOSize = AudioQueryIOSize;
            plg.Audio->Init = AudioInit;
            plg.Audio->Reset = AudioReset;
            plg.Audio->Close = AudioClose;
            plg.Audio->GetAudioParam = GetAudioParam;
            plg.Audio->EncodeFrameSubmit = AudioEncodeFrameSubmit;
            plg.Audio->DecodeHeader = AudioDecodeHeader;
            plg.Audio->DecodeFrameSubmit = AudioDecodeFrameSubmit;
        }
        else
        {
            video = {};
            plg.Video = &video;
            plg.Video->Query = Query;
            plg.Video->QueryIOSurf = QueryIOSurf;
            plg.Video->Init = Init;
            plg.Video->Reset = Reset;
            plg.Video->Close = Close;
            plg.Video->GetVideoParam = GetVideoParam;
            plg.Video->EncodeFrameSubmit = EncodeFrameSubmit;
            plg.Video->DecodeHeader = DecodeHeader;
            plg.Video->GetPayload = GetPayload;
            plg.Video->DecodeFrameSubmit = DecodeFrameSubmit;
            plg.Video->VPPFrameSubmit = VPPFrameSubmit;
            plg.Video->VPPFrameSubmitEx = VPPFrameSubmitEx;
            plg.Video->ENCFrameSubmit = ENCFrameSubmit;
        }
    }
};
int TestSuite::RunTestRegister(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];
    TestPlugin tstPlg(tc.pluginType, tc.pluginId, tc.pluginUid);
    mfxPlugin plgFunc = {};
    mfxVideoCodecPlugin video = {};
    mfxAudioCodecPlugin audio = {};
    TestPluginFunc::InitMfxPlugin(tstPlg, plgFunc, video, audio);

    MFXInit();
    g_tsStatus.expect(tc.sts);

    sRegFunctions regFunc = {};
    mfxStatus sts = MFX_ERR_NONE;

    sts = GetUserRegisterFunc(regFunc);
    if (sts == MFX_ERR_NONE)
    {
        sts = regFunc.pReg(m_session, tc.pluginType, &plgFunc);
        g_tsStatus.check(sts);
        regFunc.pUnreg(m_session, tc.pluginType);
    }
    else
    {
        g_tsStatus.check(sts);
    }

    MFXClose();
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(codec_plugin_register, RunTest_Subtype<1>, n_cases);