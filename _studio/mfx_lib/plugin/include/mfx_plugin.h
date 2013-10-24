/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfx_plugin.h

\* ****************************************************************************** */

#if !defined(__mfxplugin_hevcd_INCLUDED__)
#define __mfxplugin_hevcd_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"

class MFXLibPlugin : public MFXDecoderPlugin
{
public:
    MFXLibPlugin();
    ~MFXLibPlugin();

    void Init();
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts);
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close() 
    {
        return MFXVideoDECODE_Close(m_session);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return MFXVideoDECODE_GetVideoParam(m_session, par);
    }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
    {
        return MFXVideoDECODE_DecodeHeader(m_session, bs, par);
    }
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload)
    {
        return MFXVideoDECODE_GetPayload(m_session, ts, payload);
    }
    virtual void Release() 
    {
        delete this;
    }
    static MFXDecoderPlugin* Create() {
        return new MFXLibPlugin();
    }

    virtual mfxU32 GetPluginType() 
    {
        return MFX_PLUGINTYPE_VIDEO_DECODE;
    }
    mfxStatus Submit(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task)
    {
        return MFX_ERR_UNKNOWN;
    }
    virtual mfxStatus SetAuxParams(void* auxParam, int auxParamSize)
    {
        auxParam;
        auxParamSize;
        return MFX_ERR_UNKNOWN;
    }

protected:
    mfxStatus GetHandle();

    std::auto_ptr<MFXCoreInterface> m_pmfxCore;
    mfxCoreInterface*   m_pCore;
    mfxFrameAllocator * m_pFrameAllocator;
    mfxFrameAllocator * m_pExternalSurfaceAllocator;

    mfxSession      m_session;
    mfxSession      m_Externalsession;
    mfxPluginParam  m_PluginParam;
    mfxU16          max_AsyncDepth;
    mfxSyncPoint    m_syncp;
    mfxHDL**        m_p_surface_out;
    mfxHDL          m_device_handle;

    typedef struct {
        mfxHDL handle;
        mfxHandleType type;
    } DeviceHandle;

    DeviceHandle    m_device;

};

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

enum eMfxImplType
{
    MFX_LIB_HARDWARE            = 0,
    MFX_LIB_SOFTWARE            = 1,
    MFX_LIB_PSEUDO              = 2,

    MFX_LIB_IMPL_TYPES
};
typedef void * mfxModuleHandle;
typedef void (MFX_CDECL * mfxFunctionPointer)(void);
enum eFunc
{
    eMFXJoinSession =2,
    eMFXDisjoinSession = 6
};

struct MOCK_MFX_DISP_HANDLE
{
    MOCK_MFX_DISP_HANDLE(const mfxVersion requiredVersion) {return;}
    ~MOCK_MFX_DISP_HANDLE(void) {return;}

    mfxStatus LoadSelectedDLL(void) {return MFX_ERR_NONE;}
    mfxStatus UnLoadSelectedDLL(void) {return MFX_ERR_NONE;}
    mfxStatus Close(void) {return MFX_ERR_NONE;}
    eMfxImplType implType;
    mfxIMPL impl;
    mfxIMPL implInterface;
    mfxVersion dispVersion;
    mfxSession session;
    mfxVersion apiVersion;
    mfxModuleHandle hModule;
    mfxFunctionPointer callTable[9];
    //mfxFunctionPointer callAudioTable[eAudioFuncTotal];
private:
    // Declare assignment operator and copy constructor to prevent occasional assignment
    //MOCK_MFX_DISP_HANDLE(const MOCK_MFX_DISP_HANDLE &);
    //MOCK_MFX_DISP_HANDLE & operator = (const MOCK_MFX_DISP_HANDLE &);    
};
#endif  // __mfxplugin_hevcd_INCLUDED__
