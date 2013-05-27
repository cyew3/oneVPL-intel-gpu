/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_fps_limit_render.h"

FPSLimiterRender::FPSLimiterRender(bool bVerbose, double limit_fps, ITime * pTime, IMFXVideoRender * pDecorated)
: InterfaceProxy<IMFXVideoRender>(pDecorated)
, m_dTargetFPS(limit_fps)
, m_nFrames()
, m_pTimeProvider(pTime)
, m_nStartTick(0.0)
, m_bVerbose(bVerbose)
{
}

FPSLimiterRender::~FPSLimiterRender()
{
    PrintInfo(VM_STRING("FPSLimitedRender: numframes"), VM_STRING("%d"), m_nFrames);
}

mfxStatus FPSLimiterRender::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
{
    double fCurrentTime = 0.0;
    if (!m_nFrames)
    {
        m_nStartTick = m_pTimeProvider->Current();
    }
    else
    {
        fCurrentTime = m_pTimeProvider->Current() - m_nStartTick;
    }
    
    int time_to_sleep = (int)(1000.0 * ((double)m_nFrames / m_dTargetFPS - fCurrentTime));

    if (time_to_sleep > 0)
    {
        MPA_TRACE("FPSLimitRender::Sleep()");
        ::vm_time_sleep(time_to_sleep);
    }

    if (m_bVerbose)
    {
        double fCurrentFps = (double)m_nFrames / fCurrentTime;
        PipelineTrace((VM_STRING("Rendering FPS: %.3lf\n"), fCurrentFps));
    }

    MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::RenderFrame(surface, pCtrl));

    m_nFrames++;

    return MFX_ERR_NONE;
}
