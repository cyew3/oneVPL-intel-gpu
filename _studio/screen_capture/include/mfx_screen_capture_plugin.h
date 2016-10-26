//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2015 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_SCREEN_CAPTURE_PLUGIN_H__)
#define __MFX_SCREEN_CAPTURE_PLUGIN_H__

#include <atlbase.h>
#include <memory>
#include "mfxplugin++.h"
#include "mfx_screen_capture_ddi.h"
#include "mfx_screen_capture_dirty_rect.h"
#include "mfx_screen_capture_cm_dirty_rect.h"

namespace MfxCapture
{

class MFXScreenCapture_Plugin: public MFXDecoderPlugin
{
public:

    static MFXDecoderPlugin* Create() {
        return new MFXScreenCapture_Plugin(false);
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        if (memcmp(& guid , &MFX_PLUGINID_CAPTURE_HW, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }
        MFXScreenCapture_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFXScreenCapture_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }

        try
        {
            tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXDecoderPlugin> (tmp_pplg));
        }
        catch(std::bad_alloc&)
        {
            delete tmp_pplg;
            return MFX_ERR_MEMORY_ALLOC;
        }
        *mfxPlg = (mfxPlugin)*tmp_pplg->m_adapter;
        tmp_pplg->m_createdByDispatcher = true;

        return MFX_ERR_NONE;
    }

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);

    virtual mfxStatus DecodeHeader(mfxBitstream *, mfxVideoParam *)
    {
        return MFX_ERR_UNSUPPORTED;
    };
    virtual mfxStatus GetPayload(mfxU64 *, mfxPayload *)
    {
        return MFX_ERR_UNSUPPORTED;
    };
    virtual mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task);

    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts)
    {
        if(!task)
            return MFX_ERR_NULL_PTR;
        delete (AsyncParams*)task;
        sts = MFX_ERR_NONE;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryMode1(mfxVideoParam& out);
    virtual mfxStatus QueryMode2(const mfxVideoParam& in, mfxVideoParam& out, bool onInit = false);

    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);

    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Reset(mfxVideoParam *)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual void Release()
    {
        delete this;
    }

    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_DECODE;
    }
    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNSUPPORTED;
    }
protected:
    explicit MFXScreenCapture_Plugin(bool CreateByDispatcher);
    virtual ~MFXScreenCapture_Plugin();
    std::auto_ptr<MFXPluginAdapter<MFXDecoderPlugin> > m_adapter;

    mfxStatus DecodeFrameSubmit(mfxFrameSurface1 *surface, bool& rt_fallback_d3d, bool& rt_fallback_dxgi, mfxFrameSurface1 *ext_surface);
    mfxStatus CheckFrameInfo(const mfxFrameInfo& info);
    mfxStatus CheckOpaqBuffer(const mfxVideoParam& par, mfxVideoParam* pParOut, const mfxExtOpaqueSurfaceAlloc& opaqAlloc, mfxExtOpaqueSurfaceAlloc* pOpaqAllocOut);
    mfxStatus CheckExtBuffer(const mfxVideoParam& par, mfxVideoParam* pParOut, const mfxExtScreenCaptureParam& extPar, mfxExtScreenCaptureParam* pExtParOut);
    mfxFrameSurface1* GetFreeInternalSurface();

    mfxVersion          m_libmfxVer;
    mfxVideoParam       m_CurrentPar;
    mfxExtScreenCaptureParam m_CurExtPar;
    mfxVideoParam       m_InitPar; //for Reset() func impl
    mfxExtScreenCaptureParam m_InitExtPar;
    mfxCoreInterface*   m_pmfxCore;
    mfxPluginParam      m_PluginParam;
    mfxFrameAllocResponse m_response;
    mfxExtOpaqueSurfaceAlloc m_OpaqAlloc;
    std::list<mfxFrameSurface1> m_SurfPool;
    bool                m_createdByDispatcher;
    bool                m_inited;
    bool                m_bSysMem;

    bool                m_bDirtyRect;
    mfxU32              m_DisplayIndex;

    mfxU32              m_StatusReportFeedbackNumber;

    std::auto_ptr<Capturer>         m_pCapturer;
    std::auto_ptr<Capturer>         m_pFallbackDXGICapturer;
    std::auto_ptr<Capturer>         m_pFallbackD3D9Capturer;

    std::auto_ptr<DirtyRectFilter>         m_pDirtyRectAnalyzer;
    mfxFrameSurface1*               m_pPrevSurface;
    mfxFrameSurface1*               m_pPrevIntSurface;

    std::list<DESKTOP_QUERY_STATUS_PARAMS> m_StatusList;

    UMC::Mutex m_Guard;
    UMC::Mutex m_DirtyRectGuard;

private: // prohobit copy constructor and assignment operator
    MFXScreenCapture_Plugin(const MFXScreenCapture_Plugin& that);
    MFXScreenCapture_Plugin& operator=(const MFXScreenCapture_Plugin&);
};

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxVideoParam* par);

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

} //namespace MfxCapture

#endif