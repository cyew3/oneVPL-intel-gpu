/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */



#pragma once

#include "mfx_intrusive_ptr.h"
#include <iostream>
#include "vm_shared_object.h"
#include "mfx_pipeline_types.h"
#include "mfxplugin++.h"

class MsdkSoModule
{
protected:
    vm_so_handle m_module;
public:

    MsdkSoModule() 
        : m_module() 
    {
    }

    MsdkSoModule(const tstring & pluginName)
        : m_module()
    {
        m_module = vm_so_load(pluginName.c_str());
        if (NULL == m_module)
        {
            int lastErr = 0;
            #ifdef WIN32
                lastErr = GetLastError();
            #endif
            MFX_TRACE_ERR(VM_STRING("Failed to load shared module: ") << pluginName << VM_STRING(". Error=") << lastErr);
        }
    }
    template <class T>
    T GetAddr(const std::string & fncName)
    {
        T pCreateFunc = reinterpret_cast<T>(vm_so_get_addr(m_module, fncName.c_str()));
        if (NULL == pCreateFunc) {

            MFX_TRACE_ERR(VM_STRING("Failed to get function addres: ") << cvt_s2t(fncName));
        }
        return pCreateFunc;
    }
    bool IsLoaded() {
        return m_module != 0;
    }

    virtual ~MsdkSoModule()
    {
        if (m_module)
        {
            vm_so_free(m_module);
            m_module = NULL;
        }
    }
};


/* for plugin API*/
inline void intrusive_ptr_addref(MFXDecoderPlugin *)
{
}
inline void intrusive_ptr_release(MFXDecoderPlugin * pResource)
{
    //fully destroy plugin resource
    pResource->Release();
}
/* for plugin API*/
inline void intrusive_ptr_addref(MFXEncoderPlugin *)
{
}
inline void intrusive_ptr_release(MFXEncoderPlugin * pResource)
{
    //fully destroy plugin resource
    pResource->Release();
}

/* for plugin API*/
inline void intrusive_ptr_addref(MFXGenericPlugin *)
{
}
inline void intrusive_ptr_release(MFXGenericPlugin * pResource)
{
    //fully destroy plugin resource
    pResource->Release();
}

/* for plugin API*/
inline void intrusive_ptr_addref(MFXVPPPlugin *)
{
}
inline void intrusive_ptr_release(MFXVPPPlugin * pResource)
{
    //fully destroy plugin resource
    pResource->Release();
}


struct PluginModuleTemplate {
    typedef MFXDecoderPlugin* (*fncCreateDecoderPlugin)();
    typedef MFXEncoderPlugin* (*fncCreateEncoderPlugin)();
    typedef MFXVPPPlugin*     (*fncCreateVPPPlugin)();
    typedef MFXGenericPlugin* (*fncCreateGenericPlugin)();

    fncCreateDecoderPlugin CreateDecoderPlugin;
    fncCreateEncoderPlugin CreateEncoderPlugin;
    fncCreateGenericPlugin CreateGenericPlugin;
    fncCreateVPPPlugin     CreateVPPPlugin;
};

template<class T>
struct PluginCreateTrait
{
};

template<>
struct PluginCreateTrait<MFXDecoderPlugin>
{
    static const char* Name(){
        return "mfxCreateDecoderPlugin";
    }
    enum PluginType {
        type=MFX_PLUGINTYPE_VIDEO_DECODE
    };
    typedef PluginModuleTemplate::fncCreateDecoderPlugin TCreator;

};

template<>
struct PluginCreateTrait<MFXEncoderPlugin>
{
    static const char* Name(){
        return "mfxCreateEncoderPlugin";
    }
    enum PluginType {
        type=MFX_PLUGINTYPE_VIDEO_ENCODE
    };
    typedef PluginModuleTemplate::fncCreateEncoderPlugin TCreator;
};

template<>
struct PluginCreateTrait<MFXGenericPlugin>
{
    static const char* Name(){
        return "mfxCreateGenericPlugin";
    }
    enum PluginType {
        type=MFX_PLUGINTYPE_VIDEO_GENERAL
    };
    typedef PluginModuleTemplate::fncCreateGenericPlugin TCreator;
};

template<>
struct PluginCreateTrait<MFXVPPPlugin>
{
    static const char* Name(){
        return "mfxCreateVPPPlugin";
    }
    enum PluginType {
        type=MFX_PLUGINTYPE_VIDEO_VPP
    };
    typedef PluginModuleTemplate::fncCreateVPPPlugin TCreator;
};


typedef enum { 
    MFX_PLUGINLOAD_TYPE_GUID,
    MFX_PLUGINLOAD_TYPE_FILE
} MfxPluginLoadType;

