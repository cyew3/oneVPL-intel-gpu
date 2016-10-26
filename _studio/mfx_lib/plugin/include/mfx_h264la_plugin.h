//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_H264LA_PLUGIN_INCLUDED__)
#define __MFX_H264LA_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"
#include "mfxvideo++int.h"
#include "mfxenc.h"


#if defined( AS_H264LA_PLUGIN )
class MFXH264LAPlugin : public MFXEncPlugin
{
public:
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus EncFrameSubmit(mfxENCInput *in, mfxENCOutput *out, mfxThreadTask *task) 
    {
        return MFXVideoENC_ProcessFrameAsync(m_session, in, out, (mfxSyncPoint*) task);
    }
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 )
    {
        return MFXVideoCORE_SyncOperation(m_session, (mfxSyncPoint) task, MFX_INFINITE);
    }
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus )
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        return MFXVideoENC_Query(m_session, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *)
    {
        return MFXVideoENC_QueryIOSurf(m_session, par, in);
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        return MFXVideoENC_Init(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return MFXVideoENC_Reset(m_session, par);
    }
    virtual mfxStatus Close()
    {
        return MFXVideoENC_Close(m_session);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return MFXVideoENC_GetVideoParam(m_session, par);
    }
    virtual void Release()
    {
        delete this;
    }
    static MFXEncPlugin* Create() {
        return new MFXH264LAPlugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        if (memcmp(& guid , &MFX_PLUGINID_H264LA_HW, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }
        MFXH264LAPlugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFXH264LAPlugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(MFX_CORE_CATCH_TYPE) 
        { 
            return MFX_ERR_UNKNOWN;; 
        }

        tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXEncPlugin> (tmp_pplg));
        *mfxPlg = tmp_pplg->m_adapter->operator mfxPlugin();
        tmp_pplg->m_createdByDispatcher = true;
        return MFX_ERR_NONE;
    }
    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_ENC;
    }
    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:
    MFXH264LAPlugin(bool CreateByDispatcher);
    virtual ~MFXH264LAPlugin();

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    std::auto_ptr<MFXPluginAdapter<MFXEncPlugin> > m_adapter;
};
#endif //#if defined( AS_H264LA_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  
#endif

#endif  // __MFX_H264LA_PLUGIN_INCLUDED__
