/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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
