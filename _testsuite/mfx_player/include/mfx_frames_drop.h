/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_rnd_wrapper.h"
#include "mfx_unified_execute_proxy.h"

//drops frames according to provided pattern, could be attached to any interfaces decode,vpp,render
template<class T>
class  FramesDrop
    : public UnifiedExecuteProxy<T>
{
  typedef UnifiedExecuteProxy<T> base;
public:
    //nDrop - number of frames to drop in the end of cycle
    //nDropCycle - drop interval in frames
    FramesDrop (mfxU32 nDrop, mfxU32 nDropCycle, IMFXVideoRender *pTarget)
        : base (pTarget)
        , m_nFramesToDrop(nDrop)
        , m_nDropCycle(nDropCycle)
        , m_nCurrentFrame()
    {
        if (nDrop>nDropCycle)
        {
            m_nFramesToDrop = 0;
        }
    }

    virtual mfxStatus Execute(const ExecuteType<T>& args)
    {
        if ((m_nCurrentFrame++ % m_nDropCycle) < (m_nDropCycle - m_nFramesToDrop))
            return base::Execute(args);

        return MFX_ERR_NONE;
    }
protected:
    mfxU32 m_nFramesToDrop;
    mfxU32 m_nDropCycle;
    mfxU32 m_nCurrentFrame;
};

