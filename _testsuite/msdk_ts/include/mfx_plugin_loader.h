/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */



#pragma once
#include <sstream>
#include "test_common.h"
#include "vm_shared_object.h"
#include "mfxplugin++.h"

typedef std::basic_string<vm_char>tstring;
typedef std::basic_stringstream<vm_char> tstringstream;

class MsdkSoModule
{
protected:
    vm_so_handle m_module;
public:
    MsdkSoModule(const tstring & pluginName)
        : m_module()
    {
        m_module = vm_so_load(pluginName.c_str());
        if (NULL == m_module)
        {
            int lastErr = 0;
            std::cout << "Failed to load shared module." << std::endl;
            #ifdef WIN32
                lastErr = GetLastError();
            #endif
            std::cout << "Error = " << lastErr << std::endl;
        }
    }
    template <class T>
    T GetAddr(const std::string & fncName)
    {
        T pCreateFunc = reinterpret_cast<T>(vm_so_get_addr(m_module, fncName.c_str()));
        if (NULL == pCreateFunc) {
            std::cout << "Failed to get function addres." << std::endl;
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

struct PluginModuleTemplate {
    typedef MFXDecoderPlugin* (*fncCreateDecoderPlugin)();
    typedef MFXEncoderPlugin* (*fncCreateEncoderPlugin)();
    typedef MFXGenericPlugin* (*fncCreateGenericPlugin)();
    typedef MFXVPPPlugin*     (*fncCreateVPPPlugin)();

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

template <class T>
class PluginLoader 
{
protected:
    MsdkSoModule m_PluginModule;
    std::auto_ptr<T> m_plugin;
    MFXPluginAdapter<T> m_adapter;
    typedef PluginCreateTrait<T> Plugin;

public:
    mfxPlugin* plugin;
    PluginLoader(const tstring & pluginName)
        : m_PluginModule(pluginName)
    {
        plugin = 0;
        if (!m_PluginModule.IsLoaded())
            return;
        typename Plugin::TCreator pCreateFunc = m_PluginModule.GetAddr<typename Plugin::TCreator >(Plugin::Name());
        if(NULL == pCreateFunc)
            return;

        m_plugin.reset(pCreateFunc());
        if (NULL == m_plugin.get())
        {
            std::cout << "Failed to Create plugin." << std::endl;
            return;
        }
        m_adapter = make_mfx_plugin_adapter(m_plugin.get());
        plugin = (mfxPlugin*) &m_adapter;
    }

    ~PluginLoader()
    {
        //loaded module are freed in MsdkSoModule destructor
    }
};