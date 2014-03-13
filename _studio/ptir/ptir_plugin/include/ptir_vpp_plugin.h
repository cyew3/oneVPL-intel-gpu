/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_PTIR_VPP_PLUGIN_INCLUDED__)
#define __MFX_PTIR_VPP_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include <vector>
#include "mfxvideo.h"
#include "mfxplugin++.h"



extern "C" {
#include "..\ptir\pa\api.h"
}
//#include "mfxvideo++int.h"

struct PTIR_Task
{
    mfxU32           id;
    bool             filled;
    mfxFrameSurface1 *surface_out;
};

enum {
    //MFX_FOURCC_NV12         = MFX_MAKEFOURCC('N','V','1','2'),   /* Native Format */
    //...

    MFX_FOURCC_I420         = MFX_MAKEFOURCC('I','4','2','0'),   /* Non-native Format */
};

class MFX_PTIR_Plugin : public MFXVPPPlugin
{
    static const mfxPluginUID g_VPP_PluginGuid;
public:
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 );
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus );
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual void Release()
    {
        delete this;
    }
    static MFXVPPPlugin* Create() {
        return new MFX_PTIR_Plugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        if (memcmp(& guid , &g_VPP_PluginGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }
        MFX_PTIR_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFX_PTIR_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(...)
        {
            return MFX_ERR_UNKNOWN;;
        }
        tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXVPPPlugin> (tmp_pplg));
        *mfxPlg = tmp_pplg->m_adapter->operator mfxPlugin();
        tmp_pplg->m_createdByDispatcher = true;
        return MFX_ERR_NONE;
    }
    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_VPP;
    }
    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:
    MFX_PTIR_Plugin(bool CreateByDispatcher);
    virtual ~MFX_PTIR_Plugin();

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > m_adapter;

    mfxStatus PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out);
    mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out);

    bool b_firstFrameProceed;
    bool bInited;
    bool bInExecution;
    bool bMoreOutFrames;
    mfxFrameSurface1* prevSurf;
    std::vector<PTIR_Task*> vTasks;
    std::vector<mfxFrameSurface1*> inSurfs;

    unsigned int
        i,
        uiDeinterlacingMode,
        uiDeinterlacingMeasure,
        uiSupBuf,
        uiCur,
        uiNext,
        uiTimer,
        uiFrame,
        uiFrameOut,
        uiProgress,
        uiInWidth,
        uiInHeight,
        uiWidth,
        uiHeight,
        uiStart,
        uiCount,
        uiFrameCount,
        uiBufferCount,
        uiSize,
        uiLastFrameNumber,
        uiDispatch;
    unsigned char
        *pucIO;
    Frame
        frmIO[LASTFRAME + 1],
        *frmIn,
        *frmBuffer[LASTFRAME];
    FrameQueue
        fqIn;
    DWORD
        uiBytesRead;
    double
        dTime,
        *dSAD,
        *dRs,
        dPicSize,
        dFrameRate,
        dTimeStamp,
        dBaseTime,
        dOutBaseTime,
        dBaseTimeSw,
        dDeIntTime;
    LARGE_INTEGER
        liTime,
        liFreq,
        liFileSize;
    CONSOLE_SCREEN_BUFFER_INFO
        sbInfo;
    HANDLE
        hIn,
        hOut;
    FILE
        *fTCodeOut;
    BOOL
        patternFound,
        ferror,
        bisInterlaced,
        bFullFrameRate;
    Pattern
        mainPattern;
    errno_t
        errorT;


};

mfxStatus ColorSpaceConversionWCopy(mfxFrameSurface1* surface_in, mfxFrameSurface1* surface_out, mfxU32 dstFormat);

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif

#endif  // __MFX_PTIR_VPP_PLUGIN_INCLUDED__
