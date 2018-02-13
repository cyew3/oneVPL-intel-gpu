//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_stub_dec_plugin.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"

#include <mfx_trace.h>

#include "mfx_session.h"
#include "mfx_utils.h"

#ifndef MFX_VA
MFXStubDecoderPlugin::MFXStubDecoderPlugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    m_createdByDispatcher = CreateByDispatcher;
}

MFXStubDecoderPlugin::~MFXStubDecoderPlugin()
{
    if (m_session)
    {
        PluginClose();
    }
}

mfxStatus MFXStubDecoderPlugin::PluginInit(mfxCoreInterface *core)
{
    MFX_CHECK_NULL_PTR1(core);

    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);
    MFX_CHECK_STS(mfxRes);

#if !defined (MFX_VA)
    par.Impl = MFX_IMPL_SOFTWARE;
#endif

    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);
    MFX_CHECK_STS(mfxRes);

    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pmfxCore->pthis, m_session);
    MFX_CHECK_STS(mfxRes);

    return mfxRes;
}

mfxStatus MFXStubDecoderPlugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;
    if (m_session)
    {
        //The application must ensure there is no active task running in the session before calling this (MFXDisjoinSession) function.
        mfxRes = MFXVideoDECODE_Close(m_session);
        //Return the first met wrn or error
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED)
            mfxRes2 = mfxRes;
        mfxRes = MFXInternalPseudoDisjoinSession(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
            mfxRes2 = mfxRes;
        mfxRes = MFXClose(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
            mfxRes2 = mfxRes;
        m_session = 0;
    }
    if (m_createdByDispatcher) {
        delete this;
    }

    return mfxRes2;
}
#else
static
mfxVideoCodecPlugin* GetVideoCodecInstance()
{
    static mfxVideoCodecPlugin instance{};
    return &instance;
}

mfxStatus MFXStubDecoderPlugin::CreateByDispatcher(mfxPluginUID, mfxPlugin* mfxPlg)
{
    MFX_CHECK_NULL_PTR1(mfxPlg);

    memset(mfxPlg, 0, sizeof(mfxPlugin));

    mfxPlg->PluginInit     = _PluginInit;
    mfxPlg->PluginClose    = _PluginClose;
    mfxPlg->Submit         = _Submit;
    mfxPlg->Execute        = _Execute;
    mfxPlg->FreeResources  = _FreeResources;

    mfxPlg->Video                = GetVideoCodecInstance();
    mfxPlg->Video->Query         = _Query;
    mfxPlg->Video->QueryIOSurf   = _QueryIOSurf;
    mfxPlg->Video->Init          = _Init;
    mfxPlg->Video->Reset         = _Reset;
    mfxPlg->Video->Close         = _Close;
    mfxPlg->Video->GetVideoParam = _GetVideoParam;

    mfxPlg->Video->DecodeHeader      =  _DecodeHeader;
    mfxPlg->Video->GetPayload        =  _GetPayload;
    mfxPlg->Video->DecodeFrameSubmit =  _DecodeFrameSubmit;

    return MFX_ERR_NONE;
}
#endif

#ifndef MFX_VA
template <typename T>
mfxStatus MFXStubDecoderPlugin::CreateInstance(T** obj)
{
    MFX_CHECK_NULL_PTR1(obj);

    std::unique_ptr<T> tmp_pplg;
    try
    {
        tmp_pplg.reset(new T(false));
    }
    catch (std::bad_alloc const&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch (MFX_CORE_CATCH_TYPE)
    {
        return MFX_ERR_UNKNOWN;
    }

    try
    {
        tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXDecoderPlugin>(tmp_pplg.get()));
    }
    catch (std::bad_alloc const&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch (MFX_CORE_CATCH_TYPE)
    {
        return MFX_ERR_UNKNOWN;
    }

    *obj = tmp_pplg.release();
    return MFX_ERR_NONE;
}

/* explicit instantiation for plugins*/

#if defined( AS_HEVCD_PLUGIN )
#include "mfx_hevc_dec_plugin.h"
template
mfxStatus MFXStubDecoderPlugin::CreateInstance<MFXHEVCDecoderPlugin>(MFXHEVCDecoderPlugin**);
#endif

#if defined( AS_VP8D_PLUGIN )
#include "mfx_vp8_dec_plugin.h"
template
mfxStatus MFXStubDecoderPlugin::CreateInstance<MFXVP8DecoderPlugin>(MFXVP8DecoderPlugin**);
#endif

#if defined( AS_VP9D_PLUGIN )
#include "mfx_vp9_dec_plugin.h"
template
mfxStatus MFXStubDecoderPlugin::CreateInstance<MFXVP9DecoderPlugin>(MFXVP9DecoderPlugin**);
#endif

#endif