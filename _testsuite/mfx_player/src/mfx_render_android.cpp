/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(ANDROID)

#include "mfx_pipeline_defs.h"
#include "mfx_render_android.h"

ScreenRenderAndroid::ScreenRenderAndroid(IVideoSession *pCore, mfxStatus *pStatus, IHWDevice *pHWDevice) :
    MFXVideoRender(pCore, pStatus),
    m_pHWDevice(pHWDevice)
{
}

ScreenRenderAndroid::~ScreenRenderAndroid()
{
}

mfxStatus ScreenRenderAndroid::QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest * /*request*/)
{
    return MFX_ERR_NONE;
}

mfxStatus ScreenRenderAndroid::Init(mfxVideoParam * /*par*/, const vm_char * /*pFilename*/)
{
    MFX_CHECK_STS(m_pHWDevice->Init(0, NULL, true, MFX_SCREEN_RENDER, 0, NULL));

    return MFX_ERR_NONE;
}

mfxStatus ScreenRenderAndroid::RenderFrame(mfxFrameSurface1 *pSurface, mfxEncodeCtrl *pCtrl)
{
    if (!pSurface)
    {
        return MFX_ERR_NONE; // on EOS
    }

    if (pCtrl && pCtrl->SkipFrame)
    {
        return MFX_ERR_NONE;
    }

    MFX_CHECK_STS(m_pHWDevice->RenderFrame(pSurface, NULL));

    return MFX_ERR_NONE;
}

mfxStatus ScreenRenderAndroid::GetDevice(IHWDevice **ppDevice)
{
    *ppDevice = m_pHWDevice;
    return MFX_ERR_NONE;
}

#endif //#if defined(ANDROID)
