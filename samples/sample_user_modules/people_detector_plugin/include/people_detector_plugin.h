/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#ifndef __PEOPLE_DETECTOR_PLUGIN_H
#define __PEOPLE_DETECTOR_PLUGIN_H

#include <stddef.h>

#include <memory>
#include "mfxplugin++.h"
#include "detect_people_plugin_api.h"
#include "frameprocessor.h"

typedef struct {
    mfxFrameSurface1 *In;
    mfxFrameSurface1 *Out;
    bool bBusy;
    Processor *pProcessor;
} PeopleDetectorTask;

class PeopleDetectionPlugin : public MFXVPPPlugin
{
public:
    PeopleDetectionPlugin();
    virtual ~PeopleDetectionPlugin();

    // mfxPlugin functions
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus Submit(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task); //?
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts);

    //mfxVideoCodecPlugin functions
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
    virtual mfxStatus Init(mfxVideoParam *mfxParam);
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxThreadTask *task);
    virtual mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *, mfxFrameSurface1 *, mfxFrameSurface1 **, mfxThreadTask *)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    virtual void Release()
    {
        delete this;
    }
    static mfxStatus CreatePeopleDetector(mfxPluginUID guid, mfxPlugin* mfxPlg)
    {
        PeopleDetectionPlugin* pPlugin = NULL;
        if (memcmp(& guid , &g_PeopleDetection_PluginGuid, sizeof(mfxPluginUID)))
        {
            return MFX_ERR_NOT_FOUND;
        }
        pPlugin = new PeopleDetectionPlugin();
        if (pPlugin == NULL)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        //The plugin adapter automatically sets callbacks
        pPlugin->m_adapter.reset(new MFXPluginAdapter<MFXVPPPlugin> (pPlugin));
        *mfxPlg = pPlugin->m_adapter->operator mfxPlugin();

        return MFX_ERR_NONE;
    }

    //virtual mfxStatus SetAuxParams(void* auxParam, int auxParamSize);
    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:
    mfxSession          m_session;
    std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > m_adapter;

private:
    bool m_bInited;

    MFXCoreInterface     m_mfxCore;

    mfxVideoParam        m_VideoParam;
    mfxPluginParam       m_PluginParam;
    vaExtVPPDetectPeople m_Param;

    PeopleDetectorTask  *m_pTasks;
    mfxU32               m_MaxNumTasks;

    mfxStatus CheckInOutFrameInfo(mfxFrameInfo *pIn, mfxFrameInfo *pOut);
    mfxU32    FindFreeTaskIdx();

    bool m_bIsInOpaque;
    bool m_bIsOutOpaque;

};

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif

#endif // __PEOPLE_DETECTOR_PLUGIN_H
