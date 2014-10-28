/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_SCREEN_CAPTURE_PLUGIN_H__)
#define __MFX_SCREEN_CAPTURE_PLUGIN_H__

#include <list>
#include <atlbase.h>
#include <d3d11.h>
#include <memory>
#include "mfxplugin++.h"

typedef struct tagDESKTOP_QUERY_STATUS_PARAMS  
{
    UINT    StatusReportFeedbackNumber;
    UINT    uStatus;
} DESKTOP_QUERY_STATUS_PARAMS;

class MFXScreenCapture_Plugin: public MFXDecoderPlugin
{
public:

    struct AsyncParams
    {
        mfxFrameSurface1 *surface_work;
        mfxFrameSurface1 *surface_out;
        mfxU32 StatusReportFeedbackNumber;
    };

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

    mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);
    mfxStatus CreateVideoAccelerator(mfxU16& SampleWidth, mfxU16& SampleHeight, DXGI_FORMAT& OutputFormat);
    mfxStatus CheckFrameInfo(mfxFrameInfo *info);
    mfxStatus BeginFrame(mfxMemId   MemId, ID3D11VideoDecoderOutputView *pOutputView);
    mfxStatus GetDesktopScreenOperation(mfxFrameSurface1 *surface_work);
    mfxStatus CheckCapabilities(CComQIPtr<ID3D11VideoContext>& pD11VideoContext, mfxU16& w, mfxU16& h);
    mfxStatus QueryStatus();

    mfxVideoParam       m_CurrentPar;
    mfxVideoParam       m_InitPar; //for Reset() func impl
    mfxCoreInterface*   m_pmfxCore;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    bool                m_inited;

    mfxU32              m_StatusReportFeedbackNumber;

    CComPtr<ID3D11Device>            m_pD11Device;
    CComPtr<ID3D11DeviceContext>     m_pD11Context;

    CComQIPtr<ID3D11VideoDevice>     m_pD11VideoDevice;
    CComQIPtr<ID3D11VideoContext>    m_pD11VideoContext;


    // we own video decoder, let using com pointer 
    CComPtr<ID3D11VideoDecoder>      m_pDecoder;

    std::list<DESKTOP_QUERY_STATUS_PARAMS> m_StatusList;

};

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif