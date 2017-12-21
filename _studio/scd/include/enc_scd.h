//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#pragma once

#include "mfx_config.h"
#include "scd_tools.h"
#include "mfxscd.h"
#include "mfxplugin++.h"
#include "asc.h"

namespace MfxEncSCD
{
using namespace SCDTools;

static const mfxU16 ENC_SCD_MAX_THREADS = 1;

enum eTaskStage
{
      COPY_IN_TO_INTERNAL  = 0b0001
    , DO_SCD               = 0b1000
};

struct Task
{
    void Reset()
    {
        m_surfIn = 0;
        memset(&m_stages, 0, sizeof(m_stages));
        m_pResult = 0;
    }

    mfxU32 m_stages[ENC_SCD_MAX_THREADS];
    mfxFrameSurface1* m_surfIn;
    mfxFrameSurface1* m_surfRealIn;
    mfxFrameSurface1  m_surfNative;
    mfxExtSCD* m_pResult;
};

    typedef ns_asc::ASC SCD;
    using namespace ns_asc;

class Plugin
    : public MFXEncPlugin
    , protected MFXCoreInterface
    , protected SCD
    , protected TaskManager<Task>
    , protected InternalSurfaces
{
public:
    static MFXEncPlugin* Create();
    static mfxStatus CreateByDispatcher(const mfxPluginUID& guid, mfxPlugin* mfxPlg);

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus EncFrameSubmit(mfxENCInput *in, mfxENCOutput *out, mfxThreadTask *task);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32, mfxU32);
    virtual mfxStatus FreeResources(mfxThreadTask, mfxStatus);
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual void Release()
    {
        delete this;
    }
    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_VPP;
    }
    virtual mfxStatus SetAuxParams(void*, int)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

protected:
    explicit Plugin(bool CreateByDispatcher);
    virtual ~Plugin();

    inline bool isSysIn()
    {
        return (m_vpar.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || ((m_vpar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
                && (m_osa.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY));
    }

    bool m_createdByDispatcher;
    MFXPluginAdapter<MFXEncPlugin> m_adapter;
    bool m_bInit;
    mfxVideoParam m_vpar;
    mfxVideoParam m_vpar_init;
    mfxExtOpaqueSurfaceAlloc m_osa;
};

}