/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_render.h"
#include "mfx_ivpp.h"
#include "mfx_iyuv_source.h"
#include "mfx_ivideo_encode.h"

//parameters trait for unified execute function
template<class T>
class ExecuteType
{
};

//has single function execute for mediasdk execution functions, that can be called externally
//other functions with same interface are supported already in InterfaceProxy
//has single function Init : due to interfaces specific
//has single function GetVideoparam : due to interfaces specific

template<class T>
class UnifiedExecuteProxy
{
};

//////////////////////////////////////////////////////////////////////////
//traits section
//DECODE

template<>
class ExecuteType<IYUVSource>
    : private mfx_no_assign
{
public:
    mfxBitstream2& m_bs;
    mfxFrameSurface1 *m_surface_work;
    mfxFrameSurface1 **m_surface_out;
    mfxSyncPoint *m_syncp;

    ExecuteType(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
        : m_bs(bs), m_surface_work(surface_work), m_surface_out(surface_out), m_syncp(syncp)
    {
    }
};

template<>
class UnifiedExecuteProxy<IYUVSource>
    : public InterfaceProxy<IYUVSource>
{
public:
    UnifiedExecuteProxy(std::unique_ptr<IYUVSource> &&p)
        : InterfaceProxy<IYUVSource>(std::move(p))
    {}
    virtual mfxStatus DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        return Execute (ExecuteType<IYUVSource>(bs, surface_work, surface_out, syncp));
    }
    virtual mfxStatus Execute(const ExecuteType<IYUVSource>& args)
    {
        return InterfaceProxy<IYUVSource>::DecodeFrameAsync(args.m_bs, args.m_surface_work, args.m_surface_out, args.m_syncp);
    }
};


//////////////////////////////////////////////////////////////////////////
//VPP
template<>
class ExecuteType<IMFXVideoVPP>
{
public:
    mfxFrameSurface1 *m_in;
    mfxFrameSurface1 *m_out;
    mfxFrameSurface1 *m_work;
    mfxExtVppAuxData *m_aux;
    mfxSyncPoint     *m_syncp;
    ExecuteType(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
        : m_in(in), m_out(out), m_aux(aux), m_syncp(syncp)
    {
    }
    ExecuteType(mfxFrameSurface1 *in,  mfxFrameSurface1 *work, mfxFrameSurface1 **out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
        : m_in(in), m_out(*out), m_work(work), m_aux(aux), m_syncp(syncp)
    {
    }
};

template<>
class UnifiedExecuteProxy<IMFXVideoVPP>
    : public InterfaceProxy<IMFXVideoVPP>
{
public:
    UnifiedExecuteProxy(IMFXVideoVPP *p)
        : InterfaceProxy<IMFXVideoVPP>(p)
    {}
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
    {
        return Execute (ExecuteType<IMFXVideoVPP>(in, out, aux, syncp));
    }
    virtual mfxStatus RunFrameVPPAsyncEx(mfxFrameSurface1 *in, mfxFrameSurface1 *work, mfxFrameSurface1 **out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
    {
        return Execute (ExecuteType<IMFXVideoVPP>(in, work, out, aux, syncp));
    }
    virtual mfxStatus Execute(const ExecuteType<IMFXVideoVPP>& args)
    {
        return InterfaceProxy<IMFXVideoVPP>::RunFrameVPPAsync(args.m_in, args.m_out, args.m_aux, args.m_syncp);
    }
};

//////////////////////////////////////////////////////////////////////////
//RENDER

template<>
class ExecuteType<IMFXVideoRender>
{
public:
    mfxFrameSurface1 *m_surface;
    mfxEncodeCtrl *m_pCtrl;
    ExecuteType(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
        : m_surface(surface), m_pCtrl(pCtrl)
    {
    }
};

template<>
class UnifiedExecuteProxy<IMFXVideoRender>
    : public InterfaceProxy<IMFXVideoRender>
{
public:
    UnifiedExecuteProxy(IMFXVideoRender *p)
        : InterfaceProxy<IMFXVideoRender>(p)
    {}
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
    {
        return Execute (ExecuteType<IMFXVideoRender>(surface, pCtrl));
    }
    virtual mfxStatus Execute(const ExecuteType<IMFXVideoRender>& args)
    {
        return InterfaceProxy<IMFXVideoRender>::RenderFrame(args.m_surface, args.m_pCtrl);
    }
};

//////////////////////////////////////////////////////////////////////////
//encoder

template<>
class ExecuteType<IVideoEncode>
{
public:
    mfxEncodeCtrl * m_pCtrl;
    mfxFrameSurface1 *m_surface;
    mfxBitstream *m_pBs;
    mfxSyncPoint *m_pSync;

    ExecuteType(mfxEncodeCtrl * pCtrl, mfxFrameSurface1 *surface, mfxBitstream *pBs, mfxSyncPoint *pSync)
        : m_pCtrl(pCtrl), m_surface(surface), m_pBs(pBs), m_pSync(pSync)
    {
    }
};

template<>
class UnifiedExecuteProxy<IVideoEncode>
    : public InterfaceProxy<IVideoEncode>
{
public:
    UnifiedExecuteProxy(std::unique_ptr<IVideoEncode> &&p)
        : InterfaceProxy<IVideoEncode>(std::move(p))
    {}
    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl * pCtrl, mfxFrameSurface1 *surface, mfxBitstream *pBs, mfxSyncPoint *pSync)
    {
        return Execute (ExecuteType<IVideoEncode>(pCtrl, surface, pBs, pSync));
    }

    virtual mfxStatus Execute(const ExecuteType<IVideoEncode>& args)
    {
        return InterfaceProxy<IVideoEncode>::EncodeFrameAsync(args.m_pCtrl, args.m_surface, args.m_pBs, args.m_pSync);
    }
};
