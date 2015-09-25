//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//  This sample was distributed or derived from the Intel’s Media Samples package.
//  The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
//  or https://software.intel.com/en-us/media-client-solutions-support.
//        Copyright (c) 2005-2015 Intel Corporation. All Rights Reserved.
//


#ifndef __DECODE_D3D_RENDER_H__
#define __DECODE_D3D_RENDER_H__

#if defined(_WIN32) || defined(_WIN64)

#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#include <windows.h>
#include "igfx_s3dcontrol.h"
#endif

#include "mfxstructures.h"
#include "mfxvideo.h"

#include "hw_device.h"

typedef void* WindowHandle;
typedef void* Handle;

#if defined(_WIN32) || defined(_WIN64)

struct sWindowParams
{
    LPCTSTR lpClassName;
    LPCTSTR lpWindowName;
    DWORD dwStyle;
    int nx;
    int ny;
    int ncell;
    int nAdapter;
    int nWidth;
    int nHeight;
    HWND hWndParent;
    HMENU hMenu;
    HINSTANCE hInstance;
    LPVOID lpParam;
    bool bFullScreen; // Stretch window to full screen
};

class CDecodeD3DRender
{
public:

    CDecodeD3DRender();
    virtual ~CDecodeD3DRender();

    virtual mfxStatus Init(sWindowParams pWParams);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *pSurface, mfxFrameAllocator *pmfxAlloc);
    virtual VOID UpdateTitle(double fps);

    HWND GetWindowHandle();

    VOID OnDestroy(HWND hwnd);
    VOID OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
    VOID ChangeWindowSize(bool bFullScreen);

    void SetHWDevice(CHWDevice *dev)
    {
        m_hwdev = dev;
    }
protected:
    void AdjustWindowRect(RECT *rect);

    CHWDevice *m_hwdev;

    sWindowParams       m_sWindowParams;
    HWND                m_Hwnd;
    RECT                m_rect;
    DWORD               m_style;

    bool EnableDwmQueuing();
    static BOOL CALLBACK MonitorEnumProc(HMONITOR ,HDC ,LPRECT lprcMonitor,LPARAM dwData);
    static bool m_bIsMonitorFound;

    bool                 m_bDwmEnabled;
    int                  m_nMonitorCurrent;
    ::RECT               m_RectWindow;
};
#endif // #if defined(_WIN32) || defined(_WIN64)

#endif // __DECODE_D3D_RENDER_H__