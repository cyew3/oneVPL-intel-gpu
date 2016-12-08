//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_MAIN_H
#define __MFX_VPP_MAIN_H

using namespace std;

#include "mfxvideo++int.h"

#include "mfx_task.h"
#include "mfx_vpp_base.h"

/* ******************************************************************** */
/*                 Main (High Level) Class of MSDK VPP                  */
/* ******************************************************************** */

class VideoVPPMain : public VideoVPP {
public:

    static mfxStatus Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE* core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    VideoVPPMain(VideoCORE *core, mfxStatus* sts);

    virtual ~VideoVPPMain();

    // VideoVPP
    virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux);

    // VideoBase methods
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        MFX_CHECK_NULL_PTR1( par );

        return m_impl.get()
            ? m_impl->Reset(par)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus Close(void);

    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return m_impl.get()
            ? m_impl->GetVideoParam(par)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus GetVPPStat(
        mfxVPPStat *stat)
    {
        return m_impl.get()
            ? m_impl->GetVPPStat(stat)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus VppFrameCheck(
        mfxFrameSurface1 *in, 
        mfxFrameSurface1 *out, 
        mfxExtVppAuxData *aux,
        MFX_ENTRY_POINT pEntryPoint[],
        mfxU32 &numEntryPoints);

    virtual mfxStatus VppFrameCheck(
        mfxFrameSurface1 *, 
        mfxFrameSurface1 *)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

    // multi threading of SW_VPP functions
    mfxStatus RunVPPTask(
        mfxFrameSurface1 *in, 
        mfxFrameSurface1 *out, 
        FilterVPP::InternalParam *pParam );

    mfxStatus ResetTaskCounters();


private:

    // Opaq processing
    mfxFrameSurface1 *GetNativeSurface(mfxFrameSurface1 *surface, mfxU32 type )
    {
        if( m_bOpaqMode[type] )
        {
            mfxFrameSurface1 * nativeSurface = m_core->GetNativeSurface(surface);
            if( surface && nativeSurface )
            {
                nativeSurface->Data.FrameOrder = surface->Data.FrameOrder;
                nativeSurface->Data.TimeStamp = surface->Data.TimeStamp;
                //nativeSurface->Data.Corrupted = surface->Data.Corrupted;
                //nativeSurface->Data.DataFlag = surface->Data.DataFlag;
                nativeSurface->Data.NumExtParam = surface->Data.NumExtParam;
                nativeSurface->Data.ExtParam = surface->Data.ExtParam;
                nativeSurface->Info = surface->Info;
            }

            return nativeSurface;
        }

        return surface;
    }

    VideoCORE* m_core;
    std::auto_ptr<VideoVPP> m_impl;

    // opaque processing
    bool  m_bOpaqMode[2];
    mfxFrameAllocResponse m_responseOpaq[2];
};

#endif // __MFX_VPP_MAIN_H
#endif // MFX_ENABLE_VPP
/* EOF */
