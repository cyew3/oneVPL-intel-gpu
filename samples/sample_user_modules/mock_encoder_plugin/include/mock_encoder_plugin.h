//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include <stdlib.h>
#include <memory>
#include <list>

#include "mfxplugin++.h"
#include "sample_defs.h"
#include <iostream>

class MockEncoderPluginTask {
public:
    mfxStatus operator () () {
        MSDK_TRACE_INFO("MockEncoderPlugin::Execute task=" << this);
        return MFX_ERR_NONE;
    }
};


class MockEncoderPlugin : public MFXEncoderPlugin
{
    static const mfxPluginUID g_mockEncoderGuid;
    static std::auto_ptr<MockEncoderPlugin> g_singlePlg;
    static std::auto_ptr<MFXPluginAdapter<MFXEncoderPlugin> > g_adapter;
public:
    MockEncoderPlugin(bool createdByDispatcher);
    ~MockEncoderPlugin();


    // methods to be called by Media SDK
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts); 
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task);
    virtual mfxStatus SetAuxParams(void* , int ) {
        return MFX_ERR_NONE;
    }
    
    virtual void Release() {
        delete this;
    }
    static MFXEncoderPlugin * Create() {
        return new MockEncoderPlugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        
        if (g_singlePlg.get()) {
            return MFX_ERR_NOT_FOUND;
        } 
        //check that guid match 
        g_singlePlg.reset(new MockEncoderPlugin(true));

        if (memcmp(& guid , &g_mockEncoderGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }

        g_adapter.reset(new MFXPluginAdapter<MFXEncoderPlugin> (g_singlePlg.get()));
        *mfxPlg = g_adapter->operator mfxPlugin();

        return MFX_ERR_NONE;
    }

protected:
    bool            m_createdByDispatcher;
    mfxVideoParam   m_VideoParam;
    MFXPluginParam  m_PluginParam;

    std::list<MockEncoderPluginTask> m_Tasks;
    mfxU32          m_MaxNumTasks;
    bool            m_bIsOpaq;
    bool            m_bInited;

    //core should be release after all active tasks
    MFXCoreInterface m_mfxCore;
};
