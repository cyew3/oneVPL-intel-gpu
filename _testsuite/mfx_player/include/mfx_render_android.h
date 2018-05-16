/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_RENDER_ANDROID_H
#define __MFX_RENDER_ANDROID_H

#ifdef ANDROID

#include "mfx_renders.h"
#include "mfx_ihw_device.h"

class ScreenRenderAndroid : public MFXVideoRender
{
public:
    ScreenRenderAndroid(IVideoSession *pCore, mfxStatus *pStatus, IHWDevice *pHWDevice);
    ~ScreenRenderAndroid();

    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *pSurface, mfxEncodeCtrl *pCtrl = NULL);
    virtual mfxStatus GetDevice(IHWDevice **ppDevice);

private:
    IHWDevice *m_pHWDevice;
};

#endif // #ifdef ANDROID
#endif // #ifndef __MFX_RENDER_ANDROID_H
