/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <d3d9.h>
#include <dxva2api.h>
#include "mfx_renders.h"
#include "mfx_ihw_device.h"
#include "mfx_shared_ptr.h"
//#include "mfx_d3d_device.h"
#include "video_window.h"
#include <list>
#include "mfx_thread.h"

//#define BUFFERING_SIZE 3

class ScreenRender : public MFXVideoRender
{
public:
    struct InitParams
    {
        UINT                     nAdapter;
        VideoWindow::InitParams  window;
        mfx_shared_ptr<IHWDevice> pDevice;
        MFXThread::ThreadPool *mThreadPool;
        
        InitParams( UINT nAdapter = D3DADAPTER_DEFAULT
                  , const VideoWindow::InitParams &window = VideoWindow::InitParams()
                  , IHWDevice *pDevice = NULL
                  , MFXThread::ThreadPool *pool=NULL)
        {
            this->nAdapter = nAdapter;
            this->window   = window;
            this->pDevice.reset(pDevice);
            this->mThreadPool = pool;
        }
        InitParams(const InitParams& that)
        {
            nAdapter = that.nAdapter;
            window = that.window;
            pDevice = that.pDevice;
            mThreadPool = that.mThreadPool;
        }
    } m_initParams;
    
    ScreenRender(IVideoSession *core, mfxStatus *status, const InitParams &refParams);
    ~ScreenRender();


    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus Init(mfxVideoParam *par, const TCHAR *pFilename = NULL);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus GetDevice(IHWDevice **ppDevice);

    VOID OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
    VOID OnDestroy(HWND hwnd);
    VOID OnPaint(HWND hwnd);
    VOID OnSize(HWND hwnd, UINT state, int cx, int cy);

protected:
    mfxStatus SetupDevice();
    BOOL InitializeModule();
    BOOL RegisterSoftwareRasterizer();
    BOOL PreTranslateMessage(const MSG& msg);
    BOOL InitializeTimer();
    VOID DestroyTimer();
    BOOL EnableDwmQueuing();
    BOOL InitializeDevice();
    BOOL ResetDevice();

    LONGLONG GetTimestamp();
    IVideoSession  *m_pCore;
    mfxFrameInfo        m_mfxFrameInfo;

    VideoWindow         m_pWindow[1];
    HWND                m_Hwnd;
    bool                m_bSetup;
    //IDirect3DSurface9   *m_pRenderSurface;
    

    HMODULE m_hRgb9rastDLL;
    HMODULE m_hDwmApiDLL;
    PVOID m_pfnD3D9GetSWInfo;
    PVOID m_pfnDwmIsCompositionEnabled;
    PVOID m_pfnDwmEnableComposition;
    PVOID m_pfnDwmGetCompositionTimingInfo;
    PVOID m_pfnDwmSetPresentParameters;

    BOOL m_bTimerSet;
    HANDLE  m_hTimer;
    DWORD m_StartSysTime;
    DWORD m_PreviousTime;
    BOOL m_bDspFrameDrop;
    BOOL m_bDwmQueuing;
    BOOL m_bDwmQueuingDisabledPrinted;

    int         m_FrameCount;
    int         m_LastFrameCount;
    LONGLONG    m_LastTime;
    UINT        m_nAdapter;
    bool        m_bSetWindowText;
#ifdef BUFFERING_SIZE
    std::list<mfxFrameSurface1 *> frameBuffer;
    
#endif

    //mfxFrameAllocator   m_VPPFrameAllocator;
};

#endif // #if defined(_WIN32) || defined(_WIN64)
