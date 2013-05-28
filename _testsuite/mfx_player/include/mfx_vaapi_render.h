/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#ifdef LIBVA_X11_SUPPORT

#include "mfx_renders.h"
#include "mfx_ihw_device.h"
#include "xvideo_window.h"
#include "mfx_shared_ptr.h"
#include <list>

#include <X11/Xutil.h>
#include <X11/Xos.h>


class ScreenVAAPIRender : public MFXVideoRender
{
public:
    struct InitParams
    {
        int                     nAdapter;
        XVideoWindow::InitParams  window;
        mfx_shared_ptr<IHWDevice> pDevice;
        
        InitParams( int nAdapter = 0
                  , const XVideoWindow::InitParams &window = XVideoWindow::InitParams()
                  , IHWDevice *pDevice = NULL)
        {
            this->nAdapter = nAdapter;
            this->window   = window;
            this->pDevice.reset(pDevice);
        }
        InitParams(const InitParams& that)
        {
            nAdapter = that.nAdapter;
            window = that.window;
            pDevice = that.pDevice;
        }
    } m_initParams;
    
    ScreenVAAPIRender(IVideoSession *core, mfxStatus *status, const InitParams &refParams);
    ~ScreenVAAPIRender();


    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus GetDevice(IHWDevice **ppDevice);


protected:
    mfxStatus SetupDevice();
    bool InitializeModule();
    bool InitializeDevice();
    bool ResetDevice();

    IVideoSession*      m_pCore;
    mfxFrameInfo        m_mfxFrameInfo;

    XVideoWindow        m_pWindow[1];
    mfxHDL              m_Hwnd;
    Display *           m_Dpy;
    bool                m_bSetup;
    //IDirect3DSurface9   *m_pRenderSurface;

#ifdef BUFFERING_SIZE
    std::list<mfxFrameSurface1 *> frameBuffer;
#endif

    //mfxFrameAllocator   m_VPPFrameAllocator;
};

#endif // #ifdef LIBVA_X11_SUPPORT
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
