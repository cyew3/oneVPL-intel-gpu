/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.


File Name: mfx_ivideo_encode.h

\* ****************************************************************************** */

#pragma once

#include "mfx_iproxy.h"
#include "mfx_query_interface.h"

//mfxvideoencode with virtual member functions
class IVideoEncode : public EnableProxyForThis<IVideoEncode>
{
    friend class InterfaceProxy<IVideoEncode>;
public:
    /* Safe dynamic cast analog.
     * dvrogozh: implementation moved to the mfx_pipeline_dec.cpp since
     * declaring here specialization is invalid.
     */
    template <class TTo>
    TTo* GetInterface() {
        TTo *pInterface = NULL;
        if (!QueryInterface(QueryInterfaceMap<TTo>::id, (void**)&pInterface)) {
            return NULL;
        }
        return pInterface;
    }

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

protected:
    virtual bool      QueryInterface(int interface_id_registered_with_interface_map, void **ppInterface) = 0;
};

//proxy wrapper
template<>
class InterfaceProxy<IVideoEncode> 
    : public InterfaceProxyBase<IVideoEncode>
{
public:

    InterfaceProxy (std::unique_ptr<IVideoEncode> &&pTarget)
        : InterfaceProxyBase<IVideoEncode>(std::move(pTarget)){}
   mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) {
       return m_pTarget->Query(in, out);
   }
   mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) {
       return m_pTarget->QueryIOSurf(par, request);
   }
   mfxStatus Init(mfxVideoParam *par) {
       return m_pTarget->Init(par);
   }
   mfxStatus Reset(mfxVideoParam *par) {
       return m_pTarget->Reset(par);
   }
   mfxStatus Close(void) {
       return m_pTarget->Close();
   }
   mfxStatus GetVideoParam(mfxVideoParam *par) {
       return m_pTarget->GetVideoParam(par);
   }
   mfxStatus GetEncodeStat(mfxEncodeStat *stat) {
       return m_pTarget->GetEncodeStat(stat);
   }
   mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp) {
       return m_pTarget->EncodeFrameAsync(ctrl, surface, bs, syncp);
   }
   mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) {
       return m_pTarget->SyncOperation(syncp, wait);
   }
   bool      QueryInterface(int interface_id_registered_with_interface_map, void **ppInterface) {
       return m_pTarget->QueryInterface(interface_id_registered_with_interface_map, ppInterface);
   }
};
