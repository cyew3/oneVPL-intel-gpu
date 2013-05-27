/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.


File Name: mfx_ivideo_encode.h

\* ****************************************************************************** */

#pragma once

#include "mfx_iproxy.h"

//mfxvideoencode with virtual member functions
class IVideoEncode : EnableProxyForThis<IVideoEncode>
{
public:
    virtual ~IVideoEncode(void) { }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)  = 0;
    virtual mfxStatus Init(mfxVideoParam *par)  = 0;
    virtual mfxStatus Reset(mfxVideoParam *par)  = 0;
    virtual mfxStatus Close(void)  = 0;
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)  = 0;
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat)  = 0;
    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)  = 0;
    //not in mediasdk c++ wrappers however behavior is simpler if we include this into interface, to not create syncpoints maps
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) = 0;
};

//proxy wrapper
template<>
class InterfaceProxy<IVideoEncode> 
    : public InterfaceProxyBase<IVideoEncode>
{
public:

    InterfaceProxy (IVideoEncode * pTarget)
        : InterfaceProxyBase<IVideoEncode>(pTarget){}
   mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) 
   {
       return m_pTarget->Query(in, out);
   }
   mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) 
   {
       return m_pTarget->QueryIOSurf(par, request);
   }
   mfxStatus Init(mfxVideoParam *par) 
   {
       return m_pTarget->Init(par);
   }
   mfxStatus Reset(mfxVideoParam *par) 
   {
       return m_pTarget->Reset(par);
   }
   mfxStatus Close(void) 
   {
       return m_pTarget->Close();
   }
   mfxStatus GetVideoParam(mfxVideoParam *par) 
   {
       return m_pTarget->GetVideoParam(par);
   }
   mfxStatus GetEncodeStat(mfxEncodeStat *stat) 
   {
       return m_pTarget->GetEncodeStat(stat);
   }
   mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp) 
   {
       return m_pTarget->EncodeFrameAsync(ctrl, surface, bs, syncp);
   }
   mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait)
   {
       return m_pTarget->SyncOperation(syncp, wait);
   }
};
