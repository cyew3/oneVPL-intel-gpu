/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_imultiple.h"

/* Rationale:
    vpp usage model for SVC pipeline consist of special initialisation of dependency layers, and svcdonwlosapling buffer
    also vpp expects frameid iteration on output when it returns MFX_ERR_MORE_SURFACE
    mfx_pipiline cannot iterate output frameid, so decided to implement iteration inside vpp wrapper, lucky frames always passed via
    non const pointer so input can be modified to reflect frame id search in find free surface call
*/
class SVCedVpp
    : public InterfaceProxy<IMFXVideoVPP>
{
    static const mfxU16 uninitId = (mfxU16 )-1;

    typedef InterfaceProxy<IMFXVideoVPP> base;
    mfxU16 m_nDepIdStart;
    mfxU16 m_nDepIdCurrent;

public:
    SVCedVpp(IMFXVideoVPP *pTarget)
        : base(pTarget)
        , m_nDepIdStart(uninitId)
        , m_nDepIdCurrent(uninitId)
    {
    }

    mfxStatus RunFrameVPPAsync(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
    {
        if (uninitId != m_nDepIdStart) {
            out->Info.FrameId.DependencyId = m_nDepIdCurrent;
        } else {
            out->Info.FrameId.DependencyId = in->Info.FrameId.DependencyId;
        }

        mfxStatus sts; 
        MFX_CHECK_STS_SKIP(sts = base::RunFrameVPPAsync(in, out, aux, syncp)
            , MFX_WRN_DEVICE_BUSY
            , MFX_ERR_MORE_SURFACE);

        //increment if vpp accept
        if (MFX_ERR_MORE_SURFACE == sts) {
            if (uninitId == m_nDepIdStart) {
                m_nDepIdCurrent = m_nDepIdStart = in->Info.FrameId.DependencyId;
            } 
            ++m_nDepIdCurrent;
        }
        else if (sts != MFX_WRN_DEVICE_BUSY) { //either other warning or ok, means frame accepted by vpp
            m_nDepIdStart = uninitId;
        }

        return sts;
    }
};