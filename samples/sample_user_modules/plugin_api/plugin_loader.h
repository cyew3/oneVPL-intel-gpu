/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#include "vm/so_defs.h"
#include "sample_utils.h"
#include "mfx_plugin_module.h"
#include "intrusive_ptr.h"
#include <iostream>
#include <iomanip> // for std::setfill, std::setw
#include <memory> // for std::auto_ptr

class MsdkSoModule
{
protected:
    msdk_so_handle m_module;
public:
    MsdkSoModule()
        : m_module(NULL)
    {
    }
    MsdkSoModule(const msdk_string & pluginName)
        : m_module(NULL)
    {
        m_module = msdk_so_load(pluginName.c_str());
        if (NULL == m_module)
        {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to load shared module: ") << pluginName);
        }
    }
    template <class T>
    T GetAddr(const std::string & fncName)
    {
        T pCreateFunc = reinterpret_cast<T>(msdk_so_get_addr(m_module, fncName.c_str()));
        if (NULL == pCreateFunc) {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to get function addres: ") << fncName.c_str());
        }
        return pCreateFunc;
    }

    virtual ~MsdkSoModule()
    {
        if (m_module)
        {
            msdk_so_free(m_module);
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
    static const char* Name() {
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

/*
* Rationale: class to load+register any mediasdk plugin decoder/encoder/generic by given name
*/
template <class T>
class PluginLoader : public MFXPlugin
{
protected:
    MsdkSoModule m_PluginModule;
    intrusive_ptr<T> m_plugin;
    MFXVideoUSER *m_userModule;
    MFXPluginAdapter<T> m_adapter;
    enum {
        ePluginType = PluginCreateTrait<T>::type
    };
    typedef typename PluginCreateTrait<T>::TCreator TCreator;

    mfxSession        m_session;
    mfxPluginUID      m_uid;

    MfxPluginLoadType plugin_load_type;

private:
    const msdk_char* msdkGetPluginName(const mfxPluginUID& guid)
    {
        if (AreGuidsEqual(guid, MFX_PLUGINID_HEVCD_SW))
            return MSDK_STRING("Intel (R) Media SDK plugin for HEVC DECODE");
        else if(AreGuidsEqual(guid, MFX_PLUGINID_HEVCD_HW))
            return MSDK_STRING("Intel (R) Media SDK HW plugin for HEVC DECODE");
        else if(AreGuidsEqual(guid, MFX_PLUGINID_VP8D_HW))
            return MSDK_STRING("Intel (R) Media SDK HW plugin for VP8 DECODE");
        else if(AreGuidsEqual(guid, MFX_PLUGINID_HEVCE_SW))
            return MSDK_STRING("Intel (R) Media SDK plugin for HEVC ENCODE");
        else if(AreGuidsEqual(guid, MFX_PLUGINID_VP8E_HW))
            return MSDK_STRING("Intel (R) Media SDK HW plugin for VP8 ENCODE");
        else if(AreGuidsEqual(guid, MFX_PLUGINID_H264LA_HW))
            return MSDK_STRING("Intel (R) Media SDK plugin for LA ENC");
        else
            return MSDK_STRING("Unknown plugin");
    }

public:
    PluginLoader(MFXVideoUSER *userModule, const msdk_string & pluginName)
        : m_PluginModule(pluginName)
        , m_userModule(NULL)
    {

        plugin_load_type = MFX_PLUGINLOAD_TYPE_FILE;

        TCreator pCreateFunc = m_PluginModule.GetAddr<TCreator>(PluginCreateTrait<T>::Name());
        if(NULL == pCreateFunc)
        {
            return;
        }
        m_plugin.reset((*pCreateFunc)());

        if (NULL == m_plugin.get())
        {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to Create plugin: ")<< pluginName);
            return;
        }
        m_adapter = make_mfx_plugin_adapter(m_plugin.get());
        mfxPlugin plg = m_adapter;
        mfxStatus sts = userModule->Register(ePluginType, &plg);
        if (MFX_ERR_NONE != sts) {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to register plugin: ") << pluginName
                << MSDK_STRING(", MFXVideoUSER::Register(type=") << ePluginType << MSDK_STRING("), sts=") << sts);
            return;
        }
        MSDK_TRACE_INFO(MSDK_STRING("Plugin(type=")<< ePluginType <<MSDK_STRING("): loaded from: ")<< pluginName );
        m_userModule = userModule;
    }

    PluginLoader(mfxSession session, const mfxPluginUID & uid, mfxU32 version)
        : m_session()
        , m_uid()
    {
        msdk_stringstream strStream;
        plugin_load_type = MFX_PLUGINLOAD_TYPE_GUID;

        MSDK_MEMCPY(&m_uid, &uid, sizeof(mfxPluginUID));
        for (size_t i = 0; i != sizeof(mfxPluginUID); i++)
        {
            strStream << MSDK_STRING("0x") << std::setfill(MSDK_CHAR('0')) << std::setw(2) << std::hex << (int)m_uid.Data[i];
            if (i != (sizeof(mfxPluginUID)-1)) strStream << MSDK_STRING(", ");
        }

        mfxStatus sts = MFXVideoUSER_Load(session, &m_uid, version);
        if (MFX_ERR_NONE != sts)
        {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to load plugin from GUID, sts=") << sts << MSDK_STRING(": { ") << strStream.str().c_str() << MSDK_STRING(" } (") << msdkGetPluginName(m_uid) << MSDK_STRING(")"));
        }
        else
        {
            MSDK_TRACE_INFO(MSDK_STRING("Plugin was loaded from GUID: { ") << strStream.str().c_str() << MSDK_STRING(" } (") << msdkGetPluginName(m_uid) << MSDK_STRING(")"));
            m_session = session;
        }
    }

    virtual ~PluginLoader()
    {
        if (plugin_load_type == MFX_PLUGINLOAD_TYPE_FILE)
        {
            if (m_userModule) {
                mfxStatus sts = m_userModule->Unregister(ePluginType);
                if (sts != MFX_ERR_NONE) {
                    MSDK_TRACE_INFO(MSDK_STRING("MFXVideoUSER::Unregister(type=") << ePluginType << MSDK_STRING("), sts=") << sts);
                }
            }
        }
        else
        {
            if (m_session)
            {
                mfxStatus sts = MFXVideoUSER_UnLoad(m_session, &m_uid);
                if (sts != MFX_ERR_NONE)
                {
                     MSDK_TRACE_INFO(MSDK_STRING("MFXVideoUSER_UnLoad(session=0x") << m_session << MSDK_STRING("), sts=") << sts);
                }
            }
        }
    }
    bool IsOk() {
        if (plugin_load_type == MFX_PLUGINLOAD_TYPE_FILE)
            return m_userModule != 0;
        else
            return m_session != 0;
    }
    virtual mfxStatus Init( mfxVideoParam *par ) {
        return m_plugin->Init(par);
    }
    virtual mfxStatus PluginInit( mfxCoreInterface *core ) {
        return m_plugin->PluginInit(core);
    }
    virtual mfxStatus PluginClose() {
        return m_plugin->PluginClose();
    }
    virtual mfxStatus GetPluginParam( mfxPluginParam *par ) {
        return m_plugin->GetPluginParam(par);
    }
    virtual mfxStatus Execute( mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a ) {
        return m_plugin->Execute(task, uid_p, uid_a);
    }
    virtual mfxStatus FreeResources( mfxThreadTask task, mfxStatus sts ) {
        return m_plugin->FreeResources(task, sts);
    }
    virtual mfxStatus QueryIOSurf( mfxVideoParam *par, mfxFrameAllocRequest *in , mfxFrameAllocRequest *out ) {
        return m_plugin->QueryIOSurf(par, in, out);
    }
    virtual void Release() {
        m_plugin->Release();
    }
    virtual mfxStatus Close() {
        return m_plugin->Close();
    }
    virtual mfxStatus SetAuxParams( void* auxParam, int auxParamSize ) {
        return m_plugin->SetAuxParams(auxParam, auxParamSize);
    }
};

template <class T>
MFXPlugin * LoadPluginByType(MFXVideoUSER *userModule, const msdk_string & pluginFullPath) {
    std::auto_ptr<PluginLoader<T> > plg(new PluginLoader<T> (userModule, pluginFullPath));
    return plg->IsOk() ? plg.release() : NULL;
}

template <class T>
MFXPlugin * LoadPluginByGUID(mfxSession session, const mfxPluginUID & uid, mfxU32 version) {
    std::auto_ptr<PluginLoader<T> > plg(new PluginLoader<T> (session, uid, version));
    return plg->IsOk() ? plg.release() : NULL;
}

inline MFXPlugin * LoadPlugin(mfxPluginType type, MFXVideoUSER *userModule, const msdk_string & pluginFullPath) {
    switch(type)
    {
    case MFX_PLUGINTYPE_VIDEO_GENERAL:
        return LoadPluginByType<MFXGenericPlugin>(userModule, pluginFullPath);
    case MFX_PLUGINTYPE_VIDEO_DECODE:
        return LoadPluginByType<MFXDecoderPlugin>(userModule, pluginFullPath);
    case MFX_PLUGINTYPE_VIDEO_ENCODE:
        return LoadPluginByType<MFXEncoderPlugin>(userModule, pluginFullPath);
    default:
        MSDK_TRACE_ERROR(MSDK_STRING("Unsupported plugin type : ")<< type << MSDK_STRING(", for plugin: ")<< pluginFullPath);
        return NULL;
    }
}

inline MFXPlugin * LoadPlugin(mfxPluginType type, mfxSession session, const mfxPluginUID & uid, mfxU32 version) {
    switch(type)
    {
    case MFX_PLUGINTYPE_VIDEO_GENERAL:
        return LoadPluginByGUID<MFXGenericPlugin>(session, uid, version);
    case MFX_PLUGINTYPE_VIDEO_DECODE:
        return LoadPluginByGUID<MFXDecoderPlugin>(session, uid, version);
    case MFX_PLUGINTYPE_VIDEO_ENCODE:
        return LoadPluginByGUID<MFXEncoderPlugin>(session, uid, version);
    default:
        MSDK_TRACE_ERROR(MSDK_STRING("Unsupported plugin type : ")<< type);
        return NULL;
    }
}
