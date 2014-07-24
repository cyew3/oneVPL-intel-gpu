/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#pragma once

#include "mfx_ivpp.h"
#include "mfx_multi_vpp.h"

//adapter to samples virtual vpp class
class MFXVideoVPPAdapter 
    : public IMFXVideoVPP
{
    static const mfxFrameInfo ZEROInfo;

    std::auto_ptr<MFXVideoMultiVPP> m_pVirual;
    
    //since it is a pointers the actual data is stored 
    //externally and will be updated between ctor() and init() calls
    mfxFrameInfo *m_pInfoInput;
    mfxFrameInfo *m_pInfoOutput;

public:
    MFXVideoVPPAdapter (MFXVideoMultiVPP *pVirtualvpp,
                        mfxFrameInfo  *pluginInputInfo, 
                        mfxFrameInfo  *pluginOutputInfo)
        : m_pVirual(pVirtualvpp)
        , m_pInfoInput(pluginInputInfo)
        , m_pInfoOutput(pluginOutputInfo)
    {
    }
    
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        return m_pVirual->Query(in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest request[2])
    {
        return m_pVirual->QueryIOSurf(par, request);
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        mfxFrameInfo *in = NULL;
        mfxFrameInfo *out = NULL;
        
        //check vpp enabling in virtual vpp
        //since virtual vpp expected null pointer if pre and post vpp will be disabled
        //TODO: follow DRY principle
        if (m_pInfoInput && 
            (m_pInfoInput->FourCC != 0 ||
             m_pInfoInput->CropW != 0 ||
             m_pInfoInput->CropH != 0))
        {
            in  = m_pInfoInput;
        }

        if (m_pInfoOutput && 
            (m_pInfoOutput ->FourCC != 0 ||
            m_pInfoOutput ->CropW != 0 ||
            m_pInfoOutput ->CropH != 0))
        {
            out  = m_pInfoOutput ;
        }

        return m_pVirual->Init(par); //TODO: m_pInfoInput, m_pInfoOutput
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return m_pVirual->Reset(par);
    }
    virtual mfxStatus Close(void)
    {
        return m_pVirual->Close();
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return m_pVirual->GetVideoParam(par);
    }
    virtual mfxStatus GetVPPStat(mfxVPPStat *stat)
    {
        return m_pVirual->GetVPPStat(stat);
    }
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
    {
        return m_pVirual->RunFrameVPPAsync(in, out, aux, syncp);
    }
    virtual mfxStatus RunFrameVPPAsyncEx(mfxFrameSurface1 *in, mfxFrameSurface1 *work, mfxFrameSurface1 **out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
    {
        return m_pVirual->RunFrameVPPAsyncEx(in, work, out, aux, syncp);
    }
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait)
    {
        return m_pVirual->SyncOperation(syncp, wait);
    }
};
#endif // #if defined(_WIN32) || defined(_WIN64) 

