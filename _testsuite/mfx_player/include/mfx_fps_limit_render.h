/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_rnd_wrapper.h"
#include "mfx_itime.h"

class FPSLimiterRender
    : public InterfaceProxy<IMFXVideoRender>
{
public:

    FPSLimiterRender(bool bVerbose, double limit_fps, ITime * pTime, IMFXVideoRender * pDecorated);
    ~FPSLimiterRender();

    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);

protected:
    double m_dTargetFPS;
    mfxU32 m_nFrames;
    ITime *m_pTimeProvider;
    mfxF64 m_nStartTick;
    bool   m_bVerbose;
};