/*
* Rationale: class to load+register any mediasdk plugin decoder/encoder/generic by given name
*/
template <class T>
class PluginLoader 
{
protected:
    /* Variables related to the explicit load mechanism */
    MsdkSoModule         m_PluginModule;
    mfx_intrusive_ptr<T> m_plugin;
    MFXPluginAdapter<T>  m_adapter;

    /* Variables related to the GUID-based load mechanism */
    mfxU32       m_plugin_version;
    mfxPluginUID m_uid;

    MfxPluginLoadType plugin_load_type;

    mfxSession m_session;
    typedef PluginCreateTrait<T> Plugin;

public:
    PluginLoader(mfxSession session, const tstring & pluginName)
        : m_PluginModule(pluginName)
        , m_plugin_version()
        , m_session()
    {

        plugin_load_type = MFX_PLUGINLOAD_TYPE_FILE;

        if (!m_PluginModule.IsLoaded())
            return;

        typename Plugin::TCreator pCreateFunc = m_PluginModule.GetAddr<typename Plugin::TCreator >(Plugin::Name());
        if(NULL == pCreateFunc)
            return;
        m_plugin.reset(pCreateFunc());

        if (NULL == m_plugin.get())
        {
            MFX_TRACE_ERR(VM_STRING("Failed to Create plugin: ") << cvt_s2t(Plugin::Name())<< VM_STRING(" returned NULL") );
            return;
        }
        m_adapter = make_mfx_plugin_adapter(m_plugin.get());
        mfxPlugin plg = m_adapter;
        mfxStatus sts = MFXVideoUSER_Register(session, PluginCreateTrait<T>::type, &plg);
        if (MFX_ERR_NONE != sts)
        {
            MFX_TRACE_ERR(VM_STRING("Failed to register plugin: ") << pluginName.c_str() << VM_STRING("sts=") << sts);
            return;
        }
        MFX_TRACE_INFO(VM_STRING("Plugin: ") << pluginName.c_str() << VM_STRING(" loaded\n"));
        m_session = session;
    }

    PluginLoader(mfxSession session, const tstring & uid, mfxU32 version)
        : m_session()
    {

        plugin_load_type = MFX_PLUGINLOAD_TYPE_GUID;

        /* Convert UID from string to mfxPluginUID */
        mfxU32 i   = 0;
        mfxU32 hex = 0;
        for(i = 0; i != 16; i++) 
        {
            hex = 0;
#if defined(_WIN32) || defined(_WIN64)
            if (1 != swscanf_s(uid.c_str() + 2*i, L"%2x", &hex))
#else
            if (1 != sscanf(uid.c_str() + 2*i, "%2x", &hex))
#endif
            {
                MFX_TRACE_ERR(VM_STRING("Failed to parse plugin uid: ") << uid.c_str());
                return;
            }
#if defined(_WIN32) || defined(_WIN64)
            if (hex == 0 && (const wchar_t *)uid.c_str() + 2*i != wcsstr((const wchar_t *)uid.c_str() + 2*i, L"00"))
#else
            if (hex == 0 && uid.c_str() + 2*i != strstr(uid.c_str() + 2*i, "00"))
#endif
            {
                 MFX_TRACE_ERR(VM_STRING("Failed to parse plugin uid: ") << uid.c_str());
                return;
            }
            m_uid.Data[i] = (mfxU8)hex;
        }

        m_plugin_version = version;

        mfxStatus sts = MFXVideoUSER_Load(session, &m_uid, m_plugin_version);
        if (MFX_ERR_NONE != sts)
        {
            MFX_TRACE_ERR(VM_STRING("Failed to load plugin: ") << uid.c_str() << VM_STRING("sts=") << sts);
            return;
        }
        MFX_TRACE_INFO(VM_STRING("Plugin: ") << uid.c_str() << VM_STRING(" loaded\n"));
        m_session = session;
    }

    ~PluginLoader()
    {
        if (m_session) 
        {
            mfxStatus sts;

            if ( MFX_PLUGINLOAD_TYPE_GUID == plugin_load_type ) 
            {
                sts = MFXVideoUSER_UnLoad(m_session, &m_uid);
                if (sts != MFX_ERR_NONE) 
                {
                    MFX_TRACE_ERR(VM_STRING("MFXVideoUSER_UnLoad(session=0x") << std::hex << m_session<< VM_STRING(", type=") << Plugin::type << VM_STRING(", sts=") << sts);
                }
            } 
            else 
            {
                sts = MFXVideoUSER_Unregister(m_session, PluginCreateTrait<T>::type);
                if (sts != MFX_ERR_NONE) 
                {
                    MFX_TRACE_ERR(VM_STRING("MFXVideoUSER_Unregister(session=0x") << std::hex << m_session<< VM_STRING(", type=") << Plugin::type << VM_STRING(", sts=") << sts);
                }
            }

            m_session = 0;
        }
    }

    bool IsOk() {
        return m_session != 0;
    }


};
