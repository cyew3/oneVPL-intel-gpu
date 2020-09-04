/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_RND_WRAP_H
#define __MFX_RND_WRAP_H

#include "mfx_ivideo_render.h"
#include "mfx_iproxy.h"

// interface proxy
template <>
class InterfaceProxy<IMFXVideoRender>
    : public InterfaceProxyBase<IMFXVideoRender>
{
public:
    InterfaceProxy(IMFXVideoRender* pTarget)
        : InterfaceProxyBase<IMFXVideoRender>(pTarget)
    {
    }
    mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->Query(in, out));
        return MFX_ERR_NONE;
    }
    mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->QueryIOSurf(par, request));
        return MFX_ERR_NONE;
    }
    mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->Init(par, pFilename));
        return MFX_ERR_NONE;
    }
    mfxStatus Close()
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->Close());
        return MFX_ERR_NONE;
    }
    mfxStatus Reset(mfxVideoParam *par)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->Reset(par));
        return MFX_ERR_NONE;
    }
    mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->GetVideoParam(par));
        return MFX_ERR_NONE;
    }
    mfxStatus GetEncodeStat(mfxEncodeStat *stat)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->GetEncodeStat(stat));
        return MFX_ERR_NONE;
    }
    mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
    {
        if (NULL == m_pTarget.get())
            return MFX_ERR_NONE;

        MFX_CHECK_STS(m_pTarget->RenderFrame(surface, pCtrl));
        return MFX_ERR_NONE;
    }
    mfxStatus GetDevice(IHWDevice **ppDevice)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        //get handle used by mfx_player and handle abses isn't and actual error indicator
        return m_pTarget->GetDevice(ppDevice);
    }
    mfxStatus WaitTasks(mfxU32 nMilisecconds)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        //err_in_execution code - doesnt mean an error in this scenario
        mfxStatus sts ;
        MFX_CHECK_STS_SKIP(sts = m_pTarget->WaitTasks(nMilisecconds), MFX_WRN_IN_EXECUTION);
        return sts;
    }
    mfxStatus SetOutputFourcc(mfxU32 nFourCC)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->SetOutputFourcc(nFourCC));
        return MFX_ERR_NONE;
    }
    mfxStatus SetAutoView(bool bIsAutoViewRender)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->SetAutoView(bIsAutoViewRender));
        return MFX_ERR_NONE;
    }
    mfxStatus GetDownStream(IFile **ppFile)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->GetDownStream(ppFile));
        return MFX_ERR_NONE;
    }
    mfxStatus SetDownStream(IFile *ppFile)
    {
        MFX_CHECK_POINTER(m_pTarget.get());
        MFX_CHECK_STS(m_pTarget->SetDownStream(ppFile));
        return MFX_ERR_NONE;
    }
    InterfaceProxy<IMFXVideoRender> * Clone()
    {
        MFX_CHECK_WITH_ERR(NULL != m_pTarget.get(), NULL);

        std::auto_ptr<IMFXVideoRender> pObj;
        MFX_CHECK_WITH_ERR((pObj.reset(m_pTarget->Clone()), pObj.get()), NULL);

        std::auto_ptr<InterfaceProxy<IMFXVideoRender> > pDecorator;
        MFX_CHECK_WITH_ERR((pDecorator.reset(new InterfaceProxy<IMFXVideoRender>(pObj.release())), pDecorator.get()), NULL);

        return pDecorator.release();
    }

};


#endif//__MFX_RND_WRAP_H
